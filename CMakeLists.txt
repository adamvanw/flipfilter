cmake_minimum_required(VERSION 3.15)
project(flipfilter C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set paths to external dependencies
set(RAYLIB_PATH "C:/raylib" CACHE PATH "Path to Raylib installation")
set(FFMPEG_PATH "C:/ffmpeg" CACHE PATH "Path to FFmpeg installation")
set(RAYLIB_MEDIA_PATH "${CMAKE_SOURCE_DIR}/vendor/raylib-media" CACHE PATH "Path to raylib-media")

# Add include directories
include_directories(
    ${RAYLIB_PATH}/include
    ${FFMPEG_PATH}/include
    ${RAYLIB_MEDIA_PATH}/include
)

# Add library directories
link_directories(
    ${RAYLIB_PATH}/lib
    ${FFMPEG_PATH}/lib
)

# Build raylib-media as a static library
file(GLOB RAYLIB_MEDIA_SOURCES "${RAYLIB_MEDIA_PATH}/src/*.c")
add_library(raylib-media STATIC ${RAYLIB_MEDIA_SOURCES})
target_include_directories(raylib-media PUBLIC
    ${RAYLIB_MEDIA_PATH}/src
    ${RAYLIB_PATH}/include
    ${FFMPEG_PATH}/include
)

# Add subdirectory for source files
add_subdirectory(src)