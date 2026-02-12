# Troubleshooting Guide

This document covers common issues and solutions for the MP4 Crash-Safe Recorder.

## Build Issues

### CMake not found

**Error:** `cmake: command not found`

**Solution:**
```bash
# Linux (Ubuntu/Debian)
sudo apt-get install cmake

# macOS
brew install cmake

# Windows
# Download from https://cmake.org/download/
```

### Compiler not found

**Error:** `g++: command not found` or `clang: command not found`

**Solution:**
```bash
# Linux (Ubuntu/Debian)
sudo apt-get install build-essential

# macOS
xcode-select --install

# Windows
# Install Visual Studio or MinGW
```

### C++17 not supported

**Error:** `error: 'optional' is not a member of 'std'`

**Solution:**
```bash
# Ensure compiler supports C++17
g++ --version  # Should be GCC 7.0+
clang --version  # Should be Clang 5.0+

# Or explicitly set C++ standard
cmake -DCMAKE_CXX_STANDARD=17 ..
```

### Build fails with permission denied

**Solution:**
```bash
mkdir -p build
chmod 755 build
cd build
cmake ..
make
```

## Runtime Issues

### Recording fails to start

**Error:** `Failed to create files`

**Causes & Solutions:**

1. **No write permissions**
   ```bash
   ls -la output_directory/
   chmod 755 output_directory/
   ```

2. **Disk space full**
   ```bash
   df -h
   ```

3. **File already exists**
   ```bash
   rm -f output.mp4 output.mp4.idx output.mp4.lock
   ```

### Frames not being written

**Error:** `writeVideoFrame() returns false`

**Causes & Solutions:**

1. **Recording not started**
   ```cpp
   if (!recorder.isRecording()) {
       MCSR_LOG(ERROR) << "Recording not started";
   }
   ```

2. **Invalid frame data**
   ```cpp
   if (data == nullptr || size == 0) {
       MCSR_LOG(ERROR) << "Invalid frame data";
   }
   ```

3. **Disk full during write**
   ```bash
   df -h
   ```

### Recovery fails

**Error:** `Recovery failed`

**Causes & Solutions:**

1. **Missing index file**
   ```bash
   ls -la output.mp4.idx
   ```

2. **Corrupted index file**
   ```bash
   ls -lh output.mp4.idx
   # If size is 0 or very small, file may be corrupted
   ```

3. **Insufficient disk space**
   ```bash
   df -h
   ```

4. **File permissions**
   ```bash
   chmod 644 output.mp4*
   ```

## Playback Issues

### MP4 won't play in Windows Media Player

**Error:** Video file cannot be opened or shows black screen

**Cause:** Windows Media Player has limited codec support and does not support:
- High 4:4:4 Predictive profile
- yuv444p pixel format
- **B-frames (Bidirectional prediction frames)** ⚠️ **CRITICAL**
- Non-monotonic DTS (Decode Time Stamps)

**Solutions:**

1. **Use compatible encoding settings (Recommended)**
   ```bash
   # When generating H.264 stream with ffmpeg:
   ffmpeg -i input.mp4 \
     -c:v libx264 \
     -profile:v high \        # Use High profile (not High 4:4:4)
     -pix_fmt yuv420p \       # Use yuv420p (not yuv444p)
     -bf 0 \                  # Disable B-frames (CRITICAL for WMP)
     -preset medium \
     output.h264
   ```
   
   **Why disable B-frames?**
   - B-frames require DTS != PTS (decode order != display order)
   - Windows Media Player cannot handle complex timestamp reordering
   - B-frames cause "non monotonically increasing dts" errors
   - Disabling B-frames: `has_b_frames=0`, only I and P frames

2. **Verify MP4 compatibility**
   ```bash
   # Check codec profile, pixel format, and B-frames
   ffprobe -v error -show_streams output.mp4 | grep -E "(profile|pix_fmt|has_b_frames)"
   
   # Should show:
   # profile=High (or Main or Baseline)
   # pix_fmt=yuv420p
   # has_b_frames=0  (CRITICAL for Windows Media Player)
   
   # Check for timestamp errors
   ffmpeg -v error -i output.mp4 -f null -
   # Should have NO "non monotonically increasing dts" errors
   
   # Check frame types
   ffprobe -v error -show_frames output.mp4 | grep pict_type | sort | uniq -c
   # Should show only I and P frames (no B frames)
   ```

3. **Use alternative players**
   - VLC Media Player (supports all profiles)
   - MPC-HC (Media Player Classic)
   - PotPlayer
   - ffplay (comes with ffmpeg)

4. **Convert existing MP4 to compatible format**
   ```bash
   ffmpeg -i input.mp4 \
     -c:v libx264 \
     -profile:v high \
     -pix_fmt yuv420p \
     -bf 0 \                  # Disable B-frames
     -c:a copy \
     output_compatible.mp4
   ```

**Compatibility Matrix:**

| Player | B-frames | High 4:4:4 | yuv444p | High Profile | yuv420p |
|--------|----------|------------|---------|--------------|---------|
| Windows Media Player | ❌ | ❌ | ❌ | ✅ | ✅ |
| VLC | ✅ | ✅ | ✅ | ✅ | ✅ |
| ffplay | ✅ | ✅ | ✅ | ✅ | ✅ |
| Chrome/Firefox | ⚠️ | ❌ | ❌ | ✅ | ✅ |
| iOS Safari | ⚠️ | ❌ | ❌ | ✅ | ✅ |

### MP4 has "non monotonically increasing dts" errors

**Error:** `Application provided invalid, non monotonically increasing dts to muxer`

