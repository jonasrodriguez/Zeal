#include "auto_face.h"

#include <cmath>

#include "callbacks.h"
#include "commands.h"
#include "game_functions.h"
#include "string_util.h"
#include "zeal.h"

namespace {

// EQ heading is a float in [0, 512), increasing counter-clockwise with North at
// 0. Convert a world-space bearing (radians, -pi..pi) using the same axis
// convention as Zeal's nameplate get_bearing helper (and come_close).
constexpr float kHeadingUnits = 512.0f;

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

AutoFace::AutoFace(ZealService *zeal) {
  // /face : face current target, or toggle face-on-assist with on/off.
  zeal->commands_hook->Add(
      "/face", {}, "Turn to face the current target (NPC or player). /face on|off toggles facing the assist target.",
      [this](std::vector<std::string> &args) { return handle_face_command(args); });

  // Watch the OP_Assist response so a successful /assist also faces the new
  // target. Same hook pattern as assist.cpp.
  zeal->callbacks->AddPacket(
      [this](UINT opcode, char *buffer, UINT len) {
        if (opcode == Zeal::Packets::Assist && len == sizeof(Zeal::Packets::EntityId_Struct))
          return handle_assist_response(reinterpret_cast<Zeal::Packets::EntityId_Struct *>(buffer));
        return false;  // continue processing
      },
      callback_type::WorldMessagePost);
}

AutoFace::~AutoFace() {}

bool AutoFace::face(int target_id) {
  if (!Zeal::Game::is_in_game()) return false;
  auto *self = Zeal::Game::get_controlled();
  auto *target = Zeal::Game::get_entity_by_id(static_cast<short>(target_id));
  if (!self || !target || self == target) return false;

  // Only ever write Heading; never Position. The server treats this as an
  // ordinary turn and not a warp.
  self->Heading = bearing_to_heading(get_bearing(self, target));
  return true;
}

bool AutoFace::face_target() {
  auto *target = Zeal::Game::get_target();
  if (!target) return false;
  return face(target->SpawnId);
}

bool AutoFace::handle_face_command(const std::vector<std::string> &args) {
  if (args.size() == 2) {
    bool turn_on = Zeal::String::compare_insensitive(args[1], "on");
    bool turn_off = !turn_on && Zeal::String::compare_insensitive(args[1], "off");
    if (turn_on || turn_off) {
      FaceOnAssist.set(turn_on);
      Zeal::Game::print_chat("Face on assist: %s", turn_on ? "ON" : "OFF");
      return true;
    }
    Zeal::Game::print_chat("Usage: /face [on|off]");
    return true;
  }

  if (!face_target()) Zeal::Game::print_chat("Face: no target selected");
  return true;
}

bool AutoFace::handle_assist_response(const Zeal::Packets::EntityId_Struct *packet) {
  // After the client processes the assist response (WorldMessagePost), the
  // player's target has been updated to the assist target, so just face it.
  if (FaceOnAssist.get() && packet && packet->entity_id > 0) face(packet->entity_id);
  return false;  // never consume; observe only
}