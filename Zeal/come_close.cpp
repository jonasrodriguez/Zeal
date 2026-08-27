#include <cmath>

#include "callbacks.h"
#include "come_close.h"
#include "commands.h"
#include "game_functions.h"
#include "string_util.h"
#include "zeal.h"

namespace {

// EQ heading is a float in [0, 512), increasing counter-clockwise with North at
// 0. Convert a world-space bearing (radians, -pi..pi) using the same axis
// convention as Zeal's nameplate get_bearing helper.
constexpr float kHeadingUnits = 512.0f;

// Run-mode toggle used by the native client (same address as the /run command).
BYTE *const kRunModeFlag = reinterpret_cast<BYTE *>(0x0079856d);

// Bearing from self to target, matching nameplate.cpp's convention:
//   delta_y = target.x - self.x ; delta_x = target.y - self.y
float get_bearing(const Zeal::GameStructures::Entity *self, const Zeal::GameStructures::Entity *target) {
  float delta_y = target->Position.x - self->Position.x;
  float delta_x = target->Position.y - self->Position.y;
  //delta_x = (delta_x >= 0) ? std::max(delta_x, 1e-6f) : std::min(delta_x, -1e-6f);
  return std::atan2(delta_y, delta_x);  // -pi .. +pi
}

// Map a bearing in radians to the game's 0..512 heading range.
float bearing_to_heading(float bearing) {
  float heading = bearing * (kHeadingUnits / (2.0f * static_cast<float>(M_PI)));
  while (heading < 0) heading += kHeadingUnits;
  while (heading >= kHeadingUnits) heading -= kHeadingUnits;
  return heading;
}

}  // namespace

ComeClose::ComeClose(ZealService *zeal) : zeal_(zeal) {
  stop_distance_ = StopDistance.get();

  // Drive the steering from the main game loop.
  zeal->callbacks->AddGeneric([this]() { tick(); }, callback_type::MainLoop);

  // /cc : toggle, on/off, or a numeric stop distance.
  zeal->commands_hook->Add("/cc", {"/comeclose"}, "Move toward the current target (NPC or player) and stop nearby.",
                           [this](std::vector<std::string> &args) {
                             if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "off")) {
                               stop();
                               Zeal::Game::print_chat("ComeClose: off");
                               return true;
                             }

                             auto *target = Zeal::Game::get_target();
                             if (!target) {
                               Zeal::Game::print_chat("ComeClose: no target selected");
                               return true;
                             }

                             float dist = StopDistance.get();
                             if (args.size() == 2 && !Zeal::String::compare_insensitive(args[1], "on")) {
                               // Numeric stop distance argument.
                               if (!Zeal::String::tryParse(args[1], &dist)) {
                                 Zeal::Game::print_chat("Usage: /cc [on|off|<distance>]");
                                 return true;
                               }
                             }

                             if (is_active() && args.size() == 1) {
                               stop();  // bare /cc toggles off when already running
                               Zeal::Game::print_chat("ComeClose: off");
                               return true;
                             }

                             if (start(target->SpawnId, dist))
                               Zeal::Game::print_chat("ComeClose: moving toward %s (stop %.0f)", target->Name, dist);
                             else
                               Zeal::Game::print_chat("ComeClose: invalid target");
                             return true;
                           });
}

ComeClose::~ComeClose() {
  if (active_) stop();
}

Zeal::GameStructures::Entity *ComeClose::resolve_target() const {
  if (!target_id_) return nullptr;
  return Zeal::Game::get_entity_by_id(static_cast<short>(target_id_));
}

void ComeClose::set_autorun(bool on) { *kRunModeFlag = on ? 1 : 0; }

bool ComeClose::start(int target_id, float stop_distance) {
  if (!Zeal::Game::is_in_game()) return false;
  auto *target = Zeal::Game::get_entity_by_id(static_cast<short>(target_id));
  if (!target) return false;

  target_id_ = target_id;
  stop_distance_ = stop_distance;
  autorun_was_on_ = (*kRunModeFlag != 0);
  active_ = true;
  return true;
}

void ComeClose::stop() {
  if (!active_) return;
  active_ = false;
  target_id_ = 0;
  // Release autorun (restore the player's prior state).
  set_autorun(autorun_was_on_);
}

void ComeClose::tick() {
  if (!active_) return;

  if (!Zeal::Game::is_in_game()) {
    stop();
    return;
  }

  auto *self = Zeal::Game::get_controlled();
  auto *target = resolve_target();

  if (!self) return;
  if (!target) {
    // Target lost (zoned/despawned).
    if (StopOnTargetLost.get()) {
      Zeal::Game::print_chat("ComeClose: target lost, stopping");
      stop();
    }
    return;
  }

  float distance = static_cast<float>(self->Position.Dist2D(target->Position));
  if (distance <= stop_distance_) {
    Zeal::Game::print_chat("ComeClose: arrived (%.0f units)", distance);
    stop();
    return;
  }

  // Steer: point the character at the target and ensure autorun is engaged.
  // We only set heading and the run flag; movement is produced by the game's
  // normal locomotion, so the server sees ordinary movement.
  float bearing = get_bearing(self, target);
  self->Heading = bearing_to_heading(bearing);
  set_autorun(true);
}