from conan import ConanFile
from conan.tools.cmake import cmake_layout


class AppRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("qt/5.15.18")
        self.requires("sqlite3/3.51.0")

    def layout(self):
        cmake_layout(self)