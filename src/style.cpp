#include "style.hpp"
#include <imgui.h>
#include <imgui-SFML.h>
#include <implot.h>

void setup_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 4.0f;
    ImGuiIO& io = ImGui::GetIO();

    // If the primary font fails to load, scale the default font as a fallback for readability.
    if (!io.Fonts->AddFontFromFileTTF("Roboto-Regular.ttf", 18.0f)) {
        io.FontGlobalScale = 1.2f; 
    }

    ImGui::SFML::UpdateFontTexture();

    // set custom plot color 
    ImPlot::GetStyle().Colors[ImPlotCol_Line] = ImVec4(0.9f, 0.7f, 0.0f, 1.0f);
 }