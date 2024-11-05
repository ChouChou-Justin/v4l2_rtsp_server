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
    size_t length;
    unsigned char* frame = fCapture->getFrameWithoutStartCode(length);

    if (frame == nullptr || !fCapture->isFrameValid()) {
        logMessage("WARNING: Invalid frame received");
        handleClosure();
        return;
    }

    // Get V4L2 timestamp
    const struct timeval& v4l2Time = fCapture->getTimestamp();
    fPresentationTime = v4l2Time;
    
    // Standard frame duration for 30fps
    fDurationInMicroseconds = 33333;

    // Handle frame data
    if (length > fMaxSize) {
        frameFragmentCount++;
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = length - fMaxSize;
        logMessage("Frame fragmented: " + std::to_string(frameFragmentCount) + 
                  " remaining bytes: " + std::to_string(fNumTruncatedBytes));
    } else {
        fFrameSize = length;
        fNumTruncatedBytes = 0;
        
        // Only increment RTP timestamp for new frames, not fragments
        if (frameFragmentCount == 0) {
            lastRtpTimestamp = rtpTimestamp;
            
            // Calculate next timestamp
            uint32_t nextTimestamp = rtpTimestamp + 3000;
            rtpTimestamp = nextTimestamp;
            
            // Log timing every 30 frames
            if (fFrameCount % 30 == 0) {
                double seconds = rtpTimestamp / 90000.0;
                logMessage("Current playback position: " + std::to_string(seconds) + " seconds");
            }
        }
        frameFragmentCount = 0;
    }

    memcpy(fTo, frame, fFrameSize);
    logTimestampInfo("Processing", v4l2Time);
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