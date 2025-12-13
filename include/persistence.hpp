#pragma once
#include "market_client.hpp"
#include <string>
#include <vector>
#include <map>

struct PortfolioEntry {
    double amount = 0.0;
    double buyPrice = 0.0;
};

void save_coins(const std::vector<CoinDef>& coins);
std::vector<CoinDef> load_coins();
void save_portfolio(const std::map<std::string, PortfolioEntry>& portfolio);
std::map<std::string, PortfolioEntry> load_portfolio();