#pragma once

#include <string>
#include <vector>

class IAutoMelee {
 public:
  virtual ~IAutoMelee() = default;

  virtual bool start(const std::vector<std::string>& arguments) = 0;

  virtual void tick() = 0;

  virtual const char* name() const = 0;
};