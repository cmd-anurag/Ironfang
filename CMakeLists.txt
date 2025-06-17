cmake_minimum_required(VERSION 3.10)
project(Chess)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect Android
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(ANDROID TRUE)
else()
    set(ANDROID FALSE)
endif()

# Detect Windows
if(WIN32)
    set(WINDOWS TRUE)
else()
    set(WINDOWS FALSE)
endif()


add_compile_options(-Wall -Wextra)

# Add optimization flags for Release
if(NOT ANDROID)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
endif()

# Include  headers
include_directories(include)

# Collect all .cpp files from src/
file(GLOB SOURCES "src/*.cpp")

# Build the executable
add_executable(ironfang ${SOURCES})

# Optional run target (disable for Android/Windows)
if(NOT ANDROID AND NOT WINDOWS)
    add_custom_target(run
        COMMAND ironfang
        DEPENDS ironfang
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()



# ----------------------- BUILD COMMANDS --------------------------------------

# Linux Native x64
# mkdir -p build-linux
# cd build-linux
# cmake .. -DCMAKE_BUILD_TYPE=Release
# cmake --build . --config Release

# Windows x64
# mkdir -p build-windows
# cd build-windows
# cmake .. \
#   -DCMAKE_TOOLCHAIN_FILE=../toolchain-windows.cmake \
#   -DCMAKE_BUILD_TYPE=Release
# cmake --build . --config Release

# Android x64
# mkdir -p build-android
# cd build-android
# cmake .. \
#   -DCMAKE_TOOLCHAIN_FILE=../android-toolchain-arm64.cmake \
#   -DCMAKE_BUILD_TYPE=Release
# cmake --build . --config Release

# ----------------------------- END --------------------------------------------



