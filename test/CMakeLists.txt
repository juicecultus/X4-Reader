cmake_minimum_required(VERSION 3.16)
project(microreader-tests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Test include directories
include_directories(
  ${CMAKE_SOURCE_DIR}/test
  ${CMAKE_SOURCE_DIR}/test/mocks
  ${CMAKE_SOURCE_DIR}/test/common
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/content
  ${CMAKE_SOURCE_DIR}/src/text
  ${CMAKE_SOURCE_DIR}/src/rendering
  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/src/resources
)

add_compile_definitions(TEST_BUILD)

# Gather everything from src into a static library to make linking for tests easier
file(GLOB_RECURSE CORE_SOURCES
  ${CMAKE_SOURCE_DIR}/src/*.cpp
  ${CMAKE_SOURCE_DIR}/src/*.c
)
# Debug: print number of core sources
message(STATUS "CORE_SOURCES_COUNT=${CORE_SOURCES}")
# Exclude the main application entry if present
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/main.cpp$")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/BatteryMonitor.cpp$")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/Buttons.cpp$")

message(STATUS "CORE_SOURCES_AFTER_FILTER=${CORE_SOURCES}")

add_library(microreader_core STATIC ${CORE_SOURCES})

target_include_directories(microreader_core PUBLIC
  ${CMAKE_SOURCE_DIR}/test/mocks
  ${CMAKE_SOURCE_DIR}/test/common
  ${CMAKE_SOURCE_DIR}/src
)

# Ensure fonts and other resource headers are found
target_include_directories(microreader_core PUBLIC ${CMAKE_SOURCE_DIR}/src/resources)

# Common test helpers
set(TEST_HELPER_SOURCES
  ${CMAKE_SOURCE_DIR}/test/common/test_utils.cpp
  ${CMAKE_SOURCE_DIR}/test/mocks/platform_stubs.cpp
)

file(GLOB_RECURSE TEST_SOURCES ${CMAKE_SOURCE_DIR}/test/unit/*.cpp)

foreach(TEST_SRC ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
  add_executable(${TEST_NAME} ${TEST_SRC} ${TEST_HELPER_SOURCES})
  target_link_libraries(${TEST_NAME} PRIVATE microreader_core)
  target_include_directories(${TEST_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/test/mocks
    ${CMAKE_SOURCE_DIR}/test/common
    ${CMAKE_SOURCE_DIR}/src
  )
  # Place test executables into test/build/bin by default
  set_target_properties(${TEST_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/test/build/bin)
endforeach()

message(STATUS "Configured ${TEST_SOURCES} tests")
