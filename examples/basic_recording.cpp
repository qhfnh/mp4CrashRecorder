/*
 * MP4 Crash-Safe Recorder - Example: Basic Recording
 * 
 * Demonstrates basic recording functionality
 * 
 * License: GPL v2+
 */

#include "mp4_recorder.h"
#include "common.h"
#include <iostream>
#include <cstring>

using namespace mp4_recorder;

int main() {
    SetLogLevel(LogLevel::INFO);
    
    // Create recorder
    Mp4Recorder recorder;
    
    // Configure recording
    RecorderConfig config;
    config.video_timescale = 30000;
    config.audio_timescale = 48000;
    config.audio_sample_rate = 48000;
    config.audio_channels = 2;
    config.flush_interval_ms = 500;
    
    // Start recording
    if (!recorder.start("output.mp4", config)) {
        MCSR_LOG(ERROR) << "Failed to start recording";
        return 1;
    }
    
    // Simulate writing frames
    uint8_t video_frame[1024];
    uint8_t audio_frame[512];
    
    memset(video_frame, 0xAA, sizeof(video_frame));
    memset(audio_frame, 0xBB, sizeof(audio_frame));
    
    // Write 100 video frames and 400 audio frames
    for (int i = 0; i < 100; i++) {
        // Write video frame
        if (!recorder.writeVideoFrame(video_frame, sizeof(video_frame), i * 1000, (i % 30 == 0))) {
            MCSR_LOG(ERROR) << "Failed to write video frame";
            return 1;
        }
        
        // Write 4 audio frames per video frame
        for (int j = 0; j < 4; j++) {
            if (!recorder.writeAudioFrame(audio_frame, sizeof(audio_frame), i * 1000 + j * 250)) {
                MCSR_LOG(ERROR) << "Failed to write audio frame";
                return 1;
            }
        }
    }
    
    // Stop recording
    if (!recorder.stop()) {
        MCSR_LOG(ERROR) << "Failed to stop recording";
        return 1;
    }
    
    MCSR_LOG(INFO) << "Recording completed successfully";
    return 0;
}
