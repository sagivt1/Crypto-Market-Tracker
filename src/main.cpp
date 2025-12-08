
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
        // Fail silently if file is corrupt/missing; a new one is created on next save.
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

    std::vector<CoinDef> coins = load_coins();
    std::map<std::string, double> portfolio = load_portfolio();

    int selected_index = -1;
    bool is_loading = true; // Tracks if a network request is in-flight to prevent duplicate requests.
    bool should_reset_axes = false; // Triggers the plot to rescale after new data arrives.

    // Futures for managing non-blocking network calls.
    std::future<std::optional<CoinData>> futureCoin;
    std::future<std::map<std::string, double>> futureBatch;
    std::future<std::vector<CoinDef>> futureSearch;

    std::vector<const char*> pieLabels;
    std::vector<double> pieValue;
    double totalNetWorth = 0.0;

    // Initial data fetch for the portfolio overview.
    std::vector<std::string> allIds;
    for(auto const& coin : coins) {
        allIds.push_back(coin.api_id);
    }
    futureBatch = std::async(std::launch::async, &MarketClient::get_multi_price, &client, allIds);

    char search_buffer[128] = "";
    std::vector<CoinDef> search_results;
    bool is_searching = false;

    bool openSearchPopup = false; // One-shot flag to trigger the search popup.

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

        // Poll the future without blocking the main thread.
        if(futureBatch.valid() && futureBatch.wait_for(0s) == std::future_status::ready) {    
            auto price = futureBatch.get();
            pieLabels.clear();
            pieValue.clear();
            totalNetWorth = 0.0;
            for(int i=0; i<coins.size(); i++) {
                double amount = portfolio[coins[i].api_id];
                // Filter out dust amounts to keep the pie chart clean.
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

        // Poll the future for single-coin data.
        if(futureCoin.valid() && futureCoin.wait_for(0s) == std::future_status::ready) {
            try {
                auto result = futureCoin.get();
                if(result.has_value()) {
                    current_data = result.value();
                    status = "Updated: " + coins[selected_index].name;
                    should_reset_axes = true;
                }
            } catch (...) {
                status = "Error";
            }
            is_loading = false;  
        }

        // Poll the future for search results.
        if(futureSearch.valid() && futureSearch.wait_for(0s) == std::future_status::ready) {
            search_results = futureSearch.get();
            is_searching = false;
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

            if(ImGui::Button("+ ADD COIN ", ImVec2(-1, 30))) {
                openSearchPopup = true;
            }

            ImGui::Separator();

            // A selected index of -1 is used as a sentinel for the main portfolio overview.
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
                std::string label = amount > 0.00001 ? std::format("{} ({:.2f})", coins[i].ticker, amount) : coins[i].ticker;
                
                if(ImGui::Selectable(label.c_str(), selected_index == i)) {
                    // Prevent re-fetching data for the same selection or while another request is active.
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
                CoinDef& c = coins[selected_index];
                ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "%s (%s)", coins[selected_index].name.c_str(), coins[selected_index].ticker.c_str());
                ImGui::SameLine();
                
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 100);
                if(ImGui::Button("Delete Coin")) {
                    portfolio.erase(c.api_id);
                    save_portfolio(portfolio);

                    coins.erase(coins.begin() + selected_index);
                    save_coins(coins);

                    selected_index = -1;
                }

                // After deletion, the selected index is invalid.
                // Bail out of this frame to prevent rendering with stale data.
                if(selected_index == -1) {
                    ImGui::EndTable();
                    ImGui::End();
                    window.clear();
                    ImGui::SFML::Render(window);
                    window.display();
                    continue;
                }
                
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
                        // Enforce non-negative holdings.
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

        // Use a flag to open popups to avoid calling OpenPopup during a Begin/End pair.
        if(openSearchPopup) {
            ImGui::OpenPopup("Add Coin");
            openSearchPopup = false;
            search_results.clear();
            memset(search_buffer, 0, sizeof(search_buffer));
        }

        if(ImGui::BeginPopupModal("Add Coin", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Search CoinGecko (e.g., 'Bitcoin', 'chainlink')");
            ImGui::InputText("##Search", search_buffer, sizeof(search_buffer), ImGuiInputTextFlags_EnterReturnsTrue);

            if(ImGui::Button("Search", ImVec2(120, 0))) {
                is_searching = true;
                futureSearch = std::async(std::launch::async, &MarketClient::search_coins, &client, std::string(search_buffer));
            }
            ImGui::SameLine();

            if(ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::Separator();
            ImGui::BeginChild("SearchResult", ImVec2(300, 200), true);
            for(auto const& res : search_results) {
                std::string label = std::format("{} ({})", res.name, res.ticker);
                if(ImGui::Selectable(label.c_str())) {
                    // Prevent adding a coin that's already in the user's list.
                    bool exists = false;
                    for(auto const& existing : coins) {
                        if(existing.api_id == res.api_id) {
                            exists = true;
                            break;
                        }
                    }
                    if(!exists) {
                        coins.push_back(res);
                        save_coins(coins);
                    }
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndChild();
            ImGui::EndPopup();
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