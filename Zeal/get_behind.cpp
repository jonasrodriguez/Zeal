#include <algorithm>
#include <cmath>

#include "get_behind.h"
#include "callbacks.h"
#include "commands.h"
#include "game_functions.h"
#include "player_movement.h"
#include "string_util.h"
#include "zeal.h"

namespace {

    constexpr float kPi = 3.14159265358979f;
    constexpr float kTwoPi = 2.0f * kPi;
    constexpr float kHeadingUnits = 512.0f;
    constexpr float kCircleSlack = 10.0f;      // Start circling this far outside stop distance.
    constexpr float kArrivalSlack = 4.0f;      // Distance tolerance when declaring arrival.
    constexpr ULONGLONG kTimeoutMs = 45000;    // Safety net against endless autorun.
    constexpr ULONGLONG kCalibWindowMs = 600;  // Window used to check that strafing helps.
    constexpr float kCalibGrowthRad = 8.0f * kPi / 180.0f;

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

    void set_strafe(strafe_direction dir) {
      auto *movement = ZealService::get_instance()->movement.get();
      if (movement) movement->set_strafe(dir);
    }
}  // namespace

bool GetBehind::start(Zeal::GameStructures::Entity *target, float stop_distance) {
  if (!target || !Zeal::Game::is_in_game()) return false;
  if (target == Zeal::Game::get_controlled()) return false;

  active_ = true;
  target_id_ = target->SpawnId;
  stop_distance_ = stop_distance;
  approaching_ = true;
  start_ms_ = GetTickCount64();
  calib_active_ = false;
  return true;
}

void GetBehind::reset_state() {
  active_ = false;
  target_id_ = 0;
  calib_active_ = false;
  set_strafe(strafe_direction::None);
}

void GetBehind::stop(const char *reason) {
  if (!active_) return;
  reset_state();
  set_autorun(false);
  if (reason) Zeal::Game::print_chat("BehindTarget: %s", reason);
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
    Zeal::Game::print_chat("BehindTarget: strafing the wrong way, direction mapping flipped");
  }
  calib_start_ms_ = now;
  calib_start_err_ = abs_err;
}

void GetBehind::tick() {
  if (!active_) return;

  if (!Zeal::Game::is_in_game()) {
    reset_state();
    return;
  }

  auto *self = Zeal::Game::get_controlled();
  auto *target = Zeal::Game::get_entity_by_id(static_cast<short>(target_id_));
  if (!self || !target) {
    stop("target lost");
    return;
  }
  if (GetTickCount64() - start_ms_ > kTimeoutMs) {
    stop("timed out");
    return;
  }
  if (!Zeal::Game::can_move()) {
    stop("cannot move");
    return;
  }

  float rel_x = self->Position.x - target->Position.x;
  float rel_y = self->Position.y - target->Position.y;
  float dist = std::sqrt(rel_x * rel_x + rel_y * rel_y);

  // Always face the target; strafing then orbits it.
  if (dist > 0.5f) self->Heading = angle_to_heading(std::atan2(-rel_y, -rel_x));

  // Where we are around the target vs. where "directly behind" is (opposite its facing).
  float target_facing = target->Heading * (kTwoPi / kHeadingUnits);
  float goal_angle = target_facing + kPi;
  float self_angle = std::atan2(rel_y, rel_x);
  float err = wrap_pi(goal_angle - self_angle);  // > 0: need to move counter-clockwise around target.
  float abs_err = std::fabs(err);
  bool in_arc = abs_err <= BackArcDegrees.get() * kPi / 180.0f;

  if (!approaching_ && dist > stop_distance_ + kCircleSlack)
    approaching_ = true;
  else if (approaching_ && dist <= stop_distance_)
    approaching_ = false;

  if (in_arc && dist <= stop_distance_ + kArrivalSlack) {
    stop("in position");
    return;
  }

  // Autorun first: toggling it off can clear strafing, so strafe is applied afterwards.
  set_autorun(approaching_);

  strafe_direction desired = strafe_direction::None;
  if (!in_arc && dist <= stop_distance_ + kCircleSlack) {
    // Facing the target, moving counter-clockwise around it is a strafe to the right.
    bool ccw = err > 0;
    desired = (ccw != strafe_inverted_) ? strafe_direction::Right : strafe_direction::Left;
    update_strafe_calibration(abs_err);
  } else {
    calib_active_ = false;
  }

  auto *movement = ZealService::get_instance()->movement.get();
  if (movement && movement->get_strafe() != desired) movement->set_strafe(desired);
}

GetBehind::GetBehind(ZealService *zeal) {
  zeal->callbacks->AddGeneric([this]() { tick(); }, callback_type::MainLoop);
  zeal->callbacks->AddGeneric([this]() { reset_state(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { reset_state(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { reset_state(); }, callback_type::EnterZone);

  zeal->commands_hook->Add(
      "/behind", {"/behindtarget"}, "Walk to the target and strafe around it until you are behind it.",
      [this](std::vector<std::string> &args) {
        if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "off")) {
          if (is_active())
            stop("off");
          else
            Zeal::Game::print_chat("BehindTarget: not active");
          return true;
        }

        if (args.size() == 1 && is_active()) {
          stop("off");
          return true;
        }

        auto *target = Zeal::Game::get_target();
        if (!target) {
          Zeal::Game::print_chat("BehindTarget: no target selected");
          return true;
        }

        float dist = StopDistance.get();
        if (args.size() == 2 && (!Zeal::String::tryParse(args[1], &dist) || dist < 1.f)) {
          Zeal::Game::print_chat("Usage: /behind [off|<distance>]");
          return true;
        }

        if (start(target, dist))
          Zeal::Game::print_chat("BehindTarget: moving behind %s (distance %.0f)", target->Name, dist);
        else
          Zeal::Game::print_chat("BehindTarget: cannot start");
        return true;
      });
}
