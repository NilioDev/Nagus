// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/util/inflate.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace nagus::deflate {

namespace {

const std::array<std::uint32_t, 256>& crcTable() {
    static const auto table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
            }
            t[n] = c;
        }
        return t;
    }();
    return table;
}

// Length codes 257..285 and distance codes 0..29 (RFC 1951, 3.2.5).
constexpr std::array<std::uint16_t, 29> kLengthBase{3,  4,  5,  6,  7,  8,  9,  10, 11,  13,
                                                    15, 17, 19, 23, 27, 31, 35, 43, 51,  59,
                                                    67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<std::uint8_t, 29> kLengthExtra{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<std::uint16_t, 30> kDistBase{
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<std::uint8_t, 30> kDistExtra{0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                  6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

class BitReader {
public:
    explicit BitReader(const Pull& pull) : pull_(pull), buf_(1u << 16) {}

    // Makes sure at least `count` (up to 25) bits are in the accumulator,
    // padding with zeros at the end of the input; `bits` notices a read past
    // the real end.
    void fill(int count) {
        while (used_ < count) {
            std::uint32_t byte = 0;
            if (pos_ == len_ && !refill()) {
                ++overrun_;
            } else {
                byte = buf_[pos_++];
            }
            acc_ |= byte << used_;
            used_ += 8;
        }
    }
    std::uint32_t peek(int count) {
        fill(count);
        return acc_ & ((1u << count) - 1u);
    }
    void drop(int count) {
        acc_ >>= count;
        used_ -= count;
        if (overrun_ > 0 && used_ < 8 * overrun_) {
            throw FormatError("deflate stream ends early");
        }
    }
    std::uint32_t bits(int count) {
        if (count == 0) {
            return 0;
        }
        const std::uint32_t v = peek(count);
        drop(count);
        return v;
    }
    void alignToByte() {
        const int extra = used_ % 8;
        drop(extra);
    }

private:
    bool refill() {
        len_ = pull_(buf_);
        pos_ = 0;
        return len_ > 0;
    }

    const Pull& pull_;
    std::vector<std::uint8_t> buf_;
    std::size_t pos_ = 0;
    std::size_t len_ = 0;
    std::uint32_t acc_ = 0;
    int used_ = 0;
    int overrun_ = 0;
};

// Canonical Huffman code decoded through one table indexed by the next
// 15 bits: each entry holds the symbol and its code length.
class Huffman {
public:
    Huffman(const std::uint8_t* lengths, int n) : table_(1u << 15, 0) {
        std::array<int, 16> counts{};
        for (int i = 0; i < n; ++i) {
            ++counts[lengths[i]];
        }
        counts[0] = 0;
        std::array<int, 16> next{};
        int code = 0;
        for (int len = 1; len < 16; ++len) {
            code = (code + counts[static_cast<std::size_t>(len - 1)]) << 1;
            next[static_cast<std::size_t>(len)] = code;
        }
        for (int sym = 0; sym < n; ++sym) {
            const int len = lengths[sym];
            if (len == 0) {
                continue;
            }
            const int c = next[static_cast<std::size_t>(len)]++;
            if (c >= (1 << len)) {
                throw FormatError("bad Huffman code lengths in deflate stream");
            }
            // Deflate sends codes starting with their highest bit, but our
            // accumulator is filled from the lowest bit: reverse the code.
            int rev = 0;
            for (int b = 0; b < len; ++b) {
                rev |= ((c >> b) & 1) << (len - 1 - b);
            }
            const auto entry = static_cast<std::uint32_t>((sym << 4) | len);
            for (int fill = rev; fill < (1 << 15); fill += 1 << len) {
                table_[static_cast<std::size_t>(fill)] = entry;
            }
        }
    }

    int decode(BitReader& r) const {
        const std::uint32_t entry = table_[r.peek(15)];
        const int len = static_cast<int>(entry & 15u);
        if (len == 0) {
            throw FormatError("bad Huffman code in deflate stream");
        }
        r.drop(len);
        return static_cast<int>(entry >> 4);
    }

private:
    std::vector<std::uint32_t> table_;
};

// The last 32 KB of output (the furthest a back-reference reaches) and the
// piece of output not yet handed on.
class Output {
public:
    Output(const Push& push, std::uint64_t maxSize) : push_(push), maxSize_(maxSize), window_(kWindow) {
        pending_.reserve(kChunk);
    }

    void byte(std::uint8_t b) {
        if (total_ >= maxSize_) {
            throw FormatError("deflate stream unpacks to more than expected");
        }
        window_[total_ & (kWindow - 1)] = b;
        ++total_;
        pending_.push_back(b);
        if (pending_.size() >= kChunk) {
            flush();
        }
    }
    void copy(std::size_t distance, int length) {
        if (distance > total_ || distance > kWindow) {
            throw FormatError("deflate distance points before the start");
        }
        for (int i = 0; i < length; ++i) {
            byte(window_[(total_ - distance) & (kWindow - 1)]);
        }
    }
    void flush() {
        if (!pending_.empty()) {
            push_(pending_);
            pending_.clear();
        }
    }
    std::uint64_t total() const { return total_; }

private:
    static constexpr std::size_t kWindow = 32768;
    static constexpr std::size_t kChunk = 65536;
    const Push& push_;
    std::uint64_t maxSize_;
    std::vector<std::uint8_t> window_;
    std::vector<std::uint8_t> pending_;
    std::uint64_t total_ = 0;
};

void inflateBlock(BitReader& r, Output& out, const Huffman& lit, const Huffman& dist) {
    for (;;) {
        const int symbol = lit.decode(r);
        if (symbol < 256) {
            out.byte(static_cast<std::uint8_t>(symbol));
            continue;
        }
        if (symbol == 256) {
            return;
        }
        const int lc = symbol - 257;
        if (lc >= 29) {
            throw FormatError("bad length code in deflate stream");
        }
        const int length = kLengthBase[static_cast<std::size_t>(lc)] +
                           static_cast<int>(r.bits(kLengthExtra[static_cast<std::size_t>(lc)]));
        const int dc = dist.decode(r);
        if (dc >= 30) {
            throw FormatError("bad distance code in deflate stream");
        }
        const std::size_t distance =
            kDistBase[static_cast<std::size_t>(dc)] + r.bits(kDistExtra[static_cast<std::size_t>(dc)]);
        out.copy(distance, length);
    }
}

const std::pair<Huffman, Huffman>& fixedTables() {
    static const auto tables = [] {
        std::array<std::uint8_t, 288> lit{};
        for (int k = 0; k < 288; ++k) {
            lit[static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(k < 144 ? 8 : k < 256 ? 9 : k < 280 ? 7 : 8);
        }
        std::array<std::uint8_t, 30> dist{};
        dist.fill(std::uint8_t{5});
        return std::pair{Huffman(lit.data(), 288), Huffman(dist.data(), 30)};
    }();
    return tables;
}

} // namespace

std::uint32_t crc32(ByteView bytes, std::uint32_t crc) {
    const auto& table = crcTable();
    crc = ~crc;
    for (const std::uint8_t b : bytes) {
        crc = table[(crc ^ b) & 0xffu] ^ (crc >> 8);
    }
    return ~crc;
}

std::uint64_t inflate(const Pull& in, const Push& push, std::uint64_t maxSize) {
    BitReader r(in);
    Output out(push, maxSize);
    bool final = false;
    while (!final) {
        final = r.bits(1) != 0;
        const std::uint32_t type = r.bits(2);
        if (type == 0) {
            r.alignToByte();
            const std::uint32_t len = r.bits(16);
            const std::uint32_t nlen = r.bits(16);
            if ((len ^ 0xffffu) != nlen) {
                throw FormatError("bad stored block in deflate stream");
            }
            for (std::uint32_t k = 0; k < len; ++k) {
                out.byte(static_cast<std::uint8_t>(r.bits(8)));
            }
        } else if (type == 1) {
            inflateBlock(r, out, fixedTables().first, fixedTables().second);
        } else if (type == 2) {
            const int hlit = static_cast<int>(r.bits(5)) + 257;
            const int hdist = static_cast<int>(r.bits(5)) + 1;
            const int hclen = static_cast<int>(r.bits(4)) + 4;
            static constexpr std::array<int, 19> kOrder{16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                        11, 4,  12, 3, 13, 2, 14, 1, 15};
            std::array<std::uint8_t, 19> codeLengths{};
            for (int k = 0; k < hclen; ++k) {
                codeLengths[static_cast<std::size_t>(kOrder[static_cast<std::size_t>(k)])] =
                    static_cast<std::uint8_t>(r.bits(3));
            }
            const Huffman lengthCodes(codeLengths.data(), 19);
            std::vector<std::uint8_t> lengths;
            while (static_cast<int>(lengths.size()) < hlit + hdist) {
                const int symbol = lengthCodes.decode(r);
                if (symbol < 16) {
                    lengths.push_back(static_cast<std::uint8_t>(symbol));
                } else if (symbol == 16) {
                    if (lengths.empty()) {
                        throw FormatError("bad code lengths in deflate stream");
                    }
                    const std::uint8_t last = lengths.back();
                    for (std::uint32_t k = 3 + r.bits(2); k > 0; --k) {
                        lengths.push_back(last);
                    }
                } else {
                    const std::uint32_t repeat = symbol == 17 ? 3 + r.bits(3) : 11 + r.bits(7);
                    lengths.insert(lengths.end(), repeat, std::uint8_t{0});
                }
            }
            if (static_cast<int>(lengths.size()) != hlit + hdist) {
                throw FormatError("bad code lengths in deflate stream");
            }
            const Huffman lit(lengths.data(), hlit);
            const Huffman dist(lengths.data() + hlit, hdist);
            inflateBlock(r, out, lit, dist);
        } else {
            throw FormatError("bad block type in deflate stream");
        }
    }
    out.flush();
    return out.total();
}

Bytes inflate(ByteView in, std::size_t maxSize) {
    std::size_t pos = 0;
    const Pull pull = [&](std::span<std::uint8_t> buf) {
        const std::size_t n = std::min(buf.size(), in.size() - pos);
        std::copy_n(in.begin() + static_cast<std::ptrdiff_t>(pos), n, buf.begin());
        pos += n;
        return n;
    };
    Bytes out;
    const Push push = [&](ByteView piece) { out.insert(out.end(), piece.begin(), piece.end()); };
    inflate(pull, push, maxSize);
    return out;
}

} // namespace nagus::deflate
