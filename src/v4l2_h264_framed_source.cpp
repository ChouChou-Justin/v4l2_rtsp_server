#include "v4l2_h264_framed_source.h"
#include "logger.h"

v4l2H264FramedSource* v4l2H264FramedSource::createNew(UsageEnvironment& env, v4l2Capture* capture) {
    return new v4l2H264FramedSource(env, capture);
}

v4l2H264FramedSource::v4l2H264FramedSource(UsageEnvironment& env, v4l2Capture* capture)
    : FramedSource(env), fCapture(capture), 
      rtpTimestamp(0), lastRtpTimestamp(0),
      frameFragmentCount(0), isFirstFrame(true) {

    // Initialize timestamp to stay within first quarter of 32-bit space
    rtpTimestamp = 90000;
    lastRtpTimestamp = rtpTimestamp;
    logMessage("Initial RTP timestamp: " + std::to_string(rtpTimestamp));
}

v4l2H264FramedSource::~v4l2H264FramedSource() {
    logMessage("Successfully destroyed v4l2H264FramedSource.");
}

void v4l2H264FramedSource::doGetNextFrame() {
    // Use static time base for consistent timing
    static struct timeval lastFrameTime = {0, 0};

    size_t length;
    unsigned char* frame = fCapture->getFrameWithoutStartCode(length);

    if (frame == nullptr || !fCapture->isFrameValid()) {
        logMessage("WARNING: Invalid frame received");
        handleClosure();
        return;
    }

    // Handle frame data
    if (length > fMaxSize) {
        frameFragmentCount++;
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = length - fMaxSize;
    } else {
        fFrameSize = length;
        fNumTruncatedBytes = 0;
        
        if (frameFragmentCount == 0) {
            // Update last timestamp only for complete frames
            gettimeofday(&lastFrameTime, NULL);
        }
    }

    memcpy(fTo, frame, fFrameSize);
    
    // Use consistent time base
    fPresentationTime = lastFrameTime;
    fDurationInMicroseconds = 33333;  // ~30fps

    // Debug logging
    if (fFrameCount % 30 == 0) {
        double seconds = fPresentationTime.tv_sec + (fPresentationTime.tv_usec / 1000000.0);
        logMessage("Current playback position: " + std::to_string(seconds) + " seconds");
    }

    fFrameCount++;
    fCapture->releaseFrame();
    FramedSource::afterGetting(this);
}

void v4l2H264FramedSource::logTimestampInfo(const char* event, const struct timeval& v4l2Time) {
    std::string info = 
        "Frame " + std::to_string(fFrameCount) +
        " [" + std::string(event) + "] " +
        "Fragment: " + std::to_string(frameFragmentCount) +
        " V4L2: " + std::to_string(v4l2Time.tv_sec) + "." + 
        std::to_string(v4l2Time.tv_usec).substr(0,6) +
        " RTP: " + std::to_string(rtpTimestamp);
    
    // Check for timestamp irregularities
    if (!isFirstFrame) {
        int32_t diff = rtpTimestamp - lastRtpTimestamp;
        if (diff != 0 && diff != 3000) {
            info += " WARNING: Irregular RTP increment: " + std::to_string(diff);
        }
    }
    
    logMessage(info);
}