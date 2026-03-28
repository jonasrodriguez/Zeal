#pragma once
#include <Windows.h>

class AutoMelee {
 public:
  enum class Mode { Off, Monk, Rogue };

  AutoMelee(class ZealService *zeal);
  ~AutoMelee();

  void SetMode(Mode mode, bool do_print = false);

 private:
  void tick();
  static const char *mode_name(Mode mode);

  Mode active_mode = Mode::Off;
  ULONGLONG last_ability_time = 0;
  ULONGLONG last_use_item_time = 0;
  ULONGLONG last_hide_skill_time = 0;

  static constexpr DWORD kAbilityRetryMs = 500;   // Poll interval for /doability 1.
  static constexpr DWORD kUseItemRetryMs = 10000;   // Poll interval for the /use clicky.
  static constexpr DWORD kHideSkillRetryMs = 2000;
};