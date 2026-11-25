// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/video/avi.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace nagus {

namespace {

constexpr std::uint32_t kMaxFrameBytes = 4u << 20;
constexpr std::size_t kMaxFrames = 100000;
constexpr std::size_t kMaxSoundSamples = 32u << 20;
constexpr int kMaxDepth = 4;
constexpr int kMaxSize = 4096;

std::string fourcc(ByteView b, std::size_t at) {
    return std::string(reinterpret_cast<const char*>(b.data() + at), 4);
}

} // namespace

AviFile::AviFile(std::shared_ptr<Source> source) : source_(std::move(source)) {
    const std::uint64_t total = source_->size();
    if (total < 12) {
        throw FormatError("AVI: too short");
    }
    const Bytes head = source_->readAll(0, 12);
    if (fourcc(head, 0) != "RIFF" || fourcc(head, 8) != "AVI ") {
        throw FormatError("AVI: not an AVI file");
    }
    const std::uint64_t end = std::min<std::uint64_t>(total, 8ull + readU32(head, 4));
    readList(12, end, 0);
    if (sound_.channels < 1 || sound_.channels > 2 || sound_.rate < 1000 || sound_.rate > 192000) {
        sound_ = Pcm{};
    }
    if (videoStream_ < 0 || width_ <= 0 || height_ <= 0 || frameTime_ == 0) {
        throw FormatError("AVI: no usable picture stream");
    }
}

void AviFile::readList(std::uint64_t at, std::uint64_t end, int depth) {
    if (depth > kMaxDepth) {
        throw FormatError("AVI: lists too deep");
    }
    while (at + 8 <= end) {
        const Bytes h = source_->readAll(at, 8);
        const std::string id = fourcc(h, 0);
        const std::uint32_t size = readU32(h, 4);
        const std::uint64_t body = at + 8;
        if (size > end - body) {
            break; // a cut-off file: keep what came before
        }
        if (id == "LIST" && size >= 4) {
            readList(body + 4, body + size, depth + 1);
        } else if (id == "avih" && size >= 4) {
            frameTime_ = readU32(source_->readAll(body, 4), 0);
        } else if (id == "strh" && size >= 4) {
            const std::string type = fourcc(source_->readAll(body, 4), 0);
            if (type == "vids" && videoStream_ < 0) {
                videoStream_ = streams_;
            } else if (type == "auds" && audioStream_ < 0) {
                audioStream_ = streams_;
            }
            ++streams_;
        } else if (id == "strf" && streams_ > 0) {
            const int stream = streams_ - 1;
            if (stream == videoStream_ && size >= 20) {
                const Bytes f = source_->readAll(body, 20);
                Reader r(f);
                r.u32();
                const auto w = static_cast<std::int32_t>(r.u32());
                const auto hgt = static_cast<std::int32_t>(r.u32());
                r.u16();
                r.u16();
                if (fourcc(f, 16) != "cvid" || w <= 0 || w > kMaxSize || hgt == 0 || std::abs(hgt) > kMaxSize) {
                    throw FormatError("AVI: pictures not in Cinepak");
                }
                width_ = w;
                height_ = std::abs(hgt);
            } else if (stream == audioStream_ && size >= 16) {
                const Bytes f = source_->readAll(body, 16);
                Reader r(f);
                audioFormat_ = r.u16();
                sound_.channels = r.u16();
                sound_.rate = static_cast<int>(r.u32());
                r.u32();
                r.u16();
                audioBits_ = r.u16();
            }
        } else if (id.size() == 4 && std::isdigit(static_cast<unsigned char>(id[0])) &&
                   std::isdigit(static_cast<unsigned char>(id[1]))) {
            const int stream = (id[0] - '0') * 10 + (id[1] - '0');
            if (stream == videoStream_ && id.substr(2) != "pc" && size <= kMaxFrameBytes && video_.size() < kMaxFrames) {
                video_.push_back({body, size});
            } else if (stream == audioStream_ && audioFormat_ == 1 && sound_.channels >= 1 && sound_.channels <= 2 &&
                       sound_.rate >= 1000 && sound_.rate <= 192000 && sound_.samples.size() < kMaxSoundSamples) {
                const Bytes d = source_->readAll(body, std::min<std::uint32_t>(size, kMaxFrameBytes));
                if (audioBits_ == 16) {
                    for (std::size_t i = 0; i + 1 < d.size(); i += 2) {
                        sound_.samples.push_back(static_cast<std::int16_t>(d[i] | (d[i + 1] << 8)));
                    }
                } else if (audioBits_ == 8) {
                    for (const std::uint8_t v : d) {
                        sound_.samples.push_back(static_cast<std::int16_t>((v - 128) * 256));
                    }
                }
            }
        }
        at = body + size + (size & 1u);
    }
    if (sound_.channels >= 1) {
        sound_.samples.resize(sound_.samples.size() - sound_.samples.size() % static_cast<std::size_t>(std::clamp(sound_.channels, 1, 2)));
    }
}

Bytes AviFile::frame(std::size_t index) const {
    if (index >= video_.size()) {
        throw FormatError("AVI: no such frame");
    }
    return source_->readAll(video_[index].offset, video_[index].size);
}

} // namespace nagus
