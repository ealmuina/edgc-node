cmake_minimum_required(VERSION 3.5)
project(edgc-node C)

set(CMAKE_C_STANDARD 99)

find_package(MPI)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})

add_executable(edgc-node main.c include/flops.h flops.c utils.c include/utils.h)
add_executable(mpi_bandwidth mpi_bandwidth.c)

target_link_libraries(mpi_bandwidth ${MPI_C_LIBRARIES})
target_link_libraries(edgc-node m)
