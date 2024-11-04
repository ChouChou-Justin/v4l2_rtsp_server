#ifndef LIVE555_RTSP_SERVER_MANAGER_H
#define LIVE555_RTSP_SERVER_MANAGER_H

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include "v4l2_capture.h"

class Live555RTSPServerManager {
public:
    Live555RTSPServerManager(UsageEnvironment* env, v4l2Capture* capture, int port = 8554);
    ~Live555RTSPServerManager();

    bool initialize();
    void runEventLoop(char* shouldExit);
    void cleanup();
    void stopEventLoop();

private:
    int port_;
    UsageEnvironment* env_;
    v4l2Capture* capture_;
    RTSPServer* rtspServer_;
    ServerMediaSession* sms_;
    char shouldExit;
};

#endif // LIVE555_RTSP_SERVER_MANAGER_H
