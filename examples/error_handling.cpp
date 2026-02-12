/*
 * MP4 Crash-Safe Recorder - Example: Error Handling
 * 
 * Demonstrates comprehensive error handling including:
 * - Input validation
 * - Graceful error recovery
 * - Resource cleanup
 * - Detailed error reporting
 * 
 * License: GPL v2+
 */

#include "mp4_recorder.h"
#include "common.h"
#include <iostream>
#include <cstring>
#include <filesystem>

using namespace mp4_recorder;

class RobustRecorder {
private:
    Mp4Recorder recorder_;
    std::string output_filename_;
    bool recording_ = false;

public:
    bool validateInputs(const std::string& filename, uint32_t frame_size) {
        // Validate filename
        if (filename.empty()) {
            MCSR_LOG(ERROR) << "Filename cannot be empty";
            return false;
        }
        
        if (filename.length() > 260) {
            MCSR_LOG(ERROR) << "Filename too long (max 260 characters)";
            return false;
        }
        
        // Validate frame size
        if (frame_size == 0) {
            MCSR_LOG(ERROR) << "Frame size must be greater than 0";
            return false;
        }
        
        if (frame_size > 100 * 1024 * 1024) {  // 100MB max
            MCSR_LOG(ERROR) << "Frame size too large (max 100MB)";
            return false;
        }
        
        // Check directory exists and is writable
        std::filesystem::path filepath(filename);
        std::filesystem::path directory = filepath.parent_path();
        
        if (!directory.empty() && !std::filesystem::exists(directory)) {
            MCSR_LOG(ERROR) << "Output directory does not exist: " << directory.string();
            return false;
        }
        
        return true;
    }
    
    bool startRecording(const std::string& filename) {
        SetLogLevel(LogLevel::INFO);
        
        // Validate inputs
        if (!validateInputs(filename, 1024)) {
            MCSR_LOG(ERROR) << "Input validation failed";
            return false;
        }
        
        // Check for incomplete recordings
        if (Mp4Recorder::hasIncompleteRecording(filename)) {
            MCSR_LOG(INFO) << "Incomplete recording detected, attempting recovery...";
            if (!recorder_.recover(filename)) {
                MCSR_LOG(ERROR) << "Recovery failed, cannot proceed";
                return false;
            }
            MCSR_LOG(INFO) << "Recovery successful";
        }
        
        // Configure recording
        RecorderConfig config;
        config.video_timescale = 30000;
        config.audio_timescale = 48000;
        config.flush_interval_ms = 500;
        
        // Start recording
        if (!recorder_.start(filename, config)) {
            MCSR_LOG(ERROR) << "Failed to start recording";
            return false;
        }
        
        output_filename_ = filename;
        recording_ = true;
        MCSR_LOG(INFO) << "Recording started successfully";
        return true;
    }
    
    bool writeFrameWithErrorHandling(const uint8_t* data, uint32_t size, 
                                     int64_t pts, bool is_keyframe, bool is_audio) {
        // Validate inputs
        if (!recording_) {
            MCSR_LOG(ERROR) << "Recording not started";
            return false;
        }
        
        if (data == nullptr) {
            MCSR_LOG(ERROR) << "Frame data pointer is null";
            return false;
        }
        
        if (size == 0) {
            MCSR_LOG(ERROR) << "Frame size is zero";
            return false;
        }
        
        if (pts < 0) {
            MCSR_LOG(ERROR) << "Invalid PTS (must be non-negative)";
            return false;
        }
        
        // Write frame
        bool success = false;
        if (is_audio) {
            success = recorder_.writeAudioFrame(data, size, pts);
            if (!success) {
                MCSR_LOG(ERROR) << "Failed to write audio frame (size: " << size << ", pts: " << pts << ")";
            }
        } else {
            success = recorder_.writeVideoFrame(data, size, pts, is_keyframe);
            if (!success) {
                MCSR_LOG(ERROR) << "Failed to write video frame (size: " << size << ", pts: " << pts << ")";
            }
        }
        
        return success;
    }
    
