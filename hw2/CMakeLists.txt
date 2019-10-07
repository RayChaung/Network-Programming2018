cmake_minimum_required(VERSION 2.8)
project(0756536_np_project2)

set(CMAKE_CXX_STANDARD 14)


add_executable(np_simple main.cpp function.cpp function.h socket.cpp)
add_executable(np_single_proc sp_main.cpp sp_funct.cpp sp_funct.h sp_socket.cpp)
add_executable(np_multi_proc sm_main.cpp sm_funct.cpp sm_funct.h sm_socket.cpp)