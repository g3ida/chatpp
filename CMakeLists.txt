cmake_minimum_required(VERSION 3.2.2)

project(chatpp)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include_directories(${PROJECT_SOURCE_DIR}/extern)

option(CHATPP_BUILD_TESTS "Build tests" OFF)

add_subdirectory(src)

if(CHATPP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
