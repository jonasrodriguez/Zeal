#pragma once

#include <Windows.h>
#include <string>

#include "Iautomelee.h"
#include "game_ui.h"

class AutoRogue : public IAutoMelee {
 public:
  AutoRogue() = default;
  ~AutoRogue() override = default;

  bool start(bool click) override;
  void tick() override;

  const char *name() const override { return "rogue"; }

 private:
  enum class State { Off, On, StartHide, Hide, FinishHide, Evade };

  bool find_hotbuttons();
  void handle_combat(ULONGLONG now);
  State state = State::Off;

  bool clickies = false;

  int backstab_slot = -1;
  int hide_slot = -1;
  int evade_slot = -1;
  int clickies_slot = -1;
  Zeal::GameUI::BasicWnd *backstab_btn = nullptr;
  Zeal::GameUI::BasicWnd *hide_btn = nullptr;
  Zeal::GameUI::BasicWnd *evade_btn = nullptr;
  Zeal::GameUI::BasicWnd *clickies_btn = nullptr;

  ULONGLONG last_interval_time = 0;
  ULONGLONG last_use_item_time = 5000;
  ULONGLONG last_hide_attempt_time = 1000;

  static constexpr DWORD kCheckIntervalMs = 100;
  static constexpr DWORD kUseItemRetryMs = 10000;
  static constexpr DWORD kHideAttemptMs = 1000;
};
