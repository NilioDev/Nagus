// SPDX-License-Identifier: GPL-3.0-or-later
//
// Byte buffers and a bounds-checked little-endian reader. Every reader of an
// outside file goes through Reader, so a short or damaged file ends in a
// FormatError instead of a read past the end (see SECURITY.md).
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace nagus {

using Bytes = std::vector<std::uint8_t>;
using ByteView = std::span<const std::uint8_t>;

// A file we read is damaged, of an unknown kind, or not what we expected.
class FormatError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Reader {
public:
    explicit Reader(ByteView data, std::size_t pos = 0) : data_(data), pos_(pos) {}

    std::size_t size() const { return data_.size(); }
    std::size_t pos() const { return pos_; }
    std::size_t left() const { return pos_ <= data_.size() ? data_.size() - pos_ : 0; }
    void seek(std::size_t pos) {
        if (pos > data_.size()) {
            throw FormatError("offset past the end of the data");
        }
        pos_ = pos;
    }
    void skip(std::size_t count) {
        need(count);
        pos_ += count;
    }

    std::uint8_t u8() {
        need(1);
        return data_[pos_++];
    }
    std::uint16_t u16() {
        need(2);
        const auto v = static_cast<std::uint16_t>(data_[pos_] | (data_[pos_ + 1] << 8));
        pos_ += 2;
        return v;
    }
    std::uint32_t u32() {
        need(4);
        const std::uint32_t v = static_cast<std::uint32_t>(data_[pos_]) |
                                (static_cast<std::uint32_t>(data_[pos_ + 1]) << 8) |
                                (static_cast<std::uint32_t>(data_[pos_ + 2]) << 16) |
                                (static_cast<std::uint32_t>(data_[pos_ + 3]) << 24);
        pos_ += 4;
        return v;
    }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }

    ByteView bytes(std::size_t count) {
        need(count);
        const ByteView v = data_.subspan(pos_, count);
        pos_ += count;
        return v;
    }

    // A fixed-size text field: stops at the first NUL and drops the spaces
    // the original pads its file names with.
    std::string text(std::size_t count) {
        const ByteView raw = bytes(count);
        std::string s;
        for (const std::uint8_t c : raw) {
            if (c == 0) {
                break;
            }
            s.push_back(static_cast<char>(c));
        }
        while (!s.empty() && s.back() == ' ') {
            s.pop_back();
        }
        return s;
    }

private:
    void need(std::size_t count) const {
        if (pos_ > data_.size() || count > data_.size() - pos_) {
            throw FormatError("data ends early");
        }
    }

    ByteView data_;
    std::size_t pos_;
};

inline std::uint16_t readU16(ByteView d, std::size_t at) { return Reader(d, at).u16(); }
inline std::uint32_t readU32(ByteView d, std::size_t at) { return Reader(d, at).u32(); }

// File names in the original's archives are compared without regard to case.
inline std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

} // namespace nagus
