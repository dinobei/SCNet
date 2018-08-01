
find_package(protobuf QUIET)
if(Protobuf_FOUND)
    message("protobuf found : " ${Protobuf_INCLUDE_DIRS})

    list(APPEND ${the_library}_library_dependencies ${Protobuf_LIBRARIES})
    list(APPEND ${the_library}_include_directories ${Protobuf_INCLUDE_DIRS})
else()
    message("Can not find protobuf. Using thirdparty dir.")

    set(protobuf_lib_name libprotobuf.a)
    if(WIN32)
        set(protobuf_lib_name libprotobuf.lib)
    endif()

    set(protobuf_BUILD_TESTS OFF CACHE BOOL "build test off")
    set(protobuf_WITH_ZLIB OFF CACHE BOOL "build zlib off")
    add_subdirectory(${PROJECT_SOURCE_DIR}/third-party/protobuf/cmake)
    list(APPEND ${the_library}_library_dependencies ${PROJECT_BINARY_DIR}/third-party/protobuf/cmake/${protobuf_lib_name})
    list(APPEND ${the_library}_include_directories ${PROJECT_SOURCE_DIR}/third-party/protobuf/src)
endif()

find_package(coreutils QUIET)
if(Coreutils_FOUND)
    message("coreutils found : " ${coreutils_INCLUDE_DIRS})
else()
    message("Can not find coreutils. Using thirdparty dir.")

    set(coreutils_lib_name libcoreutils.a)
    if(WIN32)
        set(coreutils_lib_name coreutils.lib)
    endif()

    set(coreutils_BUILD_APPS OFF CACHE BOOL "build app off")

    add_subdirectory(${PROJECT_SOURCE_DIR}/third-party/coreutils)
    message("coreutils_include_directories: ${coreutils_include_directories}")
    list(APPEND ${the_library}_library_dependencies ${PROJECT_BINARY_DIR}/third-party/coreutils/bin/$<CONFIG>/${coreutils_lib_name})
    list(APPEND ${the_library}_include_directories ${PROJECT_SOURCE_DIR}/third-party/coreutils/coreutils)

    message("${the_library}_library_dependencies: ${${the_library}_library_dependencies}")
    message("${the_library}_include_directories: ${${the_library}_include_directories}")
endif()

if(WIN32)
    list(APPEND ${the_library}_library_dependencies ws2_32)
else()
    list(APPEND ${the_library}_library_dependencies pthread)
endif()
