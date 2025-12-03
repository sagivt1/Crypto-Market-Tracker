#include "market_client.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <print>
#include <format>
#include <iostream>


using json = nlohmann::json;

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

    std::println("Fetching data for: {}", coin_id); // DEBUG PRINT

    // Construct API URL
    std::string url = std::format("https://api.coingecko.com/api/v3/simple/price?ids={}&vs_currencies=usd", coin_id);

    // This is a blocking network call. It's intended to be run in a separate thread.
    cpr::Response r = cpr::Get(cpr::Url{url}, cpr::VerifySsl(false));

    if(r.status_code != 200) {
        std::println(stderr, "Price Error [{}]: Status {}", coin_id, r.status_code);
        return std::nullopt;
    }
    // First, get the current price. If this fails, no point in getting history.
    auto basic_data = parse_coin_price(r.text, coin_id);
    if(!basic_data) return std::nullopt;

    // If current price is fetched, proceed to get the 24-hour historical data.
    std::string history_url = std::format("https://api.coingecko.com/api/v3/coins/{}/market_chart?vs_currency=usd&days=1", coin_id);
    cpr::Response history_r = cpr::Get(cpr::Url{history_url}, cpr::VerifySsl(false));
    if(history_r.status_code == 200) {
        // If history fetch is successful, append it to our data object.
        basic_data->price_history = parse_history(history_r.text);
        std::println("Success! Got {} history points.", basic_data->price_history.size());
    }
    else {
        std::println(stderr, "History Error [{}]: Status {}", coin_id, history_r.status_code);
    }
    
    return basic_data;
}