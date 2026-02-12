# Testing Guide

This document covers testing procedures for the MP4 Crash-Safe Recorder.

## Quick Start

### Build and Run All Tests

```bash
cd build
cmake ..
cmake --build . --config Release
ctest --output-on-failure
```

### Run Individual Tests

```bash
# Index file tests
./Release/test_index_file.exe

# Recovery tests
./Release/test_recovery.exe

# Performance tests
./Release/test_performance.exe

# FFmpeg validation tests
./Release/test_ffmpeg_validation.exe
```

## Test Suites

### 1. Index File Tests (`test_index_file.exe`)

**Location:** `tests/test_index_file.cpp`

**Tests:**
- **Test 1:** Index file creation
- **Test 2:** Index file writing
- **Test 3:** Index file reading
- **Test 4:** Index file flushing
- **Test 5:** Multiple index files

**Expected Result:**
```
[INFO] Tests completed: 5 passed, 0 failed
```

### 2. Recovery Tests (`test_recovery.exe`)

**Location:** `tests/test_recovery.cpp`

**Tests:**

#### Test 1: Basic Recording
- Start recording
- Write 10 video + 10 audio frames
- Stop recording normally
- Verify output file exists

#### Test 2: Crash Recovery Detection
- Start recording
- Write 1 frame
- **Don't call stop()** (simulate crash)
- Verify incomplete recording detected

#### Test 3: Multi-frame Recording
- Configure fast flush (100ms or 5 frames)
- Write 100 video frames
- Verify frame count

#### Test 4: Error Handling
- Try writing without starting -> should fail
- Try stopping without starting -> should fail

#### Test 5: Double Start Protection
- Start first recording
- Try starting again -> should fail
- Stop first recording

**Expected Result:**
```
[INFO] Tests completed: 5 passed, 0 failed
```

### 3. FFmpeg Validation Tests (`test_ffmpeg_validation.exe`)

**Location:** `tests/test_ffmpeg_validation.cpp`

**Tests:**
- **Test 1:** Generate MP4 with video/audio frames
- **Test 2:** Validate MP4 format with FFmpeg
- **Test 3:** Verify MP4 playback capability
- **Test 4:** Test incomplete recording detection
- **Test 5:** Multiple sequential recordings
- **Test 6:** End-to-end test

**Expected Result:**
```
Test Results: 6 passed, 0 failed
```

## Manual Test Scenarios

### Scenario 1: Primary Demo (Recording + Validation + Playback)

`mp4_recover_demo` is the renamed version of `realtime_encoding_demo.cpp` and depends on
FFmpeg command-line tools.

```bash
ffmpeg -version
ffprobe -version
ffplay -version
```

```bash
./Release/mp4_recover_demo.exe
```

Tests:
- Generate H.264 and AAC test streams with ffmpeg
- Record crash-safe MP4 output
- Validate output with ffprobe and playback with ffplay

### Scenario 2: Basic Recording

```bash
./Release/basic_recording.exe
```

Generates:
- `output.mp4` (MP4 file)
- `output.mp4.idx` (index file)
- `output.mp4.lock` (lock file, deleted after completion)

### Scenario 3: Advanced Recording

```bash
./Release/advanced_recording.exe
```

Tests:
- Multi-track recording (video + audio)
- Custom configuration
- Frame rate control

## Crash Recovery Testing

### Using Real H.264 Video

#### Prerequisites

```bash
# Install FFmpeg
choco install ffmpeg  # Windows
brew install ffmpeg   # macOS
apt-get install ffmpeg  # Linux
```

#### Step 1: Generate Test Video

```bash
ffmpeg -f lavfi -i testsrc=s=320x240:d=10:r=30 \
       -f lavfi -i sine=f=1000:d=10 \
       -c:v libx264 -preset fast -crf 28 \
       -c:a aac -b:a 128k \
       -y test_video.mp4
```

#### Step 2: Extract H.264 Stream

```bash
ffmpeg -i test_video.mp4 \
       -c:v copy -bsf:v h264_mp4toannexb \
       -f h264 \
       -y test_video.h264
```

#### Step 3: Run Crash Simulation

```bash
mkdir crash_test_output
cd crash_test_output
../build/Release/crash_simulation_test.exe
```

#### Step 4: Verify Recovery

```bash
ffprobe -show_format -show_streams test_crash.mp4
ffplay test_crash.mp4
```

### Test Scenarios

#### Normal Recording (No Crash)
```
Record -> Stop -> Generate moov -> Delete idx/lock
Result: test_normal.mp4 plays directly
```

#### Crash Recovery
```
Record 50% -> Crash (no stop)
    |
Detect lock and idx files
    |
Read frame info from idx
    |
Rebuild moov box
    |
Append moov to mp4
    |
Delete idx and lock
Result: test_crash.mp4 recovered and playable
```

