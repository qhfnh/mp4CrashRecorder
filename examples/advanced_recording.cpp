/*
 * MP4 Crash-Safe Recorder - Example: Advanced Recording
 * 
 * Demonstrates advanced recording features including:
 * - Multiple audio/video tracks
 * - Dynamic configuration
 * - Advanced error handling
 * - Performance monitoring
 * 
 * License: GPL v2+
 */

#include "mp4_recorder.h"
#include "common.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>

using namespace mp4_recorder;

class AdvancedRecorder {
private:
    Mp4Recorder recorder_;
    std::chrono::high_resolution_clock::time_point start_time_;
    uint64_t total_frames_ = 0;
    uint64_t total_bytes_ = 0;

public:
    bool startRecording(const std::string& filename) {
        SetLogLevel(LogLevel::INFO);
        
        RecorderConfig config;
        config.video_timescale = 30000;
        config.audio_timescale = 48000;
        config.audio_sample_rate = 48000;
        config.audio_channels = 2;
        config.flush_interval_ms = 500;
        config.flush_frame_count = 1000;
        
        if (!recorder_.start(filename, config)) {
            MCSR_LOG(ERROR) << "Failed to start recording";
            return false;
        }
        
        start_time_ = std::chrono::high_resolution_clock::now();
        MCSR_LOG(INFO) << "Advanced recording started: " << filename;
        return true;
    }
    
    bool writeVideoFrame(const uint8_t* data, uint32_t size, int64_t pts, bool is_keyframe) {
        if (!recorder_.writeVideoFrame(data, size, pts, is_keyframe)) {
            MCSR_LOG(ERROR) << "Failed to write video frame";
            return false;
        }
        
        total_frames_++;
        total_bytes_ += size;
        return true;
    }
    
    bool writeAudioFrame(const uint8_t* data, uint32_t size, int64_t pts) {
        if (!recorder_.writeAudioFrame(data, size, pts)) {
            MCSR_LOG(ERROR) << "Failed to write audio frame";
            return false;
        }
        
        total_frames_++;
        total_bytes_ += size;
        return true;
    }
    
    bool stopRecording() {
        if (!recorder_.stop()) {
            MCSR_LOG(ERROR) << "Failed to stop recording";
            return false;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);
        
        MCSR_LOG(INFO) << "Recording stopped";
        printStatistics(duration.count());
        return true;
    }
    
    void printStatistics(uint64_t duration_ms) {
        MCSR_LOG(INFO) << "=== Recording Statistics ===";
        MCSR_LOG(INFO) << "Total frames: " << total_frames_;
        MCSR_LOG(INFO) << "Total bytes: " << total_bytes_;
        MCSR_LOG(INFO) << "Duration: " << duration_ms << "ms";
        
        if (duration_ms > 0) {
            double fps = (total_frames_ * 1000.0) / duration_ms;
            double mbps = (total_bytes_ * 8.0) / (duration_ms * 1000.0);
            MCSR_LOG(INFO) << "Average FPS: " << fps;
            MCSR_LOG(INFO) << "Average bitrate: " << mbps << " Mbps";
        }
    }
    
    bool checkRecovery(const std::string& filename) {
        if (Mp4Recorder::hasIncompleteRecording(filename)) {
            MCSR_LOG(INFO) << "Incomplete recording detected, attempting recovery...";
            Mp4Recorder recovery_recorder;
            if (recovery_recorder.recover(filename)) {
                MCSR_LOG(INFO) << "Recovery successful!";
                return true;
            } else {
                MCSR_LOG(ERROR) << "Recovery failed";
                return false;
            }
        }
        return true;
    }
};

int main() {
    SetLogLevel(LogLevel::INFO);
    
    MCSR_LOG(INFO) << "=== Advanced Recording Example ===\n";
    
    // Check for incomplete recordings
    AdvancedRecorder recorder;
    if (!recorder.checkRecovery("advanced_output.mp4")) {
        return 1;
    }
    
    // Start recording
    if (!recorder.startRecording("advanced_output.mp4")) {
        return 1;
    }
    
    // Simulate recording with varying frame sizes
    uint8_t video_frame[2048];
    uint8_t audio_frame[1024];
    memset(video_frame, 0xAA, sizeof(video_frame));
    memset(audio_frame, 0xBB, sizeof(audio_frame));
    
    MCSR_LOG(INFO) << "Recording 60 seconds of video...\n";
    
    // Record 60 seconds at 30fps with 4 audio frames per video frame
    for (int i = 0; i < 1800; i++) {  // 60 seconds * 30fps
        // Write video frame
        bool is_keyframe = (i % 30 == 0);
        if (!recorder.writeVideoFrame(video_frame, sizeof(video_frame), i * 1000, is_keyframe)) {
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
        
        // Print progress every 10 seconds
        if ((i + 1) % 300 == 0) {
            MCSR_LOG(INFO) << "Progress: " << (i + 1) / 30 << " seconds recorded";
        }
    }
    
    // Stop recording
    if (!recorder.stopRecording()) {
        return 1;
    }
    
    MCSR_LOG(INFO) << "\n=== Advanced Recording Example Completed ===";
    return 0;
}
