/*
 * MP4 Crash-Safe Recorder - MP4 Recover Demo
 * 
 * Demonstrates:
 * - Generating H.264 raw stream once using ffmpeg
 * - Reading H.264 stream frame-by-frame to simulate real-time capture
 * - Writing video and audio frames to MP4 file using the recorder
 * - Validating and playing the generated MP4 with ffmpeg
 * 
 * License: GPL v2+
 */

#include "mp4_recorder.h"
#include "common.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <fstream>
#include <cstdlib>
#include <vector>

using namespace mp4_recorder;

// Helper function to execute system command
int executeCommand(const std::string& cmd, std::string& output) {
    #ifdef _WIN32
        FILE* pipe = _popen(cmd.c_str(), "r");
    #else
        FILE* pipe = popen(cmd.c_str(), "r");
    #endif
    
    if (!pipe) return -1;
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    #ifdef _WIN32
        int status = _pclose(pipe);
    #else
        int status = pclose(pipe);
    #endif
    
    return status;
}

// Helper function to check if file exists
bool fileExists(const std::string& filename) {
    std::ifstream f(filename);
    return f.good();
}

// Helper function to get file size
uint64_t getFileSize(const std::string& filename) {
    std::ifstream f(filename, std::ios::binary | std::ios::ate);
    if (!f.good()) return 0;
    return f.tellg();
}

// Helper function to validate MP4 with ffprobe
bool validateMP4WithFFprobe(const std::string& filename) {
    MCSR_LOG(INFO) << "Validating MP4 with ffprobe: " << filename;
    
    std::string output;
    int result = executeCommand("ffprobe -v error -show_format -show_streams \"" + filename + "\" 2>&1", output);
    
    if (result == 0 && !output.empty()) {
        MCSR_LOG(INFO) << "ffprobe validation PASSED";
        MCSR_LOG(INFO) << "Output: " << output;
        return true;
    } else {
        MCSR_LOG(WARNING) << "ffprobe validation failed or tool not available";
        return false;
    }
}

// Helper function to play MP4 with ffplay
bool playMP4WithFFplay(const std::string& filename, uint32_t duration_seconds = 5) {
    MCSR_LOG(INFO) << "Playing MP4 with ffplay: " << filename;
    
    // Check if file exists
    if (!fileExists(filename)) {
        MCSR_LOG(ERROR) << "File not found: " << filename;
        return false;
    }
    
    uint64_t fileSize = getFileSize(filename);
    MCSR_LOG(INFO) << "File size: " << fileSize << " bytes";
    
    if (fileSize < 1000) {
        MCSR_LOG(ERROR) << "File too small to be valid MP4";
        return false;
    }
    
    // Try to play with ffplay
    std::string cmd = "ffplay -v error -autoexit -t " + std::to_string(duration_seconds) + 
                     " \"" + filename + "\" 2>&1";
    
    MCSR_LOG(INFO) << "Executing: " << cmd;
    std::string output;
    int result = executeCommand(cmd, output);
    
    if (result == 0) {
        MCSR_LOG(INFO) << "ffplay playback PASSED";
        return true;
    } else {
        MCSR_LOG(WARNING) << "ffplay playback skipped or failed (tool not available)";
        return false;
    }
}

// H.264 NAL unit structure
struct NALUnit {
    std::vector<uint8_t> data;
    int64_t timestamp;
    bool is_keyframe;
    uint8_t nal_type;
};

// H.264 SPS/PPS storage
struct H264Config {
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;
    bool has_config = false;
};

uint32_t getAdtsSampleRate(uint8_t index) {
    static const uint32_t kSampleRates[] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
        16000, 12000, 11025, 8000, 7350
    };

    if (index >= sizeof(kSampleRates) / sizeof(kSampleRates[0])) {
        return 0;
    }

    return kSampleRates[index];
}

