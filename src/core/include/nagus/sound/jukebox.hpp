// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plays the music and the sounds: the shell says which track and which
// sounds, and the app asks for the mixed samples (stereo, 22050 a second)
// as the sound device needs them. Without the sound files it stays silent.
#pragma once

#include "nagus/sound/archives.hpp"

#include <map>
#include <optional>
#include <span>

namespace nagus {

class Resources;

class Jukebox {
public:
    static constexpr int kRate = kSoundRate;

    Jukebox() = default;
    // Opens music.snd and sfx.snd from the library, when they are there.
    explicit Jukebox(const Resources& resources);
    // For tests: archives given directly (either may be null).
    Jukebox(std::unique_ptr<MusicArchive> music, std::unique_ptr<SoundArchive> effects);

    // Plays a track of music.snd from its start, its pieces in turn and
    // again from the first; the same track keeps playing; -1 stops.
    void playMusic(int track);
    int musicTrack() const { return track_; }
    // Plays a sound of sfx.snd once, over the music.
    void playEffect(int index);
    // Plays a sound once: a speech or a film's sound track (at the speech
    // volume) or a sound effect.
    void playSound(std::shared_ptr<const Pcm> sound, bool speech = true);
    void stopSounds();
    bool soundsPlaying() const { return !voices_.empty(); }

    // Volumes, 0 to 100.
    void setVolumes(int music, int effects, int speech);
    int musicVolume() const { return musicVolume_; }
    int effectsVolume() const { return effectsVolume_; }
    int speechVolume() const { return speechVolume_; }

    // Fills interleaved stereo frames.
    void mix(std::span<std::int16_t> out);

private:
    struct Voice {
        std::shared_ptr<const Pcm> pcm;
        double at = 0; // frames into it
        bool speech = false;
    };
    void nextPiece();
    void add(std::vector<int>& acc, const Pcm& pcm, double& at, int volume, std::size_t frames) const;

    std::unique_ptr<MusicArchive> music_;
    std::unique_ptr<SoundArchive> effects_;
    std::map<int, std::shared_ptr<const Pcm>> effectCache_;
    int track_ = -1;
    int piece_ = 0;
    std::optional<Voice> current_;
    std::vector<Voice> voices_;
    int musicVolume_ = 70;
    int effectsVolume_ = 80;
    int speechVolume_ = 90;
};

} // namespace nagus
