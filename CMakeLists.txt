cmake_minimum_required(VERSION 3.21)
project(SDStore C)

set(CMAKE_C_STANDARD 23)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

add_executable(sdstore src/sdstore.c)
add_executable(sdstored src/sdstored.c)

add_library(env src/env.c src/env.h)


add_executable(main src/main.c)
target_link_libraries(main env)