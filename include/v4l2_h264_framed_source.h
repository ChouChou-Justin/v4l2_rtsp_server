#ifndef V4L2_H264_FRAMED_SOURCE_H
#define V4L2_H264_FRAMED_SOURCE_H

#include <FramedSource.hh>
#include "v4l2_capture.h"

class v4l2H264FramedSource : public FramedSource {
public:
    static v4l2H264FramedSource* createNew(UsageEnvironment& env, v4l2Capture* capture);

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
    // Debug helper
    void logTimestampInfo(const char* event, const struct timeval& v4l2Time);
};

#endif // V4L2_H264_FRAMED_SOURCE_H
