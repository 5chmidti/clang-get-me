from conan import ConanFile


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
        self.requires("fmt/9.1.0")
        self.requires("spdlog/1.11.0")
        self.requires("catch2/3.3.0")
        self.requires("benchmark/1.7.1")
        self.requires("boost/1.81.0")
        self.requires("range-v3/0.12.0")
        self.requires("ftxui/3.0.0")
        self.requires("rapidfuzz/cci.20210513")
        self.requires("ctre/3.7.1")
        self.requires("onetbb/2021.7.0")

    def build_requirements(self):
        self.tool_requires("cmake/3.25.2")
