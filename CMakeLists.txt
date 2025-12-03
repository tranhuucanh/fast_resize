cmake_minimum_required(VERSION 3.15)
project(fastresize VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Optimization flags
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -flto")

# Platform-specific optimizations
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=native")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    # ARM-specific flags
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
endif()

# Options
option(FASTRESIZE_STATIC "Build static library" ON)
option(FASTRESIZE_BUILD_TESTS "Build tests" ON)
option(FASTRESIZE_BUILD_EXAMPLES "Build examples" ON)
option(FASTRESIZE_BUILD_BENCHMARKS "Build benchmarks" ON)
option(FASTRESIZE_BUILD_RUBY "Build Ruby binding" ON)

# Find dependencies
find_package(PkgConfig REQUIRED)

# JPEG (libjpeg-turbo preferred)
pkg_check_modules(JPEG libjpeg)
if(NOT JPEG_FOUND)
    find_package(JPEG REQUIRED)
endif()

# PNG
pkg_check_modules(PNG libpng)
if(NOT PNG_FOUND)
    find_package(PNG REQUIRED)
endif()

# WebP
pkg_check_modules(WEBP libwebp libwebpdecoder libwebpdemux libsharpyuv)
if(NOT WEBP_FOUND)
    message(WARNING "libwebp not found, WEBP support will be limited")
endif()

# Source files
set(FASTRESIZE_SOURCES
    src/fastresize.cpp
    src/decoder.cpp
    src/encoder.cpp
    src/resizer.cpp
    src/thread_pool.cpp
    src/pipeline.cpp
)

# Create library
add_library(fastresize ${FASTRESIZE_SOURCES})

target_include_directories(fastresize
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${JPEG_INCLUDE_DIRS}
        ${PNG_INCLUDE_DIRS}
        ${WEBP_INCLUDE_DIRS}
)

# Link with threading library
find_package(Threads REQUIRED)

# Link libraries
if(FASTRESIZE_STATIC)
    # Try to find static versions
    find_library(JPEG_STATIC_LIB NAMES libjpeg.a libturbojpeg.a jpeg turbojpeg)
    find_library(PNG_STATIC_LIB NAMES libpng.a libpng16.a png png16)
    find_library(WEBP_STATIC_LIB NAMES libwebp.a webp)
    find_library(SHARPYUV_STATIC_LIB NAMES libsharpyuv.a sharpyuv)
    find_library(Z_STATIC_LIB NAMES libz.a z)

    target_link_libraries(fastresize PRIVATE
        Threads::Threads
        ${JPEG_STATIC_LIB}
        ${PNG_STATIC_LIB}
        ${Z_STATIC_LIB}
    )

    if(WEBP_FOUND AND WEBP_STATIC_LIB)
        target_link_libraries(fastresize PRIVATE ${WEBP_STATIC_LIB})
        if(SHARPYUV_STATIC_LIB)
            target_link_libraries(fastresize PRIVATE ${SHARPYUV_STATIC_LIB})
        endif()
    endif()
else()
    target_link_libraries(fastresize PRIVATE
        Threads::Threads
        ${JPEG_LIBRARIES}
        ${PNG_LIBRARIES}
    )

    if(WEBP_FOUND)
        target_link_libraries(fastresize PRIVATE ${WEBP_LIBRARIES})
    endif()
endif()

# Tests
if(FASTRESIZE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(FASTRESIZE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Benchmarks
if(FASTRESIZE_BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()

# Install
include(GNUInstallDirs)

install(TARGETS fastresize
    EXPORT fastresize-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)
