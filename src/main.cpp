#include <csignal>
#include "live555_rtsp_server_manager.h"
#include "logger.h"
#include "constants.h"

char shouldExit = 0;

void signalHandler(int signum) {
    logMessage("Interrupt signal (" + std::to_string(signum) + ") received.");
    shouldExit = 1;
}

int main(int argc, char** argv) {
    // Set up signal handler
    std::signal(SIGINT, signalHandler);

    // Initialize and create LIVE555 environment
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

    // Set up V4L2 capture
    v4l2Capture* capture = new v4l2Capture(DEVICE);
    if (capture->getFd() < 0) {
        *env << "Failed to open v4l2 capture device.\n";
        exit(1);
    }
    if (!capture->initialize()) {
        *env << "Failed to initialize v4l2 capture.\n";
        exit(1);
    }
    
    logMessage("Successfully initialized v4l2 capture.");
    

    // Create and initialize RTSP server manager
    Live555RTSPServerManager rtspManager(env, capture);
    if (!rtspManager.initialize()) {
        *env << "Failed to initialize RTSP server manager.\n";
        exit(1);
    }

    // Start the event loop
    rtspManager.runEventLoop(&shouldExit);

    // Cleanup
    rtspManager.cleanup();
    delete capture;
    delete scheduler;
    env->reclaim();

    logMessage("Successfully shut down application.");
    return 0;
}
