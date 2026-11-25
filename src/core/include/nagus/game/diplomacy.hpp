// SPDX-License-Identifier: GPL-3.0-or-later
//
// Diplomacy: meeting other races, treaties from war to alliance, gifts, and
// minor races joining an empire. How races decide is our own design (see
// docs/rules/diplomacy.md); where they start comes from the race table.
#pragma once

#include "nagus/game/game.hpp"

#include <limits>

namespace nagus {

inline constexpr std::size_t kTextMet = std::numeric_limits<std::size_t>::max();     // "%1 met %2"
inline constexpr std::size_t kTextJoined = std::numeric_limits<std::size_t>::max() - 1; // "%2 joined %1"

// Sets up the races and their first regard for each empire (from the race
// table), all unmet.
void startDiplomacy(Game& game, const std::vector<RaceInfo>& races);

// The treaty between an empire and a race (either way round for two
// empires), and how the race regards the empire.
Treaty treaty(const Game& game, int a, int b);
int regard(const Game& game, int race, int empire);
bool atWar(const Game& game, int a, int b);
bool met(const Game& game, int a, int b);
bool isMinor(const Game& game, int race);
// The system a minor race lives on, or -1.
int minorHome(const Game& game, int race);

// Whether an empire may propose a treaty to a race now, and whether the race
// would agree.
bool canPropose(const Game& game, int from, int to, Treaty t);
bool wouldAccept(const Game& game, int from, int to, Treaty t);
enum class Reply { Accepted, Rejected, Waiting };
// A proposal to the player waits for an answer (see Game::offers).
Reply propose(Game& game, int from, int to, Treaty t, std::vector<DiplomacyEvent>& events);
bool canDeclareWar(const Game& game, int from, int to);
void declareWar(Game& game, int from, int to, std::vector<DiplomacyEvent>& events);
// Changes how a race regards an empire.
void changeRegard(Game& game, int race, int empire, int change);
bool giveGift(Game& game, int from, int to, int credits, std::vector<DiplomacyEvent>& events);
// The player answers an offer.
void answerOffer(Game& game, std::size_t offer, bool accept, std::vector<DiplomacyEvent>& events);

// A turn of diplomacy: new contacts, regard rising and falling, and the
// computer players' proposals, gifts and declarations of war.
std::vector<DiplomacyEvent> diplomacyTurn(Game& game, const std::vector<Battle>& battles,
                                          const std::vector<Assault>& assaults);

} // namespace nagus
