# Example CMake command line to create project build files:
#
# *** Windows ***
# cmake -G "Visual Studio 17 2022" -A Win32 -B Build -S .
#
# *** Linux ***
# cmake -G "Unix Makefiles" -B Build -S .

# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and language (C or C++)
project(Allocator VERSION 1.0 LANGUAGES CXX)

# Collect all .cpp and *.h source files in the current directory
# The library (src/, the Arduino library layout) and the desktop demo and
# unit tests (examples/desktop)
file(GLOB SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/examples/desktop/*.cpp" "${CMAKE_SOURCE_DIR}/examples/desktop/*.h")

# Add an executable target
add_executable(AllocatorApp ${SOURCES})
target_include_directories(AllocatorApp PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/examples/desktop)



