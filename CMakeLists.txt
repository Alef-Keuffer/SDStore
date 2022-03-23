cmake_minimum_required(VERSION 3.21)
project(SDStore C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

add_library(env src/util/env.c src/util/env.h)

add_library(util src/util.c src/util/env.h)

add_library(transformation src/util/transformation.c src/util/transformation.h)

add_library(command src/util/command.c src/util/command.h)

add_library(task src/util/task.c src/util/task.h)
target_link_libraries(task command)

add_executable(sdstored src/sdstored.c src/sdstored.h)
target_link_libraries(sdstored env util transformation command task)


add_executable(sdstore src/sdstore.c src/sdstored.h)
target_link_libraries(sdstore env util)

add_executable(main src/main.c)
target_link_libraries(main env util)