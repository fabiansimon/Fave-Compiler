cmake_minimum_required(VERSION 3.26)
project(FAVE_CUH C)

set(CMAKE_C_STANDARD 17)

add_executable(FAVE_CUH main.c
        common.h
        chunk.c
        chunk.h
        memory.c
        memory.h
        debug.c
        debug.h
        value.h
        value.c
        vm.h
        vm.c
        compiler.c
        compiler.h
        scanner.c
        scanner.h
        object.c
        object.h
        table.c
        table.h)
