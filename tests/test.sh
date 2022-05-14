#!/bin/bash

# How many test files to generate, both input and output.
num_files=5

# Size of each input file. Should be 10M+ to create scenarios with interesting delay.
file_size="100M"

for ((i=1;i<=num_files;i++)); do
    rm -f filein"$i";
    rm -f fileout"$i";
done

for ((i=1;i<=num_files;i++)); do
    head -c $file_size </dev/urandom >filein"$i";
    touch fileout"$i";
done

tmuxinator start -p concurrency_and_limit.yml