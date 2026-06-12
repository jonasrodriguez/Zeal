#pragma once

#include <Windows.h>
#include <string>

#include "game_structures.h"
#include "spell.h"

class AutoMage {
 public:
  AutoMage(class ZealService *zeal);
  ~AutoMage();

  void enable(bool castNuke, bool castMalo, bool castDS);
  void disable();

 private:

  inline static const std::string CHANNEL = "grupete";

  enum MageState { Idle, Assist, Malo, Nuke, AboutToSit };

  void tick();
  void tick_assist(ULONGLONG now);
  void tick_malo(ULONGLONG now);
  void tick_nuke(ULONGLONG now);

  bool handle_chat(const char *message, int color_index);
  void search_spells(bool castNuke, bool castMalo, bool castDS);
  void cast_spell(const Spell &spell);

  void buff_pet(ULONGLONG now);

  bool auto_mage = false;
  MageState state = Idle;

  Spell ds{-1, 1667, 420000};
  Spell malo{-1, 1577};
  Spell nuke{-1, 2118};
  Spell burnout{-1, 2119, 850000};

  WORD casting_spell_id = kInvalidSpellId;

  ULONGLONG last_interval_time = 0;
  ULONGLONG last_action_time = 0;
  ULONGLONG last_burnout_time = 0;
  ULONGLONG last_ds_time = 0;
  static constexpr DWORD kCheckIntervalMs = 500;
  static constexpr DWORD kLastActionIntervalMs = 6000;
};