class AdtsReader {
public:
    bool open(const std::string& filename) {
        file_.open(filename, std::ios::binary);
        if (!file_.good()) {
            MCSR_LOG(ERROR) << "Failed to open ADTS file: " << filename;
            return false;
        }
        sample_rate_ = 0;
        channels_ = 0;
        return true;
    }

    bool readFrame(std::vector<uint8_t>& payload) {
        payload.clear();
        if (!file_.good()) {
            return false;
        }

        uint8_t header[7] = {0};
        file_.read(reinterpret_cast<char*>(header), sizeof(header));
        if (file_.gcount() != static_cast<std::streamsize>(sizeof(header))) {
            return false;
        }

        if (header[0] != 0xFF || (header[1] & 0xF0) != 0xF0) {
            MCSR_LOG(ERROR) << "Invalid ADTS sync word";
            return false;
        }

        bool protection_absent = (header[1] & 0x01) != 0;
        uint8_t sample_rate_index = static_cast<uint8_t>((header[2] >> 2) & 0x0F);
        uint8_t channel_config = static_cast<uint8_t>(((header[2] & 0x01) << 2) | ((header[3] >> 6) & 0x03));
        uint32_t frame_length = ((header[3] & 0x03) << 11) |
                                (static_cast<uint32_t>(header[4]) << 3) |
                                ((header[5] & 0xE0) >> 5);

        uint32_t header_size = protection_absent ? 7 : 9;
        if (frame_length < header_size) {
            MCSR_LOG(ERROR) << "Invalid ADTS frame length";
            return false;
        }

        if (!protection_absent) {
            uint8_t crc[2] = {0};
            file_.read(reinterpret_cast<char*>(crc), sizeof(crc));
            if (file_.gcount() != static_cast<std::streamsize>(sizeof(crc))) {
                return false;
            }
        }

        uint32_t payload_size = frame_length - header_size;
        payload.resize(payload_size);
        file_.read(reinterpret_cast<char*>(payload.data()), payload_size);
        if (file_.gcount() != static_cast<std::streamsize>(payload_size)) {
            payload.clear();
            return false;
        }

        uint32_t parsed_sample_rate = getAdtsSampleRate(sample_rate_index);
        if (parsed_sample_rate == 0) {
            MCSR_LOG(ERROR) << "Unsupported ADTS sample rate index: " << sample_rate_index;
            return false;
        }

        if (sample_rate_ == 0) {
            sample_rate_ = parsed_sample_rate;
        } else if (sample_rate_ != parsed_sample_rate) {
            MCSR_LOG(WARNING) << "ADTS sample rate changed mid-stream";
        }

        if (channels_ == 0) {
            channels_ = channel_config;
        } else if (channels_ != channel_config) {
            MCSR_LOG(WARNING) << "ADTS channel config changed mid-stream";
        }

        return true;
    }

    void close() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    bool isOpen() const { return file_.is_open(); }
    uint32_t getSampleRate() const { return sample_rate_; }
    uint16_t getChannels() const { return channels_; }
    uint32_t getSamplesPerFrame() const { return 1024; }

private:
    std::ifstream file_;
    uint32_t sample_rate_ = 0;
    uint16_t channels_ = 0;
};

// H.264 Stream Reader - reads from pre-generated H.264 raw stream file
// Correctly handles emulation prevention bytes when finding NAL unit boundaries
class H264StreamReader {
private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t fps_ = 0;
    uint64_t frame_count_ = 0;
    bool initialized_ = false;
    std::vector<uint8_t> file_buffer_;
    std::vector<size_t> nal_offsets_;  // Start offset of each NAL unit
    size_t current_nal_ = 0;

