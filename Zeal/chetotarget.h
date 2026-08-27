#pragma once

#include <vector>

#include "game_structures.h"

class ChetoTarget {
 public:
  ChetoTarget(class ZealService *zeal);
  ~ChetoTarget();

  void Disable();

 private:
  void tick();
  void find_target(const std::string &name);
  void find_item(const std::string &item_visual_id);

  size_t target_index = 0;
  bool is_updating = false;

  Zeal::GameStructures::Entity *entity = nullptr;
  std::vector<Zeal::GameStructures::Entity *> targets;
  ULONGLONG markers_timestamp = 0;
};