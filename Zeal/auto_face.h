#pragma once

#include <string>

#include "game_functions.h"

class AutoFace {
 public:
  void face_target();

 private:
  // EQ heading is a float in [0, 512), increasing counter-clockwise with North at
  // 0. Convert a world-space bearing (radians, -pi..pi) using the same axis
  // convention as Zeal's nameplate get_bearing helper.
  static constexpr float kHeadingUnits = 512.0f;

  float get_bearing(const Zeal::GameStructures::Entity *self, const Zeal::GameStructures::Entity *target);
  float bearing_to_heading(float bearing);

};