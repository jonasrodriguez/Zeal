#pragma once

#include <Windows.h>
#include <string>

#include "game_structures.h"

#include "chat_helper.h"
#include "auto_face.h"
#include "spell_helper.h"

class AutoRanger {
 public:
  AutoRanger(class ZealService *zeal);
  ~AutoRanger();

  void enable(bool castSnare, bool castBuff);
  void disable();

 private:
  ChatHelper chat_helper;
  AutoFace auto_face;

  enum RangerState { Idle, Face, Fire, Snare, Buff };

  void tick();
  void tick_face();
  void tick_snare();
  void tick_auto_fire();
  void tick_buff();

  void check_buffs();

  void handle_chat(const char *message, int color_index);

  bool cast_snare = false;
  bool cast_buffs = false;
  bool auto_ranger = false;
  RangerState state = Idle;

  Spell snare{-1, 512};
  Spell eagle{-1, 2599}; // Eagle Eye
  Spell precision{-1, 2594};  // Nature's Precision
  Spell buff;
  SpellSet spellset{&snare, &eagle, &precision};
  SpellHelper spell_helper;

  WORD casting_spell_id = kInvalidSpellId;

  ULONGLONG last_interval_time;
  static constexpr DWORD kCheckIntervalMs = 200;
};
