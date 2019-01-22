include_directories(${${the_library}_include_directories})

file(GLOB_RECURSE header_files *.h)
file(GLOB_RECURSE source_files *.cpp)

add_executable(relay_server ${header_files} ${source_files} ${proto_compiled_files})
target_link_libraries (relay_server ${the_library})
target_include_directories(relay_server PUBLIC ${proto_output_path})
