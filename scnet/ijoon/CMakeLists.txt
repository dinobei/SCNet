file(GLOB_RECURSE ${the_library}_header_files *.h)
file(GLOB_RECURSE ${the_library}_source_files *.c*)

file(GLOB_RECURSE files "*.*")
group_files("${files}")

include(${PROJECT_SOURCE_DIR}/cmake/Version.cmake)

list(APPEND ${the_library}_include_directories ${CMAKE_CURRENT_LIST_DIR})
set(${the_library}_include_directories ${${the_library}_include_directories} PARENT_SCOPE)

include_directories(${${the_library}_include_directories})

# Compile protobuf message
set(proto_output_path ${CMAKE_CURRENT_BINARY_DIR})
set(proto_input_path ${PROJECT_SOURCE_DIR}/scnet/pb)
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

add_library(${the_library} ${the_library_type} ${${the_library}_source_files} ${${the_library}_header_files} ${proto_compiled_files})
target_link_libraries(${the_library} ${${the_library}_library_dependencies})

if(WIN32)
	set_target_properties(${the_library} PROPERTIES COMPILE_FLAGS "/wd4819")
endif()

target_include_directories(${the_library} PUBLIC ${${the_library}_include_directories})
target_include_directories(${the_library} PUBLIC ${proto_output_path})
add_custom_target(compile_protoc_${the_library} DEPENDS ${proto_compiled_files})