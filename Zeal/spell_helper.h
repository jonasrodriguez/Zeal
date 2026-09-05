#pragma once

#include <vector>

#include "game_structures.h"

#include "spell.h"

class SpellHelper {
 public:	
  void search_spells(SpellSet &spellset);
  void reset_spells(SpellSet &spells);
  bool missing_spell(const SpellSet &spells);
  bool cast_spell(const Spell &spell);
  static Spell get_fading_buff(const std::vector<Spell> &buffs);

 private:
  static constexpr int kFadingBuffThreshold = 10;  // seconds

  void cast(const Spell &spell);
  WORD casting_spell_id = kInvalidSpellId;
};