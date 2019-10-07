cmake_minimum_required(VERSION 2.8)
project(0756536_np_project3)

set(CMAKE_CXX_STANDARD 14)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_executable(http_server http_server.cpp funct.cpp funct.h)
target_link_libraries(http_server boost_thread boost_system -lpthread)

add_executable(console.cgi console.cpp)
target_link_libraries(console.cgi boost_thread boost_system -lpthread)