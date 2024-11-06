#include "v4l2_h264_framed_source.h"
#include "logger.h"

v4l2H264FramedSource* v4l2H264FramedSource::createNew(UsageEnvironment& env, v4l2Capture* capture) {
    return new v4l2H264FramedSource(env, capture);
}

v4l2H264FramedSource::v4l2H264FramedSource(UsageEnvironment& env, v4l2Capture* capture)
    : FramedSource(env), fCapture(capture), 
      gopState(WAITING_FOR_GOP), fCurTimestamp(90000)  {

    // Store SPS/PPS for reuse
    if (capture->hasSpsPps()) {
        storedSpsSize = capture->getSPSSize();
        storedPpsSize = capture->getPPSSize();
        
        storedSps = new uint8_t[storedSpsSize];
        storedPps = new uint8_t[storedPpsSize];
        
        memcpy(storedSps, capture->getSPS(), storedSpsSize);
        memcpy(storedPps, capture->getPPS(), storedPpsSize);
    }
}

v4l2H264FramedSource::~v4l2H264FramedSource() {
    delete[] storedSps;
    delete[] storedPps;
    logMessage("Successfully destroyed v4l2H264FramedSource.");
}

void v4l2H264FramedSource::doGetNextFrame() {
    if (!foundFirstGOP) {
        // Wait for first complete GOP
        if (gopState == WAITING_FOR_GOP) {
            // Try to get an IDR frame
            size_t length;
            unsigned char* frame = fCapture->getFrameWithoutStartCode(length);
            
            if (frame && length > 0 && (frame[0] & 0x1F) == 5) {
                // Found IDR, store it
                firstIDRFrame = new unsigned char[length];
                firstIDRSize = length;
                memcpy(firstIDRFrame, frame, length);
                fCapture->releaseFrame();
                
                if (storedSps && storedPps) {
                    // We have all components
                    foundFirstGOP = true;
                    currentGopTimestamp = fCurTimestamp;
                    gopState = SENDING_SPS;
                    doGetNextFrame();  // Recursive call to start sending
                }
                return;
            }
            
            if (frame) fCapture->releaseFrame();
            // Keep trying until we get a complete GOP
            envir().taskScheduler().scheduleDelayedTask(0,
                (TaskFunc*)FramedSource::afterGetting, this);
            return;
        }
    }
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    if (gopState == SENDING_SPS) {
        // Send SPS
        if (storedSps && storedSpsSize <= fMaxSize) {
            memcpy(fTo, storedSps, storedSpsSize);
            fFrameSize = storedSpsSize;
            gopState = SENDING_PPS;
            fPresentationTime = now;
            fDurationInMicroseconds = 0;
            // Use same timestamp for entire GOP
            fCurTimestamp = currentGopTimestamp;
            
            FramedSource::afterGetting(this);
            return;
        }
    }
    
    if (gopState == SENDING_PPS) {
        // Send PPS
        if (storedPps && storedPpsSize <= fMaxSize) {
            memcpy(fTo, storedPps, storedPpsSize);
            fFrameSize = storedPpsSize;
            gopState = SENDING_IDR;
            fPresentationTime = now;
            fDurationInMicroseconds = 0;
            // Use same timestamp for entire GOP
            fCurTimestamp = currentGopTimestamp;
            
            FramedSource::afterGetting(this);
            return;
        }
    }

        static unsigned char* pendingIDR = nullptr;
        static size_t pendingIDRLength = 0;
    
    if (gopState == SENDING_IDR) {
        // Send stored IDR for first GOP or waiting IDR
        unsigned char* idrToSend = firstIDRFrame ? firstIDRFrame : pendingIDR;
        size_t idrSize = firstIDRFrame ? firstIDRSize : pendingIDRLength;
        
        if (idrToSend && idrSize <= fMaxSize) {
            memcpy(fTo, idrToSend, idrSize);
            fFrameSize = idrSize;
            fPresentationTime = now;
            fDurationInMicroseconds = 33333;
            // Use same timestamp for entire GOP
            fCurTimestamp = currentGopTimestamp;
            
            if (firstIDRFrame) {
                delete[] firstIDRFrame;
                firstIDRFrame = nullptr;
                firstIDRSize = 0;
            }
            if (pendingIDR) {
                delete[] pendingIDR;
                pendingIDR = nullptr;
                pendingIDRLength = 0;
            }
            
            gopState = SENDING_FRAMES;
            FramedSource::afterGetting(this);
            return;
        }
    }
    
    // Handle regular frames
    size_t length;
    unsigned char* frame = fCapture->getFrameWithoutStartCode(length);
    
    if (frame == nullptr || !fCapture->isFrameValid()) {
        handleClosure();
        return;
    }
    
    // Check for new GOP
    if (length > 0 && (frame[0] & 0x1F) == 5) {
        // Store IDR and prepare new GOP
        pendingIDR = new unsigned char[length];
        pendingIDRLength = length;
        memcpy(pendingIDR, frame, length);
        fCapture->releaseFrame();
        
        currentGopTimestamp = fCurTimestamp + TIMESTAMP_INCREMENT;
        gopState = SENDING_SPS;
        doGetNextFrame();
        return;
    }
    
    // Send regular frame
    if (length <= fMaxSize) {
        memcpy(fTo, frame, length);
        fFrameSize = length;
        fNumTruncatedBytes = 0;
    } else {
        memcpy(fTo, frame, fMaxSize);
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = length - fMaxSize;
    }
    
    fPresentationTime = now;
    fDurationInMicroseconds = 33333;
    fCurTimestamp += TIMESTAMP_INCREMENT;
    
    fCapture->releaseFrame();
    FramedSource::afterGetting(this);
}
