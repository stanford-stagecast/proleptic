macro (add_nn_exec exec_name)
    add_executable ("${exec_name}" "${exec_name}.cc")
    target_link_libraries ("${exec_name}" nnlib ${Eigen_LDFLAGS} ${Eigen_LDFLAGS_OTHER})
endmacro (add_nn_exec)

add_nn_exec(ex1 ex1.cc)
add_nn_exec(text_parser text_parser.cc)
