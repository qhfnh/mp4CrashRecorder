/*
 * MP4 Crash-Safe Recorder - MoovBuilder Test
 * 
 * Comprehensive test for MoovBuilder implementation
 * Creates a valid MP4 file with video and audio frames
 * 
 * License: GPL v2+
 */

#include "../include/mp4_recorder.h"
#include "../include/moov_builder.h"
#include "../include/common.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>

using namespace mp4_recorder;

// Generate simple test video frame (H.264 NAL unit)
std::vector<uint8_t> generateTestVideoFrame(uint32_t frame_num, bool is_keyframe) {
    std::vector<uint8_t> frame;
    
    if (is_keyframe) {
        // Simplified H.264 keyframe (SPS + PPS + IDR slice)
        // SPS NAL unit
        frame.push_back(0x00);
        frame.push_back(0x00);
        frame.push_back(0x00);
        frame.push_back(0x01);
        frame.push_back(0x67);  // SPS NAL type
        frame.push_back(0x42);  // Profile
        frame.push_back(0x00);
        frame.push_back(0x1F);  // Level
        frame.push_back(0xE1);
        frame.push_back(0x00);
        frame.push_back(0x89);
        frame.push_back(0xA0);
        
        // PPS NAL unit
        frame.push_back(0x00);
        frame.push_back(0x00);
        frame.push_back(0x00);
        frame.push_back(0x01);
        frame.push_back(0x68);  // PPS NAL type
        frame.push_back(0xCE);
        frame.push_back(0x06);
        frame.push_back(0xE2);
        
        // IDR slice
        frame.push_back(0x00);
        frame.push_back(0x00);
        frame.push_back(0x00);
        frame.push_back(0x01);
        frame.push_back(0x65);  // IDR slice NAL type
        
        // Add some dummy data
        for (int i = 0; i < 100; i++) {
            frame.push_back((frame_num + i) & 0xFF);
        }
    } else {
        // Non-keyframe (P-slice)
        frame.push_back(0x00);
        frame.push_back(0x00);
        frame.push_back(0x00);
        frame.push_back(0x01);
        frame.push_back(0x41);  // P-slice NAL type
        
        // Add some dummy data
        for (int i = 0; i < 50; i++) {
            frame.push_back((frame_num + i) & 0xFF);
        }
    }
    
    return frame;
}

// Generate simple test audio frame (AAC)
std::vector<uint8_t> generateTestAudioFrame(uint32_t frame_num) {
    std::vector<uint8_t> frame;
    
    // AAC ADTS header
    frame.push_back(0xFF);  // Sync word
    frame.push_back(0xF1);  // Sync word + MPEG-4
    frame.push_back(0x50);  // Profile + Sampling rate
    frame.push_back(0x80);  // Channel config
    frame.push_back(0x00);  // Frame length
    frame.push_back(0x1F);  // Frame length + Buffer fullness
    frame.push_back(0xFC);  // Buffer fullness
    
    // Add some dummy audio data
    for (int i = 0; i < 200; i++) {
        frame.push_back((frame_num + i) & 0xFF);
    }
    
    return frame;
}

