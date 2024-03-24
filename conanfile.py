from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import cmake_layout, CMakeToolchain, CMake


class GetMe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    def configure(self):
        self.options["boost"].without_atomic = True
        self.options["boost"].without_chrono = True
        self.options["boost"].without_context = True
        self.options["boost"].without_contract = True
        self.options["boost"].without_coroutine = True
        self.options["boost"].without_date_time = True
        self.options["boost"].without_exception = True
        self.options["boost"].without_fiber = True
        self.options["boost"].without_filesystem = True
        self.options["boost"].without_iostreams = True
        self.options["boost"].without_json = True
        self.options["boost"].without_locale = True
        self.options["boost"].without_log = True
        self.options["boost"].without_nowide = True
        self.options["boost"].without_program_options = True
        self.options["boost"].without_stacktrace = True
        self.options["boost"].without_test = True
        self.options["boost"].without_thread = True
        self.options["boost"].without_timer = True
        self.options["boost"].without_type_erasure = True
        self.options["boost"].without_wave = True

        # Fixes build failure in CI
        self.options["onetbb"].tbbbind = False

    def requirements(self):
        self.requires("benchmark/1.8.3")
        self.requires("boost/1.84.0")
        self.requires("catch2/3.5.3")
        self.requires("ctre/3.8.1")
        self.requires("fmt/10.2.1")
        self.requires("ftxui/4.1.1")
        self.requires("onetbb/2021.10.0")
        self.requires("range-v3/0.12.0")
        self.requires("rapidfuzz/3.0.2")
        self.requires("spdlog/1.13.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        cmake = CMake(self)
        cmake.test(target="check")
