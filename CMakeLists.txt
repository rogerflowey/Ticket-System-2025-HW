# ./CMakeLists.txt
cmake_minimum_required(VERSION 3.28)
project(Ticket_System_2025_HW LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Configuration for output directories ---
# This will place the executable 'code' into ${CMAKE_SOURCE_DIR}/test_env/
# CMAKE_SOURCE_DIR is the root directory of your project (where this CMakeLists.txt is)
# You can set this globally:
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/test_env)
# Or, more specifically per target (preferred):
# (We'll do this after defining the target 'code')

add_subdirectory(src/database)
add_subdirectory(src/utils)

file(GLOB CODE_APP_SOURCES
        RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        "src/*.cpp" # Be careful if main.cpp is also in src/, it might be listed twice.
        # It's often better to list sources explicitly or ensure main.cpp is not matched.
)

add_executable(code_test
        src/main.cpp
        ${CODE_APP_SOURCES}
)



set_target_properties(code_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/test_env"
)


add_executable(code
        src/main.cpp          # Same main source as 'code'
        ${CODE_APP_SOURCES}   # Same additional sources as 'code'
)

# Set the output directory for the 'code_in_root' executable
set_target_properties(code PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}" # Output to project root
)

target_compile_options(code PRIVATE -O2 -Wall)


target_include_directories(code
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src # For main.cpp and other direct sources
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stlite
)

target_link_libraries(code PRIVATE bpt_core utils_lib)

target_include_directories(code_test
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/stlite
)

target_link_libraries(code_test PRIVATE bpt_core utils_lib)
# --- END OF NEW TARGET ---

add_custom_target(test-code-clear
        # Command to remove the directory and its contents
        # ${CMAKE_COMMAND} -E rm -rf is a platform-independent way to remove a directory recursively
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_SOURCE_DIR}/test_env"
        # Command to recreate the directory (optional, but good practice)
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/test_env"
        COMMENT "Clearing the ${CMAKE_SOURCE_DIR}/test_env directory..."
        VERBATIM # Ensures commands are passed as-is
)
