cmake_minimum_required(VERSION 4.0.2...4.3.1)

include(cmake/Utilities.cmake)
include(cmake/Toolchain.cmake)

initialize_toolchain()

project(mindtosis VERSION 0.0.0 LANGUAGES CXX)

prepare_testing()

add_subdirectory(src/)