#### Multiple Crash Recovery
```
Cycle 1: Record 100 frames -> Crash -> Recover -> test_cycle_1.mp4
Cycle 2: Record 150 frames -> Crash -> Recover -> test_cycle_2.mp4
Cycle 3: Record 200 frames -> Crash -> Recover -> test_cycle_3.mp4
```

## Verification

### Check Generated Files

```bash
ls -lh output.mp4*
```

### Verify MP4 Structure

MP4 file should contain:
1. **ftyp box** (20 bytes) - File type
2. **mdat box** - Media data
3. **moov box** - Metadata (added after recording)

### Verify Index File Format

Index file is binary format, each record:
```cpp
struct FrameInfo {
    uint64_t offset;        // 8 bytes - offset in mdat
    uint32_t size;          // 4 bytes - frame size
    int64_t  pts;           // 8 bytes - presentation timestamp
    int64_t  dts;           // 8 bytes - decoding timestamp
    uint8_t  is_keyframe;   // 1 byte - keyframe flag
    uint8_t  track_id;      // 1 byte - track ID (0=video, 1=audio)
};
// Total: 30 bytes/frame
```

## Test Checklist

### Functional Tests
- [ ] Basic recording works
- [ ] Multi-frame writing correct
- [ ] Video and audio tracks separate
- [ ] Crash detection works
- [ ] Recovery function complete
- [ ] Error handling correct
- [ ] File cleanup correct

### File Verification
- [ ] MP4 file playable
- [ ] Index file format correct
- [ ] Lock file exists during recording
- [ ] Lock file deleted on stop
- [ ] Index file deleted after recovery

### Performance Verification
- [ ] Write speed meets requirements
- [ ] Memory usage reasonable
- [ ] Flush operations don't block
- [ ] Multi-thread safe

### Edge Cases
- [ ] Empty recording (0 frames)
- [ ] Single frame recording
- [ ] Large recording (1000+ frames)
- [ ] Fast start/stop
- [ ] Concurrent recording

## Debugging

### Enable Verbose Logging

```cpp
SetLogLevel(LogLevel::DEBUG);
```

### Check File Contents

```bash
# Check index file size (should be multiple of 30)
stat output.mp4.idx

# Calculate frame count
size_in_bytes / 30 = frame_count
```

### Verify MP4 Structure

Using hex editor, check MP4 header:
- Bytes 0-3: `66 74 79 70` (ftyp)
- Bytes 8-11: `69 73 6F 6D` (isom)

## Performance Metrics

### Test Environment
- Resolution: 320x240
- Frame rate: 30fps
- Duration: 10 seconds
- Total frames: 300

### Expected Results

| Metric | Value |
|--------|-------|
| Original MP4 size | ~500KB |
| Recovered MP4 size | ~500KB |
| Recovery time | < 100ms |
| Playback success rate | 100% |

## CI/CD Integration

### GitHub Actions

```yaml
name: CI/CD Pipeline

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main, develop ]

jobs:
  build:
    strategy:
      matrix:
        include:
          - os: ubuntu-latest
            compiler: gcc
          - os: ubuntu-latest
            compiler: clang
          - os: windows-latest
            compiler: msvc
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - run: cmake -S . -B build
      - run: cmake --build build --config Release --parallel
      - run: ctest --test-dir build --output-on-failure -C Release
```

### GitLab CI

```yaml
test:
  script:
    - cd build
    - cmake --build . --config Release
    - ctest --output-on-failure
```

## Common Issues

### Q: Test fails with "Not recording"
**A:** This is normal error handling test. Check if test expects this error.

### Q: Index file not deleted
**A:** Check if recovery was called. Recovery should delete `.idx` and `.lock` files.

### Q: MP4 file won't play
**A:** Ensure:
1. Recording stopped normally (called `stop()`)
2. moov box written correctly
3. Frame data complete

### Q: Performance test slow
**A:** This may be normal. Check:
1. Disk I/O speed
2. System load
3. Flush interval configuration

## Test Coverage

| Component | Coverage | Status |
|-----------|----------|--------|
| IndexFile | Create, write, read, flush | Complete |
| Mp4Recorder | Start, write, stop, recover | Complete |
| MoovBuilder | moov box construction | Via recovery tests |
| Error handling | Edge cases, exceptions | Complete |
| File management | Create, delete, cleanup | Complete |

## Related Documentation

- [API Reference](API.md) - Complete API documentation
- [Design Document](DESIGN.md) - Architecture overview
- [Recovery Mechanism](RECOVERY.md) - Crash recovery details
- [Troubleshooting](TROUBLESHOOTING.md) - Problem solving
