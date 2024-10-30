cmake_minimum_required(VERSION 3.10)
project(v4l2_rtsp_server)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the path to Live555
set(LIVE555_DIR "/home/pi/Desktop/live")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIVE555_DIR}/UsageEnvironment/include
    ${LIVE555_DIR}/groupsock/include
    ${LIVE555_DIR}/liveMedia/include
    ${LIVE555_DIR}/BasicUsageEnvironment/include
)

# Find Live555 libraries
find_library(USAGE_ENVIRONMENT_LIB UsageEnvironment PATHS ${LIVE555_DIR}/UsageEnvironment)
find_library(BASIC_USAGE_ENVIRONMENT_LIB BasicUsageEnvironment PATHS ${LIVE555_DIR}/BasicUsageEnvironment)
find_library(GROUPSOCK_LIB groupsock PATHS ${LIVE555_DIR}/groupsock)
find_library(LIVEMEDIA_LIB liveMedia PATHS ${LIVE555_DIR}/liveMedia)

# Find OpenSSL package
find_package(OpenSSL REQUIRED)

# Find Threads package
find_package(Threads REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/logger.cpp
    src/v4l2_capture.cpp
    src/v4l2_h264_framed_source.cpp
    src/v4l2_h264_media_subsession.cpp
    src/live555_rtsp_server_manager.cpp
)

# Create executable
add_executable(v4l2_rtsp_server ${SOURCES})

# Link libraries
target_link_libraries(v4l2_rtsp_server
    ${LIVEMEDIA_LIB}
    ${GROUPSOCK_LIB}
    ${BASIC_USAGE_ENVIRONMENT_LIB}
    ${USAGE_ENVIRONMENT_LIB}
    ${CMAKE_THREAD_LIBS_INIT}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Install
install(TARGETS v4l2_rtsp_server DESTINATION bin)
