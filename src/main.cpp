
#include "market_client.hpp"
#include <imgui.h>
#include <imgui-SFML.h>
#include <implot.h>
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <print>
#include <thread>
#include <future>


int main() {

    // --- Initialization ---
    // Create the main application window with a specific size and title.
    sf::RenderWindow window(sf::VideoMode(600, 400), "Crypto Traker");
    // Limit the framerate to 60 FPS to prevent excessive CPU usage.
    window.setFramerateLimit(60);

    // Initialize the ImGui-SFML backend. This is a crucial step to connect ImGui to SFML window.
    if(!ImGui::SFML::Init(window)){
        std::println(stderr, "Failed to initialize ImGui");
        return 1;
    }

    // Initialize Plotting Context
    ImPlot::CreateContext();

    // --- Application State ---
    // client: An instance of MarketClient used to interact with the cryptocurrency API.
    MarketClient client;
    // current_data: Stores the latest fetched data for a coin, including price and history.
    CoinData current_data;
    // status: A string to display the current state of the application.
    std::string status = "Ready";
    // delta_clock: An SFML clock to measure the time elapsed between frames, used by ImGui for animations.
    sf::Clock delta_clock;

    // futureResult: Holds the result of the asynchronous network call to fetch coin data.
    std::future<std::optional<CoinData>> futureResult;
    // is_loading: A flag to indicate whether a data fetch operation is currently in progress.
    bool is_loading = false; 

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
                } else {
                    // If there was an error (e.g., network issue), update the status to reflect that.
                    status = "Network Error";
                }
                is_loading = false;
            }
        }


        // Begin defining a new ImGui window. All subsequent ImGui calls will add elements to this window.
        ImGui::Begin("Market Analysis", nullptr); // Start a new window in the app
        // Add text and a separator to the ImGui window.
        ImGui::Text("Bitcoin (BTC/USD)");
        ImGui::Separator();

        // Conditionally display the price: show the value if it's been fetched, otherwise show a placeholder.
        if (current_data.current_price > 0.0) {
            ImGui::Text("BTC: $%.2f", current_data.current_price);
        } else {
            ImGui::Text("BTC: ---");
        }

        // If historical price data is available, create a plot.
        if(!current_data.price_history.empty()) {
            if(ImPlot::BeginPlot("24h Trend")) {
                ImPlot::PlotLine(
                    "Price",
                    current_data.price_history.data(),
                    current_data.price_history.size()
                );
                ImPlot::EndPlot();
            }
        }

        // Add vertical spacing for better layout.
        ImGui::Spacing();

        if(is_loading) {
            ImGui::Text("Downloading historical data...");
        } else {
            // If not loading, display a button that the user can click.
            if(ImGui::Button("Fetch Data")) {
                // When the button is clicked, update the status and set the loading flag.
                status = "Fetching in background...";
                is_loading = true;
                // Launch an asynchronous task to fetch the coin price. This runs in a separate thread.
                // `std::async` returns a `std::future` which will hold the result of the function call.
                // `std::launch::async` ensures the task runs on a new thread, preventing UI freezes.
                futureResult = std::async(std::launch::async, &MarketClient::get_coin_data, &client, "bitcoin");
            }
        }

        // Display the current status text with a gray color.
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Status: %s", status.c_str());
                
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