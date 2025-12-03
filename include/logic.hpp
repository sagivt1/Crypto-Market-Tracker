#pragma once
#include <string>

/// @brief A utility class for application configuration and build verification.
/// Provides static methods for retrieving app info and performing sample logic.
class MarketConfig {
public:
    /// @brief Gets the application's version string.
    static std::string get_app_version();
    /// @brief Performs a sample calculation, used for unit testing the build.
    static int calculate_dummy_value(int a, int b);
};
