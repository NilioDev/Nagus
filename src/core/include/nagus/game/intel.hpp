// SPDX-License-Identifier: GPL-3.0-or-later
//
// Intelligence: each empire splits its intelligence between security at
// home and operations against the others. Points gathered against an empire
// pay for espionage (taking from it) and sabotage (harming it). The rules
// are our own (see docs/rules/intelligence.md).
#pragma once

#include "nagus/game/game.hpp"

namespace nagus {

inline constexpr int kOperationCost = 150;   // points for one operation
inline constexpr std::size_t kMaxReports = 60;
inline constexpr int kIntelStep = 5;         // percent

// The plan an empire sets: these keep every share between 0 and 100.
void setSecurity(EmpireState& empire, int percent);
void setIntelShare(EmpireState& empire, int target, bool sabotage, int percent);
void setIntelFocus(EmpireState& empire, int target, IntelFocus focus);
// Points a turn that go to security, and to operations against a target.
int securityPoints(const EmpireState& empire);
int operationPoints(const EmpireState& empire, int target, bool sabotage);

// A turn of intelligence: points are shared out, operations that have
// gathered enough points happen, and the reports are kept. Returns the
// reports the player hears of: their own operations and those caught or
// felt at home.
std::vector<IntelReport> intelTurn(Game& game);

// A computer player's plan.
void planIntel(Game& game, int empire);

} // namespace nagus
