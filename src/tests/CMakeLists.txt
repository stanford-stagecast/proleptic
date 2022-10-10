macro (add_exec exec_name)
    add_executable ("${exec_name}" "${exec_name}.cc")
    target_link_libraries ("${exec_name}" util nn graph ${Eigen_LDFLAGS} ${Eigen_LDFLAGS_OTHER})
endmacro (add_exec)

add_exec(serdes-roundtrip)
add_exec(backprop)
add_exec(gradient-descent)

