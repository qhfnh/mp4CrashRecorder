/*
 * MP4 Crash-Safe Recorder - Example: Recovery Demo
 * 
 * Demonstrates crash recovery functionality with multiple scenarios
 * 
 * License: GPL v2+
 */

#include "mp4_recorder.h"
#include "common.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

using namespace mp4_recorder;

// Helper function to simulate crash
void simulateCrash(const std::string& filename) {
    MCSR_LOG(INFO) << "Simulating crash by starting recording without stopping...";
    
    Mp4Recorder recorder;
    RecorderConfig config;
    config.flush_interval_ms = 500;
    
    if (!recorder.start(filename, config)) {
        MCSR_LOG(ERROR) << "Failed to start recording";
        return;
    }
    
    uint8_t frame[1024];
    memset(frame, 0xAA, sizeof(frame));
    
    // Write some frames
    for (int i = 0; i < 15; i++) {
        if (!recorder.writeVideoFrame(frame, sizeof(frame), i * 1000, (i % 5 == 0))) {
            MCSR_LOG(ERROR) << "Failed to write frame";
            return;
        }
    }
    
    MCSR_LOG(INFO) << "Wrote 15 frames before crash";
    // Intentionally not calling stop() to simulate crash
}

// Helper function to recover from crash
bool recoverFromCrash(const std::string& filename) {
    MCSR_LOG(INFO) << "Checking for incomplete recording...";
    
    if (!Mp4Recorder::hasIncompleteRecording(filename)) {
        MCSR_LOG(INFO) << "No incomplete recording found";
        return true;
    }
    
    MCSR_LOG(INFO) << "Found incomplete recording, attempting recovery...";
    
    Mp4Recorder recorder;
    if (recorder.recover(filename)) {
        MCSR_LOG(INFO) << "Recovery successful!";
        return true;
    } else {
        MCSR_LOG(ERROR) << "Recovery failed";
        return false;
    }
}

int main() {
    SetLogLevel(LogLevel::INFO);
    
    MCSR_LOG(INFO) << "=== MP4 Crash-Safe Recorder - Recovery Demo ===\n";
    
    // Scenario 1: Check for existing incomplete recording
    MCSR_LOG(INFO) << "--- Scenario 1: Check for Existing Incomplete Recording ---";
    if (!recoverFromCrash("output.mp4")) {
        return 1;
    }
    
    // Scenario 2: Simulate crash and recover
    MCSR_LOG(INFO) << "\n--- Scenario 2: Simulate Crash and Recover ---";
    simulateCrash("demo_crash.mp4");
    if (!recoverFromCrash("demo_crash.mp4")) {
        return 1;
    }
    
    // Scenario 3: Multiple crash scenarios
    MCSR_LOG(INFO) << "\n--- Scenario 3: Multiple Crash Scenarios ---";
    for (int i = 0; i < 3; i++) {
        std::string filename = "demo_crash_" + std::to_string(i) + ".mp4";
        MCSR_LOG(INFO) << "Crash scenario " << i << 1 << "...";
        simulateCrash(filename);
        if (!recoverFromCrash(filename)) {
            return 1;
        }
    }
    
    MCSR_LOG(INFO) << "\n=== Recovery Demo Completed Successfully ===";
    return 0;
}
