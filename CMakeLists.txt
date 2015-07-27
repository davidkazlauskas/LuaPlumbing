cmake_minimum_required( VERSION 2.7 )
project( NthPrimeGen CXX )

find_package(PkgConfig REQUIRED)

include_directories($ENV{TEMPLATIOUS_DIR})

ADD_DEFINITIONS(
    -std=c++11
    -g
    -x c++
)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_custom_target(
    copyscript
    ALL
    DEPENDS plumbing
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/plumbing.lua ${CMAKE_CURRENT_BINARY_DIR}/plumbing.lua
)

add_library( plumbing plumbing.cpp )
target_link_libraries( plumbing lua5.2 )

add_executable( tests Tests/tests-main.cpp )
target_link_libraries( tests plumbing )

