#pragma once

#include <string>
#include <vector>

// forward declaration
namespace Zeal::Packets {
  struct Damage_Struct;
}

class IAutoMelee {
 public:
  virtual ~IAutoMelee() = default;

  virtual bool start(const std::vector<std::string>& arguments) = 0;

  virtual void tick() = 0;

  // Called from AutoMelee's OP_Damage hook. Flags when a mob lands a hit on us so tick() can bail.
  virtual void handle_damage(const Zeal::Packets::Damage_Struct* dmg) = 0;

  // Called from AutoMelee's print chat hook.
  virtual void handle_chat(const std::string& message) = 0;

  virtual const char* name() const = 0;
};