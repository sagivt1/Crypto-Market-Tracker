#pragma once
#include <string>
#include <optional>
#include <vector>
#include <map>
#include <future>

/// @brief Maps a user-facing coin name to its API identifier.
struct CoinDef {
    std::string name;   // User-friendly name for display, e.g., "Bitcoin".
    std::string ticker; // Common abbreviation, e.g., "BTC".
    std::string api_id; // Unique ID for the CoinGecko API, e.g., "bitcoin".
};

/// @brief Holds all relevant data for a single cryptocurrency.
struct CoinData
{
    std::string id;
    double current_price;
    std::vector<double> price_history;
    std::vector<double> time;
    std::vector<double> open;
    std::vector<double> high;
    std::vector<double> low;
    std::vector<double> close;
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

    /// @brief Parses a JSON string containing multiple coin prices.
    /// @param json_body The raw JSON response from the simple/price endpoint.
    /// @return A map of coin API IDs to their USD price.
    static std::map<std::string, double> parse_multi_price(const std::string& json_body);

    /// @brief Parses a JSON string from a coin search query.
    /// @param json_body The raw JSON response from the search endpoint.
    /// @return A vector of `CoinDef` objects matching the search.
    static std::vector<CoinDef> parse_search_result(const std::string& json_body);

    /// @brief Fetches both the current price and 24-hour history for a coin.
    /// @param coin_id The API identifier for the coin.
    /// @return A complete CoinData object, or nullopt on network/API failure.
    std::optional<CoinData> get_coin_data(const std::string& coin_id);

    /// @brief Fetches the current price for multiple coins in a single request.
    /// @param coin_ids A vector of API identifiers for the coins.
    /// @return A map of coin API IDs to their USD price. Returns an empty map on failure.
    std::map<std::string, double> get_multi_price(const std::vector<std::string>& coin_ids);

    /// @brief Searches for coins by name, ticker, or ID.
    /// @param query The search term.
    /// @return A vector of `CoinDef` objects matching the query. Returns an empty vector on failure.
    std::vector<CoinDef> search_coins(const std::string& query);

    /// @brief Parses a JSON string to extract OHLC (Open, High, Low, Close) data.
    /// @param json_body The raw JSON response from the ohlc endpoint.
    /// @param data The CoinData object to populate with OHLC values.
    static void parse_ohlc(const std::string& json_body, CoinData& data);

    /// @brief Fetches OHLC (Open, High, Low, Close) data for a coin for a specific period.
    /// @param coin_id The API identifier for the coin.
    /// @param data The CoinData object to populate with the fetched data.
    /// @return True on success, false on network/API failure.
    bool fetch_ohlc(const std::string& coin_id, CoinData& data);

    std::future<bool> fetch_ohlc_async(const std::string& coin_id, CoinData& data);
};
