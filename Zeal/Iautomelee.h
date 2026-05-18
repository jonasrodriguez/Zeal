#pragma once

class IAutoMelee {
 public:
  virtual ~IAutoMelee() = default;

  virtual bool start(bool click) = 0;

  virtual void tick() = 0;

  virtual const char* name() const = 0;
};