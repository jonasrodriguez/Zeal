#pragma once

#include <Windows.h>

#include "game_structures.h"
#include "zeal_settings.h"

// GetBehind: walk up to the current target (autorun), then strafe around it, always facing it,
// until the player is inside the target's back arc.
//
// Usage in game:
//   /behind            toggle on/off for the current target
//   /behind off        stop
//   /behind <dist>     start, closing to <dist> units before circling
//
// Movement is driven like manual play: heading is set to face the target, the game's own autorun
// provides forward motion and PlayerMovement::set_strafe provides the sideways motion. Position is
// never written.
class GetBehind {
 public:
  GetBehind(class ZealService *zeal);
  ~GetBehind() {}

  bool start(Zeal::GameStructures::Entity *target, float stop_distance);
  void stop(const char *reason = nullptr);

  bool is_active() const { return active_; }

  // Distance to close to before circling.
  ZealSetting<float> StopDistance = {8.f, "Zeal", "BehindTargetDistance", false};

  // Half-angle (degrees) of the arc behind the target that counts as "behind".
  ZealSetting<float> BackArcDegrees = {35.f, "Zeal", "BehindTargetArc", false};

 private:
  void tick();
  void reset_state();  // Drops state and strafe without touching autorun (safe during zoning).
  void update_strafe_calibration(float abs_err);

  bool active_ = false;
  int target_id_ = 0;
  float stop_distance_ = 8.f;
  bool approaching_ = true;  // Hysteresis for the autorun on/off decision.
  ULONGLONG start_ms_ = 0;

  // The strafe-direction sign is derived assuming a right-handed (x, y) plane. If strafing is
  // observed to move away from the goal, the mapping is flipped for the rest of the session.
  bool strafe_inverted_ = false;
  bool calib_active_ = false;
  ULONGLONG calib_start_ms_ = 0;
  float calib_start_err_ = 0.f;
};
