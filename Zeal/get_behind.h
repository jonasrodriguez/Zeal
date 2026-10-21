#pragma once

#include <Windows.h>

#include "game_structures.h"

// GetBehind: helper that walks the player up to a target (autorun) and then strafes around it,
// always facing it, until the player is inside the target's back arc.
//
// It has no commands, binds or callbacks of its own. The owner (e.g. an auto class) holds an
// instance and calls update() from its own tick:
//
//   GetBehind behind;   // member
//   ...
//   switch (behind.update(target)) {
//     case GetBehind::Status::Moving:     return;           // keep calling next tick
//     case GetBehind::Status::InPosition: /* backstab */ break;
//     case GetBehind::Status::Failed:     /* give up */  break;
//   }
//
// Call it every main-loop frame (not throttled): the heading is re-aimed at the target on each call,
// and strafing drifts off the circle if calls are far apart. Call stop() if the owner aborts while
// the helper is still moving. Position is never written; movement is autorun + strafe.
class GetBehind {
 public:
  enum class Status {
    Moving,      // Still repositioning, call again next tick.
    InPosition,  // Behind the target and in range. Movement is released.
    Failed,      // No target, cannot move or timed out. Movement is released.
  };

  Status update(Zeal::GameStructures::Entity *target);

  // Releases autorun/strafe (if this helper is driving them) and forgets the current run.
  void stop();

  // True while update() is actively driving movement.
  bool is_active() const { return active_; }

  // Pure geometry check: is self within the back arc of target (2D, ignores distance)?
  static bool is_behind(const Zeal::GameStructures::Entity *self, const Zeal::GameStructures::Entity *target,
                        float arc_degrees);

  float stop_distance = 8.f;      // Close to this distance before circling.
  float back_arc_degrees = 35.f;  // Half-angle of the arc behind the target that counts as "behind".

 private:
  void update_strafe_calibration(float abs_err);

  bool active_ = false;
  int target_id_ = 0;
  bool approaching_ = true;  // Hysteresis for the autorun on/off decision.
  ULONGLONG start_ms_ = 0;

  bool calib_active_ = false;
  ULONGLONG calib_start_ms_ = 0;
  float calib_start_err_ = 0.f;

  // The strafe-direction sign is derived assuming a right-handed (x, y) plane. If strafing is observed
  // to move away from the goal the mapping is flipped. Shared so it is only learned once per session.
  inline static bool strafe_inverted_ = false;
};
