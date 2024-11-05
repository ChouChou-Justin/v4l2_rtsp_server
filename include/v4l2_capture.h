#ifndef V4L2_CAPTURE_H // Preventing multiple inclusions of header files
#define V4L2_CAPTURE_H

#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h> // for close()
#include <cstdint>  // for uint8_t
#include <chrono>
#include "constants.h"

struct Buffer {
    void *start;
    size_t length;
};

struct FrameInfo {
    struct timeval timestamp;
    uint32_t sequence;
    size_t size;
    bool valid;
};

class v4l2Capture {
public:
    v4l2Capture(const char* device);
    ~v4l2Capture();

    bool initialize();
    bool startCapture();
    bool stopCapture();
    bool reset();
    unsigned char* getFrame(size_t& length);
    unsigned char* getFrameWithoutStartCode(size_t& length);
    void releaseFrame();

    bool extractSpsPps();
    void clearSpsPps();
    bool extractSpsPpsImmediate();
    bool hasSpsPps() const { return spsPpsExtracted; }
    uint8_t* getSPS() const { return sps; }
    uint8_t* getPPS() const { return pps; }
    unsigned getSPSSize() const { return spsSize; }
    unsigned getPPSSize() const { return ppsSize; }
    

    int getFd() const { return fd; }
    // Timing information
    const FrameInfo& getCurrentFrameInfo() const { return currentFrameInfo; }
    bool isFrameValid() const { return currentFrameInfo.valid; }
    uint32_t getSequence() const { return currentFrameInfo.sequence; }
    const timeval& getTimestamp() const { return currentFrameInfo.timestamp; }

private:
    int fd;
    Buffer* buffers;
    unsigned int n_buffers;
    struct v4l2_buffer current_buf;
    bool initializeMmap();

    uint8_t* sps;
    uint8_t* pps;
    unsigned spsSize;
    unsigned ppsSize;
    bool spsPpsExtracted;

    FrameInfo currentFrameInfo;
    void updateFrameInfo(const v4l2_buffer& buf);
};

#endif // V4L2_CAPTURE_H
