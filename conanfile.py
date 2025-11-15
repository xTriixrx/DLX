import os
import shutil
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from conan.tools.build import can_run


class DlxConan(ConanFile):
    name = "dlx"
    version = "0.0.1"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        if not self._has_system_gtest():
            self.requires("gtest/1.17.0")

    def configure(self):
        if self.settings.compiler == "apple-clang":
            self.settings.compiler.libcxx = "libc++"

    def layout(self):
        self.folders.build = "build"
        self.folders.generators = os.path.join(self.folders.build, "conan")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        if not self._has_system_gtest():
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

    def _has_system_gtest(self):
        # Basic check: see if Homebrew gtest headers exist
        return bool(shutil.which("pkg-config")) and os.path.exists("/opt/homebrew/include/gtest/gtest.h")
