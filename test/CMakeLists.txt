add_custom_target(
  check
  lit --path=${CMAKE_BINARY_DIR}/bin ${CMAKE_SOURCE_DIR}/test
  DEPENDS get_me)
