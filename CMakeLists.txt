# CMakeLists.txt

# Minimum CMake version required
cmake_minimum_required(VERSION 3.28)

# Set the project name
project(ObjectHighlighter)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)

# Find the required packages
find_package(OpenCV REQUIRED)

# Include OpenCV headers
include_directories(${OpenCV_INCLUDE_DIRS})
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")

# Find all source files
file(GLOB SOURCES "*.cpp")

# Define a macro for profiling if needed
option(ENABLE_PROFILING "Enable profiling with profiling-specific code blocks" OFF)

# Add the executable
add_executable(ObjectHighlighter ${SOURCES})

# Add compiler flags for ALL builds
target_compile_options(ObjectHighlighter PRIVATE -Wall -O2)

# If the option was turned on, add the preprocessor definition
if(ENABLE_PROFILING)
    message(STATUS "Profiling code has been ENABLED.")
    target_compile_definitions(ObjectHighlighter PRIVATE ENABLE_PROFILING)

    # Add compiler flags for profiling builds
    target_compile_options(ObjectHighlighter PRIVATE -g -fno-omit-frame-pointer)
endif()

# Link OpenCV libraries
target_link_libraries(ObjectHighlighter ${OpenCV_LIBS})