// SPDX-License-Identifier: GPL-3.0-or-later
//
// Our own DEFLATE (RFC 1951) decoder and CRC-32. It streams: the game's CD
// image comes zipped at over 400 MB, and the importer unpacks it piece by
// piece without holding it in memory. The decoder grew out of the one in
// Viceroy (same author, GPL-3.0-or-later), made table-driven and streaming.
#pragma once

#include "nagus/util/bytes.hpp"

#include <cstdint>
#include <functional>

namespace nagus::deflate {

std::uint32_t crc32(ByteView bytes, std::uint32_t crc = 0);

// Gives the decoder more compressed bytes: fills the span (or part of it) and
// returns how many it wrote, 0 at the end of the input.
using Pull = std::function<std::size_t(std::span<std::uint8_t>)>;
// Takes the next piece of unpacked data.
using Push = std::function<void(ByteView)>;

// Unpacks one raw DEFLATE stream. Throws FormatError on damaged input or when
// the output would grow beyond `maxSize` (zip bombs). Returns the unpacked
// size.
std::uint64_t inflate(const Pull& in, const Push& out, std::uint64_t maxSize);

// The same for a stream already in memory.
Bytes inflate(ByteView in, std::size_t maxSize);

} // namespace nagus::deflate
