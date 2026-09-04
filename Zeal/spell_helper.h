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

 private:
  void cast(const Spell &spell);
  WORD casting_spell_id = kInvalidSpellId;
};