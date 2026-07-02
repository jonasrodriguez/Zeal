#pragma once

#include <Windows.h>

#include <string>
#include <vector>

#include "game_structures.h"
#include "spell.h"
#include "io_ini.h"

class Sentry {
 public:
  Sentry(class ZealService *zeal);
  ~Sentry();

  void enable(bool targets = false);
  void disable();

 private:
  inline static const std::string CHANNEL = "grupete";
  const float kMaxDist = 350;

  enum SentryState { Idle, Attacking };

  void search_spells();
  void cast_spell(const Spell &spell);

  void tick();
  void search_targets();

  bool sentry = false;
  SentryState state = Idle;
  bool all_targets;

  Spell stun{-1, 1696};   // Color Slant
  Spell charm{-1, 1705};  // Boltran`s Agacerie

  std::vector<std::string> targets;
  Zeal::GameStructures::Entity *target;

  WORD casting_spell_id = kInvalidSpellId;

  ULONGLONG last_interval_time = 0;
  static constexpr DWORD kCheckIntervalMs = 500;

  // File system for loading chain members
  void initialize_ini_filename();
  bool load();
  IO_ini ini = IO_ini(".\\sentry_targets.ini");
};
