#!/usr/bin/env bash
mkdir -p build-dir
cd build-dir/
cmake ..
make
mpicc ../mpi_bandwidth.c -o mpi_bandwidth