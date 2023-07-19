set (CMAKE_CXX_STANDARD 20)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)
set (CMAKE_BASE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

add_compile_options(-pedantic -pedantic-errors -Werror -Wall -Wextra -Wshadow -Weffc++ -Wformat=2 -fno-signed-zeros -fno-math-errno -freciprocal-math -DEIGEN_STACK_ALLOCATION_LIMIT=0 -DEIGEN_NO_MALLOC -g)

set (CMAKE_CXX_FLAGS_DEBUGASAN "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined -fsanitize=address")
set (CMAKE_CXX_FLAGS_RELASAN "${CMAKE_CXX_FLAGS_RELEASE} -fsanitize=undefined -fsanitize=address")
