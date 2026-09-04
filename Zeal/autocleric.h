#pragma once

#include <string>

#include "game_structures.h"
#include "zeal.h"

#include "spell_helper.h"

class AutoCleric {
 public:
  AutoCleric(class ZealService *zeal);
  ~AutoCleric();

  void enable(std::string target_name);
  void disable();

 private:
  SpellHelper spell_helper;

  void tick();
  void tick_idle();
  void tick_prepare_stun();
  void tick_stun();

  bool auto_cleric = false;

  enum ClericState { Idle, PreparoStun, Stun, Heal };

  Spell stun{-1, 216};		// Stun spell (Stuns for 4 seconds) 1.5 cast time
  Spell complete{-1, 13};  
  SpellSet spellset{&stun, &complete};

  void check_target();
  bool check_pet();

  ClericState state = Idle;

  std::string target_name;
  Zeal::GameStructures::Entity *target = nullptr;
  Zeal::GameStructures::Entity *pet = nullptr;

  ULONGLONG last_interval_time = 0;
  static constexpr DWORD kCheckIntervalMs = 500;
};

