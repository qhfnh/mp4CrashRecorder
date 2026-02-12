/*
 * MP4 Crash-Safe Recorder - File Operations Abstraction
 *
 * License: GPL v2+
 */

#include "file_ops.h"

#include <cerrno>
#include <cstring>

#ifdef _WIN32
    #include <io.h>
    #include <sys/stat.h>
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif

namespace mp4_recorder {

StdioFile::StdioFile(FILE* file)
    : file_(file) {
}

StdioFile::~StdioFile() {
    close();
}

size_t StdioFile::read(void* data, size_t size) {
    if (!file_ || size == 0) {
        return 0;
    }
    return fread(data, 1, size, file_);
}

size_t StdioFile::write(const void* data, size_t size) {
    if (!file_ || size == 0) {
        return 0;
    }
    return fwrite(data, 1, size, file_);
}

bool StdioFile::seek(int64_t offset, int origin) {
    if (!file_) {
        return false;
    }
#ifdef _WIN32
    return _fseeki64(file_, offset, origin) == 0;
#else
    return fseeko(file_, static_cast<off_t>(offset), origin) == 0;
#endif
}

int64_t StdioFile::tell() {
    if (!file_) {
        return -1;
    }
#ifdef _WIN32
    return _ftelli64(file_);
#else
    return static_cast<int64_t>(ftello(file_));
#endif
}

bool StdioFile::flush() {
    if (!file_) {
        return false;
    }
    return fflush(file_) == 0;
}

bool StdioFile::sync() {
    if (!file_) {
        return false;
    }
#ifdef _WIN32
    return _commit(_fileno(file_)) == 0;
#else
    return fsync(fileno(file_)) == 0;
#endif
}

void StdioFile::close() {
    if (file_) {
        fclose(file_);
        file_ = nullptr;
    }
}

bool StdioFile::isOpen() const {
    return file_ != nullptr;
}

std::unique_ptr<IFile> StdioFileOps::open(const std::string& path, const char* mode) {
    FILE* file = fopen(path.c_str(), mode);
    if (!file) {
        return std::unique_ptr<IFile>();
    }
    return std::unique_ptr<IFile>(new StdioFile(file));
}

bool StdioFileOps::exists(const std::string& path) {
    struct stat buffer;
    return stat(path.c_str(), &buffer) == 0;
}

bool StdioFileOps::remove(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

bool StdioFileOps::getFileSize(const std::string& path, uint64_t& size) {
#ifdef _WIN32
    struct _stat64 buffer;
    if (_stat64(path.c_str(), &buffer) != 0) {
        return false;
    }
    size = static_cast<uint64_t>(buffer.st_size);
    return true;
#else
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        return false;
    }
    size = static_cast<uint64_t>(buffer.st_size);
    return true;
#endif
}

} // namespace mp4_recorder
