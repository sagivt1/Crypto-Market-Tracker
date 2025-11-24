#include <gtest/gtest.h>
#include "logic.hpp"

TEST(SetupTest, VersionCheck) {
    EXPECT_EQ(MarketConfig::get_app_version(), "MarketTracker v1.0");
}

TEST(LogicTest, DivideByZeroThrows) {
    EXPECT_THROW(MarketConfig::calculate_dummy_value(10, 0), std::invalid_argument);
}

TEST(LogicTest, BasicMath) {
    EXPECT_EQ(MarketConfig::calculate_dummy_value(10, 2), 5);
}