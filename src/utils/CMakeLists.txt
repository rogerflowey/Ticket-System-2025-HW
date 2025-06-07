file(GLOB UTILS_SOURCES "*.cpp") # Globs *.cpp in the current directory (src/utils)

add_library(utils_lib STATIC
        ${UTILS_SOURCES}
)

target_include_directories(utils_lib
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR} # Makes <utils/header.h> work from outside
)