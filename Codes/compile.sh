#!/bin/bash

# Compile my_docker with static linking
gcc -o my_docker my_docker.c -static -pthread

# Compile print
gcc -o print print.c

gcc -o test_memory test_memory.c

gcc -o test_cpu test_cpu.c

gcc -o list list.c

gcc -o stop_resume stop_resume.c

echo "Compilation completed!"
