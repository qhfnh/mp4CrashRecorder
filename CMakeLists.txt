cmake_minimum_required(VERSION 3.10)
project(mp4_crash_safe_recorder VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler-specific flags
if(MSVC)
    # MSVC compiler flags
    add_compile_options(/W4)
else()
    # GCC/Clang compiler flags
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Source files
set(SOURCES
    src/common.cpp
    src/file_ops.cpp
    src/mp4_recorder.cpp
    src/moov_builder.cpp
    src/index_file.cpp
)

set(HEADERS
    include/mp4_recorder.h
    include/moov_builder.h
    include/index_file.h
    include/common.h
    include/file_ops.h
)

# Create library
add_library(mp4_recorder STATIC ${SOURCES} ${HEADERS})
target_include_directories(mp4_recorder PUBLIC include)

# Examples
add_executable(basic_recording examples/basic_recording.cpp)
target_link_libraries(basic_recording mp4_recorder)

add_executable(recovery_demo examples/recovery_demo.cpp)
target_link_libraries(recovery_demo mp4_recorder)

add_executable(advanced_recording examples/advanced_recording.cpp)
target_link_libraries(advanced_recording mp4_recorder)

add_executable(multithreaded_recording examples/multithreaded_recording.cpp)
target_link_libraries(multithreaded_recording mp4_recorder)

add_executable(error_handling examples/error_handling.cpp)
target_link_libraries(error_handling mp4_recorder)

add_executable(crash_simulation examples/crash_simulation.cpp)
target_link_libraries(crash_simulation mp4_recorder)

add_executable(mp4_playback_verify examples/mp4_playback_verify.cpp)
target_link_libraries(mp4_playback_verify mp4_recorder)

add_executable(realtime_encoding_demo examples/realtime_encoding_demo.cpp)
target_link_libraries(realtime_encoding_demo mp4_recorder)

add_executable(moov_builder_test examples/moov_builder_test.cpp)
target_link_libraries(moov_builder_test mp4_recorder)

# Tests
enable_testing()
add_executable(test_recovery tests/test_recovery.cpp)
target_link_libraries(test_recovery mp4_recorder)
add_test(NAME recovery_test COMMAND test_recovery)

add_executable(test_index_file tests/test_index_file.cpp)
target_link_libraries(test_index_file mp4_recorder)
add_test(NAME index_file_test COMMAND test_index_file)

add_executable(test_performance tests/test_performance.cpp)
target_link_libraries(test_performance mp4_recorder)
add_test(NAME performance_test COMMAND test_performance)

add_executable(test_ffmpeg_validation tests/test_ffmpeg_validation.cpp)
target_link_libraries(test_ffmpeg_validation mp4_recorder)
add_test(NAME ffmpeg_validation_test COMMAND test_ffmpeg_validation)

# Installation
install(TARGETS mp4_recorder
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include/mp4_recorder)
