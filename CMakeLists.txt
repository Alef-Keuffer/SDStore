cmake_minimum_required(VERSION 3.21)
project(SDStore C)

set(CMAKE_C_STANDARD 23)

add_executable(sdstore src/sdstore.c)
add_executable(sdstored src/sdstored.c)
