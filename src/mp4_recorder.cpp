/*
 * MP4 Crash-Safe Recorder - Implementation
 * 
 * Main recorder implementation
 * 
 * License: GPL v2+
 */

#include "mp4_recorder.h"
#include "moov_builder.h"
#include "index_file.h"
#include "common.h"

#include <cstring>
#include <chrono>

namespace mp4_recorder {

namespace {

bool extractH264ConfigFromSample(const std::vector<uint8_t>& sample, std::vector<uint8_t>& sps, std::vector<uint8_t>& pps)
{
    if (sample.size() < 4) {
        return false;
    }

    auto handleNal = [&](const uint8_t* nal_data, size_t nal_size) {
        const size_t max_param_size = 256;
        if (nal_size == 0) {
            return;
        }
        if (nal_size > max_param_size) {
            return;
        }
        uint8_t nal_type = nal_data[0] & 0x1F;
        if (nal_type == 7 && sps.empty()) {
            sps.assign(nal_data, nal_data + nal_size);
        } else if (nal_type == 8 && pps.empty()) {
            pps.assign(nal_data, nal_data + nal_size);
        }
    };

    bool has_start_code = false;
    if (sample.size() >= 4 && sample[0] == 0x00 && sample[1] == 0x00 && sample[2] == 0x00 && sample[3] == 0x01) {
        has_start_code = true;
    } else if (sample.size() >= 3 && sample[0] == 0x00 && sample[1] == 0x00 && sample[2] == 0x01) {
        has_start_code = true;
    }

    if (has_start_code) {
        size_t pos = 0;
        size_t start = 0;
        while (pos + 3 < sample.size()) {
            bool is_start = false;
            size_t start_len = 0;
            if (pos + 3 < sample.size() && sample[pos] == 0x00 && sample[pos + 1] == 0x00 && sample[pos + 2] == 0x01) {
                is_start = true;
                start_len = 3;
            } else if (pos + 4 < sample.size() && sample[pos] == 0x00 && sample[pos + 1] == 0x00 &&
                       sample[pos + 2] == 0x00 && sample[pos + 3] == 0x01) {
                is_start = true;
                start_len = 4;
            }

            if (is_start) {
                if (start < pos) {
                    handleNal(sample.data() + start, pos - start);
                    if (!sps.empty() && !pps.empty()) {
                        return true;
                    }
                }
                pos += start_len;
                start = pos;
            } else {
                pos++;
            }
        }

        if (start < sample.size()) {
            handleNal(sample.data() + start, sample.size() - start);
        }
        return !sps.empty() && !pps.empty();
    }

    size_t pos = 0;
    while (pos + 4 <= sample.size()) {
        uint32_t nal_size = readBE32(sample.data() + pos);
        pos += 4;
        if (nal_size == 0 || pos + nal_size > sample.size()) {
            break;
        }
        handleNal(sample.data() + pos, nal_size);
        if (!sps.empty() && !pps.empty()) {
            return true;
        }
        pos += nal_size;
    }

    return !sps.empty() && !pps.empty();
}

bool extractH264ConfigFromMdat(IFileOps& file_ops, const std::string& filename, uint64_t mdat_start,
                               const std::vector<FrameInfo>& video_frames,
                               std::vector<uint8_t>& sps, std::vector<uint8_t>& pps)
{
    std::unique_ptr<IFile> file = file_ops.open(filename, "rb");
    if (!file || !file->isOpen()) {
        MCSR_LOG(WARNING) << "Failed to open MP4 for H.264 config extraction";
        return false;
    }

    for (const auto& frame : video_frames) {
        if (frame.size == 0) {
            continue;
        }

        uint64_t offset = mdat_start + frame.offset;
        if (!file->seek(static_cast<int64_t>(offset), SEEK_SET)) {
            continue;
        }

        std::vector<uint8_t> sample(frame.size);
        size_t read_bytes = file->read(sample.data(), frame.size);
        if (read_bytes != frame.size) {
            continue;
        }

        if (extractH264ConfigFromSample(sample, sps, pps)) {
            return true;
        }
    }
    return false;
}

} // namespace

Mp4Recorder::Mp4Recorder()
    : file_ops_(std::make_shared<StdioFileOps>()) {
}

Mp4Recorder::Mp4Recorder(std::shared_ptr<IFileOps> file_ops)
    : file_ops_(file_ops ? file_ops : std::make_shared<StdioFileOps>()) {
}

Mp4Recorder::~Mp4Recorder() {
    if (recording_) {
        stop();
    }
}

bool Mp4Recorder::start(const std::string& filename, const RecorderConfig& config) {
    if (recording_) {
        MCSR_LOG(ERROR) << "Already recording";
        return false;
    }

    mp4_filename_ = filename;
    idx_filename_ = filename + ".idx";
    lock_filename_ = filename + ".lock";
    config_ = config;

    if (!createFiles(filename)) {
        MCSR_LOG(ERROR) << "Failed to create files";
        return false;
    }

    recording_ = true;
    frame_count_ = 0;
    last_flush_time_ = std::chrono::steady_clock::now();
    frames_since_flush_ = 0;

    if (config_.flush_frame_count > 0) {
        video_frames_.reserve(config_.flush_frame_count);
        audio_frames_.reserve(config_.flush_frame_count);
    }

    MCSR_LOG(INFO) << "Recording started: " << filename;
    return true;
}

bool Mp4Recorder::setH264Config(const uint8_t* sps, uint32_t sps_size, const uint8_t* pps, uint32_t pps_size) {
    if (!sps || sps_size == 0 || !pps || pps_size == 0) {
        MCSR_LOG(ERROR) << "Invalid SPS/PPS data";
        return false;
    }
    
    h264_sps_.assign(sps, sps + sps_size);
    h264_pps_.assign(pps, pps + pps_size);
    
    MCSR_LOG(INFO) << "H.264 config set: SPS size=" << sps_size << ", PPS size=" << pps_size;
    return true;
}

bool Mp4Recorder::writeVideoFrame(const uint8_t* data, uint32_t size, int64_t pts, bool is_keyframe) {
    if (!recording_) {
        MCSR_LOG(ERROR) << "Not recording";
        return false;
    }

    // Log frame info BEFORE writing (offset is current mdat_size_)
    FrameInfo frame;
    frame.offset = mdat_size_;
    frame.size = size;
    frame.pts = pts;
    frame.dts = pts;
    frame.is_keyframe = is_keyframe ? 1 : 0;
    frame.track_id = 0;  // Video track

    // Write frame to mdat
    if (!writeFrameToMdat(data, size)) {
        MCSR_LOG(ERROR) << "Failed to write video frame to mdat";
        return false;
    }

    if (!logFrameToIndex(frame)) {
        MCSR_LOG(ERROR) << "Failed to log video frame to index";
        return false;
    }

    mdat_size_ += size;
    frame_count_++;
    frames_since_flush_++;

    // Flush if needed
    if (!flushIfNeeded()) {
        MCSR_LOG(ERROR) << "Failed to flush";
        return false;
    }

    return true;
}

bool Mp4Recorder::writeAudioFrame(const uint8_t* data, uint32_t size, int64_t pts) {
    if (!recording_) {
        MCSR_LOG(ERROR) << "Not recording";
        return false;
    }

    // Write frame to mdat
    if (!writeFrameToMdat(data, size)) {
        MCSR_LOG(ERROR) << "Failed to write audio frame to mdat";
        return false;
    }

    // Log frame info
    FrameInfo frame;
    frame.offset = mdat_size_;
    frame.size = size;
    frame.pts = pts;
    frame.dts = pts;
    frame.is_keyframe = 1;  // Audio frames are always "keyframes"
    frame.track_id = 1;     // Audio track

    if (!logFrameToIndex(frame)) {
        MCSR_LOG(ERROR) << "Failed to log audio frame to index";
        return false;
    }

    mdat_size_ += size;
    frame_count_++;
    frames_since_flush_++;

    // Flush if needed
    if (!flushIfNeeded()) {
        MCSR_LOG(ERROR) << "Failed to flush";
        return false;
    }

    return true;
}

bool Mp4Recorder::stop() {
    if (!recording_) {
        MCSR_LOG(ERROR) << "Not recording";
        return false;
    }

     recording_ = false;
     
     // Flush mp4 file before writing moov
     if (mp4_file_) {
         mp4_file_->flush();
     }
     
      // Update mdat box size (mdat_start_ - 8 is the position of mdat size field)
      // mdat size = 8 (header) + mdat_size_ (data)
      uint32_t mdat_total_size = 8 + mdat_size_;
      MCSR_LOG(INFO) << "Updating mdat size: mdat_size_=" << mdat_size_ << ", mdat_total_size=" << mdat_total_size << ", mdat_start_=" << mdat_start_;
      if (mp4_file_) {
          mp4_file_->seek(static_cast<int64_t>(mdat_start_ - 8), SEEK_SET);
          uint8_t size_bytes[4] = {
              (uint8_t)((mdat_total_size >> 24) & 0xFF),
              (uint8_t)((mdat_total_size >> 16) & 0xFF),
              (uint8_t)((mdat_total_size >> 8) & 0xFF),
              (uint8_t)(mdat_total_size & 0xFF)
          };
          mp4_file_->write(size_bytes, 4);
          mp4_file_->flush();
          mp4_file_->close();  // Close mp4_file_ before buildAndWriteMoov
          mp4_file_.reset();
      }

     // Build and write moov
     if (!buildAndWriteMoov()) {
         MCSR_LOG(ERROR) << "Failed to build and write moov";
         return false;
     }

     // Close remaining files
      if (idx_file_) {
          idx_file_->close();
          idx_file_.reset();
      }
      if (lock_file_) {
          lock_file_->close();
          lock_file_.reset();
      }

    // Cleanup
    if (!cleanupFiles()) {
        MCSR_LOG(ERROR) << "Failed to cleanup files";
        return false;
    }

    MCSR_LOG(INFO) << "Recording stopped: " << mp4_filename_;
    return true;
}

bool Mp4Recorder::hasIncompleteRecording(const std::string& filename) {
    std::string lock_file = filename + ".lock";
    std::string idx_file = filename + ".idx";

    StdioFileOps ops;
    return ops.exists(lock_file) && ops.exists(idx_file);
}

bool Mp4Recorder::recover(const std::string& filename) {
    MCSR_LOG(INFO) << "Recovering from incomplete recording: " << filename;

    std::string idx_filename = filename + ".idx";
    std::string lock_filename = filename + ".lock";

    // Read index file
    IndexFile idx(file_ops_);
    if (!idx.open(idx_filename)) {
        MCSR_LOG(ERROR) << "Failed to open index file";
        return false;
    }

    // Read config from index file
    RecorderConfig recovery_config;
    if (!idx.readConfig(recovery_config)) {
        MCSR_LOG(ERROR) << "Failed to read config from index file";
        return false;
    }
    
    MCSR_LOG(INFO) << "Recovery: config read from index (timescale=" << recovery_config.video_timescale << ", resolution=" << recovery_config.video_width << "x" << recovery_config.video_height << ")";

    std::vector<FrameInfo> video_frames, audio_frames;
    if (!idx.readAllFrames(video_frames, audio_frames)) {
        MCSR_LOG(ERROR) << "Failed to read frames from index";
        return false;
    }

    MCSR_LOG(INFO) << "Recovery: read " << video_frames.size() << " video frames, " << audio_frames.size() << " audio frames";
    
    // Close index file before attempting to delete it
    idx.close();

    // Calculate mdat_start from file structure
    // ftyp box is 32 bytes, mdat header is 8 bytes, so mdat data starts at offset 40
    uint64_t mdat_start = 40;
    MCSR_LOG(INFO) << "Recovery: mdat_start=" << mdat_start;
    
    // Calculate actual mdat size from frame data
    uint64_t mdat_size = 0;
    for (const auto& frame : video_frames) {
        uint64_t frame_end = frame.offset + frame.size;
        if (frame_end > mdat_size) {
            mdat_size = frame_end;
        }
    }
    for (const auto& frame : audio_frames) {
        uint64_t frame_end = frame.offset + frame.size;
        if (frame_end > mdat_size) {
            mdat_size = frame_end;
        }
    }
    
    MCSR_LOG(INFO) << "Recovery: calculated mdat_size=" << mdat_size;
    
    // Update mdat box size in the MP4 file
    // mdat box header is at offset 32 (after ftyp), size field is first 4 bytes
    std::unique_ptr<IFile> mp4_file = file_ops_->open(filename, "r+b");
    if (!mp4_file || !mp4_file->isOpen()) {
        MCSR_LOG(ERROR) << "Failed to open MP4 file for updating mdat size";
        return false;
    }

    uint64_t file_size = 0;
    if (!file_ops_->getFileSize(filename, file_size)) {
        MCSR_LOG(ERROR) << "Failed to read MP4 file size";
        return false;
    }
    if (file_size < 40) {
        MCSR_LOG(ERROR) << "MP4 file too small to contain ftyp+mdat header";
        return false;
    }
    
    // Seek to mdat size field (offset 32)
    if (!mp4_file->seek(32, SEEK_SET)) {
        MCSR_LOG(ERROR) << "Failed to seek MP4 file to mdat size field";
        return false;
    }
    
    // Write mdat total size based on actual file size before moov is appended
    uint64_t mdat_total_size_64 = file_size - 32;
    if (mdat_total_size_64 > 0xFFFFFFFFULL) {
        MCSR_LOG(ERROR) << "mdat size exceeds 32-bit limit";
        return false;
    }

    uint32_t mdat_total_size = static_cast<uint32_t>(mdat_total_size_64);
    uint8_t size_bytes[4] = {
        (uint8_t)((mdat_total_size >> 24) & 0xFF),
        (uint8_t)((mdat_total_size >> 16) & 0xFF),
        (uint8_t)((mdat_total_size >> 8) & 0xFF),
        (uint8_t)(mdat_total_size & 0xFF)
    };
    
    if (mp4_file->write(size_bytes, 4) != 4) {
        MCSR_LOG(ERROR) << "Failed to write mdat size";
        return false;
    }
    
    mp4_file->flush();
    
    MCSR_LOG(INFO) << "Recovery: updated mdat size to " << mdat_total_size << " (file_size=" << file_size << ")";

    // Attempt to extract SPS/PPS from mdat to build a valid avcC box
    std::vector<uint8_t> recovered_sps;
    std::vector<uint8_t> recovered_pps;
    if (extractH264ConfigFromMdat(*file_ops_, filename, mdat_start, video_frames, recovered_sps, recovered_pps)) {
        MCSR_LOG(INFO) << "Recovery: extracted SPS/PPS from mdat (SPS=" << recovered_sps.size() << " bytes, PPS=" << recovered_pps.size() << " bytes)";
    } else {
        MCSR_LOG(WARNING) << "Recovery: failed to extract SPS/PPS from mdat; using fallback avcC";
    }

    // Build moov using config from index file
    MoovBuilder builder;
    std::vector<uint8_t> moov_data;
    if (!builder.buildMoov(video_frames, audio_frames, 
                          recovery_config.video_timescale, recovery_config.audio_timescale,
                          recovery_config.audio_sample_rate, recovery_config.audio_channels,
                          recovery_config.video_width, recovery_config.video_height,
                          recovered_sps.empty() ? nullptr : recovered_sps.data(),
                          recovered_sps.size(),
                          recovered_pps.empty() ? nullptr : recovered_pps.data(),
                          recovered_pps.size(),
                          mdat_start,
                          moov_data)) {
        MCSR_LOG(ERROR) << "Failed to build moov";
        return false;
    }

    // Write moov to mp4
    if (!builder.writeMoovToFile(filename, moov_data, file_ops_.get())) {
        MCSR_LOG(ERROR) << "Failed to write moov to mp4";
        return false;
    }

    // Cleanup index and lock files
    if (!file_ops_->remove(idx_filename)) {
        MCSR_LOG(WARNING) << "Failed to delete index file: " << idx_filename;
    } else {
        MCSR_LOG(INFO) << "Deleted index file: " << idx_filename;
    }
    
    if (!file_ops_->remove(lock_filename)) {
        MCSR_LOG(WARNING) << "Failed to delete lock file: " << lock_filename;
    } else {
        MCSR_LOG(INFO) << "Deleted lock file: " << lock_filename;
    }

    MCSR_LOG(INFO) << "Recovery completed successfully";
    return true;
}

bool Mp4Recorder::createFiles(const std::string& filename) {
    // Create mp4 file
    mp4_file_ = file_ops_->open(filename, "wb");
    if (!mp4_file_ || !mp4_file_->isOpen()) {
        MCSR_LOG(ERROR) << "Failed to create mp4 file";
        return false;
    }

    // Write ftyp box (32 bytes for better compatibility)
    // ftyp structure: size(4) + type(4) + major_brand(4) + minor_version(4) + compatible_brands(16)
    uint8_t ftyp[32] = {
        0x00, 0x00, 0x00, 0x20,  // size = 32
        'f', 't', 'y', 'p',       // type
        'i', 's', 'o', 'm',       // major brand (isom for better compatibility)
        0x00, 0x00, 0x02, 0x00,   // minor version
        'i', 's', 'o', 'm',       // compatible brand 1
        'i', 's', 'o', '2',       // compatible brand 2
        'a', 'v', 'c', '1',       // compatible brand 3 (H.264)
        'm', 'p', '4', '1'        // compatible brand 4
    };
    if (mp4_file_->write(ftyp, sizeof(ftyp)) != sizeof(ftyp)) {
        MCSR_LOG(ERROR) << "Failed to write ftyp";
        return false;
    }

    // Write mdat placeholder (size = 0 means until EOF)
    // This will be updated later when we know the actual size
    uint8_t mdat_header[8] = {
        0x00, 0x00, 0x00, 0x00,  // size = 0 (until EOF, will be updated on stop)
        'm', 'd', 'a', 't'        // type
    };
    if (mp4_file_->write(mdat_header, sizeof(mdat_header)) != sizeof(mdat_header)) {
        MCSR_LOG(ERROR) << "Failed to write mdat header";
        return false;
    }

    int64_t mdat_start = mp4_file_->tell();
    if (mdat_start < 0) {
        MCSR_LOG(ERROR) << "Failed to determine mdat start position";
        return false;
    }
    mdat_start_ = static_cast<uint64_t>(mdat_start);
    mdat_size_ = 0;

    // Create index file
    idx_file_ = file_ops_->open(idx_filename_, "wb");
    if (!idx_file_ || !idx_file_->isOpen()) {
        MCSR_LOG(ERROR) << "Failed to create index file";
        return false;
    }

    // Write config header to index file
    // Magic number for validation
    uint32_t magic = 0x4D503452;  // "MP4R" in hex
    if (idx_file_->write(&magic, sizeof(uint32_t)) != sizeof(uint32_t)) {
        MCSR_LOG(ERROR) << "Failed to write magic number to index";
        return false;
    }
    
    // Write config structure
    if (idx_file_->write(&config_, sizeof(RecorderConfig)) != sizeof(RecorderConfig)) {
        MCSR_LOG(ERROR) << "Failed to write config to index";
        return false;
    }
    
    idx_file_->flush();
    MCSR_LOG(INFO) << "Config written to index file";

    // Create lock file
    lock_file_ = file_ops_->open(lock_filename_, "wb");
    if (!lock_file_ || !lock_file_->isOpen()) {
        MCSR_LOG(ERROR) << "Failed to create lock file";
        return false;
    }
    lock_file_->write("RECORDING", 9);
    lock_file_->flush();

    return true;
}

bool Mp4Recorder::writeFrameToMdat(const uint8_t* data, uint32_t size) {
    if (mp4_file_->write(data, size) != size) {
        MCSR_LOG(ERROR) << "Failed to write frame to mdat";
        return false;
    }
    return true;
}

bool Mp4Recorder::logFrameToIndex(const FrameInfo& frame) {
    if (!idx_file_) {
        MCSR_LOG(ERROR) << "Index file not open";
        return false;
    }
    
    if (idx_file_->write(&frame, sizeof(FrameInfo)) != sizeof(FrameInfo)) {
        MCSR_LOG(ERROR) << "Failed to write frame to index";
        return false;
    }
    
    // Also add to in-memory frame list for moov building
     if (frame.track_id == 0) {
         video_frames_.push_back(frame);
         MCSR_LOG(VERBOSE) << "Indexed video frame: pts=" << frame.pts << ", size=" << frame.size << ", offset=" << frame.offset;
     } else if (frame.track_id == 1) {
         audio_frames_.push_back(frame);
     }
    
    return true;
}

bool Mp4Recorder::flushIfNeeded() {
    auto now = std::chrono::steady_clock::now();
    uint64_t elapsed_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_flush_time_).count());

    if (elapsed_ms >= config_.flush_interval_ms || 
        frames_since_flush_ >= config_.flush_frame_count) {
        
        // Flush C library buffers for both files
        if (!mp4_file_->flush()) {
            MCSR_LOG(ERROR) << "Failed to flush mp4 file";
            return false;
        }
        if (!idx_file_->flush()) {
            MCSR_LOG(ERROR) << "Failed to flush idx file";
            return false;
        }
        
        // Sync to disk (CRITICAL for crash safety)
        // This ensures data is written to physical disk
        if (!mp4_file_->sync()) {
            MCSR_LOG(ERROR) << "Failed to sync mp4 file to disk";
            return false;
        }
        if (!idx_file_->sync()) {
            MCSR_LOG(ERROR) << "Failed to sync idx file to disk";
            return false;
        }

        last_flush_time_ = now;
        frames_since_flush_ = 0;
    }

    return true;
}

