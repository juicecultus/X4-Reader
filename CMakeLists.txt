cmake_minimum_required(VERSION 3.16)
project(microreader-root NONE)

# Option to build tests; default ON for convenience
option(BUILD_TESTS "Build unit tests using the CMake test system" ON)

if(BUILD_TESTS)
  add_subdirectory(test)
endif()
