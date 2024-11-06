#ifndef V4L2_H264_MEDIA_SUBSESSION_H
#define V4L2_H264_MEDIA_SUBSESSION_H

#include <liveMedia.hh>
#include "v4l2_capture.h"

class v4l2H264MediaSubsession: public OnDemandServerMediaSubsession {
public:
    static v4l2H264MediaSubsession* createNew(UsageEnvironment& env, v4l2Capture* capture, Boolean reuseFirstSource);

protected:
    v4l2H264MediaSubsession(UsageEnvironment& env, v4l2Capture* capture, Boolean reuseFirstSource);
    virtual ~v4l2H264MediaSubsession();

    virtual FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate);
    virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource);
    virtual void deleteStream(unsigned clientSessionId, void*& streamToken);
    virtual char const* getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource);

private:
    v4l2Capture* fCapture;
    char* fAuxSDPLine;
    unsigned streamingSessionId;  
};

#endif // V4L2_H264_MEDIA_SUBSESSION_H
