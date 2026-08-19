#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace guidexos::appmodel {

// File::ReadAllText and File::WriteAllText are intentionally bounded whole-
// file operations for small desktop application documents.
inline constexpr std::size_t kMaximumTextFileBytes = 16U * 1024U * 1024U;

class FileError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class FileNotFoundError : public FileError {
public:
    using FileError::FileError;
};

class FileAccessError : public FileError {
public:
    using FileError::FileError;
};

class FileTooLargeError : public FileError {
public:
    using FileError::FileError;
};

class InvalidUtf8Error : public FileError {
public:
    using FileError::FileError;
};

class FileWriteError : public FileError {
public:
    using FileError::FileError;
};

class FileReplacementError : public FileError {
public:
    using FileError::FileError;
};

class File final {
public:
    // Paths and returned text use UTF-8. An optional leading UTF-8 BOM is
    // removed when reading and writing never adds a BOM.
    static std::string ReadAllText(const std::string& path);
    static void WriteAllText(const std::string& path,
                             const std::string& contents);
};

} // namespace guidexos::appmodel
