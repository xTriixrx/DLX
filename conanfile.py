import os
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from conan.tools.build import can_run


class DlxConan(ConanFile):
    name = "dlx"
    version = "0.0.1"
    settings = "os", "compiler", "build_type", "arch"

    def configure(self):
        if self.settings.compiler == "apple-clang":
            self.settings.compiler.libcxx = "libc++"

    def layout(self):
        self.folders.build = "build"
        self.folders.generators = os.path.join(self.folders.build, "conan")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if can_run(self):
            cmake.ctest()

    def package_info(self):
        pass
