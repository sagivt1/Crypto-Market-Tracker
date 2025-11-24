#pragma once
#include <string>

// A simple class to verify our build system and testing logic
class MarketConfig {
public:
    // Declaration for a static method to retrieve the application's version string.
    static std::string get_app_version();
    // Declaration for a static method to perform a sample calculation.
    static int calculate_dummy_value(int a, int b);
};

