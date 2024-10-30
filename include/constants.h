#ifndef CONSTANTS_H
#define CONSTANTS_H

// Device settings
#define DEVICE "/dev/video0"
#define BUFFER_COUNT 4

// Video format settings
#define WIDTH 640
#define HEIGHT 480

// H.264 encoding settings
#define VIDEO_BITRATE 1000000    // 1 Mbps
#define GOP_SIZE 60              // GOP size (2 seconds at 30 fps)
#define H264_PROFILE V4L2_MPEG_VIDEO_H264_PROFILE_HIGH

// Frame rate settings
#define FRAME_RATE_NUMERATOR 1
#define FRAME_RATE_DENOMINATOR 15  // 15 fps

// Camera settings
#define ROTATION_DEGREES 180

// RTSP server settings
#define DEFAULT_RTSP_PORT 8554

#endif // CONSTANTS_H