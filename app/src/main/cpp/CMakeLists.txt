cmake_minimum_required(VERSION 3.6.0)

project("faac")

add_library(
        faac

        SHARED

        src/DecodeFaac.cpp
        src/PacketQueue.cpp
        src/AudioDecoder.cpp)

set(FFMPEG_INCLUDE_DIR  "${CMAKE_SOURCE_DIR}/include/")

include_directories(
    ${FFMPEG_INCLUDE_DIR}
)

message(WANNING "FFMPEG_INCLUDE_DIR = ${FFMPEG_INCLUDE_DIR}")

set(lib_src_DIR ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})

add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION ${lib_src_DIR}/libavcodec.so)

add_library(avdevice SHARED IMPORTED)
set_target_properties(avdevice PROPERTIES IMPORTED_LOCATION ${lib_src_DIR}/libavdevice.so)

add_library(avfilter SHARED IMPORTED)
set_target_properties(avfilter PROPERTIES IMPORTED_LOCATION ${lib_src_DIR}/libavfilter.so)

add_library(avformat SHARED IMPORTED)
set_target_properties(avformat PROPERTIES IMPORTED_LOCATION ${lib_src_DIR}/libavformat.so)

add_library(avutil SHARED IMPORTED)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION ${lib_src_DIR}/libavutil.so)

add_library(swresample SHARED IMPORTED)
set_target_properties(swresample PROPERTIES IMPORTED_LOCATION ${lib_src_DIR}/libswresample.so)

add_library(swscale SHARED IMPORTED)
set_target_properties(swscale PROPERTIES IMPORTED_LOCATION ${lib_src_DIR}/libswscale.so)

add_library(fdk-aac SHARED IMPORTED)
set_target_properties(fdk-aac PROPERTIES IMPORTED_LOCATION ${lib_src_DIR}/libfdk-aac.so)

find_library(log-lib log )

target_link_libraries(faac

        swscale
        swresample
        avutil
        avformat
        avfilter
        avdevice
        avcodec
        fdk-aac

        ${log-lib} )

add_library(
        fdkdec
        SHARED
        src/fdkaccdec.cpp
)

target_link_libraries(fdkdec
        fdk-aac
        ${log-lib} )