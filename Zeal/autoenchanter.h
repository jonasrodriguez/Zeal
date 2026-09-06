#pragma once

#include <Windows.h>
#include <string>

#include "game_structures.h"

#include "chat_helper.h"
#include "auto_face.h"
#include "spell.h"
#include "spell_helper.h"

class AutoEnchanter {
 public:
  AutoEnchanter(class ZealService *zeal);
  ~AutoEnchanter() {}

  void enable();
  void disable();

 private:
  static constexpr int STUN_RANGE = 80;

  ChatHelper chat_helper;
  AutoFace auto_face;

  enum EncState { Idle, Assist, Stun, Charm, Break };

  void search_spells();
  void cast_spell(const Spell &spell);

  bool handle_chat_channel(const char *message, int color_index);

  void tick();
  void tick_idle();
  void tick_assist();
  void tick_break();
  void tick_stun();
  void tick_charm();

  bool auto_enchanter = false;
  EncState state = Idle;

  Spell stun{-1, 1696}; // Color Slant
  Spell charm{-1, 1705};  // Boltran`s Agacerie
  SpellSet spellset{&stun, &charm};
  SpellHelper spell_helper;

  Zeal::GameStructures::Entity *my_pet = nullptr;

  WORD casting_spell_id = kInvalidSpellId;

  ULONGLONG last_interval_time = 0;
  static constexpr DWORD kCheckIntervalMs = 200;
};