from conan import ConanFile
from conan.tools.cmake import cmake_layout

class MarketTrackerConan(ConanFile):
    
    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    # CMake integration helpers
    generators = "CMakeDeps", "CMakeToolchain"

    # Define runtime dependencies.
    def requirements(self):
        pass

    # Define build-time and test dependencies.
    def build_requirements(self):
        self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)




    