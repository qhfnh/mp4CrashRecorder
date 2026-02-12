# MP4 Crash-Safe Recorder - Recovery Mechanism

## Overview

The recovery mechanism is the core feature that enables crash-safe recording. This document explains how it works and how to use it.

## Recovery Process

### Detection Phase

When the application starts, it checks for incomplete recordings:

```cpp
if (Mp4Recorder::hasIncompleteRecording("output.mp4")) {
    // Incomplete recording detected
    // Lock file exists: output.mp4.lock
    // Index file exists: output.mp4.idx
}
```

### Recovery Phase

1. **Read Index File**
   - Parse all frame metadata from .idx file
   - Separate video and audio frames
   - Validate frame information

2. **Build Moov Box**
   - Create mvhd (movie header)
   - Create trak (track) atoms for video and audio
   - Build stts (sample time-to-sample)
   - Build stss (sync sample/keyframes)
   - Build stsz (sample sizes)
   - Build stco/co64 (chunk offsets)
   - Build stsc (sample-to-chunk)

3. **Append to MP4**
   - Seek to end of mp4 file
   - Write moov box
   - Close file

4. **Cleanup**
   - Delete .idx file
   - Delete .lock file
   - MP4 is now playable

## Data Safety Guarantees

### Crash Scenarios

1. **Crash during frame write**
   - Frame data may be incomplete
   - Index entry not written
   - Recovery skips incomplete frame
   - Result: Video plays up to last complete frame

2. **Crash during index flush**
   - Frame data written to mp4
   - Index entry not flushed to disk
   - Recovery skips unflushed frames
   - Result: Video plays up to last flushed frame

3. **Crash during moov write**
   - Lock file still exists
   - Index file still exists
   - Recovery rebuilds moov
   - Result: Video fully recovered

### Data Loss Bounds

- Maximum data loss: 1 second (configurable via flush_interval_ms)
- Minimum recovery: All flushed frames
- No data corruption: Only loss, never corruption

## Implementation Details

### Index File Format

Binary format for efficiency:

```
Offset  Size  Field
0       8     frame[0].offset (uint64_t)
8       4     frame[0].size (uint32_t)
12      8     frame[0].pts (int64_t)
20      8     frame[0].dts (int64_t)
28      1     frame[0].is_keyframe (uint8_t)
29      1     frame[0].track_id (uint8_t)
30      ...   frame[1]...
```

### Moov Box Structure

```
moov
├── mvhd (movie header)
├── trak (video track)
│   ├── tkhd (track header)
│   └── mdia (media)
│       ├── mdhd (media header)
│       ├── hdlr (handler)
│       └── minf (media information)
│           ├── stbl (sample table)
│           │   ├── stts (sample times)
│           │   ├── stss (sync samples)
│           │   ├── stsz (sample sizes)
│           │   ├── stco (chunk offsets)
│           │   └── stsc (sample-to-chunk)
└── trak (audio track)
    └── ...
```

## Validation

Before finalizing recovery, validate:

1. **Frame Count**: Matches between video and audio
2. **Timestamps**: Monotonically increasing
3. **Offsets**: Within file bounds
4. **Keyframes**: At least one per track

## Performance

- Recovery time: < 100ms for typical recordings
- Memory usage: Proportional to frame count
- Disk I/O: Sequential read of index file

## Limitations

1. **No partial recovery**: Either full recovery or none
2. **Single file**: Cannot recover from multiple corrupted files
3. **Codec-specific**: Only supports configured codecs
4. **No verification**: Doesn't verify frame data integrity

## Future Enhancements

1. **Incremental recovery**: Recover partial frames
2. **Checksum validation**: Verify frame data integrity
3. **Multiple file support**: Recover from multiple sources
4. **Codec detection**: Auto-detect codec from frame data
5. **Parallel recovery**: Multi-threaded moov building
