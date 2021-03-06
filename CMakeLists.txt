cmake_minimum_required(VERSION 3.9.3)

project(WaveFunctionCollapse)

set(EXECUTABLE_OUTPUT_PATH bin/)

file (GLOB_RECURSE source_files
    src/*.cpp
	src/*.hpp
)

add_executable(wavefunctioncollapse ${source_files})

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})
target_link_libraries(wavefunctioncollapse ${Boost_LIBRARIES})

list(APPEND CMAKE_MODULE_PATH "/usr/share/SFML/cmake/Modules")
find_package(SFML 2.4 COMPONENTS graphics audio REQUIRED)
target_link_libraries(wavefunctioncollapse sfml-graphics)

if(CMAKE_COMPILER_IS_GNUCXX)
        add_definitions(-std=c++17 -Wall -pedantic-errors)
        # add_definitions(-O2 -s -DNDEBUG)
        add_definitions(-g -DEBUG)
        add_definitions(${GCC_COVERAGE_LINK_FLAGS}$)
endif()

set(CMAKE_BUILD_TYPE Release)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
