# AGENTS.md - MP4 Crash-Safe Recorder

Guide for agentic coding agents working in this repository.

## Scope and Sources

- Primary sources: `README.md`, `docs/TESTING.md`, `CMakeLists.txt`, `Makefile`, `.clang-format`.
- Cursor/Copilot rules: none found (.cursor/rules, .cursorrules, .github/copilot-instructions.md).

## Build Commands

### CMake (cross-platform)
```bash
mkdir -p build
cd build
cmake ..
cmake --build . --config Release
```

### Makefile (Linux/macOS; also useful on Windows with make)
```bash
make build
make clean
make rebuild
```

### Install
```bash
cd build
cmake --build . --config Release --target install
```

## Test Commands

### Run all tests
```bash
cd build
ctest --output-on-failure
```

### Single test (binary)
```bash
./Release/test_recovery.exe
./Release/test_index_file.exe
./Release/test_performance.exe
./Release/test_ffmpeg_validation.exe
```

### Single test (build target)
```bash
cmake --build . --config Release --target test_recovery
./Release/test_recovery.exe
```

### Makefile shortcuts
```bash
make test
make test-recovery
make test-index
make test-perf
```

## Example Commands

```bash
./Release/basic_recording.exe
./Release/recovery_demo.exe
./Release/advanced_recording.exe
./Release/multithreaded_recording.exe
./Release/error_handling.exe
```

## Lint/Format

### clang-format (required)
```bash
clang-format -i src/*.cpp include/*.h tests/*.cpp examples/*.cpp
clang-format --dry-run src/*.cpp include/*.h
```

### cppcheck (Makefile target)
```bash
cppcheck --enable=all --suppress=missingIncludeSystem src/ include/ examples/
```

## Code Style (C++)

### Language and compiler
- C++17 (see `CMakeLists.txt`).
- Warnings: `-Wall -Wextra -Wpedantic`.

### Formatting (from `.clang-format`)
- Indent: 4 spaces, no tabs.
- Braces: Allman (opening brace on next line).
- Column limit: 100.
- Sort includes: enabled; include blocks preserved.

### Naming
- Classes/structs/enums: PascalCase (`Mp4Recorder`, `MoovBuilder`).
- Functions/methods: camelCase (`writeVideoFrame`, `buildMoov`).
- Members: trailing underscore (`mp4_file_`, `recording_`).
- Constants: UPPER_SNAKE_CASE (`FLUSH_INTERVAL_MS`).
- Files: snake_case (`mp4_recorder.cpp`, `index_file.h`).

### Include order
1. Project headers.
2. Standard library headers.
3. System headers.

Example:
```cpp
#include "mp4_recorder.h"
#include "moov_builder.h"
#include <cstdint>
#include <string>
```

### Types and safety
- Prefer fixed-width types: `uint32_t`, `uint64_t`, `int64_t`.
- Use `std::vector<T>` and `std::string` (avoid raw buffers/`char*`).
- Keep const-correctness for inputs and accessors.

### Error handling
- Most APIs return `bool` for success/failure.
- Log before returning false.
- Use `RTC_LOG` consistently.

Pattern:
```cpp
if (!operation())
{
    MCSR_LOG(ERROR) << "descriptive message";
    return false;
}
```

## Architecture Quick Map

- `Mp4Recorder` (`include/mp4_recorder.h`, `src/mp4_recorder.cpp`): public API, file lifecycle, recovery.
- `MoovBuilder` (`include/moov_builder.h`, `src/moov_builder.cpp`): builds moov box from index data.
- `IndexFile` (`include/index_file.h`, `src/index_file.cpp`): frame index (`.idx`) for recovery.
- `common.h`: Logger and big-endian helpers.

## Recording / Recovery Basics

- Recording writes `ftyp` + `mdat` first; `moov` appended on stop or recovery.
- Crash-safe scheme uses three files: `.mp4`, `.idx`, `.lock`.
- Recovery reads `.idx`, rebuilds `moov`, appends it, then deletes `.idx` and `.lock`.

## Testing Notes

- Tests live in `tests/`.
- Use descriptive test names and cover error paths.
- `docs/TESTING.md` has manual scenarios and expected outputs.

## Contribution Expectations

- Run at least one relevant test after changes.
- Format code before commit.
- Keep changes minimal and focused; avoid refactors during bug fixes.
