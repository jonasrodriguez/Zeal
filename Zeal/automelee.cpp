#include "automelee.h"

#include "callbacks.h"
#include "commands.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

// Forward declaration from commands.cpp - executes a command bypassing the detour.
void ForwardCommand(std::string cmd);

const char *AutoMelee::mode_name(Mode mode) {
  switch (mode) {
    case Mode::Monk:
      return "monk";
    case Mode::Rogue:
      return "rogue";
    default:
      return "off";
  }
}

void AutoMelee::tick() {
  if (active_mode == Mode::Off) return;

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  if (!Zeal::Game::is_in_game() || !self || !char_info) {
    SetMode(Mode::Off, false);
    return;
  }

  if (!Zeal::Game::get_target() || !Zeal::Game::is_autoattacking()) {
    return;
  }

  ULONGLONG now = GetTickCount64();

  // Attempt /doability 7
  if (now - last_ability_time >= kAbilityRetryMs) {
    last_ability_time = now;
    ForwardCommand("/doability 7");
  }

  // Attemp to /doability 1 (Hide)
  if (active_mode == Mode::Rogue && now - last_hide_skill_time >= kHideSkillRetryMs) {
    last_hide_skill_time = now;
    Zeal::Game::do_autoattack(false);
    ForwardCommand("/doability 1");
    Zeal::Game::do_autoattack(true);
  }

  // Attempt the class-specific /use item on its own (slower) timer.
  if (now - last_use_item_time >= kUseItemRetryMs) {
    last_use_item_time = now;

    if (active_mode == Mode::Monk)
      ForwardCommand("/use Celestial Fists");
    else 
      ForwardCommand("/use Ring of Dain Frostreaver IV");
  }
}

void AutoMelee::SetMode(Mode mode, bool do_print) {
  if (mode == active_mode) {
    if (do_print && mode != Mode::Off)
      Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoMelee already active in %s mode.", mode_name(mode));
    return;
  }

  if (do_print) {
    if (mode == Mode::Off)
      Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoMelee disabled.");
    else
      Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoMelee enabled in %s mode.", mode_name(mode));
  }

  active_mode = mode;
  last_ability_time = 0;
  last_use_item_time = 0;
}

AutoMelee::AutoMelee(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { SetMode(Mode::Off, false); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { SetMode(Mode::Off, false); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { SetMode(Mode::Off, false); }, callback_type::EnterZone);

  // Poll on each main loop iteration (same pattern as autoability).
  zeal->callbacks->AddGeneric([this]() { tick(); });

  // Register the /automelee command.
  zeal->commands_hook->Add(
      "/automelee", {"/am"},
      "Auto-repeats /doability 1 and a class click item (monk: Celestial Fists, rogue: Coldain) while in combat.",
      [this](std::vector<std::string> &args) {
        if (args.size() == 2) {
          if (Zeal::String::compare_insensitive(args[1], "off")) {
            SetMode(Mode::Off, true);
            return true;
          }
          if (Zeal::String::compare_insensitive(args[1], "monk")) {
            SetMode(Mode::Monk, true);
            return true;
          }
          if (Zeal::String::compare_insensitive(args[1], "rogue")) {
            SetMode(Mode::Rogue, true);
            return true;
          }
        }

        Zeal::Game::print_chat("Usage: /automelee <monk | rogue | off>");
        return true;
      });
}

AutoMelee::~AutoMelee() {}