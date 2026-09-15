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
  static Spell get_missing_or_fading_buff(const std::vector<Spell> &buffs);

 private:
  static constexpr int kFadingBuffThreshold = 10;  // seconds
  static constexpr int max_retries = 5;

  enum CastState { Idle, CheckCasting, Casting };

  void cast(const Spell &spell);

  bool about_to_cast(const Spell &spell);
  bool check_casting(const Spell &spell);
  bool casting();

  CastState state = Idle;
  WORD casting_spell_id = kInvalidSpellId;
  ULONGLONG casting_started_timestamp = 0;
  ULONGLONG casting_visible_timestamp = 0;
  int retry_count = 0;
};