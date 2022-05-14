#!/bin/bash

num_files=5

for ((i=1;i<=num_files;i++)); do
    rm -f filein"$i";
    rm -f fileout"$i";
done

for ((i=1;i<=num_files;i++)); do
    head -c 1M </dev/urandom >filein"$i";
    touch fileout"$i";
done

tmuxinator start -p concurrency_and_limit.yml