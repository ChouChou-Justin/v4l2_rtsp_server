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
    size_t length;
    unsigned char* frame = fCapture->getFrameWithoutStartCode(length);

    if (frame == nullptr) {
        envir() << "Failed to get frame\n";
        handleClosure();
        return;
    }

    if (length > fMaxSize) {
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = length - fMaxSize;
    } else {
        fFrameSize = length;
        fNumTruncatedBytes = 0;
    }

    memcpy(fTo, frame, fFrameSize);
    fCapture->releaseFrame();

    gettimeofday(&fPresentationTime, NULL);
    fDurationInMicroseconds = 0;

    FramedSource::afterGetting(this);
}