**Cause:** B-frames (Bidirectional prediction frames) cause decode order to differ from display order, resulting in DTS/PTS mismatches.

**Explanation:**
- **Without B-frames:** DTS == PTS (decode order = display order)
  - Frame sequence: I, P, P, P... (simple, monotonic timestamps)
- **With B-frames:** DTS != PTS (decode order != display order)
  - Display order: I, B, B, P, B, B, P...
  - Decode order: I, P, B, B, P, B, B... (P frames must be decoded before B frames that reference them)
  - This causes DTS to jump around, confusing some players

**Solutions:**

1. **Disable B-frames during encoding (Recommended)**
   ```bash
   ffmpeg -i input.mp4 -c:v libx264 -bf 0 output.mp4
   ```

2. **Re-encode existing file without B-frames**
   ```bash
   ffmpeg -i input_with_bframes.mp4 \
     -c:v libx264 \
     -bf 0 \
     -c:a copy \
     output_no_bframes.mp4
   ```

3. **Verify B-frames are disabled**
   ```bash
   # Check has_b_frames flag
   ffprobe -v error -select_streams v:0 -show_entries stream=has_b_frames output.mp4
   # Should show: has_b_frames=0
   
   # Count frame types
   ffprobe -v error -show_frames output.mp4 | grep pict_type | sort | uniq -c
   # Should show only I and P frames (no B)
   ```

**Impact of disabling B-frames:**
- ✅ Better compatibility (Windows Media Player, web browsers)
- ✅ Simpler timestamp handling (DTS == PTS)
- ✅ Lower decoding complexity
- ✅ Faster seeking
- ❌ Slightly larger file size (~10-20% increase)
- ❌ Slightly lower compression efficiency

### MP4 plays but has artifacts or corruption

**Solutions:**
1. Check if file was properly finalized (moov box written)
2. Verify frame timestamps are monotonically increasing
3. Check for disk errors during recording
4. Use recovery if recording was interrupted

### MP4 duration shows incorrectly

**Cause:** Incorrect timescale or duration calculation in moov box

**Solutions:**
1. Verify timescale matches encoding settings
2. Check PTS/DTS values are correct
3. Rebuild moov box with correct parameters

## Performance Issues

### High CPU usage

**Solutions:**

1. **Increase flush interval**
   ```cpp
   config.flush_interval_ms = 1000;  // Instead of 500
   ```

2. **Increase frame count threshold**
   ```cpp
   config.flush_frame_count = 2000;  // Instead of 1000
   ```

3. **Use faster storage (SSD)**

### High memory usage

**Solutions:**

1. Use smaller frame sizes
2. Process frames in batches
3. Check for memory leaks:
   ```bash
   valgrind --leak-check=full ./your_app
   ```

### Slow write performance

**Solutions:**

1. Use local storage instead of network
2. Defragment disk or use SSD
3. Reduce flush frequency

## Testing Issues

### Tests fail to compile

**Solution:**
```bash
cd build
cmake ..
make test_recovery
```

### Tests fail to run

**Solution:**
```bash
make test_recovery
./tests/test_recovery
```

### Test failures

**Solution:**
```bash
# Run with verbose output
./tests/test_recovery

# Enable debug logging
SetLogLevel(LogLevel::DEBUG);
```

## Platform-Specific Issues

### Linux: Segmentation fault

**Solution:**
```bash
# Use debugger
gdb ./your_app

# Or use AddressSanitizer
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address" ..
make
./your_app
```

### macOS: Library not loaded

**Solution:**
```bash
otool -L ./your_app
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH
```

### Windows: DLL missing

**Solution:**
- Install Visual C++ Redistributable
- Or link statically:
  ```bash
  cmake -DCMAKE_CXX_FLAGS="/MT" ..
  ```

## Debugging Tips

### Enable debug logging

```cpp
SetLogLevel(LogLevel::DEBUG);
```

### Use debugger

```bash
# GDB
gdb ./your_app
(gdb) run
(gdb) bt  # backtrace

# LLDB (macOS)
lldb ./your_app
(lldb) run
(lldb) bt
```

### Memory profiling

```bash
# Valgrind
valgrind --leak-check=full ./your_app

# AddressSanitizer
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address" ..
```

### Performance profiling

```bash
# Linux
perf record ./your_app
perf report

# macOS
instruments -t "System Trace" ./your_app
```

## Common Error Messages

| Error | Cause | Solution |
|-------|-------|----------|
| `Failed to create files` | Permission or disk issue | Check permissions and disk space |
| `Not recording` | start() not called | Call start() before writing frames |
| `Failed to write frame` | Disk full or I/O error | Check disk space and permissions |
| `Recovery failed` | Corrupted index file | Restore from backup |
| `Already recording` | start() called twice | Call stop() before start() again |

## Performance Tuning

### For low latency
```cpp
config.flush_interval_ms = 100;
config.flush_frame_count = 100;
```

### For high throughput
```cpp
config.flush_interval_ms = 2000;
config.flush_frame_count = 5000;
```

### For balanced performance
```cpp
config.flush_interval_ms = 500;
config.flush_frame_count = 1000;
```

## Getting Help

If you can't resolve the issue:

1. **Check documentation:** [docs/](.)
2. **Review examples:** [examples/](../examples/)
3. **Search issues:** GitHub Issues
4. **Open new issue:** Use bug report template
5. **Collect diagnostic info:**
   - Recorder logs
   - Steps to reproduce

## Related Documentation

- [API Reference](API.md) - Complete API documentation
- [Design Document](DESIGN.md) - Architecture overview
- [Testing Guide](TESTING.md) - Test procedures
