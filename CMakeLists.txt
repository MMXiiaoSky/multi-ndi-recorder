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
set(FFMPEG_LIB_ROOT "C:/ffmpeg" CACHE PATH "Path to FFmpeg root (containing lib/bin)")

include_directories(${NDI_SDK_INCLUDE} ${FFMPEG_INCLUDE} include)

set(_ffmpeg_search_paths
    ${FFMPEG_LIB_ROOT}
    ${FFMPEG_LIB_ROOT}/lib
    ${FFMPEG_LIB_ROOT}/bin
)

find_library(NDI_LIBRARY NAMES Processing.NDI.Lib.x64 PATHS ${NDI_SDK_LIB} PATH_SUFFIXES lib bin)
find_library(AVFORMAT_LIBRARY NAMES avformat PATHS ${_ffmpeg_search_paths} PATH_SUFFIXES lib bin)
find_library(AVCODEC_LIBRARY NAMES avcodec PATHS ${_ffmpeg_search_paths} PATH_SUFFIXES lib bin)
find_library(AVUTIL_LIBRARY NAMES avutil PATHS ${_ffmpeg_search_paths} PATH_SUFFIXES lib bin)
find_library(SWSCALE_LIBRARY NAMES swscale PATHS ${_ffmpeg_search_paths} PATH_SUFFIXES lib bin)

if (NOT NDI_LIBRARY)
    message(FATAL_ERROR "Could not find Processing.NDI.Lib.x64. Set NDI_SDK_LIB to the NDI SDK library directory (lib or bin).")
endif()
if (NOT AVFORMAT_LIBRARY)
    message(FATAL_ERROR "Could not find avformat library. Set FFMPEG_LIB_ROOT to the FFmpeg installation root (containing lib or bin).")
endif()

file(GLOB SRC_FILES
    src/*.cpp
)

file(GLOB HEADER_FILES
    include/*.h
)

add_executable(${PROJECT_NAME} ${SRC_FILES} ${HEADER_FILES} ui/MainWindow.ui ui/SourceSettingsDialog.ui ui/SourceTile.ui)

target_link_libraries(${PROJECT_NAME}
    Qt6::Widgets
    # NDI SDK
    ${NDI_LIBRARY}
    # FFmpeg
    ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY}
    # Windows libs
    mfplat mfreadwrite mfuuid strmiids ole32 oleaut32 uuid winmm
)

if (MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
