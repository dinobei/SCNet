include_directories(${${the_library}_include_directories})

file(GLOB_RECURSE header_files *.h)
file(GLOB_RECURSE source_files *.cpp)

find_library(MYSQLCPPCONN_LIBRARY mysqlcppconn HINTS /usr/local/lib)
message("MYSQLCPPCONN_LIBRARY=" ${MYSQLCPPCONN_LIBRARY})

add_executable(rendezvous_server ${header_files} ${source_files} ${proto_compiled_files})
target_link_libraries (rendezvous_server ${the_library})
target_link_libraries(rendezvous_server ${MYSQLCPPCONN_LIBRARY})
target_include_directories(rendezvous_server PUBLIC ${proto_output_path})
