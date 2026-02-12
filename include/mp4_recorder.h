/*
 * MP4 Crash-Safe Recorder
 * 
 * Header file for MP4 recording with crash recovery support
 * 
 * License: GPL v2+
 */

#ifndef MP4_RECORDER_H
#define MP4_RECORDER_H

#include <string>
#include <cstdint>
#include <vector>
#include <chrono>
#include <memory>

#include "file_ops.h"

namespace mp4_recorder {

// Frame information structure
struct FrameInfo {
    uint64_t offset;        // Offset in mdat
    uint32_t size;          // Frame size
    int64_t  pts;           // Presentation timestamp
    int64_t  dts;           // Decoding timestamp
    uint8_t  is_keyframe;   // 1 if keyframe, 0 otherwise
    uint8_t  track_id;      // 0 for video, 1 for audio
};

// Recording configuration
struct RecorderConfig {
    uint32_t video_timescale = 30000;
    uint32_t audio_timescale = 48000;
    uint32_t audio_sample_rate = 48000;
    uint16_t audio_channels = 2;
    uint32_t flush_interval_ms = 500;  // Flush every 500ms
    uint32_t flush_frame_count = 1000; // Or every 1000 frames
    uint32_t video_width = 640;        // Video width
    uint32_t video_height = 480;       // Video height
};

// Main recorder class
class Mp4Recorder {
public:
    Mp4Recorder();
    explicit Mp4Recorder(std::shared_ptr<IFileOps> file_ops);
    ~Mp4Recorder();

    // Start recording
    bool start(const std::string& filename, const RecorderConfig& config = RecorderConfig());

    // Write video frame
    bool writeVideoFrame(const uint8_t* data, uint32_t size, int64_t pts, bool is_keyframe);

    // Set H.264 SPS/PPS (for proper avcC box construction)
    bool setH264Config(const uint8_t* sps, uint32_t sps_size, const uint8_t* pps, uint32_t pps_size);

    // Write audio frame
    bool writeAudioFrame(const uint8_t* data, uint32_t size, int64_t pts);

    // Stop recording and finalize
    bool stop();

    // Check if there's an incomplete recording
    static bool hasIncompleteRecording(const std::string& filename);

    // Recover from incomplete recording
    bool recover(const std::string& filename);

    // Get current recording status
    bool isRecording() const { return recording_; }

    // Get number of frames recorded
    uint64_t getFrameCount() const { return frame_count_; }

private:
    // Internal methods
    bool createFiles(const std::string& filename);
    bool writeFrameToMdat(const uint8_t* data, uint32_t size);
    bool logFrameToIndex(const FrameInfo& frame);
    bool flushIfNeeded();
    bool buildAndWriteMoov();
    bool cleanupFiles();

    // Member variables
    std::string mp4_filename_;
    std::string idx_filename_;
    std::string lock_filename_;
    
    std::shared_ptr<IFileOps> file_ops_;
    std::unique_ptr<IFile> mp4_file_;
    std::unique_ptr<IFile> idx_file_;
    std::unique_ptr<IFile> lock_file_;

    RecorderConfig config_;
    bool recording_ = false;
    uint64_t frame_count_ = 0;
    uint64_t mdat_start_ = 0;
    uint64_t mdat_size_ = 0;

    std::vector<FrameInfo> video_frames_;
    std::vector<FrameInfo> audio_frames_;

    std::vector<uint8_t> h264_sps_;
    std::vector<uint8_t> h264_pps_;

    std::chrono::steady_clock::time_point last_flush_time_;
    uint32_t frames_since_flush_ = 0;
};

} // namespace mp4_recorder

#endif // MP4_RECORDER_H
