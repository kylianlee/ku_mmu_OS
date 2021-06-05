cmake_minimum_required(VERSION 3.17)
project(ku_mmu_OS C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(ku_mmu_OS
        ku_mmu.h)
