// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/galaxy.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace nagus {

namespace {

// Share of the sectors that hold an object, inside the galaxy's shape.
constexpr double kDensity = 0.18;
// Objects next to an object just placed are less likely.
constexpr double kNeighbourDamping = 0.3;
// Empire homes keep at least this far (in sectors) from minor race homes.
constexpr int kMinorDistance = 3;

double sq(double v) { return v * v; }

int distance(Sector a, Sector b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }

// How likely each sector is to hold an object, for the shape.
std::vector<double> shapeWeights(GalaxyShape shape, int w, int h, Random& rng) {
    struct Blob {
        double x, y, r;
    };
    std::vector<Blob> blobs;
    if (shape == GalaxyShape::Irregular) {
        const int n = rng.between(6, 9);
        for (int i = 0; i < n; ++i) {
            blobs.push_back({rng.unit() * 1.6 - 0.8, rng.unit() * 1.6 - 0.8, 0.25 + rng.unit() * 0.25});
        }
    }
    const double twist = 2.6 + rng.unit() * 0.8;
    const double turn = rng.unit() * 2 * std::numbers::pi;
    std::vector<double> weights(static_cast<std::size_t>(w * h));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double u = (x + 0.5) / w * 2 - 1;
            const double v = (y + 0.5) / h * 2 - 1;
            const double r = std::sqrt(u * u + v * v);
            double weight = 0;
            switch (shape) {
            case GalaxyShape::Irregular:
                for (const auto& b : blobs) {
                    weight = std::max(weight, std::exp(-(sq(u - b.x) + sq(v - b.y)) / sq(b.r)));
                }
                break;
            case GalaxyShape::Elliptical: weight = std::max(0.0, 1 - r * r); break;
            case GalaxyShape::Ring: weight = std::exp(-sq((r - 0.7) / 0.17)); break;
            case GalaxyShape::Spiral: {
                const double angle = std::atan2(v, u);
                weight = std::exp(-sq(r / 0.18));
                for (int arm = 0; arm < 2; ++arm) {
                    const double want = turn + arm * std::numbers::pi + twist * r;
                    double d = std::remainder(angle - want, 2 * std::numbers::pi);
                    weight = std::max(weight, std::exp(-sq(d * r / 0.13)) * std::max(0.0, 1 - r * r * 0.9));
                }
                break;
            }
            }
            weights[static_cast<std::size_t>(y * w + x)] = weight;
        }
    }
    return weights;
}

// Picks an index with chance in proportion to its weight.
std::size_t pickWeighted(const std::vector<double>& weights, Random& rng) {
    double total = 0;
    for (const double v : weights) {
        total += v;
    }
    double at = rng.unit() * total;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        at -= weights[i];
        if (at < 0 && weights[i] > 0) {
            return i;
        }
    }
    for (std::size_t i = weights.size(); i > 0; --i) {
        if (weights[i - 1] > 0) {
            return i - 1;
        }
    }
    return 0;
}

bool isStar(const StarObjectKind& k) { return k.code.size() == 4 && k.code[0] == 's' && k.code[1] == '-'; }

} // namespace

