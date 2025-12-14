#include "custom_plots.hpp"
#include <implot.h>
#include <imgui.h>
#include <algorithm> // For std::min, std::max
#include <cfloat>    // For DBL_MAX
#include <implot_internal.h>

// A custom implementation of PlotCandlestick for versions of ImPlot that lack it.
// This function is not part of the ImPlot namespace but uses its public API.
void PlotCandlestick(const char* label_id, const double* xs, const double* opens, const double* closes, const double* lows, const double* highs, int count, bool tooltip, float width_percent, ImVec4 bullCol, ImVec4 bearCol) {
    
    // Begin a new plot item and see if it's visible.
    if (ImPlot::BeginItem(label_id)) {
        
        // Get the draw list for custom rendering.
        ImDrawList* draw_list = ImPlot::GetPlotDrawList();

        // Calculate candle width in plot units. We estimate it based on the first two points.
        double half_width = count > 1 ? (xs[1] - xs[0]) * width_percent * 0.5 : width_percent * 0.5;

        // Loop through all data points.
        for (int i = 0; i < count; ++i) {
            // Skip invalid data points.
            if (xs[i] == -DBL_MAX || opens[i] == -DBL_MAX || closes[i] == -DBL_MAX || lows[i] == -DBL_MAX || highs[i] == -DBL_MAX) {
                continue;
            }
            
            // Determine candle color based on open vs. close price.
            ImU32 color_u32 = ImGui::GetColorU32(closes[i] >= opens[i] ? bullCol : bearCol);
            
            // Convert data points to pixel coordinates for drawing.
            // FIXED: PlotToPixels returns ImVec2 (screen coords), not ImPlotPoint.
            ImVec2 p_high = ImPlot::PlotToPixels(xs[i], highs[i]);
            ImVec2 p_low = ImPlot::PlotToPixels(xs[i], lows[i]);
            ImVec2 p_open = ImPlot::PlotToPixels(xs[i] - half_width, opens[i]);
            ImVec2 p_close = ImPlot::PlotToPixels(xs[i] + half_width, closes[i]);

            // Draw the wick (the vertical line from low to high).
            draw_list->AddLine(p_high, p_low, color_u32);

            // Draw the body (the filled rectangle from open to close).
            // We need to sort the Y coordinates because ImGui (0,0) is top-left, 
            // but Open price might be higher or lower than Close price.
            // We use the X from the calculated offsets and the Y from the Open/Close prices.
            
            // FIXED: Used std::min/std::max instead of ImMin/ImMax to avoid dependency on imgui_internal.h
            draw_list->AddRectFilled(
                ImVec2(p_open.x, std::min(p_open.y, p_close.y)), 
                ImVec2(p_close.x, std::max(p_open.y, p_close.y)), 
                color_u32
            );

            // This is crucial for auto-fitting the plot axes to the data.
            ImPlot::FitPoint(ImPlotPoint(xs[i], lows[i]));
            ImPlot::FitPoint(ImPlotPoint(xs[i], highs[i]));
        }
        ImPlot::EndItem();
    }
}