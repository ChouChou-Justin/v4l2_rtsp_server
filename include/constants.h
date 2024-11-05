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
#define GOP_SIZE 30              // GOP size (1 seconds at 30 fps)

// Frame rate settings
#define FRAME_RATE_NUMERATOR 1
#define FRAME_RATE_DENOMINATOR 30  // 30 fps

// Camera settings
#define ROTATION_DEGREES 180

// RTSP server settings
#define DEFAULT_RTSP_PORT 8554

#endif // CONSTANTS_H