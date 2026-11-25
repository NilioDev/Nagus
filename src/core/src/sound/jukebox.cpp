// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/sound/jukebox.hpp"

#include "nagus/data/resources.hpp"

#include <algorithm>
#include <filesystem>

namespace nagus {

namespace {

constexpr std::size_t kMaxVoices = 8;

template <class T> std::unique_ptr<T> open(const Resources& res, const char* name) {
    const auto path = res.loose(name);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        return nullptr;
    }
    try {
        return std::make_unique<T>(std::make_shared<FileSource>(path));
    } catch (const FormatError&) {
        return nullptr;
    }
}

} // namespace

Jukebox::Jukebox(const Resources& resources)
    : music_(open<MusicArchive>(resources, "music.snd")), effects_(open<SoundArchive>(resources, "sfx.snd")) {}

Jukebox::Jukebox(std::unique_ptr<MusicArchive> music, std::unique_ptr<SoundArchive> effects)
    : music_(std::move(music)), effects_(std::move(effects)) {}

void Jukebox::playMusic(int track) {
    if (track == track_) {
        return;
    }
    track_ = track;
    piece_ = 0;
    current_.reset();
    if (music_ && track >= 0 && music_->pieces(track) > 0) {
        piece_ = -1;
        nextPiece();
    }
}

void Jukebox::nextPiece() {
    current_.reset();
    if (!music_ || track_ < 0) {
        return;
    }
    const int n = music_->pieces(track_);
    // A piece that cannot be read is passed over; a whole track that
    // cannot be read stops the music.
    for (int tries = 0; tries < n; ++tries) {
        piece_ = (piece_ + 1) % n;
        try {
            current_ = Voice{std::make_shared<const Pcm>(music_->piece(track_, piece_)), 0};
            return;
        } catch (const FormatError&) {
        }
    }
}

void Jukebox::playEffect(int index) {
    if (!effects_ || index < 0 || static_cast<std::size_t>(index) >= effects_->size()) {
        return;
    }
    auto& cached = effectCache_[index];
    if (!cached) {
        try {
            cached = std::make_shared<const Pcm>(effects_->sound(static_cast<std::size_t>(index)));
        } catch (const FormatError&) {
            cached = std::make_shared<const Pcm>();
        }
    }
    playSound(cached);
}

void Jukebox::playSound(std::shared_ptr<const Pcm> sound) {
    if (!sound || sound->frames() == 0) {
        return;
    }
    if (voices_.size() >= kMaxVoices) {
        voices_.erase(voices_.begin());
    }
    voices_.push_back(Voice{std::move(sound), 0});
}

void Jukebox::stopSounds() { voices_.clear(); }

void Jukebox::setVolumes(int music, int effects) {
    musicVolume_ = std::clamp(music, 0, 100);
    effectsVolume_ = std::clamp(effects, 0, 100);
}

void Jukebox::add(std::vector<int>& acc, const Pcm& pcm, double& at, int volume, std::size_t frames) const {
    // Sounds at another rate are stepped through at theirs.
    const double step = static_cast<double>(pcm.rate) / kRate;
    const std::size_t total = pcm.frames();
    const auto ch = static_cast<std::size_t>(pcm.channels);
    for (std::size_t f = 0; f < frames; ++f) {
        const auto i = static_cast<std::size_t>(at);
        if (i >= total) {
            break;
        }
        const int l = pcm.samples[i * ch];
        const int r = pcm.samples[i * ch + ch - 1];
        acc[2 * f] += l * volume / 100;
        acc[2 * f + 1] += r * volume / 100;
        at += step;
    }
}

void Jukebox::mix(std::span<std::int16_t> out) {
    const std::size_t frames = out.size() / 2;
    std::vector<int> acc(frames * 2, 0);
    // The music, piece after piece.
    std::size_t done = 0;
    for (int turns = 0; current_ && done < frames && turns < 64; ++turns) {
        const std::size_t left = static_cast<std::size_t>(
            std::max(0.0, static_cast<double>(current_->pcm->frames()) - current_->at) / (static_cast<double>(current_->pcm->rate) / kRate));
        const std::size_t n = std::min(frames - done, left);
        std::vector<int> part(n * 2, 0);
        add(part, *current_->pcm, current_->at, musicVolume_, n);
        std::copy(part.begin(), part.end(), acc.begin() + static_cast<std::ptrdiff_t>(done * 2));
        done += n;
        if (n == left) {
            nextPiece();
        }
    }
    for (auto& v : voices_) {
        add(acc, *v.pcm, v.at, effectsVolume_, frames);
    }
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
                                 [](const Voice& v) { return static_cast<std::size_t>(v.at) >= v.pcm->frames(); }),
                  voices_.end());
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = i < acc.size() ? static_cast<std::int16_t>(std::clamp(acc[i], -32768, 32767)) : std::int16_t{0};
    }
}

} // namespace nagus
