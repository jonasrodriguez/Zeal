#pragma once

#include <Windows.h>

#include <string>

#include "Iautomelee.h"
#include "game_ui.h"

class AutoMonk : public IAutoMelee {
 public:
  AutoMonk() = default;
  ~AutoMonk() override = default;

  bool start(const std::vector<std::string> &arguments) override;
  void tick() override;

  const char *name() const override { return "monk"; }

 private:
  enum class State { Off, On };

  bool find_hotbuttons();
  void handle_combat(ULONGLONG now);
  State state = State::Off;

  bool clickies = false;

  int kick_slot = -1;
  int clickies_slot = -1;
  Zeal::GameUI::BasicWnd *kick_btn = nullptr;
  Zeal::GameUI::BasicWnd *clickies_btn = nullptr;

  ULONGLONG last_interval_time = 0;
  ULONGLONG last_epic_time_time = 0;
  ULONGLONG last_use_item_time = 0;

  static constexpr DWORD kCheckIntervalMs = 100;
  static constexpr DWORD kUseEpicMs = 8000;
  static constexpr DWORD kUseItemRetryMs = 10000;
};
