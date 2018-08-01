include_directories(${${the_library}_include_directories})

file(GLOB_RECURSE header_files *.h)
file(GLOB_RECURSE source_files *.cpp)

# Compile protobuf message
set(proto_output_path ${CMAKE_CURRENT_BINARY_DIR})
set(proto_input_path ${PROJECT_SOURCE_DIR}/third-party/SCNet-pb)
file(GLOB proto_files ${proto_input_path}/*.proto)

foreach(proto_file ${proto_files})
    get_filename_component(proto_file_we ${proto_file} NAME_WE)
    list(APPEND proto_compiled_files ${proto_output_path}/${proto_file_we}.pb.h)
    list(APPEND proto_compiled_files ${proto_output_path}/${proto_file_we}.pb.cc)
endforeach()

add_custom_command(
    OUTPUT ${proto_compiled_files}
    COMMAND protoc -I=${proto_input_path} --cpp_out=${proto_output_path} ${proto_files}
    COMMENT "build .proto files"
    VERBATIM
)

add_executable(simple_server ${header_files} ${source_files} ${proto_compiled_files})
target_link_libraries (simple_server ${the_library})
target_include_directories(simple_server PUBLIC ${proto_output_path})

add_custom_target(compile_protoc_simple_server DEPENDS ${proto_compiled_files})
