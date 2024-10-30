#include "v4l2_capture.h"
#include "logger.h"
#include <iostream>

v4l2Capture::v4l2Capture(const char* device) 
    : fd(-1)
    , buffers(nullptr)
    , n_buffers(0)
    , sps(nullptr)
    , pps(nullptr)
    , spsSize(0)
    , ppsSize(0)
    , spsPpsExtracted(false) {
    fd = open(device, O_RDWR);
    if (fd == -1) {
        logMessage("Cannot open device " + std::string(device) + ": " + std::string(strerror(errno)));
    }
}

v4l2Capture::~v4l2Capture() {
    if (buffers != nullptr) {
        stopCapture();
        for (unsigned int i = 0; i < n_buffers; ++i) {
            if (buffers[i].start != MAP_FAILED && buffers[i].start != nullptr) {
                if (munmap(buffers[i].start, buffers[i].length) == -1) {
                    logMessage("munmap error: " + std::string(strerror(errno)));
                }
            }
        }
        delete[] buffers;
    }
    delete[] sps;
    delete[] pps;
    if (fd >= 0) close(fd);
}

bool v4l2Capture::initialize() {
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        logMessage("VIDIOC_QUERYCAP error: " + std::string(strerror(errno)));
        return false;
    }

    // Set the format
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        logMessage("VIDIOC_S_FMT error: " + std::string(strerror(errno)));
        return false;
    }

    // Set H.264 related controls
    struct v4l2_control control;
    
    // Set bitrate to 2 Mbps
    control.id = V4L2_CID_MPEG_VIDEO_BITRATE;
    control.value = 1000000;
    if (ioctl(fd, VIDIOC_S_CTRL, &control) == -1) {
        logMessage("Failed to set bitrate: " + std::string(strerror(errno)));
    }

    // Set GOP size to 60 (2 seconds at 30 fps)
    control.id = V4L2_CID_MPEG_VIDEO_H264_I_PERIOD;
    control.value = 60;
    if (ioctl(fd, VIDIOC_S_CTRL, &control) == -1) {
        logMessage("Failed to set GOP size: " + std::string(strerror(errno)));
    }

    // Set H.264 profile to High
    control.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
    control.value = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
    if (ioctl(fd, VIDIOC_S_CTRL, &control) == -1) {
        logMessage("Failed to set H.264 profile: " + std::string(strerror(errno)));
    }

    // Set the frame rate
    struct v4l2_streamparm streamparm = {0};
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = 15;
    if (ioctl(fd, VIDIOC_S_PARM, &streamparm) == -1) {
        logMessage("Failed to set frame rate: " + std::string(strerror(errno)));
    }

    // Set rotation (if needed)
    control.id = V4L2_CID_ROTATE;
    control.value = 180;
    if (ioctl(fd, VIDIOC_S_CTRL, &control) == -1) {
        logMessage("Failed to set rotation: " + std::string(strerror(errno)));
    }

    return initializeMmap();
}

bool v4l2Capture::initializeMmap() {
    struct v4l2_requestbuffers req = {0};
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        logMessage("VIDIOC_REQBUFS error: " + std::string(strerror(errno)));
        return false;
    }

    buffers = new Buffer[req.count];
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            logMessage("VIDIOC_QUERYBUF error: " + std::string(strerror(errno)));
            return false;
        }

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = mmap(NULL, buf.length, 
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

        if (buffers[n_buffers].start == MAP_FAILED) {
            logMessage("mmap error: " + std::string(strerror(errno)));
            return false;
        }
    }
    return true;
}

bool v4l2Capture::startCapture() {    
    for (unsigned int i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            logMessage("VIDIOC_QBUF error: " + std::string(strerror(errno)));
            return false;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        logMessage("VIDIOC_STREAMON error: " + std::string(strerror(errno)));
        return false;
    }

    logMessage("Successfully start capture.");
    return true;
}

bool v4l2Capture::stopCapture() {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        logMessage("VIDIOC_STREAMOFF error: " + std::string(strerror(errno)));
        return false;
    }
    logMessage("Successfully stop capture.");
    return true;
}

bool v4l2Capture::reset() {
    logMessage("Attempting to reset capture device.");
    
    if (!stopCapture()) {
        logMessage("Failed to stop capture during reset.");
        return false;
    }
    
    // Add a small delay to allow the device to settle
    usleep(100000); // 100ms delay
    
    // Instead of re-initializing, just try to start capture again
    if (!startCapture()) {
        logMessage("Failed to start capture during reset.");
        return false;
    }
    
    logMessage("Successfully reset capture.");
    return true;
}

