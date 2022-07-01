add_custom_target(
  check
  lit
  -a
  --path=${CMAKE_BINARY_DIR}/bin
  ${CMAKE_SOURCE_DIR}/test
  DEPENDS get_me)
