cmake_minimum_required(VERSION 3.21)
project(SDStore C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)
file(GLOB_RECURSE UTIL_LIBRARY_FILES src/util/*.c src/util/*.h)
add_library(util ${UTIL_LIBRARY_FILES})
link_libraries(util)

add_executable(main src/main.c)
add_executable(client src/client.c)
add_executable(server src/server.c)

add_custom_target(
        docs
        COMMAND doxygen Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS client server main
)
add_dependencies(docs server client)

set(targets server client main util)
file(GLOB TEST_FILES tests/*)

foreach(target ${targets})
add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND doxygen Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
endforeach()
configure_file(${TEST_FILES} ${CMAKE_BINARY_DIR}/ COPYONLY)
add_custom_target(
        test1
        COMMAND konsole --noclose -e ./test_script.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)