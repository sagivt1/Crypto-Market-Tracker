# Specify the minimum required CMake version.
cmake_minimum_required(VERSION 4.2)

# Define the project name and language.
project(MarketTracker VERSION 1.0 LANGUAGES CXX)

# --- Setup C++ Standard ---
# Use C++23 for this project.
set(CMAKE_CXX_STANDARD 23)
# Require the standard, failing if the compiler doesn't support it.
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Disable non-standard compiler extensions for better portability.
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Dependencies ---
find_package(cpr REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(sfml REQUIRED)
find_package(imgui REQUIRED)
find_package(imgui-sfml REQUIRED)
find_package(implot REQUIRED)



# --- Core Library ---
# Define a library for the core application logic.
add_library(core_lib
    src/logic.cpp
    src/market_client.cpp
)
# Make the 'include' directory available to core_lib and any targets that link to it.
target_include_directories(core_lib PUBLIC include)

# Link external libraries to the core library.
target_link_libraries(core_lib 
    PUBLIC 
    cpr::cpr 
    nlohmann_json::nlohmann_json
    sfml-graphics 
    sfml-window 
    sfml-system
    imgui::imgui
    ImGui-SFML::ImGui-SFML
    implot::implot
)

# --- Main Executable ---
# Define the main application executable.
add_executable(MarketTracker src/main.cpp)

# Link the core logic library to the main executable.
# 'PRIVATE' ensures this dependency is not propagated to other targets linking MarketTracker.
target_link_libraries(MarketTracker PRIVATE core_lib)
# Link libraries to the main executable.
target_link_libraries(MarketTracker PRIVATE 
    core_lib
    imgui::imgui
    imgui-sfml
    sfml-graphics)

# --- Testing ---
# Enable the CTest testing framework.
enable_testing()
find_package(GTest)

if(GTest_FOUND)
    # Define the test executable.
    add_executable(unit_tests tests/test_main.cpp)
    # Link tests against the code being tested (core_lib) and Google Test.
    target_link_libraries(unit_tests PRIVATE core_lib GTest::gtest_main)

    # Use the GoogleTest module to automatically discover tests.
    include(GoogleTest)
    gtest_discover_tests(unit_tests)
endif()
