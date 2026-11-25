// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/sound/archives.hpp"

namespace nagus {

namespace {

constexpr std::size_t kEntry = 16;
constexpr std::size_t kMaxEntries = 100000;
constexpr int kMaxTracks = 256;
constexpr std::uint32_t kMaxPieces = 1000;

// Raw IMA ADPCM: flag 2 means two channels; blocks are 512 bytes for each
// channel.
Pcm decodeRaw(ByteView data, std::uint32_t length, std::uint32_t flags) {
    Pcm pcm;
    pcm.rate = kSoundRate;
    pcm.channels = (flags & 2u) ? 2 : 1;
    const std::size_t frames = length / (2u * static_cast<std::uint32_t>(pcm.channels));
    pcm.samples = decodeImaAdpcm(data, pcm.channels, 512u * static_cast<std::size_t>(pcm.channels), frames);
    return pcm;
}

} // namespace

SoundArchive::SoundArchive(std::shared_ptr<Source> source) : source_(std::move(source)) {
    const std::uint64_t total = source_->size();
    if (total < 4) {
        throw FormatError("sound archive: too short");
    }
    const std::uint32_t table = readU32(source_->readAll(0, 4), 0);
    if (table < 4 || table > total || (total - table) % kEntry != 0 || (total - table) / kEntry > kMaxEntries) {
        throw FormatError("sound archive: bad table");
    }
    const Bytes t = source_->readAll(table, static_cast<std::size_t>(total - table));
    Reader r(t);
    for (std::size_t i = 0; i < t.size() / kEntry; ++i) {
        Entry e;
        e.offset = r.u32();
        e.size = r.u32();
        e.length = r.u32();
        e.flags = r.u32();
        if (e.offset < 4 || e.offset > table || e.size > table - e.offset || e.size > kMaxSoundBytes) {
            throw FormatError("sound archive: entry outside the file");
        }
        entries_.push_back(e);
    }
}

Pcm SoundArchive::sound(std::size_t index) const {
    if (index >= entries_.size()) {
        throw FormatError("sound archive: no such sound");
    }
    const Entry& e = entries_[index];
    const Bytes data = source_->readAll(e.offset, e.size);
    if (data.size() >= 4 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F') {
        return readWav(data);
    }
    // Speech: the third number is its decoded length.
    if (e.length > kMaxSoundBytes * 8) {
        throw FormatError("sound archive: sound too long");
    }
    return decodeRaw(data, e.length, e.flags);
}

MusicArchive::MusicArchive(std::shared_ptr<Source> source) : source_(std::move(source)) {
    const std::uint64_t total = source_->size();
    if (total < 8) {
        throw FormatError("music: too short");
    }
    // A list of (pieces, where their table is) for each track, up to where
    // the first piece's sound begins.
    const Bytes first = source_->readAll(0, 8);
    const std::uint32_t firstTable = readU32(first, 4);
    if (firstTable > total || total - firstTable < kEntry) {
        throw FormatError("music: bad table");
    }
    const std::uint32_t headerEnd = readU32(source_->readAll(firstTable, 4), 0);
    if (headerEnd < 8 || headerEnd % 8 != 0 || headerEnd / 8 > kMaxTracks || headerEnd > total) {
        throw FormatError("music: bad header");
    }
    const Bytes header = source_->readAll(0, headerEnd);
    Reader h(header);
    for (std::uint32_t t = 0; t < headerEnd / 8; ++t) {
        const std::uint32_t count = h.u32();
        const std::uint32_t at = h.u32();
        if (count > kMaxPieces || at > total || (total - at) / kEntry < count) {
            throw FormatError("music: bad track");
        }
        const Bytes list = source_->readAll(at, count * kEntry);
        Reader r(list);
        std::vector<Piece> pieces;
        for (std::uint32_t i = 0; i < count; ++i) {
            Piece p;
            p.offset = r.u32();
            p.size = r.u32();
            p.length = r.u32();
            p.flags = r.u32();
            if (p.offset > total || p.size > total - p.offset || p.size > kMaxSoundBytes || p.length / 8 > p.size) {
                throw FormatError("music: piece outside the file");
            }
            pieces.push_back(p);
        }
        tracks_.push_back(std::move(pieces));
    }
}

int MusicArchive::pieces(int track) const {
    return track >= 0 && track < tracks() ? static_cast<int>(tracks_[static_cast<std::size_t>(track)].size()) : 0;
}

Pcm MusicArchive::piece(int track, int index) const {
    if (index < 0 || index >= pieces(track)) {
        throw FormatError("music: no such piece");
    }
    const Piece& p = tracks_[static_cast<std::size_t>(track)][static_cast<std::size_t>(index)];
    return decodeRaw(source_->readAll(p.offset, p.size), p.length, p.flags);
}

} // namespace nagus
