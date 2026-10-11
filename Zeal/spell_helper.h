#pragma once

#include <vector>

#include "game_structures.h"

#include "spell.h"

class SpellHelper {
 public:	
  void search_spells(SpellSet &spellset);
  void reset_spells(SpellSet &spells);
  bool missing_spell(const SpellSet &spells);
  static Spell get_missing_or_fading_buff(const std::vector<Spell> &buffs);

  enum CastResult { Success, OutOfRange, NotCasting, StillCasting };
  bool cast_spell(const Spell &spell, bool reset_gcd = false);


 private:
  static constexpr int kFadingBuffThreshold = 10;  // seconds
  static constexpr int max_retries = 5;

  enum CastState { Idle, CheckCasting, Casting };

  void cast(const Spell &spell);

  bool target_out_range(const Spell &spell);
  CastResult about_to_cast(const Spell &spell);
  CastResult check_casting(const Spell &spell);
  CastResult casting(bool reset_gcd);

  CastState state = Idle;
  WORD casting_spell_id = kInvalidSpellId;
  ULONGLONG casting_started_timestamp = 0;
  ULONGLONG casting_visible_timestamp = 0;
  int retry_count = 0;
};