bool Mp4Recorder::buildAndWriteMoov() {
    // Build moov from collected frame info
    MCSR_LOG(INFO) << "Building moov box with " << video_frames_.size() << " video frames and " << audio_frames_.size() << " audio frames";
    
    // Log first and last frame info for debugging
    if (!video_frames_.empty()) {
        MCSR_LOG(VERBOSE) << "First video frame: pts=" << video_frames_[0].pts << ", size=" << video_frames_[0].size << ", keyframe=" << video_frames_[0].is_keyframe << ", offset=" << video_frames_[0].offset;
        MCSR_LOG(VERBOSE) << "Last video frame: pts=" << video_frames_.back().pts << ", size=" << video_frames_.back().size << ", keyframe=" << video_frames_.back().is_keyframe << ", offset=" << video_frames_.back().offset;
    }
    
    MoovBuilder builder;
    std::vector<uint8_t> moov_data;
    
    if (!builder.buildMoov(video_frames_, audio_frames_,
                          config_.video_timescale, config_.audio_timescale,
                          config_.audio_sample_rate, config_.audio_channels,
                          config_.video_width, config_.video_height,
                          h264_sps_.empty() ? nullptr : h264_sps_.data(),
                          h264_sps_.size(),
                          h264_pps_.empty() ? nullptr : h264_pps_.data(),
                          h264_pps_.size(),
                          mdat_start_,
                          moov_data)) {
        MCSR_LOG(ERROR) << "Failed to build moov box";
        return false;
    }
    
    MCSR_LOG(INFO) << "Moov box built, size: " << moov_data.size() << " bytes";
    
    // Write moov to file
    if (!builder.writeMoovToFile(mp4_filename_, moov_data, file_ops_.get())) {
        MCSR_LOG(ERROR) << "Failed to write moov to file";
        return false;
    }
    
    MCSR_LOG(INFO) << "Moov box written successfully";
    return true;
}

bool Mp4Recorder::cleanupFiles() {
    // Remove index and lock files
    file_ops_->remove(idx_filename_);
    file_ops_->remove(lock_filename_);
    return true;
}

} // namespace mp4_recorder
