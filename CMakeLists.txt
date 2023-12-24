cmake_minimum_required(VERSION 3.10)
project(stperf VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

add_library(stperf STATIC stperf.cpp)
target_compile_options(stperf PUBLIC -Wall -Wextra -pedantic -O3)

# Enable testing only for direct compilations
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)
    
    # include(CTest)
    add_executable(stperf-test test.cpp)
    target_link_libraries(stperf-test PRIVATE stperf pthread Catch2::Catch2WithMain)
    # add_test(NAME TestSuite COMMAND stperf-test)
endif()
