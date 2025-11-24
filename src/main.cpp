#include "logic.hpp"
#include <print>
#include <exception>

int main() {
    try {
        std::println("Starting {}", MarketConfig::get_app_version());

        int result = MarketConfig::calculate_dummy_value(10, 2);
        std::println("Check Result: {}", result);

    } catch (const std::exception& e) {
        std::println(stderr, "Error: {}", e.what());
        return 1;
    }

    return 0;
}