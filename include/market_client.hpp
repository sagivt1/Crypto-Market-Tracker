#pragma once
#include <string>
#include <optional>
#include <vector>

/// @brief Holds all relevant data for a single cryptocurrency.
struct CoinData
{
    std::string id;
    double current_price;
    std::vector<double> price_history;
};

/// @brief A client for interacting with the CoinGecko cryptocurrency API.
class MarketClient {
public:
    /// @brief Parses a JSON string to extract the current price of a coin.
    /// @param json_body The raw JSON response from the API.
    /// @param coin_id The API identifier for the coin (e.g., "bitcoin").
    /// @return A CoinData object with price info, or nullopt on failure.
    static std::optional<CoinData> parse_coin_price(const std::string& json_body, const std::string& coin_id);

    /// @brief Parses a JSON string to extract historical price points.
    /// @param json_body The raw JSON response from the market_chart endpoint.
    /// @return A vector of price points. Returns an empty vector on failure.
    static std::vector<double> parse_history(const std::string& json_body);

    /// @brief Fetches both the current price and 24-hour history for a coin.
    /// @param coin_id The API identifier for the coin.
    /// @return A complete CoinData object, or nullopt on network/API failure.
    std::optional<CoinData> get_coin_data(const std::string& coin_id);
};
