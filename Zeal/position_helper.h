#pragma once

#include "game_structures.h"

using Entity = Zeal::GameStructures::Entity;

class PositionHelper {
 public:

    static float get_heading(const Entity *self, const Entity *target);

  private:
    // EQ heading is a float in [0, 512), increasing counter-clockwise with North at
    // 0. Convert a world-space bearing (radians, -pi..pi) using the same axis
    // convention as Zeal's nameplate get_bearing helper (and come_close).
    // Heading: 0 = N = -y, 128 = W = -x, 256 = S = +y, 384 = E = +x.
    constexpr static float kHeadingUnits = 512.0f;

    // Returns bearing to target from self from -pi to +pi in the world coordinate system.
    static float calculate_bearing(const Entity *self, const Entity *target);
};