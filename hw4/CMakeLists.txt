cmake_minimum_required(VERSION 2.8)
project(0756536_np_project4)

set(CMAKE_CXX_STANDARD 14)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_executable(socks_server socks_server.cpp socks4.h)
target_link_libraries(socks_server boost_thread boost_system -lpthread)

add_executable(hw4.cgi console.cpp socks4_client.h)
target_link_libraries(hw4.cgi boost_thread boost_system -lpthread)