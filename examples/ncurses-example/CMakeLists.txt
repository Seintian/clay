cmake_minimum_required(VERSION 3.27)
project(clay-ncurses-example C)

set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)

add_compile_definitions(_XOPEN_SOURCE_EXTENDED _XOPEN_SOURCE=700)

add_executable(clay-ncurses-example main.c)

target_link_libraries(clay-ncurses-example PRIVATE ${CURSES_LIBRARIES})

if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    target_link_libraries(clay-ncurses-example PRIVATE m)
endif()

target_include_directories(clay-ncurses-example PRIVATE ${CURSES_INCLUDE_DIRS} ../../)
