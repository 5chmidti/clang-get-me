from conans import ConanFile


class TemplateProject(ConanFile):
    name = "TemplateProject"

    requires = (
        "fmt/9.0.0",
        "spdlog/1.10.0",
        "gtest/1.12.1",
        "boost/1.79.0",
        "range-v3/0.12.0"
    )
    generators = "cmake_find_package"
