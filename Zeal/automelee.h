#pragma once

#include <Windows.h>
#include <memory>

#include "Iautomelee.h"

class AutoMelee {
 public:

  AutoMelee(class ZealService *zeal);
  ~AutoMelee();

  void Enable(std::unique_ptr<IAutoMelee> new_handler, bool clickies);
  void Disable();

 private:
  void tick();
  void handle_print_chat(const char *message, int color_index);
  std::unique_ptr<IAutoMelee> handler;
};