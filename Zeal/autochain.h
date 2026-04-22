#pragma once

#include <string>

#include "zeal.h"

class AutoChain {
 public:
  AutoChain(class ZealService *zeal);
  ~AutoChain();

  void enable(bool do_print = true);
  void disable(bool do_print = true);

 private:
  static constexpr int COMPLETE_HEALING_ID = 13;
  static constexpr int YAULP_V_ID = 2326;

  enum CHState { Idle, StartCasting, Casting, AboutToCast };

  void handle_print_chat(const char *message, int color_index);  // Scans chat text for CH orders.
  void tick();

  //Zeal::GameStructures::Entity get_target(std::string name);  // Gets the entity pointer for the target of the CH order.

  void search_spells();              // Searches for CH and Yaulp V in the spell gems and updates gem indices.
  bool is_gem_ready(int gem_index);  // Check if spell it's ready for castting

  bool autochain = false;
  std::string ch_target;
  int ch_gem = -1;
  int yaulp_gem = -1;
  CHState state = Idle;

  ULONGLONG last_ch_check = 0;

  static constexpr DWORD kCheckInterval = 400;
};