#include "live555_rtsp_server_manager.h"
#include "v4l2_h264_media_subsession.h"
#include "logger.h"

Live555RTSPServerManager::Live555RTSPServerManager(UsageEnvironment* env, v4l2Capture* capture, int port)
    : env_(env), capture_(capture), port_(port), rtspServer_(nullptr), sms_(nullptr) {
}

Live555RTSPServerManager::~Live555RTSPServerManager() {
}

bool Live555RTSPServerManager::initialize() {
    rtspServer_ = RTSPServer::createNew(*env_, port_, NULL);
    if (rtspServer_ == NULL) {
        *env_ << "Failed to create RTSP server: " << env_->getResultMsg() << "\n";
        return false;
    }
    logMessage("Successfully created RTSP server.");

    sms_ = ServerMediaSession::createNew(*env_, "v4l2Stream", "v4l2Stream", 
        "Session streamed by \"v4l2StreamServer\"", True);
    sms_->addSubsession(v4l2H264MediaSubsession::createNew(*env_, capture_, False));
    rtspServer_->addServerMediaSession(sms_);

    char* url = rtspServer_->rtspURL(sms_);
    logMessage("Stream URL: " + std::string(url));
    delete[] url;

    return true;
}

void Live555RTSPServerManager::runEventLoop(char* shouldExit) {
    logMessage("Starting event loop. Press Ctrl+C to exit.");
    env_->taskScheduler().doEventLoop(shouldExit);
}

void Live555RTSPServerManager::cleanup() {
    Medium::close(rtspServer_);
    logMessage("Successfully cleaned up RTSP server.");
}
