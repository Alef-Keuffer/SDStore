cmake_minimum_required(VERSION 3.21)
project(SDStore C)

set(CMAKE_C_STANDARD 23)

add_executable(SDStore src/main.c src/sdstored.c src/sdstore.c)
