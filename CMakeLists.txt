cmake_minimum_required(VERSION 3.30)
project(AmongTheStars)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CPM/CPM.cmake)

CPMAddPackage(
    NAME fmt
    GITHUB_REPOSITORY fmtlib/fmt
    GIT_TAG 11.0.2
    OPTIONS "FMT_USE_SHARED ON" "CMAKE_POSITION_INDEPENDENT_CODE ON"
)

CPMAddPackage(
  NAME args 
  GITHUB_REPOSITORY Taywee/args
  GIT_TAG 6.4.6
)
CPMAddPackage("gh:SFML/SFML#2.6.1")
add_executable(${PROJECT_NAME} main.cpp)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -pedantic)

target_link_libraries(${PROJECT_NAME} fmt::fmt sfml-graphics)

