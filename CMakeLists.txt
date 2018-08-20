cmake_minimum_required(VERSION 3.4)
set(the_library scnet)
project(${the_library})

set(CMAKE_CXX_STANDARD 11)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

include(cmake/Utils.cmake)
include(cmake/Dependencies.cmake)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

option (scnet_BUILD_SHARED "Build shared library" FALSE)
if(scnet_BUILD_SHARED)
    set(the_library_type SHARED)
    if(MSVC)
        msvc_multi_threaded_static_turn(OFF)
    endif()
else()
    set(the_library_type STATIC)
    if(MSVC)
        msvc_multi_threaded_static_turn(ON)
    endif()
endif()

option (scnet_USE_PROTOBUF_LITE "Use protobuf lite" FALSE)

add_subdirectory(${the_library})

add_subdirectory(app)
