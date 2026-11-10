#pragma once

#include <Windows.h>

#include "Iautomelee.h"
#include "game_ui.h"

#include "button_helper.h"

class AutoMonk : public IAutoMelee {
 public:
  AutoMonk() = default;
  ~AutoMonk() override = default;

  bool start(const std::vector<std::string> &arguments) override;
  void tick() override;

  const char *name() const override { return "monk"; }

  void handle_damage(const Zeal::Packets::Damage_Struct *dmg) override;
  void handle_chat(const std::string &message) override;

 private:
  enum class State { Off, On };

  bool find_hotbuttons();
  State state = State::Off;

  bool clickies = false;

  int kick_slot = -1;
  int clickies_slot = -1;
  Zeal::GameUI::BasicWnd *kick_btn = nullptr;
  Zeal::GameUI::BasicWnd *clickies_btn = nullptr;

  Button kick_button{ButtonHelper::flying_kick_label};
  Button fd_button{ButtonHelper::feign_death_label};
  Button clickies_button{ButtonHelper::clickies_label};
  Buttons buttons{&kick_button, &fd_button, &clickies_button};

  ULONGLONG last_interval_time = 0;
  ULONGLONG last_epic_time_time = 0;
  ULONGLONG last_use_item_time = 0;

  bool hit_by_mob = false;
  std::string target_enraged;

  static constexpr DWORD kCheckIntervalMs = 100;
  static constexpr DWORD kUseEpicMs = 8000;
  static constexpr DWORD kUseItemRetryMs = 10000;
};
