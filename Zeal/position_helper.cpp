#include "position_helper.h"

#include "commands.h"
#include "target_ring.h"
#include "zeal.h"

float PositionHelper::get_heading(const Entity* self, const Entity* target) {
  if (!self || !target) return 0;

  auto bearing = calculate_bearing(self, target);

  // bearing=0 is West, but EQ heading=0 is North, so subtract 1/4 of a full rotation.
  float heading = kHeadingUnits / 4.0f - bearing * (kHeadingUnits / (2.0f * static_cast<float>(M_PI)));
  heading = std::fmod(heading, kHeadingUnits);
  if (heading < 0) {
    heading += kHeadingUnits;
  }

  bool auto_run_active = *reinterpret_cast<int*>(0x00798600) != 0;
  Zeal::Game::execute_cmd(1, 1, 0);  // Toggle auto-run.
  Zeal::Game::print_chat("Autorun %s.", auto_run_active ? "disabled" : "enabled");

  return heading;
}

float PositionHelper::calculate_bearing(const Entity* self, const Entity* target) {
  if (!self || !target) return 0;

  float delta_y = target->Position.x - self->Position.x;
  float delta_x = target->Position.y - self->Position.y;
  delta_x = (delta_x >= 0) ? (max(delta_x, 1e-6)) : (min(delta_x, -1e-6));
  float bearing = std::atan2(delta_y, delta_x);  // From -pi to +pi.
  return bearing;
}

/*

zeal->commands_hook->Add("/run", {}, "Toggles autorun on or off.", [](std::vector<std::string> &args) {
    bool auto_run_active = *reinterpret_cast<int *>(0x00798600) != 0;
    Zeal::Game::execute_cmd(1, 1, 0);  // Toggle auto-run.
    Zeal::Game::print_chat("Autorun %s.", auto_run_active ? "disabled" : "enabled");
    return true;
});

*/