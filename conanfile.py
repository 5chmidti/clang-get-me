from conans import ConanFile


class TemplateProject(ConanFile):
    name = "TemplateProject"

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

    requires = (
        "fmt/9.1.0",
        "spdlog/1.11.0",
        "gtest/1.13.0",
        "benchmark/1.7.1",
        "boost/1.81.0",
        "range-v3/0.12.0",
        "ftxui/3.0.0",
        "rapidfuzz/cci.20210513",
        "ctre/3.7.1",
    )
    generators = "cmake_find_package"
