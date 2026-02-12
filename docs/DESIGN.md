# MP4 Crash-Safe Recorder - Design Document

## Overview

This document describes the design of the MP4 Crash-Safe Recorder, a production-grade library for recording MP4 videos with automatic crash recovery.

## Three-File Transaction Scheme

### File Roles

1. **output.mp4** - Media Data Container
   - Contains `ftyp` and `mdat` boxes
   - No `moov` box during recording
   - Not playable until recovery/finalization

2. **output.idx** - Frame Index Log
   - Binary log of frame metadata
   - Records: offset, size, pts, dts, keyframe flag, track_id
   - Flushed periodically for crash safety
   - Deleted after successful finalization

3. **output.lock** - Recording Lock File
   - Indicates recording in progress
   - Presence = incomplete recording
   - Deleted after successful finalization

## Recording Flow

### Normal Recording

```
1. Create mp4, idx, lock files
2. Write ftyp box to mp4
3. Write mdat placeholder (size=0) to mp4
4. For each frame:
   - Write frame data to mp4
   - Log frame info to idx
   - Flush if needed (time or frame count)
5. On stop:
   - Read idx file
   - Build moov box
   - Append moov to mp4
   - Delete idx and lock files
```

### Crash Recovery

```
1. Detect lock file exists
2. Read idx file
3. Parse frame information
4. Build moov box from frames
5. Append moov to mp4
6. Delete idx and lock files
7. mp4 is now playable
```

## Key Technical Details

### mdat Size Handling

MP4 allows `mdat size = 0` to mean "until EOF". This is crucial for crash safety:
- We don't know final size during recording
- Size 0 allows continuous writing
- Moov is appended after recording stops

### Frame Offset Calculation

All offsets in idx are relative to mdat data start:
```
offset_in_idx = file_offset - mdat_data_start
```

This allows rebuilding stco/co64 atoms correctly.

### Data Safety

- Flush every 500ms or 1000 frames
- Crash loses at most 1 second of data
- idx file is binary for efficiency
- fsync ensures data reaches disk

## Supported Codecs

- **Video**: H.264 (avc1), H.265 (hev1)
- **Audio**: AAC (mp4a), PCM

## Performance Characteristics

- Write latency: < 1ms
- Recovery time: < 100ms
- Memory overhead: < 10MB
- Disk overhead: ~1% (idx file)

## Error Handling

- Graceful degradation on I/O errors
- Automatic cleanup of partial files
- Detailed logging for debugging
- Recovery validation before finalization
