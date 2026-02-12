/*
 * MP4 Crash-Safe Recorder - Example: Multi-threaded Recording
 * 
 * Demonstrates multi-threaded recording with:
 * - Separate threads for video and audio
 * - Thread synchronization
 * - Queue-based frame handling
 * - Graceful shutdown
 * 
 * License: GPL v2+
 */

#include "mp4_recorder.h"
#include "common.h"
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstring>

using namespace mp4_recorder;

struct Frame {
    uint8_t* data;
    uint32_t size;
    int64_t pts;
    bool is_keyframe;
    bool is_audio;
};

class ThreadSafeRecorder {
private:
    Mp4Recorder recorder_;
    std::queue<Frame> frame_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    bool recording_ = false;
    bool shutdown_ = false;

public:
    bool start(const std::string& filename) {
        SetLogLevel(LogLevel::INFO);
        
        RecorderConfig config;
        config.video_timescale = 30000;
        config.audio_timescale = 48000;
        config.flush_interval_ms = 500;
        
        if (!recorder_.start(filename, config)) {
            MCSR_LOG(ERROR) << "Failed to start recording";
            return false;
        }
        
        recording_ = true;
        MCSR_LOG(INFO) << "Multi-threaded recording started";
        return true;
    }
    
    void enqueueFrame(const uint8_t* data, uint32_t size, int64_t pts, 
                     bool is_keyframe, bool is_audio) {
        Frame frame;
        frame.data = new uint8_t[size];
        memcpy(frame.data, data, size);
        frame.size = size;
        frame.pts = pts;
        frame.is_keyframe = is_keyframe;
        frame.is_audio = is_audio;
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            frame_queue_.push(frame);
        }
        queue_cv_.notify_one();
    }
    
    void processFrames() {
        while (recording_ || !frame_queue_.empty()) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !frame_queue_.empty() || shutdown_; });
            
            if (frame_queue_.empty()) {
                continue;
            }
            
            Frame frame = frame_queue_.front();
            frame_queue_.pop();
            lock.unlock();
            
            if (frame.is_audio) {
                if (!recorder_.writeAudioFrame(frame.data, frame.size, frame.pts)) {
                    MCSR_LOG(ERROR) << "Failed to write audio frame";
                }
            } else {
                if (!recorder_.writeVideoFrame(frame.data, frame.size, frame.pts, frame.is_keyframe)) {
                    MCSR_LOG(ERROR) << "Failed to write video frame";
                }
            }
            
            delete[] frame.data;
        }
    }
    
    bool stop() {
        recording_ = false;
        shutdown_ = true;
        queue_cv_.notify_all();
        
        if (!recorder_.stop()) {
            MCSR_LOG(ERROR) << "Failed to stop recording";
            return false;
        }
        
        MCSR_LOG(INFO) << "Multi-threaded recording stopped";
        return true;
    }
};

int main() {
    SetLogLevel(LogLevel::INFO);
    
    MCSR_LOG(INFO) << "=== Multi-threaded Recording Example ===\n";
    
    ThreadSafeRecorder recorder;
    if (!recorder.start("multithreaded_output.mp4")) {
        return 1;
    }
    
    // Start frame processing thread
    std::thread processor_thread([&recorder]() {
        recorder.processFrames();
    });
    
    // Simulate video thread
    std::thread video_thread([&recorder]() {
        uint8_t video_frame[1024];
        memset(video_frame, 0xAA, sizeof(video_frame));
        
        for (int i = 0; i < 300; i++) {  // 10 seconds at 30fps
            bool is_keyframe = (i % 30 == 0);
            recorder.enqueueFrame(video_frame, sizeof(video_frame), i * 1000, is_keyframe, false);
            
            if ((i + 1) % 30 == 0) {
                MCSR_LOG(INFO) << "Video: " << (i + 1) / 30 << " seconds";
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    });
    
    // Simulate audio thread
    std::thread audio_thread([&recorder]() {
        uint8_t audio_frame[512];
        memset(audio_frame, 0xBB, sizeof(audio_frame));
        
        for (int i = 0; i < 1200; i++) {  // 10 seconds at 120 audio frames/sec
            recorder.enqueueFrame(audio_frame, sizeof(audio_frame), i * 8333, true, true);
            
            if ((i + 1) % 120 == 0) {
                MCSR_LOG(INFO) << "Audio: " << (i + 1) / 120 << " seconds";
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    });
    
    // Wait for threads to complete
    video_thread.join();
    audio_thread.join();
    
    // Stop recording
    if (!recorder.stop()) {
        return 1;
    }
    
    processor_thread.join();
    
    MCSR_LOG(INFO) << "\n=== Multi-threaded Recording Example Completed ===";
    return 0;
}
