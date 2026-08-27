#pragma once

#include <Windows.h>
#include <string>

#include "game_structures.h"
#include "spell.h"

class AutoChanter {
 public:
  AutoChanter(class ZealService *zeal);
  ~AutoChanter();

  void enable();
  void disable();

 private:
  inline static const std::string CHANNEL = "grupete";
  static constexpr int STUN_RANGE = 35;

  enum EncState { Idle, Assist, Stun, Charm, AboutToSit };

  void search_spells();
  void cast_spell(const Spell &spell);

  bool handle_chat_channel(const char *message, int color_index);
  void handle_print(const char *message, int color_index);

  void tick();
  void tick_assist(ULONGLONG now);
  void tick_stun(ULONGLONG now);

  bool auto_chanter = false;
  EncState state = Idle;

  Spell stun{-1, 1696}; // Color Slant
  Spell charm{-1, 1705};  // Boltran`s Agacerie

  Zeal::GameStructures::Entity *my_pet = nullptr;

  WORD casting_spell_id = kInvalidSpellId;

  ULONGLONG last_interval_time = 0;
  static constexpr DWORD kCheckIntervalMs = 500;
};