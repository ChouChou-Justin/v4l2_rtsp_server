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
    unsigned long fFrameCount{0};  // Initialize to 0
    uint32_t rtpTimestamp;      // Current RTP timestamp
    uint32_t lastRtpTimestamp;  // For monitoring jumps
    unsigned frameFragmentCount; // Track fragments per frame
    bool isFirstFrame{true};  // New flag for first frame handling
    
    // SPS/PPS handling
    enum SpsPpsState {
        NORMAL,
        SENDING_SPS,
        SENDING_PPS
    };
    SpsPpsState spsPpsState{NORMAL};
    unsigned long lastSpsPpsTime{0};  // Track last SPS/PPS insertion time
    static const unsigned long SPS_PPS_INTERVAL = 1000000; // 1 second in microseconds
    bool needSpsPps{true};  // Flag to indicate if SPS/PPS needed
    uint8_t* storedSps{nullptr};
    uint8_t* storedPps{nullptr};
    unsigned storedSpsSize{0};
    unsigned storedPpsSize{0};
    
    void logTimestampInfo(const char* event, const struct timeval& v4l2Time); // Debug helper
};

#endif // V4L2_H264_FRAMED_SOURCE_H
