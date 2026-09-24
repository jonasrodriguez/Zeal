#pragma once

#include <string>
#include <map>
#include <unordered_map>
#include <vector>

#include "game_structures.h"

#include "entity_helper.h"

class ChetoFind {
 public:
  ChetoFind(class ZealService *zeal);
  ~ChetoFind();

  void Disable();

 private:

  constexpr static float tolerance_sq = 50.0f * 50.0f;

  void tick();
  void start(const std::string &name);

  void setupPathings();
  void filterPHs();

  std::map<std::string, FindTarget> targetList;

  std::unordered_map<int, PathInfo> path_by_grid;
  std::vector<Segment> spawn_to_path_segments;
  std::unordered_map<std::string, struct Zeal::GameStructures::Entity *> ph_list;

  bool active = false;
  FindTarget current_target;
  ULONGLONG scan_timestamp = 0;
};
