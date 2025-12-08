#include "market_client.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <print>
#include <format>
#include <iostream>


using json = nlohmann::json;

std::map<std::string, double> MarketClient::get_multi_price(const std::vector<std::string>& coin_ids) {
    std::string joinsIds = "";
    // The CoinGecko API requires a comma-separated string of coin IDs for batch requests.
    for( auto const& id : coin_ids) {
        if(!joinsIds.empty())
            joinsIds += ",";
        joinsIds += id;
    }

    std::println("Batch fetching : {}", joinsIds); // DEBUG

    std::string url = std::format("https://api.coingecko.com/api/v3/simple/price?ids={}&vs_currencies=usd", joinsIds);
    
    // WARNING: Disabling SSL verification is insecure. For production, use a proper certificate bundle.
    cpr::Response r = cpr::Get(cpr::Url{url}, cpr::VerifySsl(false));

    if(r.status_code == 200) {
        return parse_multi_price(r.text);
    } 
    
    std::println(stderr, "Price Error: Status {}", r.status_code);
    return {};

}

std::map<std::string, double> MarketClient::parse_multi_price(const std::string& json_body) {
    std::map<std::string, double> results;
    try {
        auto parsed = json::parse(json_body);
        for(auto& [key, value] : parsed.items()) {
            if(value.contains("usd")) {
                results[key] = value["usd"];
            }
        }

    } catch(...) {
        // Silently fail on parse error. The caller is expected to handle an empty map.
    }

    return results;
}

std::optional<CoinData> MarketClient::parse_coin_price(const std::string& json_body, const std::string& coin_id) {
    try {
        auto parsed = json::parse(json_body);
        
        // API response nests the price inside an object with the coin's ID as the key.
        if(parsed.contains(coin_id) && parsed[coin_id].contains("usd")) {
            double price = parsed[coin_id]["usd"];
            return CoinData{coin_id, price};
        } 
    } catch (const std::exception& e) {
        // A malformed JSON string will throw, indicating a potential API or network issue.
        std::println(stderr, "JSON Parsing Error: {}", e.what());
    }
    return std::nullopt;
}

std::vector<double> MarketClient::parse_history(const std::string& json_body) {
    std::vector<double> prices;
    try {
        auto parsed = json::parse(json_body);
        if(parsed.contains("prices")) {
            for(auto const& point : parsed["prices"]) {
                // The API returns pairs of [timestamp, price]. We only need the price.
                if(point.size() > 1) {
                    prices.push_back(point[1]);
                }
            }
        }
    } catch(...) {
        // Silently fail on parse error; the chart will simply show "No data".
    };
    return prices;
    
}

std::optional<CoinData> MarketClient::get_coin_data(const std::string& coin_id) {

    std::println("Fetching data for: {}", coin_id); // DEBUG

    std::string url = std::format("https://api.coingecko.com/api/v3/simple/price?ids={}&vs_currencies=usd", coin_id);

    // This is a blocking network call, intended to be run in a separate thread.
    // WARNING: Disabling SSL verification is insecure. For production, use a proper certificate bundle.
    cpr::Response r = cpr::Get(cpr::Url{url}, cpr::VerifySsl(false));

    if(r.status_code != 200) {
        std::println(stderr, "Price Error [{}]: Status {}", coin_id, r.status_code);
        return std::nullopt;
    }
    // First, get the current price. If this fails, no point in getting history.
    auto basic_data = parse_coin_price(r.text, coin_id);
    if(!basic_data) return std::nullopt;

    // If price is fetched, proceed to get 24-hour historical data.
    std::string history_url = std::format("https://api.coingecko.com/api/v3/coins/{}/market_chart?vs_currency=usd&days=1", coin_id);
    cpr::Response history_r = cpr::Get(cpr::Url{history_url}, cpr::VerifySsl(false));
    if(history_r.status_code == 200) {
        basic_data->price_history = parse_history(history_r.text);
        std::println("Success! Got {} history points.", basic_data->price_history.size()); // DEBUG
    }
    else {
        std::println(stderr, "History Error [{}]: Status {}", coin_id, history_r.status_code);
    }
    
    return basic_data;
}

std::vector<CoinDef> MarketClient::parse_search_result(const std::string& json_body) {
    std::vector<CoinDef> results;
    try {
        auto parsed = json::parse(json_body);
        if(parsed.contains("coins")) {
            for(auto const& coin : parsed["coins"]) {
                CoinDef def;

                def.api_id = coin["id"];
                def.name = coin["name"];
                def.ticker = coin["symbol"];

                // Standardize ticker to uppercase for display consistency.
                for(auto& c : def.ticker) {
                    c = toupper(c);
                }

                // Ensure the coin has a valid API ID before adding it to results.
                if(!def.api_id.empty()) {
                    results.push_back(def);
                }
            }
        }
    } catch(...) {
        // Silently fail on parse error, returning whatever was parsed so far.
    }
    return results;
}

std::vector<CoinDef> MarketClient::search_coins(const std::string& query) {
    std::println("Searching for: {}", query); // DEBUG

    std::string url = std::format("https://api.coingecko.com/api/v3/search?query={}", query);
    // WARNING: Disabling SSL verification is insecure. For production, use a proper certificate bundle.
    cpr::Response r = cpr::Get(cpr::Url{url}, cpr::VerifySsl(false));

    if(r.status_code == 200) {
        return parse_search_result(r.text);
    }

    std::println(stderr, "Search Error: Status {}", r.status_code);
    return {};
}