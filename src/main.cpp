
#include "market_client.hpp"
#include <imgui.h>
#include <imgui-SFML.h>
#include <implot.h>
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <future>
#include <vector>
#include <iostream>
#include <format>
#include <fstream>
#include <map>

using namespace std::chrono_literals;
using json = nlohmann::json;

/// @brief Maps a user-facing coin name to its API identifier.
struct CoinDef {
    std::string name;   // User-friendly name for display, e.g., "Bitcoin".
    std::string ticker; // Common abbreviation, e.g., "BTC".
    std::string api_id; // Unique ID for the CoinGecko API, e.g., "bitcoin".
};

/// @brief Persists the user's asset holdings to a JSON file.
/// @param portfolio A map of coin API IDs to the amount owned.
void save_portfolio(const std::map<std::string, double>& portfolio) {
    try {
        json j;
        for(auto const& [key, value] : portfolio) {
            j[key] = value;
        }
        std::ofstream file("portfolio.json");
        file << j.dump(4); // Use 4-space indentation for readability.
    } catch (...) {
        std::cerr << "Error saving portfolio \n";
    }
}

/// @brief Loads the user's asset holdings from a JSON file.
/// @return A map of coin API IDs to the amount owned. Returns an empty map if the file doesn't exist or is invalid.
std::map<std::string, double> load_portfolio() {
    std::map<std::string, double> portfolio;
    try {
        std::ifstream file("portfolio.json");
        if(file.is_open()) {
            json j;
            file >> j;
            for(auto& [key, value] : j.items()) {
                portfolio[key] = value;
            }
        }
    } catch (...) {
        // Silently fail if the file is corrupt or missing; a new one will be created on the next save.
        std::cerr << "Error loading portfolio - No portfolio found (creating new file). \n";
    }
    return portfolio;
}

