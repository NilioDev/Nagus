// SPDX-License-Identifier: GPL-3.0-or-later
//
// Research: each turn an empire's research points are shared between the
// six fields by the player's percentages; a field reaches its next level
// when it has gathered that level's cost (from techtree.tec). Sociology,
// which research and intelligence facilities need, is not researched: it
// keeps to the average level of the six fields (our choice). See
// docs/rules/research.md.
#pragma once

#include "nagus/game/economy.hpp"

#include <string>
#include <vector>

namespace nagus {

// Every field gets its share of research times this (with equal shares,
// each field as much as the whole), so that the early levels take some ten
// turns from a home system (our choice; the original's pace is not known).
inline constexpr int kFieldBoost = kResearchFields;

// The technology of a field and level, or null.
const Tech* findTech(const std::vector<Tech>& techs, TechField field, int level);

// Research points a field needs for its next level (0 at the top).
int nextLevelCost(const EmpireState& empire, TechField field, const std::vector<Tech>& techs);

// Sets one field's share and moves the others (those not locked) so that
// all add up to 100.
void setShare(EmpireState& empire, int field, int percent);

// Spends the turn's research points. Returns the technologies reached.
std::vector<const Tech*> researchTurn(EmpireState& empire, const std::vector<Tech>& techs);

// Makes sociology follow the other fields.
void updateSociology(EmpireState& empire);

} // namespace nagus