int main(int argc, char* argv[]) {
    EnableFileLogging("moov_builder_test.log");
    SetLogLevel(LogLevel::DEBUG);
    
    std::cout << "=== MP4 MoovBuilder Test ===" << std::endl;
    std::cout << "Creating test MP4 file with video and audio frames..." << std::endl;
    
    // Create test frames
    std::vector<FrameInfo> video_frames;
    std::vector<FrameInfo> audio_frames;
    
    uint64_t mdat_offset = 0;
    
    // Generate 30 video frames (1 second at 30fps)
    for (int i = 0; i < 30; i++) {
        auto frame_data = generateTestVideoFrame(i, i == 0 || i == 15);  // Keyframes at 0 and 15
        
        FrameInfo frame_info;
        frame_info.offset = mdat_offset;
        frame_info.size = frame_data.size();
        frame_info.pts = i * 1000;  // 1000 ticks per frame at 30fps
        frame_info.dts = i * 1000;
        frame_info.is_keyframe = (i == 0 || i == 15) ? 1 : 0;
        frame_info.track_id = 0;  // Video track
        
        video_frames.push_back(frame_info);
        mdat_offset += frame_data.size();
        
        std::cout << "Video frame " << i << ": offset=" << frame_info.offset 
                  << ", size=" << frame_info.size << ", keyframe=" << (int)frame_info.is_keyframe << std::endl;
    }
    
    // Generate 60 audio frames (1 second at 48kHz, ~800 samples per frame)
    for (int i = 0; i < 60; i++) {
        auto frame_data = generateTestAudioFrame(i);
        
        FrameInfo frame_info;
        frame_info.offset = mdat_offset;
        frame_info.size = frame_data.size();
        frame_info.pts = (i * 48000) / 60;  // 48kHz sample rate
        frame_info.dts = frame_info.pts;
        frame_info.is_keyframe = 1;  // Audio frames are always "keyframes"
        frame_info.track_id = 1;  // Audio track
        
        audio_frames.push_back(frame_info);
        mdat_offset += frame_data.size();
        
        if (i < 5 || i >= 55) {
            std::cout << "Audio frame " << i << ": offset=" << frame_info.offset 
                      << ", size=" << frame_info.size << std::endl;
        }
    }
    
    std::cout << "\nTotal mdat size: " << mdat_offset << " bytes" << std::endl;
    
    // Build moov box
    MoovBuilder builder;
    std::vector<uint8_t> moov_data;
    
    RecorderConfig config;
    config.video_timescale = 30000;
    config.audio_timescale = 48000;
    config.audio_sample_rate = 48000;
    config.audio_channels = 2;
    
    std::cout << "\nBuilding moov box..." << std::endl;
    if (!builder.buildMoov(video_frames, audio_frames, 
                          config.video_timescale, config.audio_timescale,
                          config.audio_sample_rate, config.audio_channels,
                          640, 480,
                          nullptr, 0, nullptr, 0,
                          40,
                          moov_data)) {
        std::cerr << "Failed to build moov box" << std::endl;
        return 1;
    }
    
    std::cout << "Moov box built successfully, size: " << moov_data.size() << " bytes" << std::endl;
    
    // Create test MP4 file
    const char* test_filename = "test_moov_output.mp4";
    std::cout << "\nCreating MP4 file: " << test_filename << std::endl;
    
    FILE* mp4_file = fopen(test_filename, "wb");
    if (!mp4_file) {
        std::cerr << "Failed to create MP4 file" << std::endl;
        return 1;
    }
    
    // Write ftyp box (20 bytes total)
    uint8_t ftyp[] = {
        0x00, 0x00, 0x00, 0x14,  // Size (20 bytes)
        'f', 't', 'y', 'p',       // Type
        'i', 's', 'o', 'm',       // Major brand
        0x00, 0x00, 0x00, 0x00,   // Minor version
        'i', 's', 'o', 'm'        // Compatible brand
    };
    fwrite(ftyp, 1, sizeof(ftyp), mp4_file);
    
    // Write mdat box header with correct size
    // mdat size = 8 (header) + mdat_offset (content)
    uint32_t mdat_size = 8 + mdat_offset;
    uint8_t mdat_header[8];
    mdat_header[0] = (mdat_size >> 24) & 0xFF;
    mdat_header[1] = (mdat_size >> 16) & 0xFF;
    mdat_header[2] = (mdat_size >> 8) & 0xFF;
    mdat_header[3] = mdat_size & 0xFF;
    mdat_header[4] = 'm';
    mdat_header[5] = 'd';
    mdat_header[6] = 'a';
    mdat_header[7] = 't';
    fwrite(mdat_header, 1, 8, mp4_file);
    
    // Write dummy mdat content
    std::cout << "Writing " << mdat_offset << " bytes of dummy mdat data..." << std::endl;
    for (uint64_t i = 0; i < mdat_offset; i++) {
        uint8_t byte = (i & 0xFF);
        fwrite(&byte, 1, 1, mp4_file);
    }
    
    // Write moov box
    std::cout << "Writing moov box (" << moov_data.size() << " bytes)..." << std::endl;
    if (fwrite(moov_data.data(), 1, moov_data.size(), mp4_file) != moov_data.size()) {
        std::cerr << "Failed to write moov box" << std::endl;
        fclose(mp4_file);
        return 1;
    }
    
    fclose(mp4_file);
    std::cout << "MP4 file created successfully!" << std::endl;
    
    // Print file info
    FILE* check_file = fopen(test_filename, "rb");
    if (check_file) {
        fseek(check_file, 0, SEEK_END);
        long file_size = ftell(check_file);
        fclose(check_file);
        std::cout << "File size: " << file_size << " bytes" << std::endl;
    }
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "Generated MP4 file: " << test_filename << std::endl;
    std::cout << "You can verify it with: ffmpeg -i " << test_filename << " -v error" << std::endl;
    std::cout << "Or play it with: ffplay " << test_filename << std::endl;
    
    return 0;
}
