#pragma once

#include <string>

#include "game_structures.h"
#include "zeal.h"

#include "spell_helper.h"

class AutoCleric {
 public:
  AutoCleric(class ZealService *zeal);
  ~AutoCleric() {}

  void enable(std::string target_name);
  void disable();

 private:
  SpellHelper spell_helper;

  void tick();
  void tick_idle();
  void tick_stun();
  void tick_heal();

  bool auto_cleric = false;

  enum ClericState { Idle, Stun, Heal };

  enum class HealType { Fast, Complete };
  HealType heal_type = HealType::Fast;

  Spell stun{-1, 125};		// Stun spell (Stuns for 8 seconds) 3s cast time
  Spell heal{-1, 2182};     // Ethereal Light spell (Heals for 1000 HP) 4.75s cast time
  Spell complete{-1, 13};   // Complete Heal
  SpellSet spellset{&stun, &heal, &complete};

  ClericState state = Idle;

  Zeal::GameStructures::Entity *target = nullptr;
  Zeal::GameStructures::Entity *pet = nullptr;

  ULONGLONG last_interval_time = 0;
  static constexpr DWORD kCheckIntervalMs = 200;
};

