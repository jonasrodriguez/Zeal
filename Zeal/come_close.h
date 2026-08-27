#pragma once

#include <Windows.h>

#include "zeal_settings.h"

// ComeClose: steer the controlled character toward the current target by
// setting heading and enabling autorun, stopping within a configurable
// distance. Movement is driven the same way the game's own auto-run works
// (heading + run speed), so the server sees ordinary movement rather than a
// position warp.
//
// Usage in game:
//   /cc            toggle navigation toward the current target on/off
//   /cc on|off     explicitly start/stop
//   /cc <dist>     start, stopping <dist> units from the target
//
// Notes:
//   - Works on any targetable entity, including NPCs (unlike /follow, which the
//     native client restricts to players).
//   - This only sets heading and toggles autorun; it never writes Position, so
//     it does not trip the server's movement/warp detection as long as run
//     speed is legal.
class ComeClose {
 public:
  ComeClose(class ZealService *zeal);
  ~ComeClose();

  // Begin navigating toward the given entity id. Pass stop_distance to override
  // the configured default. Returns false if the target is invalid.
  bool start(int target_id, float stop_distance);

  // Stop navigation and release autorun.
  void stop();

  bool is_active() const { return active_; }

  // Distance at which navigation is considered complete (settings-backed).
  ZealSetting<float> StopDistance = {15.f, "Zeal", "ComeCloseStopDistance", false};

  // If true, automatically stop when the target is lost (zoned/despawned).
  ZealSetting<bool> StopOnTargetLost = {true, "Zeal", "ComeCloseStopOnTargetLost", false};

 private:
  // Called every main loop tick; performs the heading + autorun steering.
  void tick();

  // Toggle the game's auto-run state to the requested value.
  void set_autorun(bool on);

  // Resolve the tracked entity by id, or nullptr if it is gone.
  struct Zeal::GameStructures::Entity *resolve_target() const;

  ZealService *zeal_ = nullptr;

  bool active_ = false;
  int target_id_ = 0;  // SpawnId of the entity we are navigating toward.
  float stop_distance_ = 15.f;
  bool autorun_was_on_ = false;  // Restore prior autorun state on stop.
};