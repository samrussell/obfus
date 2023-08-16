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

# Define source files for HelloWorld library
set(SOURCE_FILES_HelloWorld HelloWorld.cpp)

# Build the HelloWorld shared library
add_library(HelloWorld SHARED ${SOURCE_FILES_HelloWorld})

# Link HelloWorld against LLVM libraries
target_link_libraries(HelloWorld PRIVATE LLVM)

# Set the RPATH for the HelloWorld library to find LLVM libraries
set_target_properties(HelloWorld PROPERTIES
  INSTALL_RPATH "${LLVM_LIBRARY_DIR}"
  BUILD_WITH_INSTALL_RPATH TRUE
)

# Copy LLVM shared libraries to the output directory for HelloWorld
add_custom_command(TARGET HelloWorld POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${LLVM_LIBRARY_DIR} $<TARGET_FILE_DIR:HelloWorld>
)

# Define source files for ver2 library
set(SOURCE_FILES_ver2 ver2.cpp)

# Build the ver2 shared library
add_library(ver2 SHARED ${SOURCE_FILES_ver2})

# Link ver2 against LLVM libraries
target_link_libraries(ver2 PRIVATE LLVM)

# Set the RPATH for the ver2 library to find LLVM libraries
set_target_properties(ver2 PROPERTIES
  INSTALL_RPATH "${LLVM_LIBRARY_DIR}"
  BUILD_WITH_INSTALL_RPATH TRUE
)

# Copy LLVM shared libraries to the output directory for ver2
add_custom_command(TARGET ver2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${LLVM_LIBRARY_DIR} $<TARGET_FILE_DIR:ver2>
)
