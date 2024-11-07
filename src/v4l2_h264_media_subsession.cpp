#include "v4l2_h264_media_subsession.h"
#include "v4l2_h264_framed_source.h"
#include "logger.h"
#include <Base64.hh>

v4l2H264MediaSubsession* v4l2H264MediaSubsession::createNew(UsageEnvironment& env, v4l2Capture* capture, Boolean reuseFirstSource) {
    return new v4l2H264MediaSubsession(env, capture, reuseFirstSource);
}

v4l2H264MediaSubsession::v4l2H264MediaSubsession(UsageEnvironment& env, v4l2Capture* capture, Boolean reuseFirstSource)
    : OnDemandServerMediaSubsession(env, reuseFirstSource), 
      fCapture(capture), fAuxSDPLine(NULL) {
}

v4l2H264MediaSubsession::~v4l2H264MediaSubsession() {
    delete[] fAuxSDPLine;
}

FramedSource* v4l2H264MediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate) {
    estBitrate = 1000;
    
    // For initial setup phase
    if (clientSessionId == 0) {
        logMessage("Initial setup phase with session 0");
        // We need basic setup for session 0
        if (!fCapture->hasSpsPps()) {
            fCapture->stopCapture();
            fCapture->reset();
            fCapture->startCapture();
            if (!fCapture->extractSpsPps()) {
                logMessage("Failed to extract SPS/PPS for setup phase");
                fCapture->stopCapture();
                return nullptr;
            }
        }
    } else if (streamingSessionId != 0 && clientSessionId != streamingSessionId) {
        logMessage("Another client tried to connect (session " + std::to_string(clientSessionId));
        return nullptr;
    }
    
    // Update session ID for real session
    if (streamingSessionId == 0 && clientSessionId != 0) {
        streamingSessionId = clientSessionId;
        
        // Clear everything and start fresh for real session
        fCapture->stopCapture();
        fCapture->clearSpsPps();
        
        if (!fCapture->reset()) {
            logMessage("Failed to reset device for real session");
            return nullptr;
        }
        
        if (!fCapture->startCapture()) {
            logMessage("Failed to start capture for real session");
            return nullptr;
        }

        // Extract fresh SPS/PPS for real session
        if (!fCapture->extractSpsPps()) {
            logMessage("Failed to extract SPS/PPS for real session");
            fCapture->stopCapture();
            return nullptr;
        }
    }
    
    logMessage("Setting up stream for session: " + std::to_string(clientSessionId));
    
    // Create our custom source
    v4l2H264FramedSource* source = v4l2H264FramedSource::createNew(envir(), fCapture);
    if (source == nullptr) {
        logMessage("Failed to create v4l2H264FramedSource.");
        fCapture->stopCapture();
        return nullptr;
    }
    
    // Set the flag before wrapping with H264VideoStreamDiscreteFramer
    source->setNeedSpsPps();

    // Create and return the framer
    FramedSource* framer = H264VideoStreamDiscreteFramer::createNew(envir(), source);
    if (framer == nullptr) {
        Medium::close(source);
        logMessage("Failed to create H264VideoStreamDiscreteFramer.");
        return nullptr;
    }
    
    return framer;
}

RTPSink* v4l2H264MediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource) {
    logMessage("Creating new RTP sink with payload type: " + std::to_string(rtpPayloadTypeIfDynamic));
    
    // Ensure we have SPS/PPS
    if (!fCapture->hasSpsPps()) {
        if (!fCapture->extractSpsPps()) {
            envir() << "Failed to extract SPS/PPS. Cannot create RTP sink.\n";
            return nullptr;
        }
    }

    return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                                    fCapture->getSPS(), fCapture->getSPSSize(),
                                    fCapture->getPPS(), fCapture->getPPSSize());
}

void v4l2H264MediaSubsession::deleteStream(unsigned clientSessionId, void*& streamToken) {
    logMessage("Cleaning up session: " + std::to_string(clientSessionId));

    if (clientSessionId == streamingSessionId) {
        // Stop capture before cleanup
        fCapture->stopCapture();
        
        // Reset streaming session ID
        streamingSessionId = 0;
        
        // Reset device after clearing session
        if (!fCapture->reset()) {
            logMessage("Warning: Failed to reset capture device during cleanup");
        }
    }
    
    OnDemandServerMediaSubsession::deleteStream(clientSessionId, streamToken);
}

char const* v4l2H264MediaSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) {
    if (fAuxSDPLine != NULL) return fAuxSDPLine;

    if (!fCapture->hasSpsPps()) {
        if (!fCapture->extractSpsPps()) {
            envir() << "Failed to extract SPS and PPS. Cannot create aux SDP line.\n";
            return nullptr;
        }
    }
    
    u_int8_t* sps = fCapture->getSPS();
    u_int8_t* pps = fCapture->getPPS();
    unsigned spsSize = fCapture->getSPSSize();
    unsigned ppsSize = fCapture->getPPSSize();

    if (sps == nullptr || pps == nullptr || spsSize == 0 || ppsSize == 0) {
        envir() << "Invalid SPS or PPS. Cannot create aux SDP line.\n";
        return nullptr;
    }

    char* spsBase64 = base64Encode((char*)sps, spsSize);
    char* ppsBase64 = base64Encode((char*)pps, ppsSize);

    char const* fmtpFmt =
        "a=fmtp:%d packetization-mode=1;profile-level-id=%02X%02X%02X;sprop-parameter-sets=%s,%s\r\n";
    unsigned fmtpFmtSize = strlen(fmtpFmt)
        + 3 + 6 + strlen(spsBase64) + strlen(ppsBase64);
    char* fmtp = new char[fmtpFmtSize];
    sprintf(fmtp, fmtpFmt,
            rtpSink->rtpPayloadType(),
            sps[1], sps[2], sps[3],
            spsBase64, ppsBase64);

    delete[] spsBase64;
    delete[] ppsBase64;

    fAuxSDPLine = fmtp;
    return fAuxSDPLine;
}
