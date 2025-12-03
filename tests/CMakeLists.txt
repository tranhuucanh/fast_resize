# Test executable for Phase 1
# We'll use a simple test runner for now, can add GTest later

add_executable(test_basic
    test_basic.cpp
)

target_link_libraries(test_basic PRIVATE fastresize)

target_include_directories(test_basic PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Add test
add_test(NAME BasicTest COMMAND test_basic)

# Test executable for Phase 2 - Comprehensive Resize Tests
add_executable(test_phase2
    test_phase2.cpp
)

target_link_libraries(test_phase2 PRIVATE fastresize)

target_include_directories(test_phase2 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Add test
add_test(NAME Phase2Test COMMAND test_phase2)

# Test executable for Phase 3 - Advanced Codec Tests
add_executable(test_phase3
    test_phase3.cpp
)

target_link_libraries(test_phase3 PRIVATE fastresize)

target_include_directories(test_phase3 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Add test
add_test(NAME Phase3Test COMMAND test_phase3)

# Test all extensions
add_executable(test_all_extensions
    test_all_extensions.cpp
)

target_link_libraries(test_all_extensions PRIVATE fastresize)

target_include_directories(test_all_extensions PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Add test
add_test(NAME AllExtensionsTest COMMAND test_all_extensions)

# Test executable for Phase 4 - Batch Processing & Threading Tests
add_executable(test_phase4
    test_phase4.cpp
)

target_link_libraries(test_phase4 PRIVATE fastresize)

target_include_directories(test_phase4 PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Add test
add_test(NAME Phase4Test COMMAND test_phase4)
