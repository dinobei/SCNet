include_directories(${${the_library}_include_directories})

file(GLOB_RECURSE header_files *.h)
file(GLOB_RECURSE source_files *.cpp)

add_executable(simple_server ${header_files} ${source_files})
target_link_libraries (simple_server ${the_library})
