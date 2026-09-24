#pragma once

#include <string>
#include <unordered_map>
#include <vector>

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
};

class ChetoFind {
 public:
  ChetoFind(class ZealService *zeal);
  ~ChetoFind();

  void Disable();

 private:
  void tick();
  void start(const std::string &name);
  void stop();

  static const std::unordered_map<std::string, FindTarget> &GetKnownTargets();

  std::vector<Zeal::GameStructures::Entity *> targets;

  bool active = false;
  FindTarget current_target;
  ULONGLONG scan_timestamp = 0;
};
