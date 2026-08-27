#pragma once

#include <string>
#include <vector>

#include "game_functions.h"
#include "game_packets.h"
#include "zeal_settings.h"

// AutoFace: instantly turn the controlled character to face an entity by writing
// heading only (never Position), so the server sees an ordinary turn rather than
// a warp. This is the "face" half of ComeClose's steering, without the movement.
//
// Usage in game:
//   /face            face the current target (NPC or player)
//   /face on|off     enable/disable automatically facing the assist target
//
// Notes:
//   - Works on any targetable entity, including NPCs (unlike /follow).
//   - When FaceOnAssist is enabled, a successful /assist also snaps heading to
//     the new assist target by watching the OP_Assist response packet.
class AutoFace {
 public:
  AutoFace(class ZealService *zeal);
  ~AutoFace();

  // Turn to face the given entity id. Returns false if the target is invalid or
  // not in game. Writes only Heading.
  bool face(int target_id);

  // Turn to face the current target. Returns false if there is no target.
  bool face_target();

  // Automatically face the assist target when /assist succeeds.
  ZealSetting<bool> FaceOnAssist = {false, "Zeal", "FaceOnAssist", true};

 private:
  bool handle_face_command(const std::vector<std::string> &args);
  bool handle_assist_response(const Zeal::Packets::EntityId_Struct *packet);
};