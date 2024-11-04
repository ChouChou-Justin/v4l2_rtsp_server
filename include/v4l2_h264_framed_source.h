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
    unsigned long fFrameCount; 
};

#endif // V4L2_H264_FRAMED_SOURCE_H