    bool stopRecording() {
        if (!recording_) {
            MCSR_LOG(ERROR) << "Recording not started";
            return false;
        }
        
        if (!recorder_.stop()) {
            MCSR_LOG(ERROR) << "Failed to stop recording";
            return false;
        }
        
        recording_ = false;
        MCSR_LOG(INFO) << "Recording stopped successfully";
        return true;
    }
    
    void cleanup() {
        if (recording_) {
            MCSR_LOG(INFO) << "Cleaning up incomplete recording...";
            recorder_.stop();
        }
        
        // Remove incomplete files if recovery failed
        std::string lock_file = output_filename_ + ".lock";
        std::string idx_file = output_filename_ + ".idx";
        
        if (std::filesystem::exists(lock_file)) {
            try {
                std::filesystem::remove(lock_file);
                MCSR_LOG(INFO) << "Removed lock file";
            } catch (const std::exception& e) {
                MCSR_LOG(ERROR) << "Failed to remove lock file: " << std::string(e.what());
            }
        }
        
        if (std::filesystem::exists(idx_file)) {
            try {
                std::filesystem::remove(idx_file);
                MCSR_LOG(INFO) << "Removed index file";
            } catch (const std::exception& e) {
                MCSR_LOG(ERROR) << "Failed to remove index file: " << std::string(e.what());
            }
        }
    }
};

int main() {
    SetLogLevel(LogLevel::INFO);
    
    MCSR_LOG(INFO) << "=== Error Handling Example ===\n";
    
    RobustRecorder recorder;
    
    // Test 1: Invalid filename
    MCSR_LOG(INFO) << "Test 1: Invalid filename";
    if (!recorder.startRecording("")) {
        MCSR_LOG(INFO) << "Correctly rejected empty filename\n";
    }
    
    // Test 2: Valid recording
    MCSR_LOG(INFO) << "Test 2: Valid recording";
    if (!recorder.startRecording("error_handling_output.mp4")) {
        MCSR_LOG(ERROR) << "Failed to start recording";
        return 1;
    }
    
    // Test 3: Write valid frames
    MCSR_LOG(INFO) << "Test 3: Write valid frames";
    uint8_t video_frame[1024];
    uint8_t audio_frame[512];
    memset(video_frame, 0xAA, sizeof(video_frame));
    memset(audio_frame, 0xBB, sizeof(audio_frame));
    
    for (int i = 0; i < 30; i++) {
        if (!recorder.writeFrameWithErrorHandling(video_frame, sizeof(video_frame), 
                                                  i * 1000, (i % 30 == 0), false)) {
            MCSR_LOG(ERROR) << "Failed to write video frame";
            recorder.cleanup();
            return 1;
        }
        
        if (!recorder.writeFrameWithErrorHandling(audio_frame, sizeof(audio_frame), 
                                                  i * 1000, true, true)) {
            MCSR_LOG(ERROR) << "Failed to write audio frame";
            recorder.cleanup();
            return 1;
        }
    }
    
    MCSR_LOG(INFO) << "Successfully wrote 30 video frames and 30 audio frames\n";
    
    // Test 4: Invalid frame data
    MCSR_LOG(INFO) << "Test 4: Invalid frame data";
    if (!recorder.writeFrameWithErrorHandling(nullptr, 1024, 0, true, false)) {
        MCSR_LOG(INFO) << "Correctly rejected null frame data\n";
    }
    
    // Test 5: Invalid PTS
    MCSR_LOG(INFO) << "Test 5: Invalid PTS";
    if (!recorder.writeFrameWithErrorHandling(video_frame, sizeof(video_frame), 
                                             -1, true, false)) {
        MCSR_LOG(INFO) << "Correctly rejected negative PTS\n";
    }
    
    // Stop recording
    if (!recorder.stopRecording()) {
        MCSR_LOG(ERROR) << "Failed to stop recording";
        recorder.cleanup();
        return 1;
    }
    
    MCSR_LOG(INFO) << "\n=== Error Handling Example Completed ===";
    return 0;
}
