project(test CXX)
cmake_minimum_required(VERSION 2.8)
add_definitions(-g -Wall -Werror)
include_directories(.)

add_executable(iss main.cpp iface.cpp receiver.cpp)