    // Check if we're at a real start code (not emulated)
    // Real start codes: 0x00 0x00 0x01 or 0x00 0x00 0x00 0x01
    // Emulated sequences: 0x00 0x00 0x03 0x00/0x01/0x02/0x03 (0x03 is emulation prevention byte)
    bool isRealStartCode(size_t pos) const {
        if (pos + 2 >= file_buffer_.size()) return false;
        
        // Check for 0x00 0x00
        if (file_buffer_[pos] != 0x00 || file_buffer_[pos + 1] != 0x00) {
            return false;
        }
        
        // Check for 3-byte start code: 0x00 0x00 0x01
        if (file_buffer_[pos + 2] == 0x01) {
            return true;
        }
        
        // Check for 4-byte start code: 0x00 0x00 0x00 0x01
        if (pos + 3 < file_buffer_.size() &&
            file_buffer_[pos + 2] == 0x00 && file_buffer_[pos + 3] == 0x01) {
            return true;
        }
        
        return false;
    }

public:
    H264StreamReader() = default;
    
    ~H264StreamReader() {
        close();
    }

    // Open H.264 raw stream file and parse NAL unit boundaries
    bool open(const std::string& stream_file, uint32_t width, uint32_t height, uint32_t fps) {
        if (!fileExists(stream_file)) {
            MCSR_LOG(ERROR) << "H.264 stream file not found: " << stream_file;
            return false;
        }

        // Read entire file
        std::ifstream file(stream_file, std::ios::binary | std::ios::ate);
        if (!file.good()) {
            MCSR_LOG(ERROR) << "Failed to open H.264 stream file: " << stream_file;
            return false;
        }

        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        file_buffer_.resize(file_size);
        file.read(reinterpret_cast<char*>(file_buffer_.data()), file_size);
        file.close();

        width_ = width;
        height_ = height;
        fps_ = fps;
        initialized_ = true;
        
        // Find all NAL unit boundaries (real start codes only)
        // Skip emulation prevention sequences: 0x00 0x00 0x03 0x00/0x01/0x02/0x03
        nal_offsets_.push_back(0);
        
        for (size_t i = 0; i < file_buffer_.size(); ) {
            // Look for potential start code
            if (i + 2 < file_buffer_.size() &&
                file_buffer_[i] == 0x00 && file_buffer_[i + 1] == 0x00) {
                
                // Check if this is a real start code
                if (isRealStartCode(i)) {
                    if (i > 0) {  // Not the first NAL unit
                        nal_offsets_.push_back(i);
                    }
                    
                    // Skip past the start code
                    if (i + 3 < file_buffer_.size() && 
                        file_buffer_[i + 2] == 0x00 && file_buffer_[i + 3] == 0x01) {
                        i += 4;  // 4-byte start code
                    } else {
                        i += 3;  // 3-byte start code
                    }
                } else {
                    // Not a start code, move forward
                    i++;
                }
            } else {
                i++;
            }
        }
        
        nal_offsets_.push_back(file_buffer_.size());
        
        MCSR_LOG(INFO) << "H.264 stream reader opened: " << stream_file;
        MCSR_LOG(INFO) << "Resolution: " << width << "x" << height << " @ " << fps << "fps";
        MCSR_LOG(INFO) << "Stream file loaded: " << file_buffer_.size() << " bytes";
        MCSR_LOG(INFO) << "Found " << (nal_offsets_.size() - 1) << " NAL units";
        return true;
    }

    // Read next NAL unit from stream
    bool readNALUnit(NALUnit& nal) {
        if (!initialized_ || current_nal_ + 1 >= nal_offsets_.size()) {
            return false;
        }

        size_t nal_start_offset = nal_offsets_[current_nal_];
        size_t nal_end_offset = nal_offsets_[current_nal_ + 1];
        
        // Skip start code at the beginning
        size_t data_start = nal_start_offset;
        if (data_start + 3 < file_buffer_.size() &&
            file_buffer_[data_start] == 0x00 && file_buffer_[data_start + 1] == 0x00 &&
            file_buffer_[data_start + 2] == 0x00 && file_buffer_[data_start + 3] == 0x01) {
            data_start += 4;
        } else if (data_start + 2 < file_buffer_.size() &&
                   file_buffer_[data_start] == 0x00 && file_buffer_[data_start + 1] == 0x00 &&
                   file_buffer_[data_start + 2] == 0x01) {
            data_start += 3;
        }

        // Extract NAL unit data
        nal.data.clear();
        if (nal_end_offset > data_start) {
            nal.data.assign(file_buffer_.begin() + data_start, file_buffer_.begin() + nal_end_offset);
        }

        while (!nal.data.empty() && nal.data.back() == 0x00) {
            nal.data.pop_back();
        }

        // Get NAL type
        nal.nal_type = 0;
        nal.is_keyframe = false;
        if (!nal.data.empty()) {
            nal.nal_type = nal.data[0] & 0x1F;
            nal.is_keyframe = (nal.nal_type == 5);  // IDR slice
        }

        // Calculate timestamp (will be updated by caller for actual frames)
        nal.timestamp = (frame_count_ * 90000) / fps_;
        
        current_nal_++;
        
        // Only increment frame_count for slice NAL units
        if (nal.nal_type == 1 || nal.nal_type == 5) {
            frame_count_++;
        }
        
        return true;
    }

