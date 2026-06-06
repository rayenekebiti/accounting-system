#ifndef BINARY_RECORD_FILE_H
#define BINARY_RECORD_FILE_H
#include <string>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <fstream>
#include <utility>

// Layer 1 of the storage stack: type-erased fixed-size record I/O.
// Knows nothing about domain types — just bytes at deterministic offsets.
//
// Preconditions for callers (NOT checked):
//   - the `buffer` passed to append()/update() must be at least `recordSize_` bytes.
//   - only one BinaryRecordFile instance per path at a time (no concurrent writers).
//
// IDs are file positions: id = byteOffset / recordSize. They are uint16_t,
// limiting a file to 65,535 records — append() throws std::length_error past
// that. Soft-delete is the domain's concern; this class never inspects the bytes.
class BinaryRecordFile {
    std::fstream file_;
    std::string  path_;
    std::size_t  recordSize_;

    static constexpr std::size_t kMaxRecords = 65535;

public:
    BinaryRecordFile(std::string path, std::size_t recordSize)
        : path_(std::move(path)), recordSize_(recordSize)
    {
        if (!open())
            throw std::runtime_error("BinaryRecordFile: failed to open " + path_);
    }

    bool open()
    {
        if (file_.is_open()) return true;

        file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_.is_open()) {
            file_.clear();
            file_.open(path_, std::ios::out | std::ios::binary);
            file_.close();
            file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
        }
        return file_.is_open();
    }

    // Appends a record at the end of the file.
    // Returns the assigned id (= old count).
    // Throws std::length_error if the file already holds 65,535 records.
    // Throws std::runtime_error on write/flush failure.
    uint16_t append(const char* buffer)
    {
        file_.seekp(0, std::ios::end);
        const std::streampos endPos = file_.tellp();
        if (endPos < 0) {
            file_.clear();
            throw std::runtime_error("BinaryRecordFile: tellp failed in append");
        }

        const std::size_t recordCount = static_cast<std::size_t>(endPos) / recordSize_;
        if (recordCount >= kMaxRecords)
            throw std::length_error(
                "BinaryRecordFile: id space exhausted at " +
                std::to_string(kMaxRecords) + " records");

        file_.write(buffer, recordSize_);
        if (!file_.good()) {
            file_.clear();
            throw std::runtime_error("BinaryRecordFile: write failed in append");
        }

        file_.flush();                                 // durability: survive crashes
        if (!file_.good()) {
            file_.clear();
            throw std::runtime_error("BinaryRecordFile: flush failed in append");
        }

        return static_cast<uint16_t>(recordCount);
    }

    // Reads the record with the given id into the buffer.
    // Returns false if id is out of range or the read is short; the buffer is
    // zeroed in that case so the caller never deserializes garbage.
    bool read(uint16_t id, char* buffer)
    {
        if (id >= count()) {
            std::memset(buffer, 0, recordSize_);
            return false;
        }

        file_.seekg(static_cast<std::streamoff>(id) * static_cast<std::streamoff>(recordSize_),
                    std::ios::beg);
        file_.read(buffer, recordSize_);

        if (file_.gcount() != static_cast<std::streamsize>(recordSize_)) {
            std::memset(buffer, 0, recordSize_);
            file_.clear();
            return false;
        }
        return true;
    }

    // Overwrites the record with the given id.
    // Returns false if id is out of range or the write/flush failed.
    bool update(uint16_t id, const char* buffer)
    {
        if (id >= count()) return false;

        file_.seekp(static_cast<std::streamoff>(id) * static_cast<std::streamoff>(recordSize_),
                    std::ios::beg);
        file_.write(buffer, recordSize_);
        if (!file_.good()) {
            file_.clear();
            return false;
        }

        file_.flush();
        if (!file_.good()) {
            file_.clear();
            return false;
        }
        return true;
    }

    // Number of records currently in the file.
    // Non-const because it moves the get pointer to end-of-file; the next
    // read() seeks again before reading so the side effect is harmless.
    std::size_t count()
    {
        file_.seekg(0, std::ios::end);
        const std::streampos endPos = file_.tellg();
        if (endPos < 0) {
            file_.clear();
            return 0;
        }
        return static_cast<std::size_t>(endPos) / recordSize_;
    }
};

#endif
