#include "market_client.hpp"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <print>
#include <format>

using json = nlohmann::json;

std::optional<CoinData> MarketClient::parse_coin_price(const std::string& json_body, const std::string& coin_id) {
    try {
        auto parsed = json::parse(json_body);
        
        // Check if coin exists
        if(parsed.contains(coin_id) && parsed[coin_id].contains("usd")) {
            double price = parsed[coin_id]["usd"];
            return CoinData{coin_id, price};
        } 
    } catch (const std::exception& e) {
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
                // point[0] is timestamp, point[1] is the price
                if(point.size() > 1) {
                    prices.push_back(point[1]);
                }
            }
        }
    } catch(...) {/* Silent fail */};
    return prices;
    
}

std::optional<CoinData> MarketClient::get_coin_data(const std::string& coin_id) {
    // Construct API URL
    std::string url = std::format("https://api.coingecko.com/api/v3/simple/price?ids={}&vs_currencies=usd", coin_id);

    // Blocking network call
    cpr::Response r = cpr::Get(cpr::Url{url}, cpr::VerifySsl(false));

    if(r.status_code != 200) {
        return std::nullopt;
    }
    auto basic_data = parse_coin_price(r.text, coin_id);
    if(!basic_data) return std::nullopt;

    std::string history_url = std::format("https://api.coingecko.com/api/v3/coins/{}/market_chart?vs_currency=usd&days=1", coin_id);
    cpr::Response history_r = cpr::Get(cpr::Url{history_url}, cpr::VerifySsl(false));
    if(history_r.status_code == 200) {
        basic_data->price_history = parse_history(history_r.text);
    
    }
    
    return basic_data;
}