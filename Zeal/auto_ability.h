#pragma once
#include <Windows.h>

class AutoAbility {
 public:
  AutoAbility(class ZealService *zeal);
  ~AutoAbility();

  void SetEnabled(int ability_slot, bool do_print = false);
  void SetDisabled(bool do_print = false);

 private:
  void tick();

  bool is_active = false;
  int active_slot = -1;  // The /doability slot number (1-10).
  ULONGLONG last_attempt_time = 0;

  static constexpr DWORD kRetryIntervalMs = 500;  // Poll interval to retry the ability.
};