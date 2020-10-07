# # Compile protobuf message
# set(proto_output_path ${CMAKE_CURRENT_BINARY_DIR})
# set(proto_input_path ${PROJECT_SOURCE_DIR}/scnet/pb)
# file(GLOB proto_files ${proto_input_path}/*.proto)

# foreach(proto_file ${proto_files})
#     get_filename_component(proto_file_we ${proto_file} NAME_WE)
#     list(APPEND proto_compiled_files ${proto_output_path}/${proto_file_we}.pb.h)
#     list(APPEND proto_compiled_files ${proto_output_path}/${proto_file_we}.pb.cc)
# endforeach()

# add_custom_command(
#     OUTPUT ${proto_compiled_files}
#     COMMAND protoc -I=${proto_input_path} --cpp_out=${proto_output_path} ${proto_files}
#     COMMENT "build .proto files"
#     VERBATIM
# )
# message("###" ${proto_output_path})
# include_directories(${proto_output_path})
# target_include_directories(${the_library} PUBLIC ${proto_output_path})
# add_custom_target(compile_protoc_${the_library} DEPENDS ${proto_compiled_files})