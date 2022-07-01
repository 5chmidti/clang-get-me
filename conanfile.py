from conans import ConanFile


class TemplateProject(ConanFile):
    options = {}
    name = "TemplateProject"

    requires = (
        "fmt/8.1.1",
        "spdlog/1.10.0",
    )
    generators = "cmake_find_package"
