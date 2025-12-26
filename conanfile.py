import os
import shutil
from conan import ConanFile
from conan.errors import ConanException
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps


class DlxConan(ConanFile):
    name = "dlx"
    version = "0.0.1"
    settings = "os", "compiler", "build_type", "arch"
    options = {"run_performance_tests": [True, False]}
    default_options = {"run_performance_tests": False}

    def requirements(self):
        self.requires("yaml-cpp/0.8.0")
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
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if not can_run(self):
            self.output.info("Skipping tests because this configuration cannot run executables.")
            return
        build_folder = self._resolve_ctest_build_folder()
        jobs = self.conf.get("tools.build:jobs", default=12, check_type=int) or 12
        jobs = max(1, int(jobs))

        def _run_ctest(label_args):
            cmd = ["ctest", "--output-on-failure", *label_args, "-j", str(jobs)]
            self.run(" ".join(cmd), cwd=build_folder)

        # Always execute the standard unit suites first.
        _run_ctest(["-LE", "performance"])

        if self.options.run_performance_tests:
            _run_ctest(["-L", "performance"])

    def package_info(self):
        pass

    def _has_system_gtest(self):
        # Basic check: see if Homebrew gtest headers exist
        return bool(shutil.which("pkg-config")) and os.path.exists("/opt/homebrew/include/gtest/gtest.h")

    def _resolve_ctest_build_folder(self):
        """Return the build folder that already contains CTest metadata."""
        # When running from the local workspace we want to execute the binaries that
        # were produced under the committed build/ directory. In the cache (conan
        # create/test flows) we fall back to Conan's computed build folder.
        candidate = os.path.join(self.recipe_folder, "build")
        if not self.in_local_cache and os.path.isdir(candidate):
            return candidate
        if os.path.isdir(self.build_folder):
            return self.build_folder
        raise ConanException(
            "CTest metadata not found. Please build the project first (conan build .) "
            "so that the 'build' folder exists." 
        )
