#include "auto_ability.h"

#include "callbacks.h"
#include "commands.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

// Forward declaration from commands.cpp, uses ForwardCommand to execute "/doability"
void ForwardCommand(std::string cmd);

void AutoAbility::tick() {
  if (!is_active || active_slot < 1 || active_slot > 10) return;

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  if (!Zeal::Game::is_in_game() || !self || !char_info) {
    SetDisabled(false);
    return;
  }

  // Terminate if the player is sitting, stunned, feigned, or dead
  if (self->StandingState == Stance::Sit || self->StandingState == Stance::Feign ||
      self->StandingState == Stance::Dead || char_info->StunnedState) {
    SetDisabled(true);
    return;
  }

  // Wait at least kRetryIntervalMs between attempts to prevent spamming
  ULONGLONG now = GetTickCount64();
  if (now - last_attempt_time < kRetryIntervalMs) return;
  last_attempt_time = now;

  // Execute the /doability command through the game's command interpreter.
  ForwardCommand(std::format("/doability {}", active_slot));
}

void AutoAbility::SetEnabled(int ability_slot, bool do_print) {
  if (ability_slot < 1 || ability_slot > 10) {
    if (do_print)
      Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "AutoAbility: slot must be between 1 and 10.");
    return;
  }

  is_active = true;
  active_slot = ability_slot;
  last_attempt_time = 0;

  if (do_print)
    Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoAbility enabled for slot %d.", ability_slot);
}

void AutoAbility::SetDisabled(bool do_print) {
  if (do_print && is_active)
    Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoAbility disabled.");

  is_active = false;
  active_slot = -1;
}

AutoAbility::AutoAbility(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { SetDisabled(false); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { SetDisabled(false); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { SetDisabled(false); }, callback_type::EnterZone);

  // Poll on each main loop iteration (same pattern as melody).
  zeal->callbacks->AddGeneric([this]() { tick(); });

  // Register the /autoability command.
  zeal->commands_hook->Add(
      "/autoability", {"/aa"},
      "Auto-repeats a /doability ability slot when it is off cooldown (toggle, off, or slot 1-10).",
      [this](std::vector<std::string> &args) {
        if (args.size() == 1) {
          // No arguments: toggle off if active, otherwise show usage.
          if (is_active) {
            SetDisabled(true);
          } else {
            Zeal::Game::print_chat("Usage: /autoability <1-10> to enable, /autoability off to disable.");
          }
          return true;
        }

        if (args.size() == 2) {
          if (Zeal::String::compare_insensitive(args[1], "off")) {
            SetDisabled(true);
            return true;
          }

          int slot = 0;
          if (Zeal::String::tryParse(args[1], &slot)) {
            SetEnabled(slot, true);
            return true;
          }
        }

        Zeal::Game::print_chat("Usage: /autoability <1-10> | off");
        return true;
      });
}

AutoAbility::~AutoAbility() {}