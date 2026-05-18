#pragma once

#include <Windows.h>

#include <string>

#include "game_ui.h"

class AutoRogue {
 public:
  AutoRogue() = default;
  ~AutoRogue() = default;

  bool start();
  void tick();

  const char *name() const { return "rogue"; }

 private:
  enum class State { Off, On, StartHide, Hide, FinishHide, Evade };

  bool find_hotbuttons();
  void handle_combat(ULONGLONG now);
  State state = State::Off;

  bool clickies = false;

  int backstab_slot = -1;
  int hide_slot = -1;
  int clickies_slot = -1;
  Zeal::GameUI::BasicWnd *backstab_btn = nullptr;
  Zeal::GameUI::BasicWnd *hide_btn = nullptr;
  Zeal::GameUI::BasicWnd *clickies_btn = nullptr;

  ULONGLONG last_interval_time = 0;
  ULONGLONG last_use_item_time = 0;
  ULONGLONG last_hide_attempt_time = 0;

  static constexpr DWORD kCheckIntervalMs = 200;
  static constexpr DWORD kUseItemRetryMs = 10000;
  static constexpr DWORD kHideAttemptMs = 100;
};
