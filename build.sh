#!/bin/bash

# Build SDStore executables, and return to project root.
# Using subshell to avoid having to cd ..
# https://www.shellcheck.net/wiki/SC2103
(
    cd ./bin/sdstore-transformations || return;
    make clean;
    make;
)

# Build the project executables, and return to project's root.
# Subshell, same as above.
cmake --build ./build --config Release --target all -j 18 --

rm -f tests/server
rm -f tests/client
cp build/server tests
cp build/client tests

cp ./etc/sdstored.conf tests/sdstored.conf

mkdir -p tests/bin
cp -r ./bin/sdstore-transformations/* ./tests/bin/