int Galaxy::systemAt(Sector s) const {
    for (std::size_t i = 0; i < systems.size(); ++i) {
        if (systems[i].sector == s) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::pair<int, int> galaxyDimensions(GalaxySize size) {
    switch (size) {
    case GalaxySize::Small: return {20, 15};
    case GalaxySize::Medium: return {30, 22};
    case GalaxySize::Large: return {40, 30};
    }
    return {20, 15};
}

int minorRaceCount(GalaxySize size, MinorRaces minors, int available) {
    static constexpr int kCounts[3][4] = {{0, 5, 10, 15}, {0, 8, 15, 22}, {0, 12, 20, 29}};
    return std::min(available, kCounts[static_cast<int>(size)][static_cast<int>(minors)]);
}

Galaxy makeGalaxy(const GalaxySetup& setup, const GalaxyData& data) {
    if (data.kinds.empty()) {
        throw FormatError("no map object kinds");
    }
    Random rng(setup.seed);
    Galaxy g;
    std::tie(g.width, g.height) = galaxyDimensions(setup.size);
    g.homes.fill(-1);

    std::vector<double> weights = shapeWeights(setup.shape, g.width, g.height, rng);
    const int wanted = static_cast<int>(std::lround(g.width * g.height * kDensity));

    // Object kinds, by their shares.
    std::vector<double> shares;
    for (const auto& k : data.kinds) {
        shares.push_back(std::max(0.0f, k.share));
    }
    int starKind = -1;
    for (std::size_t i = 0; i < data.kinds.size(); ++i) {
        if (data.kinds[i].code == "s-y-" || (starKind < 0 && isStar(data.kinds[i]))) {
            starKind = static_cast<int>(i);
        }
    }

    for (int n = 0; n < wanted; ++n) {
        const std::size_t at = pickWeighted(weights, rng);
        if (weights[at] <= 0) {
            break;
        }
        StarSystem s;
        s.sector = {static_cast<int>(at) % g.width, static_cast<int>(at) / g.width};
        s.kind = static_cast<int>(pickWeighted(shares, rng));
        s.star = isStar(data.kinds[static_cast<std::size_t>(s.kind)]);
        g.systems.push_back(s);
        weights[at] = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const Sector n2{s.sector.x + dx, s.sector.y + dy};
                if (g.inside(n2)) {
                    weights[static_cast<std::size_t>(n2.y * g.width + n2.x)] *= kNeighbourDamping;
                }
            }
        }
    }

    // The empires' homes: as far from each other as the map allows. Any
    // object may become a home; it turns into a star.
    std::vector<int> taken;
    for (int e = 0; e < kEmpires && !g.systems.empty(); ++e) {
        int best = -1;
        int bestScore = -1;
        for (std::size_t i = 0; i < g.systems.size(); ++i) {
            if (std::find(taken.begin(), taken.end(), static_cast<int>(i)) != taken.end()) {
                continue;
            }
            int nearest = 1000;
            for (const int t : taken) {
                nearest = std::min(nearest, distance(g.systems[i].sector, g.systems[static_cast<std::size_t>(t)].sector));
            }
            // Away from the edge, and a little chance in the choice.
            const Sector s = g.systems[i].sector;
            const int edge = std::min({s.x, s.y, g.width - 1 - s.x, g.height - 1 - s.y});
            const int score = (taken.empty() ? 0 : nearest * 16) + std::min(edge, 3) * 12 + static_cast<int>(rng.below(8));
            if (score > bestScore) {
                bestScore = score;
                best = static_cast<int>(i);
            }
        }
        taken.push_back(best);
        auto& home = g.systems[static_cast<std::size_t>(best)];
        home.race = e;
        home.kind = starKind >= 0 ? starKind : home.kind;
        home.star = true;
        home.name = data.homeNames[static_cast<std::size_t>(e)];
        g.homes[static_cast<std::size_t>(e)] = best;
    }

    // Minor races, on stars away from the empires.
    std::vector<int> minors;
    for (int r = kEmpires; r < static_cast<int>(data.races.size()); ++r) {
        minors.push_back(r);
    }
    rng.shuffle(minors);
    const int minorCount = minorRaceCount(setup.size, setup.minors, static_cast<int>(minors.size()));
    // Stars away from the empires first, then nearer stars, then other
    // objects (which become stars), each group in random order.
    std::vector<int> far, near, other;
    for (std::size_t i = 0; i < g.systems.size(); ++i) {
        const auto& s = g.systems[i];
        if (s.race >= 0) {
            continue;
        }
        bool away = true;
        for (const int h : g.homes) {
            away = away && (h < 0 || distance(s.sector, g.systems[static_cast<std::size_t>(h)].sector) >= kMinorDistance);
        }
        (s.star ? (away ? far : near) : other).push_back(static_cast<int>(i));
    }
    rng.shuffle(far);
    rng.shuffle(near);
    rng.shuffle(other);
    far.insert(far.end(), near.begin(), near.end());
    far.insert(far.end(), other.begin(), other.end());
    for (int m = 0; m < minorCount && m < static_cast<int>(far.size()); ++m) {
        auto& s = g.systems[static_cast<std::size_t>(far[static_cast<std::size_t>(m)])];
        s.race = minors[static_cast<std::size_t>(m)];
        if (!s.star && starKind >= 0) {
            s.kind = starKind;
            s.star = true;
        }
    }

    // Names for the other stars.
    std::vector<std::string> names = data.starNames;
    rng.shuffle(names);
    std::size_t next = 0;
    for (auto& s : g.systems) {
        if (s.star && s.name.empty()) {
            while (next < names.size() &&
                   std::find(data.homeNames.begin(), data.homeNames.end(), names[next]) != data.homeNames.end()) {
                ++next;
            }
            s.name = next < names.size() ? names[next++] : "System " + std::to_string(next++ + 1);
        }
    }
    return g;
}

} // namespace nagus
