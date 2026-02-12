/*
 * MP4 Crash-Safe Recorder - Moov Builder
 * 
 * Builds moov box from frame index
 * 
 * License: GPL v2+
 */

#ifndef MOOV_BUILDER_H
#define MOOV_BUILDER_H

#include <cstdint>
#include <vector>
#include <string>
#include "common.h"
#include "file_ops.h"

namespace mp4_recorder {

// Forward declaration
struct FrameInfo;

// Moov box builder
class MoovBuilder {
public:
    MoovBuilder();
    ~MoovBuilder();

    // Build moov box from frame information
    bool buildMoov(
        const std::vector<FrameInfo>& video_frames,
        const std::vector<FrameInfo>& audio_frames,
        uint32_t video_timescale,
        uint32_t audio_timescale,
        uint32_t audio_sample_rate,
        uint16_t audio_channels,
        uint32_t video_width,
        uint32_t video_height,
        const uint8_t* h264_sps,
        uint32_t h264_sps_size,
        const uint8_t* h264_pps,
        uint32_t h264_pps_size,
        uint64_t mdat_start,
        std::vector<uint8_t>& moov_data
    );

    // Write moov box to file
    bool writeMoovToFile(const std::string& filename, const std::vector<uint8_t>& moov_data,
                         IFileOps* file_ops = nullptr);

private:
    // Helper methods for building atoms
    bool buildFtyp(std::vector<uint8_t>& data);
    bool buildMvhd(uint32_t duration, std::vector<uint8_t>& data);
    bool buildTrak(
        const std::vector<FrameInfo>& frames,
        uint32_t track_id,
        uint32_t timescale,
        const std::string& codec,
        uint32_t video_width,
        uint32_t video_height,
        uint32_t audio_sample_rate,
        uint16_t audio_channels,
        const uint8_t* h264_sps,
        uint32_t h264_sps_size,
        const uint8_t* h264_pps,
        uint32_t h264_pps_size,
        uint64_t mdat_start,
        std::vector<uint8_t>& data
    );
    bool buildStts(const std::vector<FrameInfo>& frames, std::vector<uint8_t>& data,
                   uint32_t default_duration);
    bool buildStss(const std::vector<FrameInfo>& frames, std::vector<uint8_t>& data);
    bool buildStsz(const std::vector<FrameInfo>& frames, std::vector<uint8_t>& data);
    bool buildStco(const std::vector<FrameInfo>& frames, uint64_t mdat_start, std::vector<uint8_t>& data);
    bool buildStsc(const std::vector<FrameInfo>& frames, std::vector<uint8_t>& data);
    bool buildStsd(const std::string& codec, uint32_t width, uint32_t height,
                   uint32_t audio_sample_rate, uint16_t audio_channels,
                   const uint8_t* h264_sps, uint32_t h264_sps_size,
                   const uint8_t* h264_pps, uint32_t h264_pps_size,
                   std::vector<uint8_t>& data);

    // Utility methods
    void writeAtomHeader(std::vector<uint8_t>& data, const char* type, uint32_t size);
    void writeUint32BE(std::vector<uint8_t>& data, uint32_t value);
    void writeUint64BE(std::vector<uint8_t>& data, uint64_t value);
    void writeUint16BE(std::vector<uint8_t>& data, uint16_t value);
    void writeUint8(std::vector<uint8_t>& data, uint8_t value);
    void writeDescriptorLength(std::vector<uint8_t>& data, uint32_t length);
    uint8_t getSampleRateIndex(uint32_t sample_rate) const;
};

} // namespace mp4_recorder

#endif // MOOV_BUILDER_H
