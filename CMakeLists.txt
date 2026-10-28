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

# Build the active bitboard engine explicitly. The old mailbox Board sources
# remain in the repository as legacy code, but are not part of the UCI engine.
set(SOURCES
    src/bitboard.cpp
    src/evaluate.cpp
    src/main.cpp
    src/magic.cpp
    src/move.cpp
    src/nnue.cpp
    src/precomputedEvalTables.cpp
    src/precomputedMagics.cpp
    src/search.cpp
    src/timeman.cpp
    src/tt.cpp
    src/uci.cpp
    src/zobrist.cpp
)

# Build the executable
add_executable(ironfang ${SOURCES})
find_package(Threads REQUIRED)
target_link_libraries(ironfang PRIVATE Threads::Threads)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/ironfang.nnue)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/ironfang.nnue
        ${CMAKE_CURRENT_BINARY_DIR}/resources/ironfang.nnue
        COPYONLY
    )
endif()

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

# Windows x64 (If compiling from Linux)
# mkdir -p build-windows
# cd build-windows
# cmake .. \
#   -DCMAKE_TOOLCHAIN_FILE=../toolchain-windows.cmake \
#   -DCMAKE_BUILD_TYPE=Release
# cmake --build . --config Release

# Windows Native (if compiling from Windows)
# cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release
# cmake --build build-windows

# Android x64
# mkdir -p build-android
# cd build-android
# cmake .. \
#   -DCMAKE_TOOLCHAIN_FILE=../android-toolchain-arm64.cmake \
#   -DCMAKE_BUILD_TYPE=Release
# cmake --build . --config Release

# ----------------------------- END --------------------------------------------



