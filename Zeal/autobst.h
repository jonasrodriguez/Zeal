#pragma once

#include "game_structures.h"
#include "spell.h"

class AutoBst {
 public:
  AutoBst(class ZealService *zeal);
  ~AutoBst();

  void enable();
  void disable();

 private:
  inline static const std::string CHANNEL = "grupete";

  enum MageState { Idle, Assist, Slow, Malo, Nuke, AboutToSit };

  void tick();
  void tick_assist(ULONGLONG now);
  void tick_slow(ULONGLONG now);

  bool handle_chat(const char *message, int color_index);
  void search_spells();
  void cast_spell(const Spell &spell);

  void buff_pet(ULONGLONG now);

  bool auto_bst = false;
  bool cast_slow = false;
  MageState state = Idle;

  Spell slow{-1, 2634};
  Spell pethaste{-1, 2628, 1000000};

  WORD casting_spell_id = kInvalidSpellId;

  ULONGLONG last_interval_time = 0;
  ULONGLONG last_action_time = 0;
  ULONGLONG last_pet_haste_time = 0;
  static constexpr DWORD kCheckIntervalMs = 500;
  static constexpr DWORD kLastActionIntervalMs = 6000;
};
