# Benchmark executables

add_executable(bench_phase2
    bench_phase2.cpp
)

target_link_libraries(bench_phase2 PRIVATE fastresize)

target_include_directories(bench_phase2 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Phase 3 benchmark
add_executable(bench_phase3
    bench_phase3.cpp
)

target_link_libraries(bench_phase3 PRIVATE fastresize)

target_include_directories(bench_phase3 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Phase 4 benchmark - Batch Processing
add_executable(bench_phase4
    bench_phase4.cpp
)

target_link_libraries(bench_phase4 PRIVATE fastresize)

target_include_directories(bench_phase4 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Resource Usage benchmark - CPU and RAM monitoring
add_executable(bench_resource_usage
    bench_resource_usage.cpp
)

target_link_libraries(bench_resource_usage PRIVATE fastresize)

target_include_directories(bench_resource_usage PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Format-specific benchmark - Test JPG, PNG, WEBP, BMP separately
add_executable(bench_formats
    bench_formats.cpp
)

target_link_libraries(bench_formats PRIVATE fastresize)

target_include_directories(bench_formats PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Phase C Pipeline benchmark - Compare pipeline vs thread pool
add_executable(bench_pipeline
    bench_pipeline.cpp
)

target_link_libraries(bench_pipeline PRIVATE fastresize)

target_include_directories(bench_pipeline PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Benchmark vs libvips - Compare FastResize with libvips
find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS vips-cpp)

if(VIPS_FOUND)
    add_executable(bench_vs_libvips
        bench_vs_libvips.cpp
    )

    target_link_libraries(bench_vs_libvips PRIVATE
        fastresize
    )

    target_link_directories(bench_vs_libvips PRIVATE ${VIPS_LIBRARY_DIRS})
    target_link_libraries(bench_vs_libvips PRIVATE ${VIPS_LIBRARIES})

    target_include_directories(bench_vs_libvips PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${VIPS_INCLUDE_DIRS}
    )

    target_compile_options(bench_vs_libvips PRIVATE ${VIPS_CFLAGS_OTHER})

    message(STATUS "libvips found - bench_vs_libvips will be built")
else()
    message(WARNING "libvips not found - bench_vs_libvips will NOT be built")
endif()
