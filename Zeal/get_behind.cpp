#include "get_behind.h"

#include <algorithm>
#include <cmath>

#include "game_functions.h"
#include "player_movement.h"
#include "zeal.h"

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kHeadingUnits = 512.0f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kCircleSlack = 10.0f;          // Start circling this far outside stop distance.
constexpr float kArrivalSlack = 4.0f;          // Distance tolerance when declaring arrival.
constexpr float kRepositionMarginDeg = 15.0f;  // Extra arc tolerated once in position before moving again.
constexpr ULONGLONG kTimeoutMs = 45000;        // Safety net against endless autorun.
constexpr ULONGLONG kCalibWindowMs = 600;      // Window used to check that strafing helps.
constexpr float kCalibGrowthRad = 8.0f * kDegToRad;

int *const kAutoRunFlag = reinterpret_cast<int *>(0x00798600);

float wrap_pi(float a) {
  while (a > kPi) a -= kTwoPi;
  while (a <= -kPi) a += kTwoPi;
  return a;
}

// Heading 0 faces +x and increases toward +y (matches AutoFace), 512 units per revolution.
float angle_to_heading(float angle) {
  float heading = angle * (kHeadingUnits / kTwoPi);
  while (heading < 0) heading += kHeadingUnits;
  while (heading >= kHeadingUnits) heading -= kHeadingUnits;
  return heading;
}

void set_autorun(bool on) {
  if ((*kAutoRunFlag != 0) != on) Zeal::Game::execute_cmd(1, 1, 0);  // Toggles.
}

// Signed angle (radians, > 0 means counter-clockwise around the target) from where self is to the point
// directly behind the target. Also returns the 2D offset of self from the target.
float back_error(const Zeal::GameStructures::Entity *self, const Zeal::GameStructures::Entity *target, float *rel_x,
                 float *rel_y) {
  *rel_x = self->Position.x - target->Position.x;
  *rel_y = self->Position.y - target->Position.y;
  float target_facing = target->Heading * (kTwoPi / kHeadingUnits);
  float goal_angle = target_facing + kPi;
  float self_angle = std::atan2(*rel_y, *rel_x);
  return wrap_pi(goal_angle - self_angle);
}

}  // namespace

bool GetBehind::is_behind(const Zeal::GameStructures::Entity *self, const Zeal::GameStructures::Entity *target,
                          float arc_degrees) {
  if (!self || !target) return false;
  float rel_x, rel_y;
  return std::fabs(back_error(self, target, &rel_x, &rel_y)) <= arc_degrees * kDegToRad;
}

void GetBehind::stop() {
  if (active_) {
    set_autorun(false);
    auto *movement = ZealService::get_instance()->movement.get();
    if (movement) movement->set_strafe(strafe_direction::None);  // After autorun: toggling it can clear strafe.
  }
  active_ = false;
  calib_active_ = false;
}

void GetBehind::update_strafe_calibration(float abs_err) {
  ULONGLONG now = GetTickCount64();
  if (!calib_active_) {
    calib_active_ = true;
    calib_start_ms_ = now;
    calib_start_err_ = abs_err;
    return;
  }
  if (now - calib_start_ms_ < kCalibWindowMs) return;

  if (abs_err > calib_start_err_ + kCalibGrowthRad) {
    strafe_inverted_ = !strafe_inverted_;
    Zeal::Game::print_chat("GetBehind: strafing the wrong way, direction mapping flipped");
  }
  calib_start_ms_ = now;
  calib_start_err_ = abs_err;
}

GetBehind::Status GetBehind::update(Zeal::GameStructures::Entity *target) {
  auto *self = Zeal::Game::get_controlled();
  if (!target || !self || target == self || !Zeal::Game::is_in_game()) {
    stop();
    target_id_ = 0;
    return Status::Failed;
  }

  if (target->SpawnId != target_id_) {  // New target: restart.
    stop();
    target_id_ = target->SpawnId;
  }

  float rel_x, rel_y;
  float err = back_error(self, target, &rel_x, &rel_y);  // > 0: need to move counter-clockwise around target.
  float abs_err = std::fabs(err);
  float dist = std::sqrt(rel_x * rel_x + rel_y * rel_y);
  bool in_arc = abs_err <= back_arc_degrees * kDegToRad;

  ULONGLONG now = GetTickCount64();
  if (!active_) {
    // Already settled (with some tolerance so a slightly turning target does not make us jitter).
    bool settled =
        abs_err <= (back_arc_degrees + kRepositionMarginDeg) * kDegToRad && dist <= stop_distance + kCircleSlack;
    if (settled) return Status::InPosition;
    active_ = true;
    approaching_ = true;
    start_ms_ = now;
  }

  if (now - start_ms_ > kTimeoutMs || !Zeal::Game::can_move()) {
    stop();
    return Status::Failed;
  }

  // Always face the target; strafing then orbits it.
  if (dist > 0.5f) self->Heading = angle_to_heading(std::atan2(-rel_y, -rel_x));

  if (in_arc && dist <= stop_distance + kArrivalSlack) {
    stop();
    return Status::InPosition;
  }

  if (!approaching_ && dist > stop_distance + kCircleSlack)
    approaching_ = true;
  else if (approaching_ && dist <= stop_distance)
    approaching_ = false;

  // Autorun first: toggling it off can clear strafing, so strafe is applied afterwards.
  set_autorun(approaching_);

  strafe_direction desired = strafe_direction::None;
  if (!in_arc && dist <= stop_distance + kCircleSlack) {
    // Facing the target, moving counter-clockwise around it is a strafe to the right.
    bool ccw = err > 0;
    desired = (ccw != strafe_inverted_) ? strafe_direction::Right : strafe_direction::Left;
    update_strafe_calibration(abs_err);
  } else {
    calib_active_ = false;
  }

  auto *movement = ZealService::get_instance()->movement.get();
  if (movement && movement->get_strafe() != desired) movement->set_strafe(desired);
  return Status::Moving;
}
