cmake_minimum_required(VERSION 3.24.0)
project(bf-compiler)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Ensure the compiler supports C++17# Set your project compile flags.
# E.g. if using the C++ header files
# you will need to enable C++11 support
# for your compiler.

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Now build our tools
set(COMPILE_WARNING_AS_ERROR YES)
add_executable(compiler.out compiler.cpp)
add_executable(interpreter.out interpreter.cpp)
set(COMPILE_WARNING_AS_ERROR NO)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support core irreader)


target_compile_options(compiler.out PRIVATE -std=c++17)
target_compile_options(interpreter.out PRIVATE -std=c++17 -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self  -Wmissing-include-dirs -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused)
if(CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_options(compiler.out PRIVATE -g -O0)
    target_compile_options(interpreter.out PRIVATE -g -O0)
endif()

if(CMAKE_BUILD_TYPE MATCHES Release)
    target_compile_options(compiler.out PRIVATE -O3)
    target_compile_options(interpreter.out PRIVATE -O3)
endif()

# Link against LLVM libraries
target_link_libraries(compiler.out ${llvm_libs})