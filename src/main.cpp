
#include "market_client.hpp"
#include <imgui.h>
#include <imgui-SFML.h>
#include <implot.h>
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <chrono>
#include <thread>
#include <future>

using namespace std::chrono_literals;


struct CoinDef { // Defines the structure for representing a cryptocurrency in the UI.
    std::string name;   // Display: "Bitcoin"
    std::string ticker; // Display: "BTC"
    std::string api_id; // Logic:   "bitcoin"
};

int main() {

    // --- Initialization ---
    // Create the main application window with a specific size and title.
    sf::RenderWindow window(sf::VideoMode(1000, 700), "Crypto Traker");
    // Limit the framerate to 60 FPS to prevent excessive CPU usage.
    window.setFramerateLimit(60);

    // Initialize the ImGui-SFML backend. This is a crucial step to connect ImGui to SFML window.
    if(!ImGui::SFML::Init(window)){
        return 1;
    }

    // Initialize Plotting Context
    ImPlot::CreateContext();

    // --- Application State & Data ---
    // client: An instance of MarketClient used to interact with the cryptocurrency API.
    MarketClient client;
    // current_data: Stores the latest fetched data for a coin, including price and history.
    CoinData current_data;
    // status: A string to display the current state of the application.
    std::string status = "Ready";
    // delta_clock: An SFML clock to measure the time elapsed between frames, used by ImGui for animations.
    sf::Clock delta_clock;

    // A predefined list of cryptocurrencies to be displayed in the UI.
    std::vector<CoinDef> coins = {
        {"Bitcoin",  "BTC", "bitcoin"},
        {"Ethereum", "ETH", "ethereum"},
        {"Solana",   "SOL", "solana"},
        {"Dogecoin", "DOGE","dogecoin"},
        {"Cardano",  "ADA", "cardano"},
        {"Ripple",   "XRP", "ripple"},
        {"Polkadot", "DOT", "polkadot"}
    };

    // The index of the currently selected coin in the `coins` vector. Defaults to the first coin.
    int selected_index = 0;

    // --- Asynchronous Data Fetching State ---
    // futureResult: Holds the result of the asynchronous network call to fetch coin data.
    std::future<std::optional<CoinData>> futureResult;
    // is_loading: A flag to indicate whether a data fetch operation is currently in progress.
    bool is_loading = true; 
    // should_reset_axes: A flag to control when the plot axes should be reset to fit the new data.
    bool should_reset_axes = false; 

    futureResult =std::async(std::launch::async, &MarketClient::get_coin_data, &client, coins[selected_index].api_id);

    // --- Main Application Loop ---
    // This loop runs continuously as long as the window is open. Each iteration is one frame.
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
        // Tell ImGui to start a new frame. The delta time is used for animations and timing-dependent operations.
        ImGui::SFML::Update(window, delta_clock.restart());

        // Check if we are waiting for data from the network.
        if(is_loading) {
            // Check if the future has a result available without blocking.
            if(futureResult.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                // Retrieve the result from the future. This call is now guaranteed not to block.
                auto result = futureResult.get();
                // If the result is valid (i.e., not std::nullopt), update the price and status.
                if(result) {
                    current_data = *result;
                    status = "Updated: " + current_data.id;

                    should_reset_axes = true;
                } else {
                    // If there was an error (e.g., network issue), update the status to reflect that.
                    status = "Network Error";
                }
                is_loading = false;
            }
        }

        // --- DASHBOARD LAYOUT ---
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)window.getSize().x, (float)window.getSize().y));
        ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        
        // Create a resizable two-column layout.
        if(ImGui::BeginTable("MainLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)){
            
            // --- SIDEBAR ---
            ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Analysis", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            // --- Column 1: Coin Selection ---
            
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("COINS");
            ImGui::Separator();

            for(int i = 0; i < coins.size(); i++) {
                bool is_selected = (selected_index == i);

                std::string label = std::format("{} ({})", coins[i].name, coins[i].ticker);

                // When a new coin is selected, trigger an asynchronous data fetch.
                if(ImGui::Selectable(label.c_str(), is_selected)) {
                    selected_index = i;

                    is_loading = true;
                    status = "Fetching " + coins[i].name + "...";
                    current_data.price_history.clear();
                    current_data.current_price = 0.0;

                    // Launch a new asynchronous task to get data for the selected coin.
                    futureResult = std::async(std::launch::async, &MarketClient::get_coin_data, &client, coins[i].api_id);
                }
            }

            // --- Column 2: Main Content (Price & Chart) ---
            ImGui::TableSetColumnIndex(1);

            ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "%s (%s)", coins[selected_index].name.c_str(), coins[selected_index].ticker.c_str());
            ImGui::Separator();
            
            if(is_loading) {
                ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x * 0.5f, ImGui::GetWindowSize().y * 0.4f));
                // Display a loading message while data is being fetched.
                ImGui::Text("Loading Market Data..");
            } else {
                if(current_data.current_price > 0.0) {
                    ImGui::SetWindowFontScale(2.5f);
                    ImGui::Text("$%.2f", current_data.current_price);
                    ImGui::SetWindowFontScale(1.5f);
                } else {
                    ImGui::Text("---");
                }

                ImGui::Spacing();

                // Only attempt to draw the plot if there is historical price data available.
                if(!current_data.price_history.empty()) {
                    if(should_reset_axes) {
                        ImPlot::SetNextAxesToFit();
                        should_reset_axes = false;
                    }
                    if(ImPlot::BeginPlot("24 Hours Trend", ImVec2(-1,-1))) {
                        ImPlot::PlotLine("Price (USD)", 
                            current_data.price_history.data(),
                            current_data.price_history.size()
                        );
                        ImPlot::EndPlot();
                    }
                } else {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "No chart data available.");
                }  
            }

            ImGui::EndTable();
        }
        
        // --- STATUS BAR ---
        ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 30);
        ImGui::Separator();
        ImGui::TextDisabled("Status: %s", status.c_str());
                
        // End the definition of the "Dashboard" window.
        ImGui::End(); 

        // --- Rendering ---
        // Clear the SFML window with a default color (black).
        window.clear();
        // Tell ImGui-SFML to render all the ImGui elements that have been defined.
        ImGui::SFML::Render(window);
        // Display the contents of the window on the screen.
        window.display();
    }

    // --- Shutdown ---
    ImPlot::DestroyContext();
    // Clean up ImGui-SFML resources before the application exits.
    ImGui::SFML::Shutdown();
    return 0;
    
}