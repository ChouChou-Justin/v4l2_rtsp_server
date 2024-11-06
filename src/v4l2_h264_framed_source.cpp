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
                    // Get initial time once
                    gettimeofday(&fInitialTime, NULL);
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
    
    if (gopState == SENDING_SPS) {
        // Send SPS
        if (storedSps && storedSpsSize <= fMaxSize) {
            memcpy(fTo, storedSps, storedSpsSize);
            fFrameSize = storedSpsSize;
            gopState = SENDING_PPS;

            // Calculate presentation time from initial time
            unsigned long long elapsedMicros = (fCurTimestamp / 90) * 1000;
            fPresentationTime = fInitialTime;
            fPresentationTime.tv_sec += elapsedMicros / 1000000;
            fPresentationTime.tv_usec += elapsedMicros % 1000000;
            if (fPresentationTime.tv_usec >= 1000000) {
                fPresentationTime.tv_sec += fPresentationTime.tv_usec / 1000000;
                fPresentationTime.tv_usec %= 1000000;
            }
            fDurationInMicroseconds = 0;
            
            // Don't increment timestamp for SPS
            
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

            // Use same presentation time and timestamp as SPS
            fPresentationTime = fInitialTime;
            unsigned long long elapsedMicros = (fCurTimestamp / 90) * 1000;
            fPresentationTime.tv_sec += elapsedMicros / 1000000;
            fPresentationTime.tv_usec += elapsedMicros % 1000000;
            if (fPresentationTime.tv_usec >= 1000000) {
                fPresentationTime.tv_sec += fPresentationTime.tv_usec / 1000000;
                fPresentationTime.tv_usec %= 1000000;
            }
            fDurationInMicroseconds = 0;
            
            // Don't increment timestamp for PPS
            
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

            // Use same presentation time as SPS/PPS
            fPresentationTime = fInitialTime;
            unsigned long long elapsedMicros = (fCurTimestamp / 90) * 1000;
            fPresentationTime.tv_sec += elapsedMicros / 1000000;
            fPresentationTime.tv_usec += elapsedMicros % 1000000;
            if (fPresentationTime.tv_usec >= 1000000) {
                fPresentationTime.tv_sec += fPresentationTime.tv_usec / 1000000;
                fPresentationTime.tv_usec %= 1000000;
            }
            fDurationInMicroseconds = 33333;

            // Start incrementing timestamp from here
            fCurTimestamp += TIMESTAMP_INCREMENT;
            
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
        
        gopState = SENDING_SPS;
        // Don't increment timestamp here, keep current
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

    // Calculate presentation time mathematically
    unsigned long long elapsedMicros = (fCurTimestamp / 90) * 1000;  // Convert from 90kHz to microseconds
    fPresentationTime = fInitialTime;
    fPresentationTime.tv_sec += elapsedMicros / 1000000;
    fPresentationTime.tv_usec += elapsedMicros % 1000000;
    if (fPresentationTime.tv_usec >= 1000000) {
        fPresentationTime.tv_sec += fPresentationTime.tv_usec / 1000000;
        fPresentationTime.tv_usec %= 1000000;
    }
    
    fDurationInMicroseconds = 33333;  // 30fps, 33.33ms
    fCurTimestamp += TIMESTAMP_INCREMENT;  // Simple increment by 3000

    fCapture->releaseFrame();
    FramedSource::afterGetting(this);
}
