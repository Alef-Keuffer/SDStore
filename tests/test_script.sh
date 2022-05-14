#!/bin/bash

# Move to project root
cd ..

# Build SDStore executables, and return to project root.
# Using subshell to avoid having to cd ..
# https://www.shellcheck.net/wiki/SC2103
(
    cd bin/sdstore-transformations || return
    make
)


# Build the project executables, and return to project's root.
# Subshell, same as above.
(
    cd build || exit
    cmake ..;
    make;
)

# Move to test directory to avoid cluttering root of project with files.
cd tests || return

# How many test files to generate, both input and output.
num_files=5

# Size of each input file. Should be 10M+ to create scenarios with interesting delay.
file_size="25M"

for ((i=1;i<=num_files;i++)); do
    rm -f filein"$i";
    rm -f fileout"$i";
done

for ((i=1;i<=num_files;i++)); do
    head -c $file_size </dev/urandom >filein"$i";
    touch fileout"$i";
done

tmuxinator start -p concurrency_and_limit.yml