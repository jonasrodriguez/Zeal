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
  void find_target(const std::string &name, bool mapa);
  void find_item(const std::string &item_visual_id);

  Zeal::GameStructures::Entity *entity = nullptr;
  std::vector<Zeal::GameStructures::Entity *> targets;
  ULONGLONG markers_timestamp = 0;
};