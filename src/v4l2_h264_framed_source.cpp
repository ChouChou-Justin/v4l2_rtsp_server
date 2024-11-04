#include "v4l2_h264_framed_source.h"
#include "logger.h"

v4l2H264FramedSource* v4l2H264FramedSource::createNew(UsageEnvironment& env, v4l2Capture* capture) {
    return new v4l2H264FramedSource(env, capture);
}

v4l2H264FramedSource::v4l2H264FramedSource(UsageEnvironment& env, v4l2Capture* capture)
    : FramedSource(env), fCapture(capture) {
    logMessage("Successfully created v4l2H264FramedSource.");
}

v4l2H264FramedSource::~v4l2H264FramedSource() {
    logMessage("Successfully destroyed v4l2H264FramedSource.");
}

void v4l2H264FramedSource::doGetNextFrame() {
    static struct timeval lastFrameTime = {0, 0};

    size_t length;
    unsigned char* frame = fCapture->getFrameWithoutStartCode(length);

    if (frame == nullptr || !fCapture->isFrameValid()) {
        envir() << "Failed to get frame or invalid frame\n";
        handleClosure();
        return;
    }

    if (length > fMaxSize) {
        // Only log significant truncations
        if (length > fMaxSize * 2) {
            logMessage("Significant frame truncation: " + std::to_string(length - fMaxSize) + " bytes");
        }
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = length - fMaxSize;
    } else {
        fFrameSize = length;
        fNumTruncatedBytes = 0;
    }

    memcpy(fTo, frame, fFrameSize);
    gettimeofday(&lastFrameTime, NULL);

    // Set presentation time
    fPresentationTime = lastFrameTime;
    fDurationInMicroseconds = 33333; // ~30fps

    fFrameCount++;
    if (fFrameCount % 30 == 0) {
        logMessage("Processed " + std::to_string(fFrameCount) + " frames");
    }

    fCapture->releaseFrame();
    FramedSource::afterGetting(this);
}
