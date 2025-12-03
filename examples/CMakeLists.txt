# Example programs

add_executable(basic_resize
    basic_resize.cpp
)

target_link_libraries(basic_resize PRIVATE fastresize)

target_include_directories(basic_resize PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
