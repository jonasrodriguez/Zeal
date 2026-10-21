#pragma once

#include <Windows.h>
#include <memory>
#include <string>
#include <vector>

#include "game_packets.h"

#include "Iautomelee.h"

class AutoMelee {
 public:

  AutoMelee(class ZealService *zeal);
  ~AutoMelee();

  void Enable(std::unique_ptr<IAutoMelee> new_handler, const std::vector<std::string>& arguments);
  void Disable();

 private:
  void tick();
  void handle_print_chat(const char *message, int color_index);
  void handle_damage_packet(const Zeal::Packets::Damage_Struct *dmg);
  std::unique_ptr<IAutoMelee> handler;
};