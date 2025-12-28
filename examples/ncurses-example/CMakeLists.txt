cmake_minimum_required(VERSION 3.27)
project(clay-ncurses-example C)

find_package(Curses REQUIRED)

add_executable(clay-ncurses-example main.c)

target_link_libraries(clay-ncurses-example PRIVATE ${CURSES_LIBRARIES})

if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    target_link_libraries(clay-ncurses-example PRIVATE m)
endif()

target_include_directories(clay-ncurses-example PRIVATE ${CURSES_INCLUDE_DIRS} ../../)
