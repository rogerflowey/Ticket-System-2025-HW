cmake_minimum_required(VERSION 3.28)
project(Ticket_System_2025_HW)

set(CMAKE_CXX_STANDARD 14)

include_directories(src)
include_directories(src/database/include)
include_directories(src/database/include/stlite)

add_executable(code src/main.cpp)
