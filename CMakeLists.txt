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

# Add the executable
add_executable(ObjectHighlighter ${SOURCES})
target_link_libraries(ObjectHighlighter ${OpenCV_LIBS})