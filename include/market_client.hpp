#pragma once
#include <string>
#include <optional>
#include <vector>

struct CoinData
{
    std::string id;
    double current_price;
    std::vector<double> price_history;
};


// A simple class to verify our build system and testing logic
class MarketClient {
public:
    // Parse string -> Object
    static std::optional<CoinData> parse_coin_price(const std::string& json_body, const std::string& coin_id);

    static std::vector<double> parse_history(const std::string& json_body);

    std::optional<CoinData> get_coin_data(const std::string& coin_id);

    
};
