#include "v4l2_h264_framed_source.h"
#include "logger.h"

v4l2H264FramedSource* v4l2H264FramedSource::createNew(UsageEnvironment& env, v4l2Capture* capture) {
    return new v4l2H264FramedSource(env, capture);
}

v4l2H264FramedSource::v4l2H264FramedSource(UsageEnvironment& env, v4l2Capture* capture)
    : FramedSource(env), fCapture(capture), 
      rtpTimestamp(0), lastRtpTimestamp(0),
      frameFragmentCount(0), isFirstFrame(true),
      spsPpsState(NORMAL)  {

    // Initialize timestamp to stay within first quarter of 32-bit space
    rtpTimestamp = 90000;
    lastRtpTimestamp = rtpTimestamp;

    // Store SPS/PPS for reuse
    if (capture->hasSpsPps()) {
        storedSpsSize = capture->getSPSSize();
        storedPpsSize = capture->getPPSSize();
        
        storedSps = new uint8_t[storedSpsSize];
        storedPps = new uint8_t[storedPpsSize];
        
        memcpy(storedSps, capture->getSPS(), storedSpsSize);
        memcpy(storedPps, capture->getPPS(), storedPpsSize);
    }

    logMessage("Initial RTP timestamp: " + std::to_string(rtpTimestamp));
}

v4l2H264FramedSource::~v4l2H264FramedSource() {
    delete[] storedSps;
    delete[] storedPps;
    logMessage("Successfully destroyed v4l2H264FramedSource.");
}

void v4l2H264FramedSource::doGetNextFrame() {
    struct timeval now;
    gettimeofday(&now, NULL);
    unsigned long currentTime = now.tv_sec * 1000000 + now.tv_usec;
    
    static unsigned char* pendingIDR = nullptr;
    static size_t pendingIDRLength = 0;
    
    // First, check if we have a pending frame to analyze
    size_t length = 0;
    unsigned char* frame = nullptr;
    bool isIDR = false;
    
    if (spsPpsState == NORMAL && !pendingIDR) {
        frame = fCapture->getFrameWithoutStartCode(length);
        if (frame != nullptr && length > 0) {
            isIDR = (frame[0] & 0x1F) == 5;  // Check if it's an IDR frame
            
            // If it's IDR and we need SPS/PPS, store it
            if (isIDR && (needSpsPps || (currentTime - lastSpsPpsTime > SPS_PPS_INTERVAL))) {
                pendingIDR = new unsigned char[length];
                pendingIDRLength = length;
                memcpy(pendingIDR, frame, length);
                fCapture->releaseFrame();
                frame = nullptr;
            }
        }
    }
    
    // Handle SPS/PPS/IDR sequence
    bool needSequence = needSpsPps || 
                       (currentTime - lastSpsPpsTime > SPS_PPS_INTERVAL) ||
                       pendingIDR != nullptr;
    
    if (needSequence || spsPpsState != NORMAL) {
        switch (spsPpsState) {
            case NORMAL:
                if (storedSps && storedSpsSize <= fMaxSize) {
                    memcpy(fTo, storedSps, storedSpsSize);
                    fFrameSize = storedSpsSize;
                    spsPpsState = SENDING_PPS;
                    gettimeofday(&fPresentationTime, NULL);
                    fDurationInMicroseconds = 0;  // Send PPS immediately after
                    
                    if (frame) fCapture->releaseFrame();
                    FramedSource::afterGetting(this);
                    return;
                }
                break;
                
            case SENDING_PPS:
                if (storedPps && storedPpsSize <= fMaxSize) {
                    memcpy(fTo, storedPps, storedPpsSize);
                    fFrameSize = storedPpsSize;
                    spsPpsState = NORMAL;
                    needSpsPps = false;
                    lastSpsPpsTime = currentTime;
                    gettimeofday(&fPresentationTime, NULL);
                    fDurationInMicroseconds = 0;  // Send IDR immediately after
                    
                    FramedSource::afterGetting(this);
                    return;
                }
                break;
        }
    }
    
    // Handle pending IDR frame first
    if (pendingIDR) {
        if (pendingIDRLength <= fMaxSize) {
            memcpy(fTo, pendingIDR, pendingIDRLength);
            fFrameSize = pendingIDRLength;
            delete[] pendingIDR;
            pendingIDR = nullptr;
            pendingIDRLength = 0;
            
            gettimeofday(&fPresentationTime, NULL);
            fDurationInMicroseconds = 33333;  // ~30fps
            
            FramedSource::afterGetting(this);
            return;
        }
    }
    
    // Handle regular frame
    if (!frame) {
        frame = fCapture->getFrameWithoutStartCode(length);
    }

    if (frame == nullptr || !fCapture->isFrameValid()) {
        logMessage("WARNING: Invalid frame received");
        handleClosure();
        return;
    }

    if (length > fMaxSize) {
        frameFragmentCount++;
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = length - fMaxSize;
    } else {
        fFrameSize = length;
        fNumTruncatedBytes = 0;
        frameFragmentCount = 0;
    }

    memcpy(fTo, frame, fFrameSize);
    
    gettimeofday(&fPresentationTime, NULL);
    fDurationInMicroseconds = 33333;  // ~30fps

    fFrameCount++;
    fCapture->releaseFrame();
    FramedSource::afterGetting(this);
}


