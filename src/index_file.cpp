/*
 * MP4 Crash-Safe Recorder - Index File Implementation
 * 
 * License: GPL v2+
 */

#include "index_file.h"
#include "mp4_recorder.h"
#include "common.h"
#include <cstring>

namespace mp4_recorder {

IndexFile::IndexFile()
    : file_ops_(std::make_shared<StdioFileOps>()) {
}

IndexFile::IndexFile(std::shared_ptr<IFileOps> file_ops)
    : file_ops_(file_ops ? file_ops : std::make_shared<StdioFileOps>()) {
}

IndexFile::~IndexFile() {
    close();
}

bool IndexFile::create(const std::string& filename) {
    filename_ = filename;
    file_ = file_ops_->open(filename, "wb");
    if (!file_ || !file_->isOpen()) {
        MCSR_LOG(ERROR) << "Failed to create index file: " << filename;
        return false;
    }
    
    frame_count_ = 0;
    dirty_ = false;
    
    return true;
}

bool IndexFile::writeConfig(const RecorderConfig& config) {
    if (!file_) {
        MCSR_LOG(ERROR) << "Index file not open";
        return false;
    }
    
    // Write magic number for validation
    uint32_t magic = 0x4D503452;  // "MP4R" in hex
    if (file_->write(&magic, sizeof(uint32_t)) != sizeof(uint32_t)) {
        MCSR_LOG(ERROR) << "Failed to write magic number to index";
        return false;
    }
    
    // Write config structure
    if (file_->write(&config, sizeof(RecorderConfig)) != sizeof(RecorderConfig)) {
        MCSR_LOG(ERROR) << "Failed to write config to index";
        return false;
    }
    
    dirty_ = true;
    MCSR_LOG(INFO) << "Config written to index file";
    return true;
}

bool IndexFile::open(const std::string& filename) {
    filename_ = filename;
    file_ = file_ops_->open(filename, "rb");
    if (!file_ || !file_->isOpen()) {
        MCSR_LOG(ERROR) << "Failed to open index file: " << filename;
        return false;
    }
    
    // Count frames (skip header: magic + config)
    if (!file_->seek(0, SEEK_END)) {
        MCSR_LOG(ERROR) << "Failed to seek index file for size";
        return false;
    }
    int64_t file_size = file_->tell();
    uint64_t header_size = sizeof(uint32_t) + sizeof(RecorderConfig);
    if (file_size > 0 && static_cast<uint64_t>(file_size) > header_size) {
        frame_count_ = (static_cast<uint64_t>(file_size) - header_size) / sizeof(FrameInfo);
    } else {
        frame_count_ = 0;
    }
    file_->seek(0, SEEK_SET);
    
    return true;
}

bool IndexFile::readConfig(RecorderConfig& config) {
    if (!file_) {
        MCSR_LOG(ERROR) << "Index file not open";
        return false;
    }
    
    // Read and validate magic number
    uint32_t magic = 0;
    if (file_->read(&magic, sizeof(uint32_t)) != sizeof(uint32_t)) {
        MCSR_LOG(ERROR) << "Failed to read magic number from index";
        return false;
    }
    
    if (magic != 0x4D503452) {  // "MP4R"
        MCSR_LOG(ERROR) << "Invalid index file format (bad magic number)";
        return false;
    }
    
    // Read config structure
    if (file_->read(&config, sizeof(RecorderConfig)) != sizeof(RecorderConfig)) {
        MCSR_LOG(ERROR) << "Failed to read config from index";
        return false;
    }
    
    MCSR_LOG(INFO) << "Config read from index file: timescale=" << config.video_timescale << ", resolution=" << config.video_width << "x" << config.video_height;
    return true;
}

bool IndexFile::writeFrame(const FrameInfo& frame) {
    if (!file_) {
        MCSR_LOG(ERROR) << "Index file not open";
        return false;
    }
    
    if (file_->write(&frame, sizeof(FrameInfo)) != sizeof(FrameInfo)) {
        MCSR_LOG(ERROR) << "Failed to write frame to index";
        return false;
    }
    
    frame_count_++;
    dirty_ = true;
    
    return true;
}

bool IndexFile::readAllFrames(std::vector<FrameInfo>& video_frames, std::vector<FrameInfo>& audio_frames) {
    if (!file_) {
        MCSR_LOG(ERROR) << "Index file not open";
        return false;
    }
    
    video_frames.clear();
    audio_frames.clear();

    if (frame_count_ > 0) {
        size_t reserve_count = static_cast<size_t>(frame_count_);
        video_frames.reserve(reserve_count);
        audio_frames.reserve(reserve_count);
    }
    
    // Skip header (magic + config) and start reading frames
    uint64_t header_size = sizeof(uint32_t) + sizeof(RecorderConfig);
    file_->seek(static_cast<int64_t>(header_size), SEEK_SET);
    
    FrameInfo frame;
    while (file_->read(&frame, sizeof(FrameInfo)) == sizeof(FrameInfo)) {
        if (frame.track_id == 0) {
            video_frames.push_back(frame);
        } else if (frame.track_id == 1) {
            audio_frames.push_back(frame);
        }
    }
    
    return true;
}

bool IndexFile::flush() {
    if (!file_ || !dirty_) {
        return true;
    }
    
    if (!file_->flush()) {
        MCSR_LOG(ERROR) << "Failed to flush index file";
        return false;
    }
    
    dirty_ = false;
    return true;
}

void IndexFile::close() {
    if (file_) {
        flush();
        file_->close();
        file_.reset();
    }
}

bool IndexFile::exists(const std::string& filename) {
    StdioFileOps ops;
    return ops.exists(filename);
}

} // namespace mp4_recorder
