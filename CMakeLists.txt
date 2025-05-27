cmake_minimum_required(VERSION 3.10)
project(Chess)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -g")

# Make headers available
include_directories(include)

# Grab all source files
file(GLOB SOURCES "src/*.cpp")

# Create the final executable
add_executable(chess ${SOURCES})

# Optional: custom run target
add_custom_target(run
    COMMAND chess
    DEPENDS chess
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)