#pragma once

#include <vector>

struct Spell {
  int gem;
  int spell_id;
  int duration;
  int range;
};

using SpellSet = std::vector<Spell*>;