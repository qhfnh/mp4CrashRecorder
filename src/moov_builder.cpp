/*
 * MP4 Crash-Safe Recorder - Moov Builder Implementation
 * 
 * License: GPL v2+
 */

#include "moov_builder.h"
#include "mp4_recorder.h"
#include "common.h"
#include <cstring>
#include <algorithm>

namespace mp4_recorder {

MoovBuilder::MoovBuilder() {
}

MoovBuilder::~MoovBuilder() {
}

bool MoovBuilder::buildMoov(
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
    std::vector<uint8_t>& moov_data) {
    
     moov_data.clear();
     
      // Calculate total duration
      uint32_t video_duration = 0;
      if (!video_frames.empty()) {
          // Duration is already in video_timescale units
          // For mvhd, we need to convert to mvhd timescale (1000)
          // mvhd_duration = video_duration * (mvhd_timescale / video_timescale)
          // mvhd_duration = video_duration * (1000 / video_timescale)
          video_duration = (video_frames.back().pts * 1000) / video_timescale;
          MCSR_LOG(INFO) << "Video duration calculation: pts=" << video_frames.back().pts << ", timescale=" << video_timescale << ", mvhd_duration=" << video_duration;
      }
    
    // Build mvhd
    std::vector<uint8_t> mvhd_data;
    if (!buildMvhd(video_duration, mvhd_data)) {
        MCSR_LOG(ERROR) << "Failed to build mvhd";
        return false;
    }
    MCSR_LOG(INFO) << "mvhd built, size: " << mvhd_data.size();
    
    // Build video track
    std::vector<uint8_t> video_trak;
    if (!video_frames.empty()) {
        if (!buildTrak(video_frames, 1, video_timescale, "avc1", video_width, video_height,
                      0, 0,
                      h264_sps, h264_sps_size, h264_pps, h264_pps_size, mdat_start, video_trak)) {
            MCSR_LOG(ERROR) << "Failed to build video trak";
            return false;
        }
        MCSR_LOG(INFO) << "video trak built, size: " << video_trak.size();
    }
    
    // Build audio track
    std::vector<uint8_t> audio_trak;
    if (!audio_frames.empty()) {
        if (!buildTrak(audio_frames, 2, audio_timescale, "mp4a", 0, 0,
                      audio_sample_rate, audio_channels,
                      nullptr, 0, nullptr, 0, mdat_start, audio_trak)) {
            MCSR_LOG(ERROR) << "Failed to build audio trak";
            return false;
        }
        MCSR_LOG(INFO) << "audio trak built, size: " << audio_trak.size();
    }
    
    // Combine into moov
    uint32_t moov_size = 8 + mvhd_data.size() + video_trak.size() + audio_trak.size();
    MCSR_LOG(INFO) << "moov_size calculated: " << moov_size;
    moov_data.reserve(moov_size);
    writeAtomHeader(moov_data, "moov", moov_size);
    moov_data.insert(moov_data.end(), mvhd_data.begin(), mvhd_data.end());
    moov_data.insert(moov_data.end(), video_trak.begin(), video_trak.end());
    moov_data.insert(moov_data.end(), audio_trak.begin(), audio_trak.end());
    
    MCSR_LOG(INFO) << "moov_data final size: " << moov_data.size();
    return true;
}

bool MoovBuilder::writeMoovToFile(const std::string& filename, const std::vector<uint8_t>& moov_data,
                                  IFileOps* file_ops) {
    MCSR_LOG(INFO) << "Opening file for writing moov: " << filename;
    MCSR_LOG(INFO) << "moov_data first 20 bytes (hex):";
    std::string hex_str;
    for (int i = 0; i < std::min(20, (int)moov_data.size()); i++) {
        char buf[4];
        sprintf(buf, "%02x ", moov_data[i]);
        hex_str += buf;
    }
    MCSR_LOG(INFO) << "  " << hex_str;
    
    StdioFileOps default_ops;
    IFileOps* ops = file_ops ? file_ops : &default_ops;
    std::unique_ptr<IFile> file = ops->open(filename, "ab");
    if (!file || !file->isOpen()) {
        MCSR_LOG(ERROR) << "Failed to open file for writing moov";
        return false;
    }
    
    // Seek to end of file to ensure we're appending
    if (!file->seek(0, SEEK_END)) {
        MCSR_LOG(ERROR) << "Failed to seek to end of file";
        return false;
    }
    int64_t file_pos = file->tell();
    MCSR_LOG(INFO) << "File position before write: " << file_pos;
    
    MCSR_LOG(INFO) << "Writing " << moov_data.size() << " bytes to file";
    size_t written = file->write(moov_data.data(), moov_data.size());
    MCSR_LOG(INFO) << "Actually wrote " << written << " bytes";
    
    // Flush to ensure data is written
    file->flush();
    
    file_pos = file->tell();
    MCSR_LOG(INFO) << "File position after write: " << file_pos;
    
    if (written != moov_data.size()) {
        MCSR_LOG(ERROR) << "Failed to write all moov data. Expected: " << moov_data.size() << ", Written: " << written;
        return false;
    }

    file->close();
    MCSR_LOG(INFO) << "Moov data written successfully";
    return true;
}

bool MoovBuilder::buildMvhd(uint32_t duration, std::vector<uint8_t>& data) {
    data.clear();
    
    // mvhd box (version 0)
    // Size: 8 (header) + 4 (version/flags) + 4 (creation) + 4 (modification) + 4 (timescale) +
    //       4 (duration) + 4 (playback speed) + 2 (volume) + 2 (reserved) + 8 (reserved) +
    //       36 (matrix) + 24 (pre-defined) + 4 (next track ID) = 108
    uint32_t size = 108;  // mvhd size
    writeAtomHeader(data, "mvhd", size);
    
    // Version and flags (combined into single 4-byte field)
    writeUint32BE(data, 0);  // version 0 + flags 0
    
    // Creation time
    writeUint32BE(data, 0);
    
    // Modification time
    writeUint32BE(data, 0);
    
    // Timescale
    writeUint32BE(data, 1000);
    
    // Duration
    writeUint32BE(data, duration);
    
    // Playback speed
    writeUint32BE(data, 0x00010000);  // 1.0
    
    // Volume
    writeUint16BE(data, 0x0100);  // 1.0
    
    // Reserved (2 + 8 bytes)
    writeUint16BE(data, 0);
    for (int i = 0; i < 8; i++) {
        writeUint8(data, 0);
    }
    
    // Matrix
    for (int i = 0; i < 9; i++) {
        if (i == 0 || i == 4 || i == 8) {
            writeUint32BE(data, 0x00010000);
        } else {
            writeUint32BE(data, 0);
        }
    }
    
    // Pre-defined (6 x 32-bit)
    for (int i = 0; i < 6; i++) {
        writeUint32BE(data, 0);
    }
    
    // Next track ID
    writeUint32BE(data, 3);
    
    return true;
}

bool MoovBuilder::buildTrak(
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
    std::vector<uint8_t>& data) {
    
    data.clear();
    
      // Build tkhd (version 0)
      std::vector<uint8_t> tkhd_data;
      // tkhd size: 8 (header) + 4 (version/flags) + 4 (creation) + 4 (modification) + 4 (track_id) +
      //            4 (reserved) + 4 (duration) + 8 (reserved) + 2 (layer) + 2 (alternate group) +
      //            2 (volume) + 2 (reserved) + 36 (matrix) + 4 (width) + 4 (height) = 92
      uint32_t tkhd_size = 92;
     writeAtomHeader(tkhd_data, "tkhd", tkhd_size);
     writeUint32BE(tkhd_data, 0x0000000F);  // version 0 + flags (track enabled + in movie + in preview)
     writeUint32BE(tkhd_data, 0);  // creation time
     writeUint32BE(tkhd_data, 0);  // modification time
     writeUint32BE(tkhd_data, track_id);  // track ID
      writeUint32BE(tkhd_data, 0);  // reserved
      // Duration in tkhd should be in mvhd timescale (1000), not video timescale
      uint32_t tkhd_duration = (frames.back().pts * 1000) / timescale;
      writeUint32BE(tkhd_data, tkhd_duration);  // duration
      // Reserved (8 bytes)
      writeUint32BE(tkhd_data, 0);
      writeUint32BE(tkhd_data, 0);
      
      // Layer and alternate group
      writeUint16BE(tkhd_data, 0);
      writeUint16BE(tkhd_data, 0);
      
      // Volume (audio tracks use 1.0, video uses 0)
      uint16_t volume = (codec == "avc1") ? 0 : 0x0100;
      writeUint16BE(tkhd_data, volume);
      
      // Reserved
      writeUint16BE(tkhd_data, 0);
      
      // Matrix
      for (int i = 0; i < 9; i++) {
          if (i == 0 || i == 4 || i == 8) {
              writeUint32BE(tkhd_data, 0x00010000);
          } else {
              writeUint32BE(tkhd_data, 0);
          }
      }
      
      // Width and height in fixed-point 16.16 format
      if (codec == "avc1" && video_width > 0 && video_height > 0) {
          writeUint32BE(tkhd_data, video_width << 16);   // width in fixed-point
          writeUint32BE(tkhd_data, video_height << 16);  // height in fixed-point
      } else {
          writeUint32BE(tkhd_data, 0x00010000);  // width (default 1.0)
          writeUint32BE(tkhd_data, 0x00010000);  // height (default 1.0)
      }
    
    // Build mdia
    std::vector<uint8_t> mdia_data;
    
    // Build mdhd
    std::vector<uint8_t> mdhd_data;
    uint32_t mdhd_size = 32;
    writeAtomHeader(mdhd_data, "mdhd", mdhd_size);
    writeUint32BE(mdhd_data, 0);  // version 0 + flags 0
    writeUint32BE(mdhd_data, 0);  // creation time
    writeUint32BE(mdhd_data, 0);  // modification time
    writeUint32BE(mdhd_data, timescale);  // timescale
    writeUint32BE(mdhd_data, frames.back().pts);  // duration
    writeUint16BE(mdhd_data, 0x55C4);  // language
    writeUint16BE(mdhd_data, 0);  // quality
    
    mdia_data.insert(mdia_data.end(), mdhd_data.begin(), mdhd_data.end());
    
    // Build hdlr
    std::vector<uint8_t> hdlr_data;
    std::string handler_type = (codec == "avc1") ? "vide" : "soun";
    // hdlr size: 8 (header) + 4 (version/flags) + 4 (pre_defined) + 4 (handler_type) + 48 (reserved) = 68
    uint32_t hdlr_size = 68;
    writeAtomHeader(hdlr_data, "hdlr", hdlr_size);
    writeUint32BE(hdlr_data, 0);  // version 0 + flags 0
    writeUint32BE(hdlr_data, 0);  // pre_defined
    for (int i = 0; i < 4; i++) writeUint8(hdlr_data, handler_type[i]);
    for (int i = 0; i < 12; i++) writeUint32BE(hdlr_data, 0);  // reserved (48 bytes)
    
    mdia_data.insert(mdia_data.end(), hdlr_data.begin(), hdlr_data.end());
    
    // Build minf (Media Information Box)
    std::vector<uint8_t> minf_data;
    
    // Build vmhd (Video Media Header) or smhd (Sound Media Header)
    std::vector<uint8_t> media_header;
    if (codec == "avc1") {
        // Video media header
        // vmhd size: 8 (header) + 4 (version/flags) + 2 (graphics mode) + 6 (opcolor) = 20
        uint32_t vmhd_size = 20;
        writeAtomHeader(media_header, "vmhd", vmhd_size);
        writeUint32BE(media_header, 0);  // version 0 + flags 0
        writeUint16BE(media_header, 0);  // graphics mode
        for (int i = 0; i < 3; i++) writeUint16BE(media_header, 0);  // opcolor
     } else {
         // Audio media header
         uint32_t smhd_size = 16;
         writeAtomHeader(media_header, "smhd", smhd_size);
         writeUint32BE(media_header, 0);  // version 0 + flags 0 (combined into single 4-byte field)
         writeUint16BE(media_header, 0);  // balance
         writeUint16BE(media_header, 0);  // reserved
     }
    minf_data.insert(minf_data.end(), media_header.begin(), media_header.end());
    
    // Build dinf (Data Information Box)
    std::vector<uint8_t> dinf_data;
    std::vector<uint8_t> dref_data;
    writeUint32BE(dref_data, 0);  // version 0 + flags 0
    writeUint32BE(dref_data, 1);  // entry count
    // url box
    writeUint32BE(dref_data, 12);  // size
    writeUint8(dref_data, 'u');
    writeUint8(dref_data, 'r');
    writeUint8(dref_data, 'l');
    writeUint8(dref_data, ' ');
    writeUint32BE(dref_data, 0x00000001);  // flags (self-contained)
    
    uint32_t dref_size = 8 + dref_data.size();
    std::vector<uint8_t> dref_box;
    writeAtomHeader(dref_box, "dref", dref_size);
    dref_box.insert(dref_box.end(), dref_data.begin(), dref_data.end());
    
    uint32_t dinf_size = 8 + dref_box.size();
    std::vector<uint8_t> dinf_box;
    writeAtomHeader(dinf_box, "dinf", dinf_size);
    dinf_box.insert(dinf_box.end(), dref_box.begin(), dref_box.end());
    minf_data.insert(minf_data.end(), dinf_box.begin(), dinf_box.end());
    
    // Build stbl (Sample Table Box)
    std::vector<uint8_t> stbl_data;
    
    // Build stsd (Sample Description)
    std::vector<uint8_t> stsd_data;
    if (!buildStsd(codec, video_width, video_height,
                   audio_sample_rate, audio_channels,
                   h264_sps, h264_sps_size, h264_pps, h264_pps_size, stsd_data)) {
        return false;
    }
    MCSR_LOG(INFO) << "stsd_data size: " << stsd_data.size();
    stbl_data.insert(stbl_data.end(), stsd_data.begin(), stsd_data.end());
    MCSR_LOG(INFO) << "After adding stsd, stbl_data.size() = " << stbl_data.size();
    
    // Build stts
    std::vector<uint8_t> stts_data;
    // Default duration used for the final sample when there is no next PTS.
    uint32_t stts_default_duration = 1;
    if (codec == "mp4a") {
        // AAC-LC uses 1024 samples per frame at the audio timescale.
        stts_default_duration = 1024;
    } else if (timescale >= 30) {
        // Approximate 30fps for video to avoid zero or invalid duration.
        stts_default_duration = timescale / 30;
    }
    if (!buildStts(frames, stts_data, stts_default_duration)) {
        return false;
    }
    MCSR_LOG(INFO) << "stts_data size: " << stts_data.size();
    stbl_data.insert(stbl_data.end(), stts_data.begin(), stts_data.end());
    MCSR_LOG(INFO) << "After adding stts, stbl_data.size() = " << stbl_data.size();
    
    // Build stss (only for video with keyframes)
    if (codec == "avc1") {
        std::vector<uint8_t> stss_data;
        if (!buildStss(frames, stss_data)) {
            return false;
        }
        MCSR_LOG(INFO) << "stss_data size: " << stss_data.size();
        stbl_data.insert(stbl_data.end(), stss_data.begin(), stss_data.end());
        MCSR_LOG(INFO) << "After adding stss, stbl_data.size() = " << stbl_data.size();
    }
    
    // Build stsz
    std::vector<uint8_t> stsz_data;
    if (!buildStsz(frames, stsz_data)) {
        return false;
    }
    MCSR_LOG(INFO) << "stsz_data size: " << stsz_data.size();
    stbl_data.insert(stbl_data.end(), stsz_data.begin(), stsz_data.end());
    MCSR_LOG(INFO) << "After adding stsz, stbl_data.size() = " << stbl_data.size();
    
    // Build stco
    std::vector<uint8_t> stco_data;
    if (!buildStco(frames, mdat_start, stco_data)) {
        return false;
    }
    MCSR_LOG(INFO) << "stco_data size: " << stco_data.size();
    stbl_data.insert(stbl_data.end(), stco_data.begin(), stco_data.end());
    MCSR_LOG(INFO) << "After adding stco, stbl_data.size() = " << stbl_data.size();
    
    // Build stsc
    std::vector<uint8_t> stsc_data;
    if (!buildStsc(frames, stsc_data)) {
        return false;
    }
    MCSR_LOG(INFO) << "stsc_data size: " << stsc_data.size();
    stbl_data.insert(stbl_data.end(), stsc_data.begin(), stsc_data.end());
    MCSR_LOG(INFO) << "After adding stsc, stbl_data.size() = " << stbl_data.size();
    
    // Combine stbl
    uint32_t stbl_size = 8 + stbl_data.size();
    MCSR_LOG(INFO) << "stbl size calculation: 8 + " << stbl_data.size() << " = " << stbl_size;
    std::vector<uint8_t> stbl_box;
    writeAtomHeader(stbl_box, "stbl", stbl_size);
    MCSR_LOG(INFO) << "After writeAtomHeader, stbl_box.size() = " << stbl_box.size();
    stbl_box.insert(stbl_box.end(), stbl_data.begin(), stbl_data.end());
    MCSR_LOG(INFO) << "After insert, stbl_box.size() = " << stbl_box.size();
    minf_data.insert(minf_data.end(), stbl_box.begin(), stbl_box.end());
    MCSR_LOG(INFO) << "After adding stbl_box to minf_data, minf_data.size() = " << minf_data.size();
    
    // Combine minf
    uint32_t minf_size = 8 + minf_data.size();
    MCSR_LOG(INFO) << "minf size calculation: 8 + " << minf_data.size() << " = " << minf_size;
    std::vector<uint8_t> minf_box;
    writeAtomHeader(minf_box, "minf", minf_size);
    minf_box.insert(minf_box.end(), minf_data.begin(), minf_data.end());
    
    // Combine mdia
    uint32_t mdia_size = 8 + mdhd_data.size() + hdlr_data.size() + minf_box.size();
    MCSR_LOG(INFO) << "mdia size calculation: 8 + " << mdhd_data.size() << " + " << hdlr_data.size() << " + " << minf_box.size() << " = " << mdia_size;
    std::vector<uint8_t> mdia_box;
    MCSR_LOG(INFO) << "Before writeAtomHeader, mdia_box.size() = " << mdia_box.size();
    writeAtomHeader(mdia_box, "mdia", mdia_size);
    MCSR_LOG(INFO) << "After writeAtomHeader, mdia_box.size() = " << mdia_box.size() << ", mdia_size = " << mdia_size;
    mdia_box.insert(mdia_box.end(), mdhd_data.begin(), mdhd_data.end());
    MCSR_LOG(INFO) << "After adding mdhd, mdia_box.size() = " << mdia_box.size();
    mdia_box.insert(mdia_box.end(), hdlr_data.begin(), hdlr_data.end());
    MCSR_LOG(INFO) << "After adding hdlr, mdia_box.size() = " << mdia_box.size();
    mdia_box.insert(mdia_box.end(), minf_box.begin(), minf_box.end());
    MCSR_LOG(INFO) << "After adding minf_box, mdia_box.size() = " << mdia_box.size();
    
    // Combine into trak
    uint32_t trak_size = 8 + tkhd_data.size() + mdia_box.size();
    MCSR_LOG(INFO) << "trak size calculation: 8 + " << tkhd_data.size() << " + " << mdia_box.size() << " = " << trak_size;
    MCSR_LOG(INFO) << "Before writeAtomHeader, data.size() = " << data.size();
    writeAtomHeader(data, "trak", trak_size);
    MCSR_LOG(INFO) << "After writeAtomHeader, data.size() = " << data.size();
    data.insert(data.end(), tkhd_data.begin(), tkhd_data.end());
    MCSR_LOG(INFO) << "After adding tkhd, data.size() = " << data.size();
    data.insert(data.end(), mdia_box.begin(), mdia_box.end());
    MCSR_LOG(INFO) << "After adding mdia_box, data.size() = " << data.size();
    
    return true;
}

void MoovBuilder::writeAtomHeader(std::vector<uint8_t>& data, const char* type, uint32_t size) {
    writeUint32BE(data, size);
    for (int i = 0; i < 4; i++) {
        writeUint8(data, type[i]);
    }
}

void MoovBuilder::writeUint32BE(std::vector<uint8_t>& data, uint32_t value) {
    data.push_back((value >> 24) & 0xFF);
    data.push_back((value >> 16) & 0xFF);
    data.push_back((value >> 8) & 0xFF);
    data.push_back(value & 0xFF);
}

void MoovBuilder::writeUint64BE(std::vector<uint8_t>& data, uint64_t value) {
    writeUint32BE(data, (uint32_t)(value >> 32));
    writeUint32BE(data, (uint32_t)value);
}

void MoovBuilder::writeUint16BE(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back((value >> 8) & 0xFF);
    data.push_back(value & 0xFF);
}

void MoovBuilder::writeUint8(std::vector<uint8_t>& data, uint8_t value) {
    data.push_back(value);
}

void MoovBuilder::writeDescriptorLength(std::vector<uint8_t>& data, uint32_t length) {
    uint32_t value = length & 0x0FFFFFFF;
    uint8_t bytes[4];
    bytes[0] = (value >> 21) & 0x7F;
    bytes[1] = (value >> 14) & 0x7F;
    bytes[2] = (value >> 7) & 0x7F;
    bytes[3] = value & 0x7F;

    int first = 0;
    while (first < 3 && bytes[first] == 0) {
        first++;
    }

    for (int i = first; i < 4; i++) {
        uint8_t out = bytes[i];
        if (i < 3) {
            out |= 0x80;
        }
        data.push_back(out);
    }
}

uint8_t MoovBuilder::getSampleRateIndex(uint32_t sample_rate) const {
    switch (sample_rate) {
        case 96000: return 0;
        case 88200: return 1;
        case 64000: return 2;
        case 48000: return 3;
        case 44100: return 4;
        case 32000: return 5;
        case 24000: return 6;
        case 22050: return 7;
        case 16000: return 8;
        case 12000: return 9;
        case 11025: return 10;
        case 8000: return 11;
        case 7350: return 12;
        default: return 3;
    }
}

bool MoovBuilder::buildStts(const std::vector<FrameInfo>& frames, std::vector<uint8_t>& data,
                            uint32_t default_duration) {
    // Decoding Time to Sample Box
    data.clear();
    
    if (frames.empty()) {
        return false;
    }
    
    // Count sample groups with same duration
    std::vector<std::pair<uint32_t, uint32_t>> stts_entries;  // (count, duration)
    uint32_t current_duration = 0;
    uint32_t current_count = 0;
    
    // Calculate default duration for last frame (use previous frame's duration)
    if (frames.size() >= 2) {
        default_duration = frames[frames.size() - 1].pts - frames[frames.size() - 2].pts;
    }
    
    for (size_t i = 0; i < frames.size(); i++) {
        uint32_t duration = (i + 1 < frames.size()) ?
            (frames[i + 1].pts - frames[i].pts) : default_duration;
        
        if (duration == current_duration || current_count == 0) {
            current_duration = duration;
            current_count++;
        } else {
            stts_entries.push_back({current_count, current_duration});
            current_duration = duration;
            current_count = 1;
        }
    }
    if (current_count > 0) {
        stts_entries.push_back({current_count, current_duration});
    }
    
    // Build stts box
    uint32_t stts_size = 8 + 8 + (stts_entries.size() * 8);
    writeAtomHeader(data, "stts", stts_size);
    writeUint32BE(data, 0);  // version 0 + flags 0
    writeUint32BE(data, stts_entries.size());  // entry count
    
    for (const auto& entry : stts_entries) {
        writeUint32BE(data, entry.first);   // sample count
        writeUint32BE(data, entry.second);  // sample duration
    }
    
    return true;
}

bool MoovBuilder::buildStss(const std::vector<FrameInfo>& frames, std::vector<uint8_t>& data) {
    // Sync Sample Box (Keyframes)
    data.clear();
    
    // Find keyframe indices
    std::vector<uint32_t> keyframe_indices;
    for (size_t i = 0; i < frames.size(); i++) {
        if (frames[i].is_keyframe) {
            keyframe_indices.push_back(i + 1);  // 1-based index
        }
    }
    
    // Build stss box
    uint32_t stss_size = 8 + 8 + (keyframe_indices.size() * 4);
    writeAtomHeader(data, "stss", stss_size);
    writeUint32BE(data, 0);  // version 0 + flags 0
    writeUint32BE(data, keyframe_indices.size());  // entry count
    
    for (uint32_t idx : keyframe_indices) {
        writeUint32BE(data, idx);
    }
    
    return true;
}

bool MoovBuilder::buildStsz(const std::vector<FrameInfo>& frames, std::vector<uint8_t>& data) {
    // Sample Size Box
    data.clear();
    
    if (frames.empty()) {
        return false;
    }
    
    // Build stsz box
    uint32_t stsz_size = 8 + 8 + 4 + (frames.size() * 4);
    writeAtomHeader(data, "stsz", stsz_size);
    writeUint32BE(data, 0);  // version 0 + flags 0
    writeUint32BE(data, 0);  // sample size (0 = variable)
    writeUint32BE(data, frames.size());  // sample count
    
    for (const auto& frame : frames) {
        writeUint32BE(data, frame.size);
    }
    
    return true;
}

bool MoovBuilder::buildStco(const std::vector<FrameInfo>& frames, uint64_t mdat_start, std::vector<uint8_t>& data) {
    // Chunk Offset Box
    data.clear();
    
    if (frames.empty()) {
        return false;
    }
    
    // Each frame is a separate chunk
    // Offset = mdat_start + frame offset (relative to mdat data start)
    MCSR_LOG(INFO) << "buildStco: mdat_start=" << mdat_start;
    
    // Build stco box
    uint32_t stco_size = 8 + 8 + (frames.size() * 4);
    writeAtomHeader(data, "stco", stco_size);
    writeUint32BE(data, 0);  // version 0 + flags 0
    writeUint32BE(data, frames.size());  // entry count (one chunk per frame)
    
    for (const auto& frame : frames) {
        // Calculate absolute offset: mdat_start + frame offset within mdat
        uint64_t chunk_offset_64 = mdat_start + frame.offset;
        
        // Check for 32-bit overflow (MP4 standard limitation)
        if (chunk_offset_64 > 0xFFFFFFFFULL) {
            MCSR_LOG(ERROR) << "Chunk offset overflow: " << chunk_offset_64 << " exceeds 32-bit limit";
            return false;
        }
        
        uint32_t chunk_offset = (uint32_t)chunk_offset_64;
        MCSR_LOG(VERBOSE) << "Frame offset: " << frame.offset << " -> chunk_offset: " << chunk_offset;
        writeUint32BE(data, chunk_offset);
    }
    
    return true;
}

bool MoovBuilder::buildStsc(const std::vector<FrameInfo>& frames, std::vector<uint8_t>& data) {
     // Sample to Chunk Box
     // Maps samples to chunks. Since each frame is a separate chunk,
     // we need one entry per chunk.
     data.clear();
     
     if (frames.empty()) {
         return false;
     }
     
     // Build stsc box with one entry (all chunks have 1 sample each)
     // Each chunk contains exactly 1 sample
     uint32_t stsc_size = 8 + 8 + 12;  // 1 entry
     writeAtomHeader(data, "stsc", stsc_size);
     writeUint32BE(data, 0);  // version 0 + flags 0
     writeUint32BE(data, 1);  // entry count (1 entry for all chunks)
     
      // All chunks have 1 sample each
      writeUint32BE(data, 1);  // first chunk (1-based)
      writeUint32BE(data, 1);  // samples per chunk (1 sample per chunk)
      writeUint32BE(data, 1);  // sample description index
      
      return true;
 }
 
bool MoovBuilder::buildStsd(const std::string& codec, uint32_t width, uint32_t height,
                           uint32_t audio_sample_rate, uint16_t audio_channels,
                           const uint8_t* h264_sps, uint32_t h264_sps_size,
                           const uint8_t* h264_pps, uint32_t h264_pps_size,
                           std::vector<uint8_t>& data) {
    // Sample Description Box
    data.clear();
    
    std::vector<uint8_t> stsd_entries;
    
      if (codec == "avc1") {
          // AVC1 (H.264) sample description
          // Use provided SPS/PPS if available, otherwise use defaults
          std::vector<uint8_t> sps_data;
          std::vector<uint8_t> pps_data;
          
           if (h264_sps && h264_sps_size > 0) {
               // Strip NAL start code if present (0x00 0x00 0x00 0x01 or 0x00 0x00 0x01)
               const uint8_t* sps_start = h264_sps;
               uint32_t sps_len = h264_sps_size;
               
               if (h264_sps_size >= 4 && h264_sps[0] == 0x00 && h264_sps[1] == 0x00 && 
                   h264_sps[2] == 0x00 && h264_sps[3] == 0x01) {
                   sps_start = h264_sps + 4;
                   sps_len = h264_sps_size - 4;
                   MCSR_LOG(INFO) << "Stripped 4-byte NAL start code from SPS";
               } else if (h264_sps_size >= 3 && h264_sps[0] == 0x00 && h264_sps[1] == 0x00 && 
                          h264_sps[2] == 0x01) {
                   sps_start = h264_sps + 3;
                   sps_len = h264_sps_size - 3;
                   MCSR_LOG(INFO) << "Stripped 3-byte NAL start code from SPS";
               }
               
               sps_data.assign(sps_start, sps_start + sps_len);
               MCSR_LOG(INFO) << "Using provided SPS, size=" << sps_data.size();
           } else {
               // Fallback SPS for 640x480 H.264 Baseline Profile
               MCSR_LOG(WARNING) << "Using fallback SPS - this may cause playback issues";
               sps_data.push_back(0x42);  // Profile IDC (Baseline)
               sps_data.push_back(0x00);  // Constraint flags
               sps_data.push_back(0x1E);  // Level IDC (3.0)
               sps_data.push_back(0xE1);  // Encoded parameters
               sps_data.push_back(0x00);  // Encoded parameters
               sps_data.push_back(0x00);  // Encoded parameters
               sps_data.push_back(0x00);  // Encoded parameters
           }
           
           if (h264_pps && h264_pps_size > 0) {
               // Strip NAL start code if present
               const uint8_t* pps_start = h264_pps;
               uint32_t pps_len = h264_pps_size;
               
               if (h264_pps_size >= 4 && h264_pps[0] == 0x00 && h264_pps[1] == 0x00 && 
                   h264_pps[2] == 0x00 && h264_pps[3] == 0x01) {
                   pps_start = h264_pps + 4;
                   pps_len = h264_pps_size - 4;
                   MCSR_LOG(INFO) << "Stripped 4-byte NAL start code from PPS";
               } else if (h264_pps_size >= 3 && h264_pps[0] == 0x00 && h264_pps[1] == 0x00 && 
                          h264_pps[2] == 0x01) {
                   pps_start = h264_pps + 3;
                   pps_len = h264_pps_size - 3;
                   MCSR_LOG(INFO) << "Stripped 3-byte NAL start code from PPS";
               }
               
               pps_data.assign(pps_start, pps_start + pps_len);
               MCSR_LOG(INFO) << "Using provided PPS, size=" << pps_data.size();
           } else {
               // Fallback PPS
               MCSR_LOG(WARNING) << "Using fallback PPS - this may cause playback issues";
               pps_data.push_back(0xE1);  // Encoded parameters
               pps_data.push_back(0x00);  // Encoded parameters
           }
        
        // Calculate avcC size first
        // avcC: 8 (header) + 1 (version) + 1 (profile) + 1 (compatibility) + 1 (level) +
        //       1 (reserved + nal_length_size) + 1 (num_sps) + 2 (sps_length) + sps_size +
        //       1 (num_pps) + 2 (pps_length) + pps_size
        uint32_t avcc_size = 8 + 1 + 1 + 1 + 1 + 1 + 1 + 2 + sps_data.size() + 1 + 2 + pps_data.size();
        MCSR_LOG(INFO) << "avcC size calculation: 8+1+1+1+1+1+1+2+" << sps_data.size() << "+1+2+" << pps_data.size() << " = " << avcc_size;
        
        // avc1 size: 4 (size) + 4 (type) + 6 (reserved) + 2 (ref index) + 2 (version) + 2 (revision) +
        //            4 (vendor) + 4 (temporal) + 4 (spatial) + 2 (width) + 2 (height) +
        //            4 (h-res) + 4 (v-res) + 4 (data size) + 2 (frame count) + 32 (compressor) +
        //            2 (depth) + 2 (color table) + avcc_size
        uint32_t avc1_size = 4 + 4 + 6 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2 + 4 + 4 + 4 + 2 + 32 + 2 + 2 + avcc_size;
        
        writeUint32BE(stsd_entries, avc1_size);
        writeUint8(stsd_entries, 'a');
        writeUint8(stsd_entries, 'v');
        writeUint8(stsd_entries, 'c');
        writeUint8(stsd_entries, '1');
        
        // Reserved
        for (int i = 0; i < 6; i++) writeUint8(stsd_entries, 0);
        
        // Data reference index
        writeUint16BE(stsd_entries, 1);
        
        // Version and revision
        writeUint16BE(stsd_entries, 0);
        writeUint16BE(stsd_entries, 0);
        
        // Vendor
        writeUint32BE(stsd_entries, 0);
        
        // Temporal quality
        writeUint32BE(stsd_entries, 0);
        
        // Spatial quality
        writeUint32BE(stsd_entries, 0);
        
        // Width - USE ACTUAL WIDTH
        writeUint16BE(stsd_entries, width);
        
        // Height - USE ACTUAL HEIGHT
        writeUint16BE(stsd_entries, height);
        
        // Horizontal resolution
        writeUint32BE(stsd_entries, 0x00480000);  // 72 dpi
        
        // Vertical resolution
        writeUint32BE(stsd_entries, 0x00480000);  // 72 dpi
        
        // Data size
        writeUint32BE(stsd_entries, 0);
        
        // Frame count
        writeUint16BE(stsd_entries, 1);
        
        // Compressor name (32 bytes)
        for (int i = 0; i < 32; i++) writeUint8(stsd_entries, 0);
        
        // Depth
        writeUint16BE(stsd_entries, 24);
        
        // Color table ID
        writeUint16BE(stsd_entries, 0xFFFF);
        
         // avcC box - must include SPS and PPS
         writeUint32BE(stsd_entries, avcc_size);
         writeUint8(stsd_entries, 'a');
         writeUint8(stsd_entries, 'v');
         writeUint8(stsd_entries, 'c');
         writeUint8(stsd_entries, 'C');
         writeUint8(stsd_entries, 0x01);  // version
         
         // Extract profile, compatibility, and level from SPS if available
         uint8_t profile = 0x42;  // Default Baseline
         uint8_t compatibility = 0x00;
         uint8_t level = 0x1F;  // Default Level 3.1
         
          if (sps_data.size() >= 4) {
              profile = sps_data[1];
              compatibility = sps_data[2];
              level = sps_data[3];
          }
         
         writeUint8(stsd_entries, profile);  // profile
         writeUint8(stsd_entries, compatibility);  // compatibility
         writeUint8(stsd_entries, level);  // level
         writeUint8(stsd_entries, 0xFF);  // reserved (6 bits) + nal_length_size-1 (2 bits)
        
        // Number of SPS (5 bits reserved + 3 bits count)
        writeUint8(stsd_entries, 0xE1);  // 1 SPS
        
        // SPS length and data
        writeUint16BE(stsd_entries, sps_data.size());
        for (uint8_t b : sps_data) {
            writeUint8(stsd_entries, b);
        }
        
        // Number of PPS
        writeUint8(stsd_entries, 0x01);  // 1 PPS
        
        // PPS length and data
        writeUint16BE(stsd_entries, pps_data.size());
        for (uint8_t b : pps_data) {
            writeUint8(stsd_entries, b);
        }
        
    } else {
        // MP4A (AAC) sample description
        // mp4a box
        uint16_t channel_count = audio_channels > 0 ? audio_channels : 2;
        uint32_t sample_rate = audio_sample_rate > 0 ? audio_sample_rate : 48000;
        uint8_t sample_rate_index = getSampleRateIndex(sample_rate);

        // AudioSpecificConfig (AAC-LC)
        uint8_t audio_object_type = 2;
        uint16_t asc_bits = static_cast<uint16_t>((audio_object_type & 0x1F) << 11) |
                            static_cast<uint16_t>((sample_rate_index & 0x0F) << 7) |
                            static_cast<uint16_t>((channel_count & 0x0F) << 3);
        uint8_t asc_bytes[2] = {
            static_cast<uint8_t>((asc_bits >> 8) & 0xFF),
            static_cast<uint8_t>(asc_bits & 0xFF)
        };

        std::vector<uint8_t> esds_payload;
        writeUint32BE(esds_payload, 0);  // version and flags

        // ES_Descriptor
        esds_payload.push_back(0x03);
        std::vector<uint8_t> es_descriptor;
        writeUint16BE(es_descriptor, 2);  // ES_ID (audio track id)
        writeUint8(es_descriptor, 0x00);  // flags

        // DecoderConfigDescriptor
        std::vector<uint8_t> decoder_config;
        writeUint8(decoder_config, 0x40);  // objectTypeIndication (AAC)
        writeUint8(decoder_config, 0x15);  // streamType (audio) << 2 | 1
        writeUint8(decoder_config, 0x00);  // bufferSizeDB (24-bit)
        writeUint8(decoder_config, 0x00);
        writeUint8(decoder_config, 0x00);
        writeUint32BE(decoder_config, 0x00000000);  // maxBitrate
        writeUint32BE(decoder_config, 0x00000000);  // avgBitrate

        // DecoderSpecificInfo (AudioSpecificConfig)
        decoder_config.push_back(0x05);
        writeDescriptorLength(decoder_config, sizeof(asc_bytes));
        decoder_config.push_back(asc_bytes[0]);
        decoder_config.push_back(asc_bytes[1]);

        // SLConfigDescriptor
        std::vector<uint8_t> sl_config;
        sl_config.push_back(0x06);
        writeDescriptorLength(sl_config, 1);
        sl_config.push_back(0x02);

        std::vector<uint8_t> decoder_config_descriptor;
        decoder_config_descriptor.push_back(0x04);
        writeDescriptorLength(decoder_config_descriptor,
                              static_cast<uint32_t>(decoder_config.size()));
        decoder_config_descriptor.insert(decoder_config_descriptor.end(),
                                         decoder_config.begin(), decoder_config.end());

        es_descriptor.insert(es_descriptor.end(),
                             decoder_config_descriptor.begin(), decoder_config_descriptor.end());
        es_descriptor.insert(es_descriptor.end(), sl_config.begin(), sl_config.end());

        writeDescriptorLength(esds_payload, static_cast<uint32_t>(es_descriptor.size()));
        esds_payload.insert(esds_payload.end(), es_descriptor.begin(), es_descriptor.end());

        std::vector<uint8_t> esds_box;
        uint32_t esds_size = 8 + static_cast<uint32_t>(esds_payload.size());
        writeAtomHeader(esds_box, "esds", esds_size);
        esds_box.insert(esds_box.end(), esds_payload.begin(), esds_payload.end());

        uint32_t mp4a_size = 36 + static_cast<uint32_t>(esds_box.size());
        writeUint32BE(stsd_entries, mp4a_size);
        writeUint8(stsd_entries, 'm');
        writeUint8(stsd_entries, 'p');
        writeUint8(stsd_entries, '4');
        writeUint8(stsd_entries, 'a');
        
        // Reserved
        for (int i = 0; i < 6; i++) writeUint8(stsd_entries, 0);
        
        // Data reference index
        writeUint16BE(stsd_entries, 1);
        
        // Version and revision
        writeUint16BE(stsd_entries, 0);
        writeUint16BE(stsd_entries, 0);
        
        // Vendor
        writeUint32BE(stsd_entries, 0);
        
        // Channel count
        writeUint16BE(stsd_entries, channel_count);
        
        // Sample size
        writeUint16BE(stsd_entries, 16);
        
        // Compression ID
        writeUint16BE(stsd_entries, 0);
        
        // Packet size
        writeUint16BE(stsd_entries, 0);
        
        // Sample rate (16.16 fixed point)
        writeUint32BE(stsd_entries, sample_rate << 16);

        stsd_entries.insert(stsd_entries.end(), esds_box.begin(), esds_box.end());
    }
    
    // Build stsd box
    uint32_t stsd_size = 8 + 8 + stsd_entries.size();
    writeAtomHeader(data, "stsd", stsd_size);
    writeUint32BE(data, 0);  // version 0 + flags 0
    writeUint32BE(data, 1);  // entry count
    data.insert(data.end(), stsd_entries.begin(), stsd_entries.end());
    
    return true;
}

} // namespace mp4_recorder
