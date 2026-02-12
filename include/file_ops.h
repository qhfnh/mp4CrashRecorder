/*
 * MP4 Crash-Safe Recorder - File Operations Abstraction
 *
 * Abstracts file I/O for testability and portability
 *
 * License: GPL v2+
 */

#ifndef FILE_OPS_H
#define FILE_OPS_H

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace mp4_recorder {

class IFile {
public:
    virtual ~IFile() {}

    virtual size_t read(void* data, size_t size) = 0;
    virtual size_t write(const void* data, size_t size) = 0;
    virtual bool seek(int64_t offset, int origin) = 0;
    virtual int64_t tell() = 0;
    virtual bool flush() = 0;
    virtual bool sync() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
};

class IFileOps {
public:
    virtual ~IFileOps() {}

    virtual std::unique_ptr<IFile> open(const std::string& path, const char* mode) = 0;
    virtual bool exists(const std::string& path) = 0;
    virtual bool remove(const std::string& path) = 0;
    virtual bool getFileSize(const std::string& path, uint64_t& size) = 0;
};

class StdioFile : public IFile {
public:
    explicit StdioFile(FILE* file);
    ~StdioFile() override;

    size_t read(void* data, size_t size) override;
    size_t write(const void* data, size_t size) override;
    bool seek(int64_t offset, int origin) override;
    int64_t tell() override;
    bool flush() override;
    bool sync() override;
    void close() override;
    bool isOpen() const override;

private:
    FILE* file_;
};

class StdioFileOps : public IFileOps {
public:
    std::unique_ptr<IFile> open(const std::string& path, const char* mode) override;
    bool exists(const std::string& path) override;
    bool remove(const std::string& path) override;
    bool getFileSize(const std::string& path, uint64_t& size) override;
};

} // namespace mp4_recorder

#endif // FILE_OPS_H
