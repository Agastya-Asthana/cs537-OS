cmake_minimum_required(VERSION 3.25)
project(Project_7___Drive_Recovery C)

set(CMAKE_C_STANDARD 11)

include_directories(starter_code)

add_executable(Project_7___Drive_Recovery
        starter_code/ext2_fs.h
        starter_code/read_ext2.c
        starter_code/read_ext2.h
        starter_code/runscan.c starter_code/runscantest.c starter_code/runscantest2.c starter_code/runscantest3.c)
