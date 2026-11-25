// SPDX-License-Identifier: GPL-3.0-or-later
//
// Random-access input. The importer reads CD images and zip files that can be
// hundreds of megabytes, so it never loads them whole: it reads the parts it
// needs through a Source. The app implements it on top of SDL (which also
// opens Android's content:// links); the core and the tests use FileSource.
#pragma once

#include "nagus/util/bytes.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace nagus {

class Source {
public:
    virtual ~Source() = default;
    virtual std::uint64_t size() const = 0;
    // Fills `out` from `offset`. Throws FormatError when the range is not
    // inside the source or the read fails.
    virtual void read(std::uint64_t offset, std::span<std::uint8_t> out) = 0;

    Bytes readAll(std::uint64_t offset, std::size_t count) {
        Bytes b(count);
        read(offset, b);
        return b;
    }

protected:
    void check(std::uint64_t offset, std::size_t count) const {
        const std::uint64_t total = size();
        if (offset > total || count > total - offset) {
            throw FormatError("read past the end of the file");
        }
    }
};

// A part of another source, for example one file inside a CD image.
class SliceSource : public Source {
public:
    SliceSource(std::shared_ptr<Source> parent, std::uint64_t offset, std::uint64_t size)
        : parent_(std::move(parent)), offset_(offset), size_(size) {}
    std::uint64_t size() const override { return size_; }
    void read(std::uint64_t offset, std::span<std::uint8_t> out) override {
        check(offset, out.size());
        parent_->read(offset_ + offset, out);
    }

private:
    std::shared_ptr<Source> parent_;
    std::uint64_t offset_;
    std::uint64_t size_;
};

class MemorySource : public Source {
public:
    explicit MemorySource(Bytes data) : data_(std::move(data)) {}
    std::uint64_t size() const override { return data_.size(); }
    void read(std::uint64_t offset, std::span<std::uint8_t> out) override {
        check(offset, out.size());
        std::copy_n(data_.begin() + static_cast<std::ptrdiff_t>(offset), out.size(), out.begin());
    }

private:
    Bytes data_;
};

class FileSource : public Source {
public:
    explicit FileSource(const std::filesystem::path& path) : in_(path, std::ios::binary) {
        if (!in_) {
            throw FormatError("cannot open " + path.string());
        }
        in_.seekg(0, std::ios::end);
        size_ = static_cast<std::uint64_t>(in_.tellg());
    }
    std::uint64_t size() const override { return size_; }
    void read(std::uint64_t offset, std::span<std::uint8_t> out) override {
        check(offset, out.size());
        in_.clear();
        in_.seekg(static_cast<std::streamoff>(offset));
        in_.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
        if (!in_) {
            throw FormatError("read failed");
        }
    }

private:
    std::ifstream in_;
    std::uint64_t size_ = 0;
};

} // namespace nagus
