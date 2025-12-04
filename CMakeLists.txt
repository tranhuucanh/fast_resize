cmake_minimum_required(VERSION 3.15)
project(fast_resize VERSION 1.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Optimization flags
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -flto")

# Option for building shared vs static libraries
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

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
    src/simd_resize.cpp
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

# Link libraries - improved static library finding like FastQR
if(FASTRESIZE_STATIC OR NOT BUILD_SHARED_LIBS)
    message(STATUS "🔧 Configuring for static linking")

    # Find static libraries with explicit paths (like FastQR)
    find_library(JPEG_STATIC_LIBRARY
        NAMES libjpeg.a libturbojpeg.a
        PATHS
            /usr/local/lib
            /opt/homebrew/lib
            /usr/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
        NO_DEFAULT_PATH
    )

    find_library(PNG_STATIC_LIBRARY
        NAMES libpng.a libpng16.a
        PATHS
            /usr/local/lib
            /opt/homebrew/lib
            /usr/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
        NO_DEFAULT_PATH
    )

    find_library(WEBP_STATIC_LIBRARY
        NAMES libwebp.a
        PATHS
            /usr/local/lib
            /opt/homebrew/lib
            /usr/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
        NO_DEFAULT_PATH
    )

    find_library(SHARPYUV_STATIC_LIBRARY
        NAMES libsharpyuv.a
        PATHS
            /usr/local/lib
            /opt/homebrew/lib
            /usr/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
        NO_DEFAULT_PATH
    )

    find_library(ZLIB_STATIC_LIBRARY
        NAMES libz.a
        PATHS
            /usr/local/lib
            /usr/local/opt/zlib/lib
            /opt/homebrew/lib
            /opt/homebrew/opt/zlib/lib
            /usr/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
        NO_DEFAULT_PATH
    )

    message(STATUS "📚 Static libraries found:")
    message(STATUS "  JPEG:     ${JPEG_STATIC_LIBRARY}")
    message(STATUS "  PNG:      ${PNG_STATIC_LIBRARY}")
    message(STATUS "  WEBP:     ${WEBP_STATIC_LIBRARY}")
    message(STATUS "  SHARPYUV: ${SHARPYUV_STATIC_LIBRARY}")
    message(STATUS "  ZLIB:     ${ZLIB_STATIC_LIBRARY}")

    # Link with required static libraries (check if they exist first)
    if(JPEG_STATIC_LIBRARY AND PNG_STATIC_LIBRARY AND ZLIB_STATIC_LIBRARY)
        target_link_libraries(fastresize PRIVATE
            Threads::Threads
            ${JPEG_STATIC_LIBRARY}
            ${PNG_STATIC_LIBRARY}
            ${ZLIB_STATIC_LIBRARY}
        )

        if(WEBP_STATIC_LIBRARY)
            target_link_libraries(fastresize PRIVATE ${WEBP_STATIC_LIBRARY})
            if(SHARPYUV_STATIC_LIBRARY)
                target_link_libraries(fastresize PRIVATE ${SHARPYUV_STATIC_LIBRARY})
            endif()
        endif()
    else()
        message(WARNING "⚠️  Some static libraries not found, falling back to dynamic linking")
        message(WARNING "  JPEG: ${JPEG_STATIC_LIBRARY}")
        message(WARNING "  PNG: ${PNG_STATIC_LIBRARY}")
        message(WARNING "  ZLIB: ${ZLIB_STATIC_LIBRARY}")

        target_link_libraries(fastresize PRIVATE
            Threads::Threads
            ${JPEG_LIBRARIES}
            ${PNG_LIBRARIES}
        )

        if(WEBP_FOUND)
            target_link_libraries(fastresize PRIVATE ${WEBP_LIBRARIES})
        endif()
    endif()

    # On Linux: ensure full static linking
    if(UNIX AND NOT APPLE)
        target_link_options(fastresize PRIVATE -static-libgcc -static-libstdc++)
    endif()
else()
    message(STATUS "🔧 Configuring for dynamic linking")
    target_link_libraries(fastresize PRIVATE
        Threads::Threads
        ${JPEG_LIBRARIES}
        ${PNG_LIBRARIES}
    )

    if(WEBP_FOUND)
        target_link_libraries(fastresize PRIVATE ${WEBP_LIBRARIES})
    endif()
endif()

# ============================================================================
# CLI Tool
# ============================================================================

option(FASTRESIZE_BUILD_CLI "Build CLI tool" ON)

if(FASTRESIZE_BUILD_CLI)
    message(STATUS "🔧 Building CLI tool")

    # Read version from VERSION file
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" FASTRESIZE_VERSION)
    string(STRIP "${FASTRESIZE_VERSION}" FASTRESIZE_VERSION)

    # CLI executable
    add_executable(fast_resize-cli
        src/cli.cpp
    )

    # Pass version to CLI
    target_compile_definitions(fast_resize-cli PRIVATE
        FASTRESIZE_VERSION="${FASTRESIZE_VERSION}"
    )

    # For standalone CLI binary, link statically
    if(FASTRESIZE_STATIC OR NOT BUILD_SHARED_LIBS)
        # Link CLI directly with library sources
        target_sources(fast_resize-cli PRIVATE ${FASTRESIZE_SOURCES})

        target_include_directories(fast_resize-cli PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${JPEG_INCLUDE_DIRS}
            ${PNG_INCLUDE_DIRS}
            ${WEBP_INCLUDE_DIRS}
        )

        # Link with static libraries
        if(JPEG_STATIC_LIBRARY AND PNG_STATIC_LIBRARY AND ZLIB_STATIC_LIBRARY)
            target_link_libraries(fast_resize-cli PRIVATE
                Threads::Threads
                ${JPEG_STATIC_LIBRARY}
                ${PNG_STATIC_LIBRARY}
                ${ZLIB_STATIC_LIBRARY}
            )

            if(WEBP_STATIC_LIBRARY)
                target_link_libraries(fast_resize-cli PRIVATE ${WEBP_STATIC_LIBRARY})
                if(SHARPYUV_STATIC_LIBRARY)
                    target_link_libraries(fast_resize-cli PRIVATE ${SHARPYUV_STATIC_LIBRARY})
                endif()
            endif()

            # On Linux: ensure full static linking
            if(UNIX AND NOT APPLE)
                target_link_options(fast_resize-cli PRIVATE -static)
            endif()
        else()
            # Fallback to library
            target_link_libraries(fast_resize-cli PRIVATE fastresize)
        endif()
    else()
        # Dynamic linking
        target_link_libraries(fast_resize-cli PRIVATE fastresize)
    endif()

    # Set output name to fast_resize (without -cli suffix)
    set_target_properties(fast_resize-cli PROPERTIES
        OUTPUT_NAME fast_resize
    )

    # Install CLI
    install(TARGETS fast_resize-cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
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
