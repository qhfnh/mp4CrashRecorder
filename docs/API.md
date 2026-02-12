# MP4 Crash-Safe Recorder - API Reference

## Core Classes

### Mp4Recorder

Main class for recording MP4 videos with crash recovery support.

#### Constructor
```cpp
Mp4Recorder();
```

#### Methods

##### start()
```cpp
bool start(const std::string& filename, const RecorderConfig& config = RecorderConfig());
```
Start recording to the specified file.

**Parameters:**
- `filename`: Output MP4 filename
- `config`: Recording configuration (optional)

**Returns:** true on success, false on failure

##### writeVideoFrame()
```cpp
bool writeVideoFrame(const uint8_t* data, uint32_t size, int64_t pts, bool is_keyframe);
```
Write a video frame to the recording.

**Parameters:**
- `data`: Frame data pointer
- `size`: Frame size in bytes
- `pts`: Presentation timestamp
- `is_keyframe`: true if this is a keyframe

**Returns:** true on success, false on failure

##### writeAudioFrame()
```cpp
bool writeAudioFrame(const uint8_t* data, uint32_t size, int64_t pts);
```
Write an audio frame to the recording.

**Parameters:**
- `data`: Audio data pointer
- `size`: Audio data size in bytes
- `pts`: Presentation timestamp

**Returns:** true on success, false on failure

##### stop()
```cpp
bool stop();
```
Stop recording and finalize the MP4 file.

**Returns:** true on success, false on failure

##### hasIncompleteRecording()
```cpp
static bool hasIncompleteRecording(const std::string& filename);
```
Check if there's an incomplete recording that needs recovery.

**Parameters:**
- `filename`: MP4 filename to check

**Returns:** true if incomplete recording exists, false otherwise

##### recover()
```cpp
bool recover(const std::string& filename);
```
Recover from an incomplete recording.

**Parameters:**
- `filename`: MP4 filename to recover

**Returns:** true on success, false on failure

##### isRecording()
```cpp
bool isRecording() const;
```
Check if currently recording.

**Returns:** true if recording, false otherwise

##### getFrameCount()
```cpp
uint64_t getFrameCount() const;
```
Get the number of frames recorded so far.

**Returns:** Frame count

### RecorderConfig

Configuration structure for recording parameters.

```cpp
struct RecorderConfig {
    uint32_t video_timescale = 30000;      // Video timescale
    uint32_t audio_timescale = 48000;      // Audio timescale
    uint32_t audio_sample_rate = 48000;    // Audio sample rate
    uint16_t audio_channels = 2;           // Number of audio channels
    uint32_t flush_interval_ms = 500;      // Flush interval in milliseconds
    uint32_t flush_frame_count = 1000;     // Flush every N frames
};
```

### FrameInfo

Frame metadata structure.

```cpp
struct FrameInfo {
    uint64_t offset;        // Offset in mdat
    uint32_t size;          // Frame size
    int64_t  pts;           // Presentation timestamp
    int64_t  dts;           // Decoding timestamp
    uint8_t  is_keyframe;   // 1 if keyframe, 0 otherwise
    uint8_t  track_id;      // 0 for video, 1 for audio
};
```

## Usage Example

```cpp
#include "mp4_recorder.h"

using namespace mp4_recorder;

int main() {
    // Create recorder
    Mp4Recorder recorder;
    
    // Configure
    RecorderConfig config;
    config.video_timescale = 30000;
    config.audio_timescale = 48000;
    
    // Start recording
    if (!recorder.start("output.mp4", config)) {
        return 1;
    }
    
    // Write frames
    uint8_t video_frame[1024];
    uint8_t audio_frame[512];
    
    for (int i = 0; i < 100; i++) {
        recorder.writeVideoFrame(video_frame, sizeof(video_frame), i * 1000, (i % 30 == 0));
        recorder.writeAudioFrame(audio_frame, sizeof(audio_frame), i * 1000);
    }
    
    // Stop recording
    recorder.stop();
    
    return 0;
}
```

## Error Handling

All methods return `bool` indicating success or failure. Use `Logger` class for detailed error messages:

```cpp
SetLogLevel(LogLevel::DEBUG);  // Set log level
MCSR_LOG(ERROR) << "Error message";
MCSR_LOG(INFO) << "Info message";
MCSR_LOG(VERBOSE) << "Debug message";
```

## Thread Safety

**Note:** Current implementation is NOT thread-safe. For multi-threaded use, add external synchronization.

## File Format

### Index File (.idx)

Binary format with repeated FrameInfo structures:
```
[FrameInfo][FrameInfo][FrameInfo]...
```

Each FrameInfo is 25 bytes:
- offset: 8 bytes (uint64_t)
- size: 4 bytes (uint32_t)
- pts: 8 bytes (int64_t)
- dts: 8 bytes (int64_t)
- is_keyframe: 1 byte (uint8_t)
- track_id: 1 byte (uint8_t)

### Lock File (.lock)

Simple text file containing "RECORDING" to indicate in-progress recording.
