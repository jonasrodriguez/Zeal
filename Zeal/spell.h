#pragma once

#include <vector>

struct Spell {
  int gem;
  int spell_id;
  int duration;
};

using SpellSet = std::vector<Spell*>;