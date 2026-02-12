/*
 * MP4 Crash-Safe Recorder - Index File Handler
 * 
 * Manages frame index file for crash recovery
 * 
 * License: GPL v2+
 */

#ifndef INDEX_FILE_H
#define INDEX_FILE_H

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#include "file_ops.h"

namespace mp4_recorder {

struct FrameInfo;
struct RecorderConfig;

// Index file handler
class IndexFile {
public:
    IndexFile();
    explicit IndexFile(std::shared_ptr<IFileOps> file_ops);
    ~IndexFile();

    // Create and open index file
    bool create(const std::string& filename);

    // Open existing index file
    bool open(const std::string& filename);

    // Write recorder config header (must be called after create, before any frames)
    bool writeConfig(const RecorderConfig& config);

    // Read recorder config header (must be called after open, before reading frames)
    bool readConfig(RecorderConfig& config);

    // Write frame info to index
    bool writeFrame(const FrameInfo& frame);

    // Read all frames from index
    bool readAllFrames(std::vector<FrameInfo>& video_frames, std::vector<FrameInfo>& audio_frames);

    // Flush to disk
    bool flush();

    // Close index file
    void close();

    // Get frame count
    uint64_t getFrameCount() const { return frame_count_; }

    // Check if file exists and is valid
    static bool exists(const std::string& filename);

private:
    std::shared_ptr<IFileOps> file_ops_;
    std::unique_ptr<IFile> file_;
    std::string filename_;
    uint64_t frame_count_ = 0;
    bool dirty_ = false;
};

} // namespace mp4_recorder

#endif // INDEX_FILE_H
