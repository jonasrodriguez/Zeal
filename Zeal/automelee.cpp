#include "automelee.h"

#include "callbacks.h"
#include "commands.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"


void AutoMelee::tick() {
  if (!handler) return;
  handler->tick();
}

void AutoMelee::Enable(std::unique_ptr<IAutoMelee> new_handler, bool clickies = false) {
  if (!new_handler) return;

  if (handler) {
    Zeal::Game::print_chat("AutoMelee is already enabled in %s mode.", handler->name());
    return;
  }

  handler = std::move(new_handler);
  if (handler->start(clickies)) {
    Zeal::Game::print_chat("AutoMelee enabled in %s mode.", handler->name());
  } else {
    handler.reset();
  }
}

void AutoMelee::Disable() { handler.reset(); }

AutoMelee::AutoMelee(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { Disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { Disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { Disable(); }, callback_type::EnterZone);

  // Poll on each main loop iteration (same pattern as autoability).
  zeal->callbacks->AddGeneric([this]() { tick(); });

  // Register the /automelee command.
  zeal->commands_hook->Add(
      "/automelee", {"/am"},
      "Auto-repeats /doability 1 and a class click item (monk: Celestial Fists, rogue: Coldain) while in combat.",
      [this](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          bool clickies = false;
          if (args.size() == 3 && Zeal::String::compare_insensitive(args[2], "yes")) {
            clickies = true;
          }

          if (Zeal::String::compare_insensitive(args[1], "off")) {
            Disable();
            return true;
          }
          if (Zeal::String::compare_insensitive(args[1], "monk")) {            
            return true;
          }
          if (Zeal::String::compare_insensitive(args[1], "rogue")) {
            Enable(std::make_unique<AutoRogue>(), clickies);
            return true;
          }
        }

        Zeal::Game::print_chat("Usage: /automelee <monk | rogue | off> <clickies: true | false>");
        return true;
      });
}

AutoMelee::~AutoMelee() {}