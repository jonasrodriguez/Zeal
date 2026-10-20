#pragma once

#include "game_structures.h"

struct Target {
  Zeal::GameStructures::Entity *entity = nullptr;
  bool engaged = false;
  bool snared = false;
  bool stunned = false;
};

static Target kResetTarget = {nullptr, false, false, false};