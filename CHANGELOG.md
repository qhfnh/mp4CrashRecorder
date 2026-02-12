# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-02-02

### Added
- Initial release of MP4 Crash-Safe Recorder
- Core recording functionality with crash recovery
- Three-file transaction scheme (mp4 + idx + lock)
- Automatic moov box reconstruction from index file
- Support for video and audio frame writing
- Configurable flush strategy (time or frame count based)
- Comprehensive error handling and logging
- Basic recording example
- Recovery demonstration example
- Complete test suite with 5 test cases
- CMake build system
- GitHub Actions CI/CD pipeline
- Comprehensive documentation (DESIGN.md, API.md, RECOVERY.md)
- AGENTS.md guide for agentic coding agents
- Contributing guidelines
- GPL v2+ license

### Features
- **Crash Safety**: Automatic recovery from program crashes
- **Efficient I/O**: Minimal disk writes with configurable flush intervals
- **Production Ready**: Complete error handling and logging
- **Well Documented**: Extensive API and design documentation

### Documentation
- AGENTS.md: Guide for agentic coding agents
- DESIGN.md: Architecture and design decisions
- API.md: Complete API reference
- RECOVERY.md: Crash recovery mechanism details
- CONTRIBUTING.md: Contribution guidelines
- README.md: Quick start guide

### Testing
- test_recovery.cpp: Comprehensive test suite
  - Basic recording test
  - Crash recovery detection test
  - Multiple frames test
  - Error handling test
  - Double start prevention test

### Build System
- CMakeLists.txt: CMake configuration
- Support for Linux, macOS, and Windows
- Static library build
- Example and test executables

### CI/CD
- GitHub Actions workflow
- Multi-platform testing (Ubuntu and Windows)
- Linux compilers: GCC and Clang; Windows compiler: MSVC
- Code quality checks (clang-format, cppcheck)
- Code coverage reporting

## Future Enhancements

### Planned for v1.1.0
- [ ] Dynamic library support (.so, .dylib, .dll)
- [ ] Doxygen API documentation generation
- [ ] Performance benchmarks
- [ ] Additional codec support
- [ ] Streaming API for real-time recording

### Planned for v1.2.0
- [ ] Python bindings
- [ ] C# bindings
- [ ] Advanced recovery options
- [ ] Metadata preservation
- [ ] Custom frame handlers

### Under Consideration
- [ ] Hardware acceleration support
- [ ] Network streaming
- [ ] Cloud storage integration
- [ ] Advanced analytics

## Known Issues

None currently reported.

## Support

For issues, questions, or suggestions, please:
1. Check existing documentation in `docs/`
2. Review AGENTS.md for project guidelines
3. Open a GitHub issue with detailed information
4. Submit a pull request with improvements

## License

This project is licensed under GPL v2+ - see LICENSE file for details.