    void close() {
        file_buffer_.clear();
        nal_offsets_.clear();
        initialized_ = false;
        current_nal_ = 0;
        frame_count_ = 0;
    }

    bool isOpen() const { return initialized_; }
    uint32_t getWidth() const { return width_; }
    uint32_t getHeight() const { return height_; }
    uint32_t getFps() const { return fps_; }
    uint64_t getFrameCount() const { return frame_count_; }
};

// Generate H.264 raw stream using ffmpeg (one-time generation)
bool generateH264Stream(const std::string& output_stream, uint32_t width, uint32_t height, 
                       uint32_t fps, uint32_t duration_seconds) {
    MCSR_LOG(INFO) << "Generating H.264 raw stream with ffmpeg...";
    MCSR_LOG(INFO) << "Output: " << output_stream;
    MCSR_LOG(INFO) << "Resolution: " << width << "x" << height << " @ " << fps
                   << "fps, Duration: " << duration_seconds << "s";

    // Build ffmpeg command to generate H.264 raw stream
    // Using testsrc for test pattern
    // Use medium preset to generate single slice per frame (ultrafast uses multiple slices)
    // Force yuv420p pixel format and High profile for maximum compatibility with Windows Media Player
    // Disable B-frames (-bf 0) for better compatibility and simpler timestamp handling
    std::string cmd = "ffmpeg -f lavfi -i testsrc=s=" + std::to_string(width) + "x" + 
                     std::to_string(height) + ":d=" + std::to_string(duration_seconds) +
                     " -c:v libx264 -preset medium -profile:v high -pix_fmt yuv420p -bf 0 -r " + std::to_string(fps) +
                     " -f h264 \"" + output_stream + "\" -y 2>&1";

    MCSR_LOG(INFO) << "Executing: " << cmd;
    std::string output;
    int result = executeCommand(cmd, output);

    if (result == 0 && fileExists(output_stream)) {
        uint64_t file_size = getFileSize(output_stream);
        MCSR_LOG(INFO) << "H.264 stream generated successfully";
        MCSR_LOG(INFO) << "Stream file size: " << file_size << " bytes";
        return true;
    } else {
        MCSR_LOG(ERROR) << "Failed to generate H.264 stream";
        MCSR_LOG(ERROR) << "Command output: " << output;
        return false;
    }
}

bool generateAacAdtsStream(const std::string& output_stream, uint32_t sample_rate,
                           uint16_t channels, uint32_t duration_seconds) {
    MCSR_LOG(INFO) << "Generating AAC ADTS audio stream with ffmpeg...";
    MCSR_LOG(INFO) << "Output: " << output_stream;
    MCSR_LOG(INFO) << "Sample rate: " << sample_rate
                   << ", channels: " << channels
                   << ", duration: " << duration_seconds << "s";

    std::string cmd = "ffmpeg -f lavfi -i sine=frequency=440:duration=" +
                      std::to_string(duration_seconds) +
                      " -c:a aac -profile:a aac_low -ar " + std::to_string(sample_rate) +
                      " -ac " + std::to_string(channels) +
                      " -f adts \"" + output_stream + "\" -y 2>&1";

    MCSR_LOG(INFO) << "Executing: " << cmd;
    std::string output;
    int result = executeCommand(cmd, output);

    if (result == 0 && fileExists(output_stream)) {
        uint64_t file_size = getFileSize(output_stream);
        MCSR_LOG(INFO) << "AAC ADTS stream generated successfully";
        MCSR_LOG(INFO) << "Audio file size: " << file_size << " bytes";
        return true;
    } else {
        MCSR_LOG(ERROR) << "Failed to generate AAC ADTS stream";
        MCSR_LOG(ERROR) << "Command output: " << output;
        return false;
    }
}

