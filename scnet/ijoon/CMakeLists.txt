file(GLOB_RECURSE ${the_library}_header_files *.h)
file(GLOB_RECURSE ${the_library}_source_files *.c*)

file(GLOB_RECURSE files "*.*")
group_files("${files}")

include(${PROJECT_SOURCE_DIR}/cmake/Version.cmake)

list(APPEND ${the_library}_include_directories ${CMAKE_CURRENT_LIST_DIR})
set(${the_library}_include_directories ${${the_library}_include_directories} PARENT_SCOPE)

include_directories(${${the_library}_include_directories})

add_library(${the_library} ${the_library_type} ${${the_library}_source_files} ${${the_library}_header_files})
target_link_libraries(${the_library} ${${the_library}_library_dependencies})

if(WIN32)
	set_target_properties(${the_library} PROPERTIES COMPILE_FLAGS "/wd4819")
endif()

target_include_directories(${the_library} PUBLIC ${${the_library}_include_directories})
