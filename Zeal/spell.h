#pragma once

#include <vector>

struct Spell {
  int gem = -1;
  int spell_id = -1;
  int duration = -1;
  int range = -1;
};

using SpellSet = std::vector<Spell*>;