# Documentation Index

Welcome to the MP4 Crash-Safe Recorder documentation.

## Quick Links

| Document | Description |
|----------|-------------|
| [API Reference](API.md) | Complete API documentation |
| [Design Document](DESIGN.md) | Architecture and design decisions |
| [Recovery Mechanism](RECOVERY.md) | Crash recovery details |
| [Testing Guide](TESTING.md) | Test procedures |
| [Troubleshooting](TROUBLESHOOTING.md) | Problem solving |

## Getting Started

### Installation

See [INSTALL.md](../INSTALL.md) for build and installation instructions.

### Basic Usage

```cpp
#include "mp4_recorder.h"
using namespace mp4_recorder;

// Create recorder
Mp4Recorder recorder;

// Configure
RecorderConfig config;
config.video_timescale = 30000;
config.flush_interval_ms = 500;

// Start recording
recorder.start("output.mp4", config);

// Write frames
recorder.writeVideoFrame(data, size, pts, is_keyframe);

// Stop recording
recorder.stop();
```

### Crash Recovery

```cpp
// Check for incomplete recording
if (Mp4Recorder::hasIncompleteRecording("output.mp4")) {
    Mp4Recorder recorder;
    recorder.recover("output.mp4");
}
```

## Documentation by Topic

### Core Library

- **[API Reference](API.md)** - Classes, methods, and data structures
- **[Design Document](DESIGN.md)** - Three-file transaction scheme, architecture
- **[Recovery Mechanism](RECOVERY.md)** - How crash recovery works

### Development

- **[Testing Guide](TESTING.md)** - Test suites and procedures
- **[Troubleshooting](TROUBLESHOOTING.md)** - Common issues and solutions

## Project Structure

```
mp4_crash_safe_recorder/
├── include/              # Header files
│   ├── mp4_recorder.h   # Main recorder API
│   ├── moov_builder.h   # Moov box construction
│   ├── index_file.h     # Index file management
│   └── common.h         # Common utilities
├── src/                  # Implementation
├── examples/             # Example programs
├── tests/                # Test suite
├── docs/                 # Documentation (you are here)
└── README.md            # Project overview
```

## Key Concepts

### Three-File Transaction Scheme

During recording, three files are created:

1. **output.mp4** - Media data (ftyp + mdat boxes)
2. **output.idx** - Frame index (metadata for recovery)
3. **output.lock** - Lock file (indicates recording in progress)

On successful stop or recovery, .idx and .lock files are deleted.

### Crash Recovery Flow

```
1. Detect lock file exists
2. Read idx file
3. Parse frame information
4. Build moov box from frames
5. Append moov to mp4
6. Delete idx and lock files
7. MP4 is now playable
```

### Data Flow

```
Input Frames
    |
MP4 Recording
    |
MP4 File
```

## Configuration

### RecorderConfig

```cpp
struct RecorderConfig {
    uint32_t video_timescale = 30000;      // Video timescale
    uint32_t audio_timescale = 48000;      // Audio timescale
    uint32_t audio_sample_rate = 48000;    // Audio sample rate
    uint16_t audio_channels = 2;           // Audio channels
    uint32_t flush_interval_ms = 500;      // Flush interval
    uint32_t flush_frame_count = 1000;     // Flush frame count
};
```

## Performance

| Metric | Value |
|--------|-------|
| Write latency | < 1ms per frame |
| Recovery time | < 100ms |
| Memory usage | < 10MB |

## Supported Platforms

- Windows (MSVC, MinGW)
- Linux (GCC, Clang)
- macOS (Clang)

## Supported Codecs

- **Video**: H.264 (avc1), H.265 (hev1)
- **Audio**: AAC (mp4a), PCM

## Examples

### Primary Demo
```bash
./mp4_recover_demo
```

### Recovery Demo
```bash
./recovery_demo
```

## Related Files

### Root Directory

| File | Description |
|------|-------------|
| [README.md](../README.md) | Project overview |
| [INSTALL.md](../INSTALL.md) | Installation guide |
| [CONTRIBUTING.md](../CONTRIBUTING.md) | Contribution guidelines |
| [CHANGELOG.md](../CHANGELOG.md) | Version history |
| [AGENTS.md](../AGENTS.md) | Agent coding guide |
| [ROADMAP.md](../ROADMAP.md) | Future plans |

### Examples Directory

| File | Description |
|------|-------------|
| mp4_recover_demo.cpp | Primary end-to-end crash-safe recording and recovery demo |
| basic_recording.cpp | Basic recording example |
| recovery_demo.cpp | Recovery demonstration |
| advanced_recording.cpp | Advanced recording example |
| multithreaded_recording.cpp | Multi-threaded recording example |
| error_handling.cpp | Error handling example |

## License

GPL v2+ - See [LICENSE](../LICENSE) for details.
