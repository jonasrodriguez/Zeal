#pragma once

class AutoMelee {
 public:
  AutoMelee(class ZealService *zeal);
  ~AutoMelee();

 private:
  void tick();
};