void v4l2Capture::updateFrameInfo(const v4l2_buffer& buf) {
    currentFrameInfo.timestamp = buf.timestamp;
    currentFrameInfo.sequence = buf.sequence;
    currentFrameInfo.size = buf.bytesused;
    currentFrameInfo.valid = true;
}

double v4l2Capture::getFrameDelta(const timeval& previous) const {
    double current_sec = currentFrameInfo.timestamp.tv_sec + 
                        (currentFrameInfo.timestamp.tv_usec / 1000000.0);
    double prev_sec = previous.tv_sec + (previous.tv_usec / 1000000.0);
    return current_sec - prev_sec;
}

unsigned char* v4l2Capture::getFrame(size_t& length) {
    memset(&current_buf, 0, sizeof(current_buf));
    current_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    current_buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_DQBUF, &current_buf) == -1) {
        logMessage("VIDIOC_DQBUF error: " + std::string(strerror(errno)));
        currentFrameInfo.valid = false;
        return nullptr;
    }

    // Enhanced debugging
    if (current_buf.sequence % 60 == 0) {  // Log every 60 frames (2 seconds at 30fps)
        std::string msg = "Buffer info - index: " + std::to_string(current_buf.index) +
                         ", bytesused: " + std::to_string(current_buf.bytesused) +
                         ", flags: " + std::to_string(current_buf.flags);
        logMessage(msg);
    }

    // Update frame info with timing data
    updateFrameInfo(current_buf);
    
    // Log frame information for debugging
    if (current_buf.sequence % 300 == 0) {  // Log every 300 frames to avoid spam
        std::string msg = "Frame sequence: " + std::to_string(current_buf.sequence) + 
                         ", timestamp: " + std::to_string(current_buf.timestamp.tv_sec) + "." +
                         std::to_string(current_buf.timestamp.tv_usec);
        logMessage(msg);
    }

    length = current_buf.bytesused;
    return static_cast<unsigned char*>(buffers[current_buf.index].start);
}

unsigned char* v4l2Capture::getFrameWithoutStartCode(size_t& length) {
    unsigned char* frame = getFrame(length);
    if (frame == nullptr) return nullptr;

    if (length > 3 && frame[0] == 0x00 && frame[1] == 0x00 && 
        ((frame[2] == 0x00 && frame[3] == 0x01) || frame[2] == 0x01)) {
        size_t startCodeSize = (frame[2] == 0x00) ? 4 : 3;
        memmove(frame, frame + startCodeSize, length - startCodeSize);
        length -= startCodeSize;
    }

    return frame;
}

void v4l2Capture::releaseFrame() {
    if (ioctl(fd, VIDIOC_QBUF, &current_buf) == -1) {
        logMessage("VIDIOC_QBUF error: " + std::string(strerror(errno)));
        logMessage("Failed to queue buffer index: " + std::to_string(current_buf.index));
        return;
    }
}

bool v4l2Capture::extractSpsPps() {
    if (spsPpsExtracted) {
        return true;
    }

    const int MAX_ATTEMPTS = 30;
    
    for (int i = 0; i < MAX_ATTEMPTS; ++i) {
        size_t frameSize;
        unsigned char* frame = getFrame(frameSize);
        if (frame == nullptr) {
            std::cout << "Got null frame on attempt " << i << std::endl;
            continue;
        }

        size_t offset = 0;
        while (offset + 4 < frameSize) {
            if (frame[offset] == 0 && frame[offset+1] == 0 && 
                frame[offset+2] == 0 && frame[offset+3] == 1) {
                offset += 4;
                if (offset >= frameSize) break;

                uint8_t nalType = frame[offset] & 0x1F;

                size_t nextNalOffset = offset;
                while (nextNalOffset + 4 < frameSize) {
                    if (frame[nextNalOffset] == 0 && frame[nextNalOffset+1] == 0 && 
                        frame[nextNalOffset+2] == 0 && frame[nextNalOffset+3] == 1) {
                        break;
                    }
                    nextNalOffset++;
                }

                size_t nalUnitSize = nextNalOffset - offset;

                if (nalType == 7 && sps == nullptr) {
                    spsSize = nalUnitSize;
                    sps = new uint8_t[spsSize];
                    memcpy(sps, frame + offset, spsSize);
                } else if (nalType == 8 && pps == nullptr) {
                    ppsSize = nalUnitSize;
                    pps = new uint8_t[ppsSize];
                    memcpy(pps, frame + offset, ppsSize);
                }

                offset = nextNalOffset;
            } else {
                offset++;
            }
        }

        releaseFrame();

        if (sps != nullptr && pps != nullptr) {
            spsPpsExtracted = true;
            logMessage("Successfully extract SPS and PPS.");
            return true;
        }
    }

    std::cerr << "Failed to extract SPS and PPS within " << MAX_ATTEMPTS << " attempts." << std::endl;
    return false;
}
