#include "automelee.h"

#include "callbacks.h"
#include "chat.h"
#include "commands.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

#include "autorogue.h"
#include "automonk.h"
#include "autowarrior.h"

#include <sstream>
#include <numeric>

void AutoMelee::tick() {
  if (!handler) return;
  handler->tick();
}

void AutoMelee::Enable(std::unique_ptr<IAutoMelee> new_handler, const std::vector<std::string> &arguments) {
  if (!new_handler) return;

  handler = std::move(new_handler);
  if (handler->start(arguments)) {
    Zeal::Game::print_chat("AutoMelee habilitado en modo %s.", handler->name());
  } else {
    handler.reset();
  }
}

void AutoMelee::Disable() { 
  handler.reset(); 
}

void AutoMelee::handle_print_chat(const char *message, int color_index) {
  if (!handler || !message) return;

  // Notify autowarrior about enraged or disciplines
  if (auto* warrior = dynamic_cast<AutoWarrior*>(handler.get())) {
    warrior->handle_chat(message);
  }
}

AutoMelee::AutoMelee(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { Disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { Disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { Disable(); }, callback_type::EnterZone);

    // Hook all printed chat lines to match for combat data
  zeal->chat_hook->add_print_chat_callback(
      [this](const char *data, int color_index) { handle_print_chat(data, color_index); });

  // Poll on each main loop iteration (same pattern as autoability).
  zeal->callbacks->AddGeneric([this]() { tick(); });

  // Register the /automelee command.
  zeal->commands_hook->Add(
      "/automelee", {"/am"},
      "Auto handles class specific skills like Kick or Backstab while in combat.",
      [this](std::vector<std::string> &args) {
        if (args.size() >= 2) {
          std::vector<std::string> arguments;
          arguments.assign(args.begin() + 1, args.end());
          std::ostringstream oss;
          for (size_t i = 0; i < arguments.size(); ++i) {
            oss << arguments[i];
            if (i != arguments.size() - 1) {
              oss << " ";  // Add space delimiter between words
            }
          }

          Zeal::Game::print_chat("AutoMelee: Arguments: %s", oss.str().c_str());

          if (Zeal::String::compare_insensitive(args[1], "off")) {
            Zeal::Game::print_chat("AutoMelee disabled.");  
            Disable();
            return true;
          }
          if (Zeal::String::compare_insensitive(args[1], "monk")) {            
            Enable(std::make_unique<AutoMonk>(), arguments);
            return true;
          }
          if (Zeal::String::compare_insensitive(args[1], "rogue")) {
            Enable(std::make_unique<AutoRogue>(), arguments);
            return true;
          }
          if (Zeal::String::compare_insensitive(args[1], "warrior")) {
            Enable(std::make_unique<AutoWarrior>(), arguments);
            return true;
          }
        }

        Zeal::Game::print_chat("Uso: /automelee <monk | rogue | warrior | off> <clickies | >");
        return true;
      });
}

AutoMelee::~AutoMelee() {}