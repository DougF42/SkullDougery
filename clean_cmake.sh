#!/bin/bash
# Shell script to remove all generated files, including CMAKE configuration
#   stuff
rm -rf CMakeCache.txt CMakeFiles cmake_install.cmake Makefile *~i

# Removee project files
rm -f lucas_sparse tracker *~
