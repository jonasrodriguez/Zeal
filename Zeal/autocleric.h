#pragma once

#include <string>

#include "zeal.h"

class AutoCleric {
 public:
  AutoCleric(class ZealService *zeal);
  ~AutoCleric();

  void enable();
  void disable();

 private:
  static constexpr int COMPLETE_HEALING_ID = 13;
  static constexpr int YAULP_V_ID = 2326;

  void handle_print_chat(const char *data, int color_index);  // Scans chat text for CH orders.
  void tick();

  void search_spells();					// Searches for CH and Yaulp V in the spell gems and updates gem indices.
  bool is_gem_ready(int gem_index);     // Check if spell it's ready for castting

  bool autocleric = false;
  bool is_active = false;
  bool cast_pending = false;
  std::string heal_target;
  int ch_gem = -1;
  int yaulp_gem = -1;
};