#pragma once

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "entity_helper.h"
#include "game_structures.h"

struct TrackedPH {
  WORD spawn_id = 0;
  float last_heading = 0.0f;
  ULONGLONG last_heading_change = 0;
  bool stationary = false;
};

class ChetoFind {
 public:
  ChetoFind(class ZealService *zeal);
  ~ChetoFind();

  void Disable();

 private:
  static constexpr float kToleranceSq = 50.0f * 50.0f;
  static constexpr ULONGLONG kStationaryThresholdMs = 5000;

  void tick();
  void start(const std::string &name);

  void setupPathings();
  void refreshEntities();
  bool isOnPathOrSpawnSegment(Zeal::GameStructures::Entity *ent);

  std::unordered_map<int, PathInfo> path_by_grid;
  std::vector<Segment> spawn_to_path_segments;

  std::unordered_map<WORD, TrackedPH> tracked_phs;
  ULONGLONG respawn_timer = 0;

  bool active = false;
  FindTarget current_target;
  ULONGLONG scan_timestamp = 0;
};
