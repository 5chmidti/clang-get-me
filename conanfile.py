from conan import ConanFile
from conan.tools.cmake import cmake_layout


class GetMe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

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

    def requirements(self):
        self.requires("fmt/10.1.1")
        self.requires("spdlog/1.12.0")
        self.requires("catch2/3.4.0")
        self.requires("benchmark/1.8.3")
        self.requires("boost/1.83.0")
        self.requires("range-v3/0.12.0")
        self.requires("ftxui/4.1.1")
        self.requires("rapidfuzz/cci.20210513")
        self.requires("ctre/3.8")
        self.requires("onetbb/2021.10.0")

    def layout(self):
        cmake_layout(self)