int main() {

    // --- Initialization ---
    // Create the main application window with a specific size and title.
    sf::RenderWindow window(sf::VideoMode(1000, 700), "Crypto Tracker");
    // Prevent excessive CPU usage when the app is idle.
    window.setFramerateLimit(60);

    // Binds the ImGui user interface to the SFML window.
    if(!ImGui::SFML::Init(window)){
        return 1;
    }

    ImPlot::CreateContext();

    // --- Application State & Data ---
    MarketClient client;
    CoinData current_data;
    std::string status = "Ready";
    sf::Clock delta_clock;

    std::vector<CoinDef> coins = {
        {"Bitcoin",  "BTC", "bitcoin"},
        {"Ethereum", "ETH", "ethereum"},
        {"Solana",   "SOL", "solana"},
        {"Dogecoin", "DOGE","dogecoin"},
        {"Cardano",  "ADA", "cardano"},
        {"Ripple",   "XRP", "ripple"},
        {"Polkadot", "DOT", "polkadot"}
    };

    std::map<std::string, double> portfolio = load_portfolio();

    int selected_index = -1;

    // --- Asynchronous Data Fetching State ---
    std::future<std::optional<CoinData>> futureResult;
    bool is_loading = true; // Tracks if a network request is in-flight to prevent duplicate requests.
    bool should_reset_axes = false; // Triggers the plot to rescale after new data arrives.

    std::future<std::optional<CoinData>> futureCoin;
    std::future<std::map<std::string, double>> futureBatch;

    std::vector<const char*> pieLabels;
    std::vector<double> pieValue;
    double totalNetWorth = 0.0;

    // Initial: Fetch Batch Prices for Overview
    std::vector<std::string> allIds;
    for(auto const& coin : coins) {
        allIds.push_back(coin.api_id);
    }
    futureBatch = std::async(std::launch::async, &MarketClient::get_multi_price, &client, allIds);

    // --- Main Application Loop ---
    while(window.isOpen()) {
        // --- Event Handling  ---
        // Process all pending events, such as mouse clicks, key presses, or window close requests.
        sf::Event event;
        while(window.pollEvent(event)) {
            // Pass the event to ImGui to handle its own interactions (e.g., clicking on a button).
            ImGui::SFML::ProcessEvent(window, event);
            // Check if the user has requested to close the window.
            if(event.type == sf::Event::Closed){
                window.close();
            }
        }

        // --- GUI Update & Drawing ---
        // Restart the clock and inform ImGui of the time delta for animations and UI responsiveness.
        ImGui::SFML::Update(window, delta_clock.restart());

        if(futureBatch.valid()) {
            if(futureBatch.wait_for(0s) == std::future_status::ready) {
                
                auto price = futureBatch.get();

                pieLabels.clear();
                pieValue.clear();
                totalNetWorth = 0.0;

                for(int i=0; i<coins.size(); i++) {
                    double amount = portfolio[coins[i].api_id];
                    if(amount > 0.00001) {
                        double val = amount * price[coins[i].api_id];
                        pieLabels.push_back(coins[i].ticker.c_str());
                        pieValue.push_back(val);
                        totalNetWorth += val;
                    }
                }
                
                status = "Portfolio Synced.";
                is_loading = false;
            }
        } 


        if(futureCoin.valid()) {
            if(futureCoin.wait_for(0s) == std::future_status::ready) {
                try {
                    auto result = futureCoin.get();
                    if(result) {
                        current_data = result.value();
                        status = "Updated: " + coins[selected_index].name;
                        should_reset_axes = true;
                    }
                } catch (...) {
                    status = "Error";
                }
                is_loading = false;
            }
        }

        // --- DASHBOARD LAYOUT ---
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        // Ensure the main window always fills the entire application window.
        ImGui::SetNextWindowSize(ImVec2((float)window.getSize().x, (float)window.getSize().y));
        ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        
        // Create a resizable two-column layout.
        if(ImGui::BeginTable("MainLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)){
            
            // --- SIDEBAR ---
            ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Analysis", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            
            if(ImGui::Selectable(" PORTFOLIO OVERVIEW", selected_index == -1)) {
                selected_index = -1;
                is_loading = true;
                status = "Updating Total Balance...";
                futureBatch = std::async(std::launch::async, &MarketClient::get_multi_price, &client, allIds);
            }

            // --- Column 1: Coin Selection ---
            
            ImGui::Spacing();
            ImGui::TextDisabled("COINS");
            ImGui::Separator();

            for(int i=0; i<coins.size(); i++) {
                double amount = portfolio[coins[i].api_id];
                std::string label = amount > 0.00001 ? std::format("{} ({:.2f})", coins[i].name, amount) : coins[i].name;
                
                if(ImGui::Selectable(label.c_str(), selected_index == i)) {
                    if(selected_index != i && !is_loading) {
                        selected_index = i;
                        is_loading = true;
                        status = "Fetching " + coins[i].name;
                        current_data.price_history.clear();
                        current_data.current_price = 0.0;
                        futureCoin = std::async(std::launch::async, &MarketClient::get_coin_data, &client, coins[i].api_id);   
                    }
                }
            }

            // --- Column 2: Main Content (Price & Chart) ---
            ImGui::TableSetColumnIndex(1);

            if(selected_index == -1) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Total Worth Net");
                ImGui::SetWindowFontScale(3.0f);
                ImGui::Text("$%.2f", totalNetWorth);
                ImGui::SetWindowFontScale(1.0f);

                ImGui::Separator();
                ImGui::Spacing();

                if(pieValue.empty()) {
                    ImGui::TextDisabled("No assets found. Select a coin to add holdings.");
                } else {
                    ImGui::Text("Asset Allocation");

                    if(ImPlot::BeginPlot("##Pie", ImVec2(-1, -1), ImPlotFlags_Equal | ImPlotFlags_NoMouseText)) {
                        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                        ImPlot::PlotPieChart(
                            pieLabels.data(), 
                            pieValue.data(), 
                            (int)pieValue.size(), 
                            0.5, 0.5, 0.35,         
                            "%.1f", 90 
                        );
                        ImPlot::EndPlot();
                    }
                }

            } else {

                ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "%s (%s)", coins[selected_index].name.c_str(), coins[selected_index].ticker.c_str());
                ImGui::Separator();
                
                if(is_loading && futureCoin.valid()) {

                    ImGui::Text("Loading Data");
                } else {
                    if(current_data.current_price > 0.0) {
                        ImGui::SetWindowFontScale(2.5f);
                        ImGui::Text("$%.2f", current_data.current_price);
                        ImGui::SetWindowFontScale(1.0f);
                    }

                    if(!current_data.price_history.empty()) {
                        if(should_reset_axes) {
                            ImPlot::SetNextAxesToFit();
                            should_reset_axes = false;
                        }

                        if(ImPlot::BeginPlot("24H Trend", ImVec2(-1, 350))) {
                            ImPlot::PlotLine("Price (USD)", current_data.price_history.data(), current_data.price_history.size());
                            ImPlot::EndPlot();
                        }
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("Portfolio");
                    double ownedAmount = portfolio[coins[selected_index].api_id];
                    ImGui::Text("Amount Owned");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(150);
                    if(ImGui::InputDouble("##Edit", &ownedAmount  , 0.0, 0.0, "%.6f")) {
                        if (ownedAmount < 0.0)
                        {
                            ownedAmount = 0.0;
                        }
                        portfolio[coins[selected_index].api_id] = ownedAmount;
                        save_portfolio(portfolio);     
                    }
                    if(current_data.current_price > 0) {
                        ImGui::SameLine();
                        ImGui::Text("$%.2f", ownedAmount * current_data.current_price);
                    }
                }
            }

            ImGui::EndTable();
        }
                
        ImGui::End(); 

        // --- Rendering ---
        window.clear();
        ImGui::SFML::Render(window);
        window.display();
    }

    // --- Shutdown ---
    ImPlot::DestroyContext();
    ImGui::SFML::Shutdown();
    return 0;
    
}