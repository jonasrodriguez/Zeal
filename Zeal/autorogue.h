#pragma once

#include <Windows.h>
#include <string>

#include "Iautomelee.h"
#include "game_packets.h"
#include "game_structures.h"
#include "game_ui.h"

#include "button_helper.h"

class AutoRogue : public IAutoMelee {
 public:
  AutoRogue() = default;
  ~AutoRogue() override = default;

  bool start(const std::vector<std::string> &arguments) override;
  void tick() override;

  const char *name() const override { return "rogue"; }

  // Called from AutoMelee's OP_Damage hook. Flags when a mob lands a hit on us so tick() can bail.
  void handle_damage(const Zeal::Packets::Damage_Struct *dmg);
  void handle_chat(const std::string &message);

 private:
  enum class State { Off, On };

  bool find_hotbuttons();
  State state = State::Off;

  bool clickies = false;
  bool hides = false;
  bool stop_on_hit = false;

  Button backstab_button{ButtonHelper::backstab_label};
  Button hide_button{ButtonHelper::hide_label};
  Button evade_button{ButtonHelper::evade_label};
  Button clickies_button{ButtonHelper::clickies_label};
  Buttons buttons{&backstab_button, &hide_button, &evade_button, &clickies_button};

  ULONGLONG last_interval_time = 0;
  ULONGLONG last_use_item_time = 5000;
  ULONGLONG last_hide_attempt_time = 1000;

  bool hit_by_mob = false;
  Zeal::GameStructures::Entity *target_enraged = nullptr;

  static constexpr DWORD kCheckIntervalMs = 100;
  static constexpr DWORD kUseItemRetryMs = 10000;
  static constexpr DWORD kHideAttemptMs = 1000;
};
