// SPDX-License-Identifier: GPL-3.0-or-later
//
// Random numbers that come out the same on every system for the same seed,
// so that a game started from a seed (or a test) is the same everywhere.
// std::mt19937 itself is fixed by the standard; its distributions are not,
// so these are written out here.
#pragma once

#include <cstdint>
#include <random>

namespace nagus {

class Random {
public:
    explicit Random(std::uint32_t seed = 1) : engine_(seed) {}

    std::uint32_t next() { return static_cast<std::uint32_t>(engine_()); }
    // A whole number from 0 up to (not including) n; 0 when n is 0.
    std::uint32_t below(std::uint32_t n) {
        if (n == 0) {
            return 0;
        }
        // Rejects the uneven top end so that every result is equally likely.
        const std::uint32_t limit = 0xffffffffu - (0xffffffffu % n);
        std::uint32_t v = next();
        while (v >= limit) {
            v = next();
        }
        return v % n;
    }
    int between(int lo, int hi) { return lo + static_cast<int>(below(static_cast<std::uint32_t>(hi - lo + 1))); }
    // A number from 0 up to (not including) 1.
    double unit() { return static_cast<double>(next() >> 5) / static_cast<double>(1u << 27); }
    bool chance(double p) { return unit() < p; }

    template <class T> void shuffle(T& items) {
        for (std::size_t i = items.size(); i > 1; --i) {
            const std::size_t j = below(static_cast<std::uint32_t>(i));
            std::swap(items[i - 1], items[j]);
        }
    }

private:
    std::mt19937 engine_;
};

} // namespace nagus
