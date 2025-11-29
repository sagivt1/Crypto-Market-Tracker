from conan import ConanFile
from conan.tools.cmake import cmake_layout

class MarketTrackerConan(ConanFile):
    
    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    # CMake integration helpers
    generators = "CMakeDeps", "CMakeToolchain"

    # Define runtime dependencies.
    def requirements(self):
        # HTTP request library
        self.requires("cpr/1.12.0")
        # JSON parsing library
        self.requires("nlohmann_json/3.12.0")
        # Gui & Windows
        self.requires("imgui/1.90.5", override=True)
        self.requires("sfml/2.6.2")
        self.requires("imgui-sfml/2.6.1")
        self.requires("implot/0.16")

    # Define build-time and test dependencies.
    def build_requirements(self):
        self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)




    