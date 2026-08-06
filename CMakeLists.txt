cmake_minimum_required(VERSION 3.16)
project(KronaPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Dependencies
# =============================================================================
find_package(PkgConfig REQUIRED)

# SDL2
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

# FFmpeg components
pkg_check_modules(AVFORMAT  REQUIRED libavformat)
pkg_check_modules(AVCODEC   REQUIRED libavcodec)
pkg_check_modules(SWSCALE   REQUIRED libswscale)
pkg_check_modules(AVUTIL    REQUIRED libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)

# Aggregate include / lib dirs for convenience
set(FFMPEG_INCLUDE_DIRS
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
)

set(FFMPEG_LIBRARIES
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
)

set(FFMPEG_LIBRARY_DIRS
    ${AVFORMAT_LIBRARY_DIRS}
    ${AVCODEC_LIBRARY_DIRS}
    ${SWSCALE_LIBRARY_DIRS}
    ${AVUTIL_LIBRARY_DIRS}
    ${SWRESAMPLE_LIBRARY_DIRS}
)

# =============================================================================
# Core Library (testable logic, no main())
# =============================================================================
add_library(core_lib STATIC
    src/decoder.cpp
)

target_include_directories(core_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_directories(core_lib PUBLIC
    ${SDL2_LIBRARY_DIRS}
    ${SDL2_TTF_LIBRARY_DIRS}
    ${FFMPEG_LIBRARY_DIRS}
)

target_link_libraries(core_lib PUBLIC
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${FFMPEG_LIBRARIES}
    pthread
)

# =============================================================================
# Main Executable
# =============================================================================
add_executable(krona_player
    src/main.cpp
)

target_link_libraries(krona_player PRIVATE core_lib)

# =============================================================================
# Tests (Google Test)
# =============================================================================
enable_testing()

find_package(GTest QUIET)

if(GTest_FOUND)
    add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE core_lib GTest::GTest GTest::Main)
    add_test(NAME CoreTests COMMAND test_core)
else()
    # Fallback: link manually (Ubuntu 20.04 may not have GTestConfig.cmake)
    find_library(GTEST_LIB gtest)
    find_library(GTEST_MAIN_LIB gtest_main)
    if(GTEST_LIB AND GTEST_MAIN_LIB)
        add_executable(test_core tests/test_core.cpp)
        target_include_directories(test_core PRIVATE /usr/include)
        target_link_libraries(test_core PRIVATE core_lib ${GTEST_LIB} ${GTEST_MAIN_LIB} pthread)
        add_test(NAME CoreTests COMMAND test_core)
    else()
        message(WARNING "Google Test not found — tests will not be built.")
    endif()
endif()
