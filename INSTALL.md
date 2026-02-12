# Installation Guide

## Prerequisites

- CMake 3.10 or higher
- C++17 compatible compiler (GCC, Clang, or MSVC)
- Standard C++ library

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install cmake build-essential
```

### macOS
```bash
brew install cmake
```

### Windows
- Install CMake from https://cmake.org/download/
- Install Visual Studio or MinGW

## Building from Source

### 1. Clone the repository
```bash
git clone https://github.com/qhfnh/mp4CrashRecorder.git
cd mp4_crash_safe_recorder
```

### 2. Create build directory
```bash
mkdir build
cd build
```

### 3. Configure with CMake
```bash
cmake ..
```

### 4. Build
```bash
make
```

### 5. Run tests
```bash
make test
```

### 6. Install (optional)
```bash
sudo make install
```

## Building on Different Platforms

### Linux
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
make test
```

### macOS
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)
make test
```

### Windows (Visual Studio)
```bash
mkdir build
cd build
cmake -G "Visual Studio 16 2019" ..
cmake --build . --config Release
ctest -C Release
```

### Windows (MinGW)
```bash
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
mingw32-make test
```

## Verification

After building, verify the installation:

```bash
# Run primary recovery demo
./examples/mp4_recover_demo

# Run recovery demo
./examples/recovery_demo

# Run all tests
./build/test_recovery
```

## Troubleshooting

### CMake not found
- Ensure CMake is installed and in your PATH
- On Linux: `sudo apt-get install cmake`
- On macOS: `brew install cmake`

### Compiler errors
- Ensure C++17 support: `g++ --version` should show C++17 capable compiler
- Update compiler: `sudo apt-get install g++` (Linux) or `brew install gcc` (macOS)

### Build failures
- Clean build directory: `rm -rf build && mkdir build`
- Check CMakeLists.txt is in root directory
- Verify all source files are present in `src/` and `include/`

## Next Steps

- Read [AGENTS.md](AGENTS.md) for development guidelines
- Check [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines
- Review [docs/API.md](docs/API.md) for API reference
- See [docs/DESIGN.md](docs/DESIGN.md) for architecture details
