#pragma once

#include <vector>

#include "game_structures.h"

class ChetoTarget {
 public:
  ChetoTarget(class ZealService *zeal);
  ~ChetoTarget();

  void Disable();

 private:
  void find_target(const std::string &name);
  void find_item(const std::string &item_visual_id);

  std::vector<WORD> tracked_spawn_ids;
};