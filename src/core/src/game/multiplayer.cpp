// SPDX-License-Identifier: GPL-3.0-or-later
#include "nagus/game/multiplayer.hpp"

#include "nagus/game/diplomacy.hpp"

#include <algorithm>
#include <limits>

namespace nagus {

namespace {

// Limits for what a report may hold.
constexpr std::uint32_t kMaxItems = 10000;
constexpr std::uint32_t kMaxValues = 16;
constexpr int kBig = 1 << 30;
constexpr std::uint32_t kMet = 0xffffffffu;
constexpr std::uint32_t kJoined = 0xfffffffeu;

std::uint32_t count(Reader& r, std::uint32_t max) {
    const std::uint32_t n = r.u32();
    if (n > max) {
        throw FormatError("damaged turn report");
    }
    return n;
}

int number(Reader& r, int lo, int hi) {
    const auto v = static_cast<std::int32_t>(r.u32());
    if (v < lo || v > hi) {
        throw FormatError("damaged turn report");
    }
    return v;
}

std::string text(Reader& r) {
    const ByteView b = r.bytes(r.u16());
    return std::string(b.begin(), b.end());
}

void writeTexts(Writer& w, const std::vector<std::string>& texts) {
    w.u32(static_cast<std::uint32_t>(texts.size()));
    for (const auto& t : texts) {
        w.text(t);
    }
}

std::vector<std::string> readTexts(Reader& r, std::uint32_t max) {
    std::vector<std::string> out;
    for (std::uint32_t i = 0, n = count(r, max); i < n; ++i) {
        out.push_back(text(r));
    }
    return out;
}

void writeInts(Writer& w, const std::vector<int>& v) {
    w.u32(static_cast<std::uint32_t>(v.size()));
    for (const int x : v) {
        w.i32(x);
    }
}

std::vector<int> readInts(Reader& r) {
    std::vector<int> out;
    for (std::uint32_t i = 0, n = count(r, kMaxItems); i < n; ++i) {
        out.push_back(number(r, -kBig, kBig));
    }
    return out;
}

bool sameOffer(const Offer& a, const Offer& b) {
    return a.from == b.from && a.to == b.to && a.treaty == b.treaty && a.turn == b.turn;
}

bool sameRelation(const Relation& a, const Relation& b) {
    return a.treaty == b.treaty && a.regard == b.regard && a.since == b.since;
}

const Colony* colonyOn(const Game& g, int system) {
    for (const auto& c : g.colonies) {
        if (c.system == system) {
            return &c;
        }
    }
    return nullptr;
}

} // namespace

void mergeOrders(Game& merged, const Game& start, const Game& played, int empire) {
    if (empire < 0 || empire >= kEmpires) {
        return;
    }
    // Colonies: the player's own, and those that changed hands by what the
    // player did (a minor race joining).
    for (const auto& c : played.colonies) {
        const Colony* before = colonyOn(start, c.system);
        if (c.owner != empire && before != nullptr && before->owner == c.owner) {
            continue;
        }
        if (Colony* now = merged.colonyAt(c.system); now != nullptr) {
            *now = c;
        } else {
            merged.colonies.push_back(c);
        }
    }
    // Fleets: where they go and what they do, and how the player split
    // and joined them. Fleets split off this turn get new numbers, which
    // other players' new fleets cannot have.
    std::erase_if(merged.fleets, [&](const Fleet& f) { return f.owner == empire; });
    for (Fleet f : played.fleets) {
        if (f.owner != empire) {
            continue;
        }
        const Fleet* before = start.fleet(f.id);
        if (before == nullptr || before->owner != empire) {
            f.id = merged.nextFleetId++;
        }
        merged.fleets.push_back(std::move(f));
    }
    // Credits: whatever the player spent or gave, from everyone's purse.
    for (std::size_t e = 0; e < merged.empires.size(); ++e) {
        merged.empires[e].credits += played.empires[e].credits - start.empires[e].credits;
    }
    // Research and intelligence plans.
    const std::size_t me = static_cast<std::size_t>(empire);
    const int credits = merged.empires[me].credits;
    merged.empires[me] = played.empires[me];
    merged.empires[me].credits = credits;
    // Treaties and regard the player changed.
    if (played.relations.size() == start.relations.size() && merged.relations.size() == start.relations.size()) {
        for (std::size_t i = 0; i < played.relations.size(); ++i) {
            if (!sameRelation(played.relations[i], start.relations[i])) {
                merged.relations[i] = played.relations[i];
            }
        }
    }
    // Proposals the player answered, and those they made.
    for (const auto& o : start.offers) {
        const bool answered = std::none_of(played.offers.begin(), played.offers.end(), [&](const Offer& p) { return sameOffer(o, p); });
        if (answered && o.to == empire) {
            std::erase_if(merged.offers, [&](const Offer& m) { return sameOffer(m, o); });
        }
    }
    for (const auto& o : played.offers) {
        const bool made = std::none_of(start.offers.begin(), start.offers.end(), [&](const Offer& s) { return sameOffer(o, s); });
        if (made && o.from == empire &&
            std::none_of(merged.offers.begin(), merged.offers.end(), [&](const Offer& m) { return sameOffer(m, o); })) {
            merged.offers.push_back(o);
        }
    }
}

std::vector<PlayerTurn> endMultiplayerTurn(const Game& merged) {
    std::vector<PlayerTurn> out;
    std::vector<int> humans{merged.player};
    for (int e = 0; e < kEmpires; ++e) {
        if (e != merged.player && merged.isHuman(e)) {
            humans.push_back(e);
        }
    }
    for (const int h : humans) {
        PlayerTurn t;
        t.empire = h;
        t.game = merged;
        // Every human player stays one when another is "the player".
        for (const int o : humans) {
            t.game.humans[static_cast<std::size_t>(o)] = true;
        }
        t.game.player = h;
        t.summary = endTurn(t.game);
        out.push_back(std::move(t));
    }
    return out;
}

Bytes saveSummary(const TurnSummary& s) {
    Writer w;
    writeTexts(w, s.events);
    writeTexts(w, s.researched);
    writeTexts(w, s.colonized);
    w.u32(static_cast<std::uint32_t>(s.battles.size()));
    for (const auto& b : s.battles) {
        w.i32(b.at.x);
        w.i32(b.at.y);
        writeInts(w, b.owners);
        writeInts(w, b.ships);
        writeInts(w, b.lost);
    }
    w.u32(static_cast<std::uint32_t>(s.assaults.size()));
    for (const auto& a : s.assaults) {
        w.i32(a.system);
        w.i32(a.attacker);
        w.i32(a.defender);
        w.i32(a.killed);
        w.u8(a.captured ? 1 : 0);
    }
    w.u32(static_cast<std::uint32_t>(s.diplomacy.size()));
    for (const auto& d : s.diplomacy) {
        w.u32(d.text == kTextMet ? kMet : d.text == kTextJoined ? kJoined : static_cast<std::uint32_t>(d.text));
        w.i32(d.a);
        w.i32(d.b);
    }
    w.u32(static_cast<std::uint32_t>(s.intel.size()));
    for (const auto& r : s.intel) {
        w.i32(r.turn);
        w.i32(r.attacker);
        w.i32(r.target);
        w.u8(r.sabotage ? 1 : 0);
        w.u8(static_cast<std::uint32_t>(r.focus));
        w.u8(r.success ? 1 : 0);
        w.i32(r.value);
        w.i32(r.system);
    }
    writeInts(w, s.fallen);
    w.u32(static_cast<std::uint32_t>(s.happenings.size()));
    for (const auto& h : s.happenings) {
        w.u32(static_cast<std::uint32_t>(h.text));
        writeTexts(w, h.values);
        w.i32(h.empire);
        w.i32(h.system);
    }
    w.u8(static_cast<std::uint32_t>(s.ending));
    return w.take();
}

TurnSummary loadSummary(ByteView data) {
    Reader r(data);
    TurnSummary s;
    s.events = readTexts(r, kMaxItems);
    s.researched = readTexts(r, kMaxItems);
    s.colonized = readTexts(r, kMaxItems);
    for (std::uint32_t i = 0, n = count(r, kMaxItems); i < n; ++i) {
        Battle b;
        b.at.x = number(r, -1, kBig);
        b.at.y = number(r, -1, kBig);
        b.owners = readInts(r);
        b.ships = readInts(r);
        b.lost = readInts(r);
        if (b.ships.size() != b.owners.size() || b.lost.size() != b.owners.size()) {
            throw FormatError("damaged turn report");
        }
        s.battles.push_back(std::move(b));
    }
    for (std::uint32_t i = 0, n = count(r, kMaxItems); i < n; ++i) {
        Assault a;
        a.system = number(r, -1, kBig);
        a.attacker = number(r, -1, kBig);
        a.defender = number(r, -1, kBig);
        a.killed = number(r, 0, kBig);
        a.captured = r.u8() != 0;
        s.assaults.push_back(a);
    }
    for (std::uint32_t i = 0, n = count(r, kMaxItems); i < n; ++i) {
        DiplomacyEvent d;
        const std::uint32_t t = r.u32();
        d.text = t == kMet ? kTextMet : t == kJoined ? kTextJoined : t;
        d.a = number(r, -1, kBig);
        d.b = number(r, -1, kBig);
        s.diplomacy.push_back(d);
    }
    for (std::uint32_t i = 0, n = count(r, kMaxItems); i < n; ++i) {
        IntelReport ir;
        ir.turn = number(r, 0, kBig);
        ir.attacker = number(r, -1, kEmpires - 1);
        ir.target = number(r, -1, kEmpires - 1);
        ir.sabotage = r.u8() != 0;
        const int focus = r.u8();
        if (focus >= kIntelFocuses) {
            throw FormatError("damaged turn report");
        }
        ir.focus = static_cast<IntelFocus>(focus);
        ir.success = r.u8() != 0;
        ir.value = number(r, -kBig, kBig);
        ir.system = number(r, -1, kBig);
        s.intel.push_back(ir);
    }
    s.fallen = readInts(r);
    for (std::uint32_t i = 0, n = count(r, kMaxItems); i < n; ++i) {
        GameEvent h;
        h.text = r.u32();
        h.values = readTexts(r, kMaxValues);
        h.empire = number(r, -1, kBig);
        h.system = number(r, -1, kBig);
        s.happenings.push_back(std::move(h));
    }
    const int ending = r.u8();
    if (ending >= kEndings) {
        throw FormatError("damaged turn report");
    }
    s.ending = static_cast<Ending>(ending);
    return s;
}

} // namespace nagus
