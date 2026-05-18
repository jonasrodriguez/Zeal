#pragma once

#include <Windows.h>
#include <memory>

 #include "autorogue.h"

class AutoMelee {
 public:

  AutoMelee(class ZealService *zeal);
  ~AutoMelee();

  void Enable(bool clickies);
  void Disable();

 private:
  void tick();
  std::unique_ptr<AutoRogue> auto_rogue;
};