# Contributing to MP4 Crash-Safe Recorder

Thank you for your interest in contributing to the MP4 Crash-Safe Recorder project! This document provides guidelines and instructions for contributing.

## Code of Conduct

- Be respectful and inclusive
- Provide constructive feedback
- Focus on the code, not the person
- Help others learn and grow

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/yourusername/mp4_crash_safe_recorder.git`
3. Create a feature branch: `git checkout -b feature/your-feature-name`
4. Make your changes
5. Test your changes: `cd build && make test`
6. Commit with clear messages: `git commit -m "Add feature: description"`
7. Push to your fork: `git push origin feature/your-feature-name`
8. Create a Pull Request

## Coding Standards

Please follow the guidelines in [AGENTS.md](AGENTS.md):

- **Naming**: PascalCase for classes, camelCase for functions/variables
- **Formatting**: 4-space indentation, Allman style braces
- **Types**: Use fixed-width types (uint8_t, int64_t, etc.)
- **Error Handling**: Return bool, log errors before returning false
- **Documentation**: Add comments for complex logic, file headers with GPL v2+ license

## Testing

- Write tests for new features
- Ensure all tests pass: `make test`
- Test both success and failure paths
- Verify file cleanup after operations

## Pull Request Process

1. Update documentation if needed
2. Add tests for new functionality
3. Ensure all tests pass
4. Update CHANGELOG.md
5. Provide clear PR description
6. Link related issues

## Reporting Issues

- Use GitHub Issues for bug reports
- Include reproduction steps
- Provide system information
- Attach relevant logs

## Questions?

- Check existing documentation in `docs/`
- Review AGENTS.md for project guidelines
- Open a discussion issue

Thank you for contributing!
