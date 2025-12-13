#include "persistence.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

void save_coins(const std::vector<CoinDef>& coins) {
    try{
        json j  = json::array();
        for(auto const& coin : coins) {
            j.push_back({
                {"name", coin.name},
                {"ticker", coin.ticker},
                {"api_id", coin.api_id}
            });
        }
        std::ofstream file("coins.json");
        file << j.dump(4); // Use 4-space indentation for readability.
    } catch(...) {} // Silently fail on file I/O error; not a critical operation.
}

std::vector<CoinDef> load_coins() {
    std::vector<CoinDef> coins;
    try {
            std::ifstream file("coins.json");
            
            if(file.is_open()) {
                json j;
                file >> j;
                for(auto const& coin : j) { 
                    coins.push_back({
                        coin["name"],
                        coin["ticker"],
                        coin["api_id"]
                    });
                }
            }
        } catch(...) {} // Silently fail on parse/read error.

    if(coins.empty()) {
        // If no file exists or it's empty, provide a default list for first-time users.
        coins = {
        {"Bitcoin",  "BTC", "bitcoin"},
        {"Ethereum", "ETH", "ethereum"},
        {"Solana",   "SOL", "solana"},
        {"Dogecoin", "DOGE","dogecoin"},
        {"Cardano",  "ADA", "cardano"},
        {"Polkadot", "DOT", "polkadot"}
        };
    }
    return coins;
}

void save_portfolio(const std::map<std::string, PortfolioEntry>& portfolio) {
    try {
        json j;
        for(auto const& [key, entry] : portfolio) {
            j[key] = {
                {"amount", entry.amount},
                {"buyPrice", entry.buyPrice}
            };
        }
        std::ofstream file("portfolio.json");
        file << j.dump(4); // Use 4-space indentation for readability.
    } catch (...) {
        std::cerr << "Error saving portfolio \n";
    }
}

std::map<std::string, PortfolioEntry> load_portfolio() {
    std::map<std::string, PortfolioEntry> portfolio;
    try {
        std::ifstream file("portfolio.json");
        if(file.is_open()) {
            json j;
            file >> j;
            for(auto& element : j.items()) {
               portfolio[element.key()] = {
                    element.value()["amount"].get<double>(),
                    element.value()["buyPrice"].get<double>()
               };
            }
        }
    } catch (...) {
        // Fail gracefully if file is corrupt/missing; a new one is created on next save.
        std::cerr << "Error loading portfolio - No portfolio found (creating new file). \n";
    }
    return portfolio;
}