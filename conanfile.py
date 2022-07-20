from conans import ConanFile


class TemplateProject(ConanFile):
    options = {}
    name = "TemplateProject"

    requires = (
        "fmt/8.1.1",
        "spdlog/1.10.0",
        "gtest/1.11.0",
        "boost/1.79.0",
        "range-v3/0.12.0"
    )
    generators = "cmake_find_package"
