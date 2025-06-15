cmake_minimum_required(VERSION 3.10)
project(Chess)

set(CMAKE_CXX_STANDARD 17)

# Detect Android environment
if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(ANDROID TRUE)
else()
    set(ANDROID FALSE)
endif()

# Base flags for all builds (including debug info and warnings)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -g")

# Add optimization flags for Release build only
if(NOT ANDROID)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
endif()

# Include headers
include_directories(include)

# Grab all source files
file(GLOB SOURCES "src/*.cpp")

# Create executable
add_executable(ironfang ${SOURCES})

# Optional: custom run target (only for desktop)
if(NOT ANDROID)
    add_custom_target(run
        COMMAND ironfang
        DEPENDS ironfang
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()
