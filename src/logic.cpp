#include "logic.hpp"
#include <stdexcept>

std::string MarketConfig::get_app_version() {
    return "MarketTracker v1.0";
}

int MarketConfig::calculate_dummy_value(int a, int b) {
    if(b == 0) {
        // Throwing an exception here demonstrates robust error handling for invalid inputs.
        throw std::invalid_argument("Cannot divide by zero");
    } else {
        return a / b;
    }
}
