macro (add_exec exec_name)
    add_executable ("${exec_name}" "${exec_name}.cc")
    target_link_libraries ("${exec_name}" nn graph util ${Eigen_LDFLAGS} ${Eigen_LDFLAGS_OTHER})
endmacro (add_exec)

add_exec(ex1)
add_exec(text_parser)
add_exec(ex2)
add_exec(ex3)
