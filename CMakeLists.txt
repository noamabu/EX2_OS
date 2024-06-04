cmake_minimum_required(VERSION 3.28)
project(EX2_OS)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(EX2_OS
        demo_itimer.c
        demo_jmp.c
        demo_singInt_handler.c
        uthreads.cpp
        uthreads.h)
