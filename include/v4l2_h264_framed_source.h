#ifndef V4L2_H264_FRAMED_SOURCE_H
#define V4L2_H264_FRAMED_SOURCE_H

#include <FramedSource.hh>
#include "v4l2_capture.h"

class v4l2H264FramedSource : public FramedSource {
public:
    static v4l2H264FramedSource* createNew(UsageEnvironment& env, v4l2Capture* capture);
    void setNeedSpsPps() { needSpsPps = true; }
    
protected:
    v4l2H264FramedSource(UsageEnvironment& env, v4l2Capture* capture);
    virtual ~v4l2H264FramedSource();

private:
    virtual void doGetNextFrame();
    v4l2Capture* fCapture;
    uint32_t fCurTimestamp{0};  // Current RTP timestamp
    static const uint32_t TIMESTAMP_INCREMENT = 3000;  // 90kHz/30fps
    struct timeval fInitialTime;  // Base time for all calculations

    enum GopState {
        WAITING_FOR_GOP,  // Initial state
        SENDING_SPS,
        SENDING_PPS,
        SENDING_IDR,
        SENDING_FRAMES
    };
    GopState gopState{WAITING_FOR_GOP};
    uint32_t currentGopTimestamp{0};  // Timestamp for current GOP
    
    // Buffer for first IDR
    unsigned char* firstIDRFrame{nullptr};
    size_t firstIDRSize{0};
    bool foundFirstGOP{false};

    bool needSpsPps{true};  // Flag to indicate if SPS/PPS needed
    uint8_t* storedSps{nullptr};
    uint8_t* storedPps{nullptr};
    unsigned storedSpsSize{0};
    unsigned storedPpsSize{0};

};

#endif // V4L2_H264_FRAMED_SOURCE_H
