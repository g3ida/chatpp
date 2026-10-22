cmake_minimum_required(VERSION 3.20)

project(chatpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHATPP_BUILD_TESTS "Build tests" OFF)

find_package(Threads REQUIRED)

add_custom_target(update-deps
  COMMAND ${CMAKE_COMMAND} "-DWORK_DIR=${CMAKE_BINARY_DIR}/deps"
    -P "${PROJECT_SOURCE_DIR}/cmake/UpdateDeps.cmake"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Updating vendored dependencies"
  USES_TERMINAL
  VERBATIM)

add_subdirectory(src)

if(CHATPP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
