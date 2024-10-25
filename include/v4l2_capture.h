#ifndef V4L2_CAPTURE_H
#define V4L2_CAPTURE_H

#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h> // for close()
#include <cstdint> // for uint8_t
#include "constants.h"

struct Buffer {
    void *start;
    size_t length;
};

class v4l2Capture {
public:
    v4l2Capture(const char* device);
    ~v4l2Capture();

    bool initialize();
    bool startCapture();
    bool stopCapture();
    unsigned char* getFrame(size_t& length);
    unsigned char* getFrameWithoutStartCode(size_t& length);
    void releaseFrame();

    bool extractSpsPps();
    uint8_t* getSPS() const { return sps; }
    uint8_t* getPPS() const { return pps; }
    unsigned getSPSSize() const { return spsSize; }
    unsigned getPPSSize() const { return ppsSize; }
    bool hasSpsPps() const { return spsPpsExtracted; }

    bool reset();
    int getFd() const { return fd; }
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
};

#endif // V4L2_CAPTURE_H
