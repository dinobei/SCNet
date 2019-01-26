include_directories(${${the_library}_include_directories})

file(GLOB_RECURSE header_files *.h)
file(GLOB_RECURSE source_files *.cpp)

add_executable(rendezvous_server ${header_files} ${source_files} ${proto_compiled_files})
target_link_libraries (rendezvous_server ${the_library})
target_include_directories(rendezvous_server PUBLIC ${proto_output_path})