// Demo: Read H.264 stream frame-by-frame and write to MP4 using Mp4Recorder
bool demoRealtimeH264ToMP4(const std::string& h264_stream_file, const std::string& audio_stream_file,
                           const std::string& output_mp4, uint32_t width, uint32_t height,
                           uint32_t fps, uint32_t duration_seconds) {
    MCSR_LOG(INFO) << "\n=== Demo: MP4 Recover Demo (Using Mp4Recorder) ===";
    MCSR_LOG(INFO) << "Input H.264 stream: " << h264_stream_file;
    MCSR_LOG(INFO) << "Input audio stream: " << audio_stream_file;
    MCSR_LOG(INFO) << "Output MP4: " << output_mp4;

    // Open H.264 stream reader
    H264StreamReader reader;
    if (!reader.open(h264_stream_file, width, height, fps)) {
        MCSR_LOG(ERROR) << "Failed to open H.264 stream";
        return false;
    }

    AdtsReader audio_reader;
    if (!audio_reader.open(audio_stream_file)) {
        MCSR_LOG(ERROR) << "Failed to open audio stream";
        return false;
    }

    std::vector<uint8_t> pending_audio_frame;
    if (!audio_reader.readFrame(pending_audio_frame)) {
        MCSR_LOG(ERROR) << "Failed to read first audio frame";
        return false;
    }

    uint32_t audio_sample_rate = audio_reader.getSampleRate();
    uint16_t audio_channels = audio_reader.getChannels();
    if (audio_sample_rate == 0 || audio_channels == 0) {
        MCSR_LOG(ERROR) << "Invalid audio stream parameters";
        return false;
    }

    // Configure recorder
    RecorderConfig config;
    config.video_timescale = 1200000;  // Use 1.2MHz timescale (standard for MP4)
    config.audio_timescale = audio_sample_rate;
    config.audio_sample_rate = audio_sample_rate;
    config.audio_channels = audio_channels;
    config.flush_interval_ms = 500;
    config.flush_frame_count = fps * 2;
    config.video_width = width;
    config.video_height = height;

    MCSR_LOG(INFO) << "Recorder config: timescale=" << config.video_timescale
                   << ", width=" << config.video_width
                   << ", height=" << config.video_height;
    MCSR_LOG(INFO) << "Audio config: sample_rate=" << audio_sample_rate
                   << ", channels=" << audio_channels;

    // Start recording
    Mp4Recorder recorder;
    if (!recorder.start(output_mp4, config)) {
        MCSR_LOG(ERROR) << "Failed to start recorder";
        return false;
    }

    MCSR_LOG(INFO) << "Recording started, reading H.264 stream frame-by-frame...";

    uint64_t nal_count = 0;
    uint64_t keyframe_count = 0;
    uint64_t slice_count = 0;  // Count actual video slices (frames)
    auto start_time = std::chrono::steady_clock::now();
    uint32_t frame_interval_ms = 1000 / fps;
    H264Config h264_config;
    bool config_set = false;
    bool config_sent_in_stream = false;

    uint32_t audio_samples_per_frame = audio_reader.getSamplesPerFrame();
    int64_t next_audio_pts = 0;
    bool has_audio_frame = true;
    int64_t last_video_pts_audio_ts = -1;
    
    // Buffer for accumulating NAL units into access units
    std::vector<uint8_t> au_buffer;
    int64_t au_pts = -1;
    bool au_is_keyframe = false;

    auto writeAudioUpTo = [&](int64_t max_audio_pts) -> bool {
        while (has_audio_frame && next_audio_pts <= max_audio_pts) {
            if (!recorder.writeAudioFrame(pending_audio_frame.data(),
                                          static_cast<uint32_t>(pending_audio_frame.size()),
                                          next_audio_pts)) {
                MCSR_LOG(ERROR) << "Failed to write audio frame to MP4";
                return false;
            }

            next_audio_pts += audio_samples_per_frame;
            if (!audio_reader.readFrame(pending_audio_frame)) {
                has_audio_frame = false;
                break;
            }
        }

        return true;
    };

    auto flushAccessUnit = [&](const char* reason) -> bool {
        if (au_buffer.empty() || au_pts < 0)
        {
            return true;
        }

        if (!recorder.writeVideoFrame(au_buffer.data(), au_buffer.size(), au_pts, au_is_keyframe))
        {
            MCSR_LOG(ERROR) << "Failed to write H.264 access unit to MP4 (" << reason << ")";
            return false;
        }

        last_video_pts_audio_ts = (au_pts * audio_sample_rate) / config.video_timescale;
        if (!writeAudioUpTo(last_video_pts_audio_ts)) {
            return false;
        }

        slice_count++;
        if (au_is_keyframe)
        {
            keyframe_count++;
        }

        MCSR_LOG(VERBOSE) << "AU (" << reason << ") size=" << au_buffer.size()
                          << ", keyframe=" << (au_is_keyframe ? "yes" : "no")
                          << ", timestamp=" << au_pts;

        au_buffer.clear();
        au_pts = -1;
        au_is_keyframe = false;
        return true;
    };

    // Read and write frames
    while (true) {
        auto frame_start = std::chrono::steady_clock::now();

        NALUnit nal;
        if (!reader.readNALUnit(nal)) {
            MCSR_LOG(INFO) << "H.264 stream ended";
            if (!flushAccessUnit("end of stream"))
            {
                return false;
            }
            break;
        }

        if (nal.data.empty()) {
            continue;
        }

        nal_count++;

        // Handle SPS (type 7) and PPS (type 8)
        if (nal.nal_type == 7)
        {
            MCSR_LOG(VERBOSE) << "Found SPS NAL unit, size=" << nal.data.size();
            h264_config.sps = nal.data;
            MCSR_LOG(INFO) << "SPS stored: size=" << h264_config.sps.size();
            continue;
        }
        if (nal.nal_type == 8)
        {
            MCSR_LOG(VERBOSE) << "Found PPS NAL unit, size=" << nal.data.size();
            h264_config.pps = nal.data;
            h264_config.has_config = true;
            MCSR_LOG(INFO) << "PPS stored: size=" << h264_config.pps.size();

            if (!config_set && !h264_config.sps.empty() && !h264_config.pps.empty())
            {
                if (!recorder.setH264Config(h264_config.sps.data(), h264_config.sps.size(),
                                            h264_config.pps.data(), h264_config.pps.size()))
                {
                    MCSR_LOG(ERROR) << "Failed to set H.264 config";
                    return false;
                }
                config_set = true;
                MCSR_LOG(INFO) << "H.264 config set successfully (SPS: " << h264_config.sps.size()
                               << " bytes, PPS: " << h264_config.pps.size() << " bytes)";
            }
            continue;
        }

        // Skip non-slice NAL units (SEI, AUD, etc.)
        if (nal.nal_type != 1 && nal.nal_type != 5)
        {
            MCSR_LOG(VERBOSE) << "Skipping non-slice NAL type " << static_cast<int>(nal.nal_type)
                              << ", size=" << nal.data.size();
            continue;
        }

        MCSR_LOG(VERBOSE) << "Processing slice NAL type " << static_cast<int>(nal.nal_type)
                          << ", size=" << nal.data.size()
                          << ", keyframe=" << (nal.is_keyframe ? "yes" : "no");

        // With slices=1, each slice is a complete frame
        // Flush previous frame if any
        if (!au_buffer.empty())
        {
            if (!flushAccessUnit("new slice"))
            {
                return false;
            }
        }

        // Calculate timestamp based on slice count (frames that will be written)
        // slice_count was incremented in flushAccessUnit, so this is the correct PTS for the new frame
        int64_t slice_pts = static_cast<int64_t>(slice_count) * 1200000 / fps;

        au_buffer.clear();
        au_pts = slice_pts;
        au_is_keyframe = nal.is_keyframe;

        auto appendLengthPrefixedNal = [&](const std::vector<uint8_t>& data) {
            uint32_t nal_size = data.size();
            au_buffer.push_back((nal_size >> 24) & 0xFF);
            au_buffer.push_back((nal_size >> 16) & 0xFF);
            au_buffer.push_back((nal_size >> 8) & 0xFF);
            au_buffer.push_back(nal_size & 0xFF);
            au_buffer.insert(au_buffer.end(), data.begin(), data.end());
        };

        if (nal.is_keyframe && config_set && !config_sent_in_stream &&
            !h264_config.sps.empty() && !h264_config.pps.empty()) {
            appendLengthPrefixedNal(h264_config.sps);
            appendLengthPrefixedNal(h264_config.pps);
            config_sent_in_stream = true;
        }
         
         // NAL unit data (without start code, but keep all bytes including trailing zeros)
         std::vector<uint8_t> nal_data = nal.data;
         
          appendLengthPrefixedNal(nal_data);
         
         if (slice_count < 3) {
             MCSR_LOG(VERBOSE) << "Slice " << slice_count << ": NAL type="
                               << static_cast<int>(nal.nal_type)
                               << ", size=" << nal_data.size() << ", first 10 bytes: ";
             for (size_t i = 0; i < std::min(size_t(10), nal_data.size()); i++) {
                 MCSR_LOG(VERBOSE) << static_cast<int>(nal_data[i]) << " ";
             }
         }
        // Simulate real-time frame rate
        auto frame_end = std::chrono::steady_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            frame_end - frame_start);
        int32_t sleep_ms = frame_interval_ms - frame_duration.count();
        if (sleep_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        }

        // Check duration
        auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time);
        // Note: We don't check duration here because we want to read all NAL units from the stream
        // The stream generation already limits the duration
    }

    MCSR_LOG(INFO) << "H.264 stream reading completed";
    MCSR_LOG(INFO) << "NAL units written: " << nal_count;
    MCSR_LOG(INFO) << "Video slices (frames): " << slice_count;
    MCSR_LOG(INFO) << "Keyframes: " << keyframe_count;

    if (last_video_pts_audio_ts >= 0) {
        if (!writeAudioUpTo(last_video_pts_audio_ts)) {
            return false;
        }
    }

    // Stop recording
    if (!recorder.stop()) {
        MCSR_LOG(ERROR) << "Failed to stop recorder";
        return false;
    }

    reader.close();
    audio_reader.close();

    MCSR_LOG(INFO) << "Recording stopped";
    MCSR_LOG(INFO) << "Output file: " << output_mp4;
    
    uint64_t mp4_size = getFileSize(output_mp4);
    MCSR_LOG(INFO) << "MP4 file size: " << mp4_size << " bytes";

    return true;
}

