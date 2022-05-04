cmake_minimum_required(VERSION 3.21)
project(SDStore C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

file(GLOB_RECURSE UTIL_LIBRARY_FILES src/util/*.c src/util/*.h)
add_library(util ${UTIL_LIBRARY_FILES})
link_libraries(util)

add_executable(main src/main.c)
add_executable(client src/client.c)
add_executable(server src/server.c)
