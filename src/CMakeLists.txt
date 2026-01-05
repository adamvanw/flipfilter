# Collect all source files
file(GLOB SOURCES "*.c" "*.h")

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    raylib-media
    raylib
    opengl32
    gdi32
    winmm
    avcodec
    avformat
    avutil
    swscale
    swresample
)

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy FFmpeg DLLs to build directory after build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${FFMPEG_PATH}/bin/avcodec-61.dll"
    "${FFMPEG_PATH}/bin/avformat-61.dll"
    "${FFMPEG_PATH}/bin/avutil-59.dll"
    "${FFMPEG_PATH}/bin/swscale-8.dll"
    "${FFMPEG_PATH}/bin/swresample-5.dll"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying FFmpeg DLLs to build directory"
)

# Optionally copy Raylib DLL if using shared library
if(EXISTS "${RAYLIB_PATH}/lib/raylib.dll")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RAYLIB_PATH}/lib/raylib.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying Raylib DLL to build directory"
    )
endif()