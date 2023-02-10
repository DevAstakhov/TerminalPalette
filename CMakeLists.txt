cmake_minimum_required(VERSION 3.24)

project(colors VERSION 1.0.2 LANGUAGES C)

add_executable(colors main.c)
target_compile_definitions(colors PRIVATE VERSION="${CMAKE_PROJECT_VERSION}")
target_link_libraries(colors PRIVATE ncurses)