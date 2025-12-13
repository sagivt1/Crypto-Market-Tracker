#include "analysis.hpp"
#include <numeric>
#include <limits>

std::vector<double> calculate_sma(const std::vector<double>& prices, int period) {

    if(prices.size() <  period) return {};

    std::vector<double> sma(prices.size());

    for(size_t i=0; i<prices.size(); i++) {
        if(i < period - 1) {
            sma[i] = std::numeric_limits<double>::quiet_NaN();
        } else {
            auto startItr = prices.begin() + (i - period + 1);
            auto endItr = prices.begin() + i + 1;

            double sum = std::accumulate(startItr, endItr, 0.0);
            sma[i] = sum / period;
        }
    }

    return sma;
}