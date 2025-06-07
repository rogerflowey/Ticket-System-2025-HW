cmake_minimum_required(VERSION 3.28) # Lowered slightly for broader compatibility, 3.28 is fine too.
project(MyBPTLib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Good practice

add_library(bpt_core
        my-bpt/disk/IO_manager.cpp
        my-bpt/disk/IO_utils.cpp
)

# --- Include Directories ---
target_include_directories(bpt_core
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>

        PRIVATE
        my-bpt
)

# --- Compile Options ---
target_compile_options(bpt_core PRIVATE -O2)