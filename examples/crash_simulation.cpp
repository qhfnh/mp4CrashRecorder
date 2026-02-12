/*
 * Crash Simulation Test - Simulates a crash during recording
 * 
 * This program:
 * 1. Starts recording
 * 2. Writes a few frames
 * 3. Exits WITHOUT calling stop() (simulating a crash)
 * 4. Leaves .mp4, .idx, and .lock files behind
 * 
 * Then run recovery_demo or call Mp4Recorder::recover() to test recovery
 */

#include "mp4_recorder.h"
#include "common.h"
#include <iostream>
#include <vector>
#include <cstdlib>

using namespace mp4_recorder;

int main() {
    SetLogLevel(LogLevel::INFO);
    
    std::cout << "=== Crash Simulation Test ===" << std::endl;
    std::cout << "This will create an incomplete recording to test recovery" << std::endl;
    
    std::string output_file = "crash_simulation.mp4";
    
    // Configure recorder
    RecorderConfig config;
    config.video_timescale = 1200000;
    config.video_width = 640;
    config.video_height = 480;
    config.flush_interval_ms = 100;  // Flush frequently
    
    // Start recording
    Mp4Recorder recorder;
    if (!recorder.start(output_file, config)) {
        std::cerr << "Failed to start recording" << std::endl;
        return 1;
    }
    
    std::cout << "Recording started..." << std::endl;
    
    // Set H.264 config (minimal SPS/PPS)
    uint8_t sps[] = {0x67, 0x42, 0x00, 0x1e, 0x8c, 0x8d, 0x40, 0x50, 0x17, 0xfc, 0xb0, 0x0f, 0x08, 0x84, 0x6a};
    uint8_t pps[] = {0x68, 0xce, 0x3c, 0x80};
    recorder.setH264Config(sps, sizeof(sps), pps, sizeof(pps));
    
    // Write a few fake frames
    std::vector<uint8_t> frame_data(1000, 0x42);  // Fake frame data
    
    for (int i = 0; i < 10; i++) {
        int64_t pts = i * 40000;  // 40ms per frame (25fps)
        bool is_keyframe = (i == 0);
        
        if (!recorder.writeVideoFrame(frame_data.data(), frame_data.size(), pts, is_keyframe)) {
            std::cerr << "Failed to write frame " << i << std::endl;
            return 1;
        }
        
        std::cout << "Wrote frame " << i << " (pts=" << pts << ", keyframe=" << is_keyframe << ")" << std::endl;
    }
    
    std::cout << "\n=== SIMULATING CRASH (exiting without stop()) ===" << std::endl;
    std::cout << "Files left behind:" << std::endl;
    std::cout << "  - " << output_file << std::endl;
    std::cout << "  - " << output_file << ".idx" << std::endl;
    std::cout << "  - " << output_file << ".lock" << std::endl;
    std::cout << "\nNow run: ./recovery_demo.exe" << std::endl;
    std::cout << "Or test recovery with: Mp4Recorder::recover(\"" << output_file << "\")" << std::endl;
    
    // EXIT WITHOUT CALLING recorder.stop() - this simulates a crash!
    // The destructor will NOT be called because we use exit()
    std::exit(0);  // Simulate crash - files remain incomplete
}
