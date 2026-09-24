#pragma once

#include <string>

#include "game_structures.h"

class EntityHelper {
 public:
  static Zeal::GameStructures::Entity *get_player_by_name(const std::string &name);
  static Zeal::GameStructures::Entity *get_pet_by_owner(Zeal::GameStructures::Entity *owner);
  static std::string get_base_name(const std::string &s);
};
