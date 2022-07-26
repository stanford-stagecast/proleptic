enable_testing ()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 10 -R '^t_')

add_test(NAME t_serdes_roundtrip COMMAND serdes-roundtrip)
