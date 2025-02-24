from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMake
from conan.tools.scm import Git


class QSSHConanFile(ConanFile):
    name = "qssh"
    version = "1.0"

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("botan/3.6.1")
        self.requires("qt/6.7.3")

    exports_sources = "*"

    def layout(self):
        return cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