int main(int argc, char* argv[]) {
    SetLogLevel(LogLevel::DEBUG);
    
    std::string header = "=== MP4 Crash-Safe Recorder - MP4 Recover Demo ===";
    MCSR_LOG(INFO) << header;

    // Parameters
    uint32_t width = 640;
    uint32_t height = 480;
    uint32_t fps = 30;
    uint32_t duration = 5;
    uint32_t audio_sample_rate = 48000;
    uint16_t audio_channels = 2;
    
    std::string h264_stream_file = "test_stream.h264";
    std::string audio_stream_file = "test_audio.aac";
    std::string output_mp4 = "mp4_recover_output.mp4";

    // Check for incomplete recording from previous crash
    if (Mp4Recorder::hasIncompleteRecording(output_mp4)) {
        MCSR_LOG(INFO) << "\n=== Crash Recovery Detected ===";
        MCSR_LOG(INFO) << "Found incomplete recording: " << output_mp4;
        
        Mp4Recorder recovery_recorder;
        if (recovery_recorder.recover(output_mp4)) {
            MCSR_LOG(INFO) << "Recovery successful - MP4 file reconstructed";
            
            // Validate recovered file
            MCSR_LOG(INFO) << "\n--- Validating Recovered MP4 ---";
            if (validateMP4WithFFprobe(output_mp4)) {
                MCSR_LOG(INFO) << "Recovered MP4 validation PASSED";
            } else {
                MCSR_LOG(WARNING) << "Recovered MP4 validation failed";
            }
            
            MCSR_LOG(INFO) << "\nRecovery complete. Exiting program.";
            return 0;
        } else {
            MCSR_LOG(ERROR) << "Recovery failed";
            return 1;
        }
    }

    MCSR_LOG(INFO) << "Demo Parameters:";
    MCSR_LOG(INFO) << "  Resolution: " << width << "x" << height;
    MCSR_LOG(INFO) << "  FPS: " << fps;
    MCSR_LOG(INFO) << "  Duration: " << duration << " seconds";
    MCSR_LOG(INFO) << "  H.264 stream file: " << h264_stream_file;
    MCSR_LOG(INFO) << "  Audio stream file: " << audio_stream_file;
    MCSR_LOG(INFO) << "  Audio sample rate: " << audio_sample_rate;
    MCSR_LOG(INFO) << "  Audio channels: " << audio_channels;
    MCSR_LOG(INFO) << "  Output MP4: " << output_mp4;

    // Step 1: Generate H.264 stream (one-time)
    MCSR_LOG(INFO) << "\n--- Step 1: Generate H.264 Raw Stream ---";
    if (!generateH264Stream(h264_stream_file, width, height, fps, duration)) {
        MCSR_LOG(ERROR) << "Failed to generate H.264 stream";
        return 1;
    }

    // Step 2: Generate AAC ADTS audio stream (one-time)
    MCSR_LOG(INFO) << "\n--- Step 2: Generate AAC ADTS Audio Stream ---";
    if (!generateAacAdtsStream(audio_stream_file, audio_sample_rate, audio_channels, duration)) {
        MCSR_LOG(ERROR) << "Failed to generate AAC ADTS audio stream";
        return 1;
    }

    // Step 3: Read streams and write to MP4
    MCSR_LOG(INFO) << "\n--- Step 3: Read H.264 + AAC Streams and Write to MP4 ---";
    if (!demoRealtimeH264ToMP4(h264_stream_file, audio_stream_file, output_mp4,
                               width, height, fps, duration)) {
        MCSR_LOG(ERROR) << "Failed to write MP4";
        return 1;
    }

    // Step 4: Validate MP4 with ffprobe
    MCSR_LOG(INFO) << "\n--- Step 4: Validate MP4 with ffprobe ---";
    if (!validateMP4WithFFprobe(output_mp4)) {
        MCSR_LOG(ERROR) << "MP4 validation failed";
        return 1;
    }

    // Step 5: Play MP4 with ffplay
    MCSR_LOG(INFO) << "\n--- Step 5: Play MP4 with ffplay ---";
    if (!playMP4WithFFplay(output_mp4, 3)) {
        MCSR_LOG(ERROR) << "MP4 playback failed";
        return 1;
    }

    // Final summary
    std::string success_msg = "\n=== Demo Completed Successfully ===";
    MCSR_LOG(INFO) << success_msg;
    MCSR_LOG(INFO) << "Generated MP4 file is playable with ffplay";
    MCSR_LOG(INFO) << "All validations PASSED";

    return 0;
}
