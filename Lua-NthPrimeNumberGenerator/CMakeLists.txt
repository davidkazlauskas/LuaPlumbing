cmake_minimum_required( VERSION 2.7 )
project( NthPrimeGen CXX )

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTKMM gtkmm-3.0)

include_directories(${GTKMM_INCLUDE_DIRS} $ENV{TEMPLATIOUS_DIR})
link_directories(${GTKMM_LIBRARY_DIRS})

ADD_DEFINITIONS(
    -std=c++11
    -g
    -x c++
)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(COPY ui.glade DESTINATION ${CMAKE_BINARY_DIR})

add_custom_target(
    copyscript
    ALL
    DEPENDS generator
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/main.lua ${CMAKE_CURRENT_BINARY_DIR}/main.lua
)

add_custom_target(
    copyscript2
    ALL
    DEPENDS generator
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/mobdebug.lua ${CMAKE_CURRENT_BINARY_DIR}/mobdebug.lua
)

add_executable( generator main.cpp plumbing.cpp )
target_link_libraries( generator ${GTKMM_LIBRARIES} lua5.2 lualib50 pthread )
