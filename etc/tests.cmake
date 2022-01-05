enable_testing ()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 10 -R '^t_')

add_test(NAME t_eigentest1 COMMAND eigentest1)
add_test(NAME t_formulagradienttest1 COMMAND formulagradienttest1)
add_test(NAME t_formulagradienttest2 COMMAND formulagradienttest2)
