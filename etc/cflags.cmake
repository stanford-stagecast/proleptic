set (CMAKE_CXX_STANDARD 20)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)
set (CMAKE_BASE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

add_compile_options(-pedantic -pedantic-errors -Werror -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-qual -Wformat=2 -Weffc++ -ffast-math -march=native -mtune=native -DEIGEN_STACK_ALLOCATION_LIMIT=0 -DEIGEN_NO_MALLOC)
