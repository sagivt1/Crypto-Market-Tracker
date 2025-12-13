#include "market_client.hpp"
#include "persistence.hpp"
#include "analysis.hpp"
#include "style.hpp"
#include <imgui.h>
#include <imgui-SFML.h>
#include <implot.h>
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <chrono>
#include <future>
#include <vector>
#include <iostream>
#include <format>
#include <map>

using namespace std::chrono_literals;

int main() {

    // --- Initialization ---
    // Create the main application window with a specific size and title.
    sf::RenderWindow window(sf::VideoMode(1000, 700), "Crypto Tracker");
    // Prevent excessive CPU usage when the app is idle.
    window.setFramerateLimit(60);

    // Binds the ImGui context to the SFML window, enabling GUI rendering.
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
    std::map<std::string, PortfolioEntry> portfolio = load_portfolio();

    int selected_index = -1;
    PortfolioEntry temp_entry;
    bool is_loading = true; // Tracks if a network request is in-flight to prevent duplicate requests.
    bool should_reset_axes = false; // Triggers the plot to rescale after new data arrives.

    // analysis state
    bool showSmaShort = false;
    bool showSmaLong = false;
    std::vector<double> smaShortData;
    std::vector<double> smaLongData;

    //Temp editor variables
    double tempAmount = 0.0;
    double tempBuyPrice = 0.0;

    // Futures for managing non-blocking network calls.
    std::future<std::optional<CoinData>> futureCoin;
    std::future<std::map<std::string, double>> futureBatch;
    std::future<std::vector<CoinDef>> futureSearch;

    std::vector<const char*> pieLabels;
    std::vector<double> pieValue;
    double totalNetWorth = 0.0;
    double totalCostBasis = 0.0;

    sf::Clock refreshClock;
    float const REFRESH_INTERVAL = 60.f;

    // Initial data fetch for the portfolio overview.
    std::vector<std::string> allIds;
    for(auto const& coin : coins) {
        allIds.push_back(coin.api_id);
    }
    futureBatch = std::async(std::launch::async, &MarketClient::get_multi_price, &client, allIds);

    char search_buffer[128] = "";
    std::vector<CoinDef> search_results;
    bool is_searching = false;

    bool openSearchPopup = false; // Use a flag to safely open popups outside of the main ImGui Begin/End block.

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
        ImGui::SFML::Update(window, delta_clock.restart());

        // Trigger a data refresh automatically if the interval has passed and no other request is active.
        if(!is_loading && refreshClock.getElapsedTime().asSeconds() >= REFRESH_INTERVAL) {
            is_loading = true;
            status = "Auto-Refreshing...";
            refreshClock.restart();

            if(selected_index == -1) {
                allIds.clear();
                for(auto const& coin : coins)
                    allIds.push_back(coin.api_id);

                futureBatch = std::async(std::launch::async, &MarketClient::get_multi_price, &client, allIds);
            } else {
                futureCoin = std::async(std::launch::async, &MarketClient::get_coin_data, &client, coins[selected_index].api_id);
            }
        }

        // Check if the batch price fetch is complete without blocking the main thread.
        if(futureBatch.valid() && futureBatch.wait_for(0s) == std::future_status::ready) {    
            auto price = futureBatch.get();
            pieLabels.clear();
            pieValue.clear();
            totalNetWorth = 0.0;
            totalCostBasis = 0.0;
            for(int i=0; i<coins.size(); i++) {
                PortfolioEntry& entry = portfolio[coins[i].api_id];
                // Filter out dust amounts to keep the pie chart clean.
                if(entry.amount > 0.00001) {

                    double currentVal = entry.amount * price[coins[i].api_id];
                    double costVal = entry.amount * entry.buyPrice;

                    if(currentVal > 0.00001) {
                        pieLabels.push_back(coins[i].ticker.c_str());
                        pieValue.push_back(currentVal);
                        totalNetWorth += currentVal;
                        totalCostBasis += costVal;
                    }
                }
            }
            status = "Portfolio Synced.";
            is_loading = false; 
            refreshClock.restart(); 
        } 

        // Check if the single coin data fetch is complete.
        if(futureCoin.valid() && futureCoin.wait_for(0s) == std::future_status::ready) {
            try {
                auto result = futureCoin.get();
                if(result.has_value()) {
                    current_data = result.value();

                    if(!current_data.price_history.empty()) {
                        smaShortData = calculate_sma(current_data.price_history, 7);
                        smaLongData = calculate_sma(current_data.price_history, 25);
                    }

                    status = "Updated: " + coins[selected_index].name;
                }
            } catch (...) {
                status = "Error";
            }
            is_loading = false; 
            refreshClock.restart(); 
        }

        // Check if the coin search is complete.
        if(futureSearch.valid() && futureSearch.wait_for(0s) == std::future_status::ready) {
            search_results = futureSearch.get();
            is_searching = false;
        }

        // --- DASHBOARD LAYOUT ---
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        // Force the main dashboard window to fill the entire application window.
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

            // Use a selected index of -1 as a sentinel for the main portfolio overview.
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
                double amount = portfolio[coins[i].api_id].amount;
                std::string label = amount > 0.00001 ? std::format("{} ({:.2f})", coins[i].ticker, amount) : coins[i].ticker;
                
                if(ImGui::Selectable(label.c_str(), selected_index == i)) {
                    // Fetch data only if a new coin is selected and no other request is active.
                    if(selected_index != i && !is_loading) {
                        selected_index = i;
                        temp_entry = portfolio[coins[i].api_id];
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

            // Status text on the left, refresh timer on the right
            ImGui::TextDisabled(status.c_str());

            float timeLeft = REFRESH_INTERVAL - refreshClock.getElapsedTime().asSeconds();
            if(timeLeft < 0) {
                timeLeft = 0;
            }
            ImGui::SameLine();
            std::string refresh_text = std::format("Refresh: {:.0f}s", timeLeft);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(refresh_text.c_str()).x);
            ImGui::TextDisabled(refresh_text.c_str());

            if(selected_index == -1) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Total Worth Net");
                ImGui::SetWindowFontScale(3.0f);
                ImGui::Text("$%.2f", totalNetWorth);
                ImGui::SetWindowFontScale(1.0f);

                // calculate profit and loss
                double totalPNL = totalNetWorth - totalCostBasis;
                double totalPNLPercent = (totalCostBasis > 0) ? totalPNL / totalCostBasis * 100.0 : 0.0;

                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 50);
                ImGui::BeginGroup();
                ImGui::Text("Total PNL");

                ImVec4 pnlColor = (totalPNL >= 0) ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1);
                ImGui::TextColored(pnlColor, "$%.2f (%.2f%%)", totalPNL);
                ImGui::EndGroup();

                ImGui::Separator();
        
                if(!pieValue.empty()) {
                    if(ImPlot::BeginPlot("##Pie", ImVec2(-1, -1), ImPlotFlags_Equal | ImPlotFlags_NoMouseText)) {
                        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                        ImPlot::PlotPieChart(pieLabels.data(), pieValue.data(), static_cast<int>(pieValue.size()), 0.5, 0.5, 0.35, "%.1f", 90);
                        ImPlot::EndPlot();
                    }
                }
            } else {
                CoinDef& c = coins[selected_index];
                ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "%s (%s)", coins[selected_index].name.c_str(), coins[selected_index].ticker.c_str());
                ImGui::SameLine();
                
                const char* delete_text = "Delete Coin";
                float button_width = ImGui::CalcTextSize(delete_text).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - button_width);
                if(ImGui::Button(delete_text)) {
                    portfolio.erase(c.api_id);
                    save_portfolio(portfolio);

                    coins.erase(coins.begin() + selected_index);
                    save_coins(coins);

                    selected_index = -1;
                }

                // After deletion, the index is invalid. Bail out of this frame's rendering to prevent a crash.
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

                        ImGui::Checkbox("Show SMA-7", &showSmaShort);
                        ImGui::Checkbox("Show SMA-25", &showSmaLong);

                        // Auto-fit the plot axes on the first frame after new data arrives.
                        if(should_reset_axes) {
                            ImPlot::SetNextAxesToFit();
                            should_reset_axes = false;
                        }

                        if(ImPlot::BeginPlot("24H Trend", ImVec2(-1, 350))) {
                            ImPlot::PlotLine("Price (USD)", current_data.price_history.data(), current_data.price_history.size());

                            if(showSmaShort && !smaShortData.empty()) {
                                ImPlot::SetNextLineStyle(ImVec4(0, 1, 1, 1));
                                ImPlot::PlotLine("SMA-7", smaShortData.data(), smaShortData.size());
                            }
                            
                            if(showSmaLong && !smaLongData.empty()) {
                                ImPlot::SetNextLineStyle(ImVec4(1, 0, 1, 1));
                                ImPlot::PlotLine("SMA-25", smaLongData.data(), smaLongData.size());
                            }


                            ImPlot::EndPlot();
                        }
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("Portfolio");

                    // Use temp_entry for editing, commit to portfolio on button press

                    ImGui::Text("Holdings:");
                    ImGui::SameLine(100);
                    ImGui::SetNextItemWidth(150);

                    if(ImGui::InputDouble("##Amount", &temp_entry.amount, 0.0, 0.0, "%.6f")) {
                        if (temp_entry.buyPrice == 0.0 && current_data.current_price > 0.0) {
                            temp_entry.buyPrice = current_data.current_price;
                        }
                    }

                    ImGui::Text("Avg Buy Price:");
                    ImGui::SameLine(100);
                    ImGui::SetNextItemWidth(150);
                    ImGui::InputDouble("##BuyPrice", &temp_entry.buyPrice, 0.0, 0.0, "%.2f");

                    if(ImGui::Button("Update Portfolio")) {
                        if(temp_entry.amount < 0) temp_entry.amount = 0;
                        if(temp_entry.buyPrice < 0) temp_entry.buyPrice = 0;
                        
                        portfolio[coins[selected_index].api_id] = temp_entry;
                        save_portfolio(portfolio);
                    }

                    // Calculate PNL for specific coin
                    if (current_data.current_price > 0.0 && temp_entry.amount > 0) {
                        double currentVal = temp_entry.amount * current_data.current_price;
                        double costVal = temp_entry.amount * temp_entry.buyPrice;
                        double pnl = currentVal - costVal;
                        double pnlPercent = (costVal > 0) ? pnl / costVal * 100.0 : 0.0;
                        ImGui::Spacing();
                        ImGui::Text("Current value: $%.2f", currentVal);
                        ImGui::SameLine();

                        ImGui::Text("| PNL: ");
                        ImGui::SameLine();
                        ImVec4 color = (pnl >= 0) ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1);
                        ImGui::TextColored(color, "$%.2f (%.2f%%)", pnl, pnlPercent);
                    }
                }
            }
            ImGui::EndTable();
        }

        // Trigger popup opening safely outside of a Begin/End pair.
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
                    // Check if the coin already exists in the portfolio to avoid duplicates.
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