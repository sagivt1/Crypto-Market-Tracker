#pragma once
#include <imgui.h> // For ImVec4

/// @brief Draws a custom candlestick plot using ImPlot primitives.
/// This is a custom implementation for ImPlot versions that do not include a built-in function.
/// @param label_id A unique ID for the plot item.
/// @param xs Pointer to the x-axis data (e.g., timestamps).
/// @param opens Pointer to the opening prices.
/// @param closes Pointer to the closing prices.
/// @param lows Pointer to the low prices.
/// @param highs Pointer to the high prices.
/// @param count The number of data points.
/// @param tooltip Whether to show a tooltip on hover (Note: tooltip display is not implemented in this version).
/// @param width_percent The width of the candle body as a percentage of the space between points.
/// @param bullCol The color for bullish candles (close > open).
/// @param bearCol The color for bearish candles (close <= open).
void PlotCandlestick(const char* label_id, const double* xs, const double* opens, const double* closes, const double* lows, const double* highs, int count, bool tooltip = true, float width_percent = 0.25f, ImVec4 bullCol = ImVec4(0, 1, 0, 1), ImVec4 bearCol = ImVec4(1, 0, 0, 1));