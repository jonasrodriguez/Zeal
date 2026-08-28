#include "auto_face.h"

void AutoFace::face_target() {

  auto *self = Zeal::Game::get_controlled();
  auto *target = Zeal::Game::get_target();

  if (!self || !target) return;

  float bearing = get_bearing(self, target);
  float heading = bearing_to_heading(bearing);
  self->Heading = heading;
}

// Bearing from self to target, matching nameplate.cpp's convention:
//   delta_y = target.x - self.x ; delta_x = target.y - self.y
float AutoFace::get_bearing(const Zeal::GameStructures::Entity *self, const Zeal::GameStructures::Entity *target) {
  float delta_y = target->Position.x - self->Position.x;
  float delta_x = target->Position.y - self->Position.y;
  // delta_x = (delta_x >= 0) ? std::max(delta_x, 1e-6f) : std::min(delta_x, -1e-6f);
  return std::atan2(delta_y, delta_x);  // -pi .. +pi
}

float AutoFace::bearing_to_heading(float bearing) {
  constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);
  constexpr float kHalfPi = static_cast<float>(M_PI) / 2.0f;

  // Rotate so bearing == pi/2 (pointing along +Position.x, i.e. "North")
  // maps to heading 0, and flip sign so heading increases CCW.
  float adjusted = kHalfPi - bearing;

  float heading = adjusted * (kHeadingUnits / kTwoPi);
  while (heading < 0) heading += kHeadingUnits;
  while (heading >= kHeadingUnits) heading -= kHeadingUnits;
  return heading;
}

