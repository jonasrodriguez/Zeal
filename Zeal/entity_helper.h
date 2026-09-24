#pragma once

#include <string>
#include <unordered_map>

#include "game_structures.h"

struct Waypoint {
  float x, y, z;
};

struct SpawnPath {
  int grid_id;
  int grid_type;  // 0=Circular, 3=Patrol (see GridWanderType)
  std::vector<Waypoint> waypoints;
};

struct SpawnPoint {
  int spawn2_id;
  float x, y, z;
  int pathgrid;
  int respawn_seconds;
};

struct FindTarget {
  std::string target_name;
  std::vector<std::string> ph_names;
  std::vector<SpawnPoint> spawn_points;
  std::vector<SpawnPath> paths;
  int respawn_seconds = 0;
};

struct PathInfo {
  std::vector<Vec3> points;
  bool circular;
};

struct Segment {
  Vec2 a, b;
};

class EntityHelper {
 public:

  using EntityMap = std::unordered_map<std::string, struct Zeal::GameStructures::Entity *>;

  static Zeal::GameStructures::Entity *get_player_by_name(const std::string &name);
  static Zeal::GameStructures::Entity *get_pet_by_owner(Zeal::GameStructures::Entity *owner);
  static std::string get_base_name(const std::string &s);
  static EntityMap filterNonPhs(const std::vector<std::string> &ph_names);
};
