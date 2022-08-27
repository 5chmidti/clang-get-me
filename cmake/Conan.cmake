set_property(
  DIRECTORY
  APPEND
  PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/conanfile.py)
macro(run_conan)
  # Download automatically, you can also just copy the conan.cmake file
  if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
    message(
      STATUS
        "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD
         "https://github.com/conan-io/cmake-conan/raw/v0.16.1/conan.cmake"
         "${CMAKE_BINARY_DIR}/conan.cmake")
  endif()

  list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
  list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

  include(${CMAKE_BINARY_DIR}/conan.cmake)

  conan_cmake_autodetect(settings BUILD_TYPE ${TYPE})

  conan_cmake_install(
    PATH_OR_REFERENCE
    ${CMAKE_SOURCE_DIR}
    BUILD
    missing
    ENV
    "CC=${CMAKE_C_COMPILER}"
    "CXX=${CMAKE_CXX_COMPILER}"
    SETTINGS
    ${settings})

  find_package(fmt REQUIRED)
  find_package(spdlog REQUIRED)

endmacro()
