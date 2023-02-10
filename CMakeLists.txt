cmake_minimum_required(VERSION 3.24)

project(colors VERSION 1.0.2 LANGUAGES C)

add_executable(colors main.c)
target_link_libraries(colors PRIVATE ncurses)