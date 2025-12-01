cmake_minimum_required(VERSION 3.20)
project(MultiNdiRecorder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/ui)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Placeholder paths - adjust for your environment
set(NDI_SDK_INCLUDE "C:/Program Files/NDI SDK/Include" CACHE PATH "Path to NDI SDK include")
set(NDI_SDK_LIB "C:/Program Files/NDI SDK/Lib/x64" CACHE PATH "Path to NDI SDK lib")
set(FFMPEG_INCLUDE "C:/ffmpeg/include" CACHE PATH "Path to FFmpeg includes")
set(FFMPEG_LIB "C:/ffmpeg/lib" CACHE PATH "Path to FFmpeg libraries")

include_directories(${NDI_SDK_INCLUDE} ${FFMPEG_INCLUDE} include)
link_directories(${NDI_SDK_LIB} ${FFMPEG_LIB})

file(GLOB SRC_FILES
    src/*.cpp
)

add_executable(${PROJECT_NAME} ${SRC_FILES} ui/MainWindow.ui ui/SourceSettingsDialog.ui ui/SourceTile.ui)

target_link_libraries(${PROJECT_NAME}
    Qt6::Widgets
    # NDI SDK
    Processing.NDI.Lib.x64
    # FFmpeg
    avformat avcodec avutil swscale swresample
    # Windows libs
    mfplat mfreadwrite mfuuid strmiids ole32 oleaut32 uuid winmm
)

if (MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
