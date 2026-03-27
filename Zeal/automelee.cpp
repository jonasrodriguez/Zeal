#include "automelee.h"

#include "callbacks.h"
#include "commands.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

// Forward declaration from commands.cpp, uses ForwardCommand to execute "/doability"
void ForwardCommand(std::string cmd);

void AutoMelee::tick() {}

void AutoMelee::SetEnabled(int ability_slot, bool do_print) {
  if (ability_slot < 1 || ability_slot > 10) {
	if (do_print) Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "AutoMelee: slot must be between 1 and 10.");
	return;
  }
  is_active = true;
  active_slot = ability_slot;
  last_attempt_time = 0;
  if (do_print) Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoMelee enabled for slot %d.", ability_slot);
}

void AutoMelee::SetDisabled(bool do_print) {
  is_active = false;
  active_slot = 0;
  last_attempt_time = 0;
  if (do_print) Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoMelee disabled.");
}

AutoMelee::AutoMelee(ZealService *zeal) {

  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { SetDisabled(false); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { SetDisabled(false); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { SetDisabled(false); }, callback_type::EnterZone);

  zeal->callbacks->AddGeneric([this]() { tick(); });


}

AutoMelee::~AutoMelee() {}