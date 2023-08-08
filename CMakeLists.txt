cmake_minimum_required(VERSION 3.12)

project(HelloWorld)

# Set the C++ standard to C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find LLVM package
find_package(LLVM 14 REQUIRED CONFIG)

# Add include directories for LLVM headers
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Define your source file
set(SOURCE_FILES HelloWorld.cpp)

# Build a shared library
add_library(HelloWorld SHARED ${SOURCE_FILES})

# Link against LLVM libraries
target_link_libraries(HelloWorld PRIVATE LLVM)

# Set the RPATH for the library to find LLVM libraries
set_target_properties(HelloWorld PROPERTIES
  INSTALL_RPATH "${LLVM_LIBRARY_DIR}"
  BUILD_WITH_INSTALL_RPATH TRUE
)

# Copy LLVM shared libraries to the output directory
add_custom_command(TARGET HelloWorld POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${LLVM_LIBRARY_DIR} $<TARGET_FILE_DIR:HelloWorld>
)
