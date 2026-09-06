#include "autocleric.h"

#include "callbacks.h"
#include "chat.h"
#include "commands.h"
#include "entity_manager.h"
#include "game_functions.h"
#include "string_util.h"
#include "zeal.h"

#include "entity_helper.h"

void AutoCleric::enable(std::string name) { 

  // Get target entity, try to get target's pet if available
  target = EntityHelper::get_player_by_name(name);
  if (!target) {
    Zeal::Game::print_chat("AutoCleric: Target \"%s\" not found.", name.c_str());
    auto_cleric = false;
    return;
  }
  pet = EntityHelper::get_pet_by_owner(target);

  spell_helper.search_spells(spellset);
  if (spell_helper.missing_spell(spellset)) {
    Zeal::Game::print_chat("AutoCleric: Missing \"Sound of Force\" or \"Ethereal Light\".");
    auto_cleric = false;
    return;
  } 

  Zeal::Game::print_chat("AutoCleric habilitado.");
  auto_cleric = true;
}

void AutoCleric::disable() { 
  target = nullptr;
  pet = nullptr;
  spell_helper.reset_spells(spellset);
  auto_cleric = false;
}

void AutoCleric::tick() {

  if (!auto_cleric || !target) return;

   auto now = GetTickCount64();
  if (now - last_interval_time < kCheckIntervalMs) return;
  last_interval_time = now;

    switch (state) {
    case Stun:
      tick_stun();
      break;
    case Heal:
      tick_heal();
      break;
    case Idle:
      tick_idle();
      break;
    default:
      return;
  }
}

void AutoCleric::tick_idle() {

  // Check if pet break
  if (pet && pet->PetOwnerSpawnId == 0) {
    Zeal::Game::print_chat("AutoCleric: Pet break detected, trying to stun!");
    Zeal::Game::set_target(pet);
    state = Stun;
    return;
  }

  // Check target health
  if (target) {
    // If below 60% health, heal target
    if (target->HpCurrent < target->HpMax * 0.6) {
      Zeal::Game::print_chat("AutoCleric: Target \"%s\" below 60%% health, trying to heal!", target->Name);
      Zeal::Game::set_target(target);
      state = Heal;
      return;
    }
  }

  // Check if target has a new pet
  if (target) {
    pet = EntityHelper::get_pet_by_owner(target);
  }
}

void AutoCleric::tick_stun() {  
  if (spell_helper.cast_spell(stun)) {
    state = Idle;
  }
}

void AutoCleric::tick_heal() {
  if (spell_helper.cast_spell(heal)) {
    state = Idle;
  }
}

AutoCleric::AutoCleric(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EnterZone);

  // Poll on each main loop iteration.
  zeal->callbacks->AddGeneric([this]() { tick(); });

  // Register the /autocleric command.
  zeal->commands_hook->Add("/autocleric", {}, "Tries to keep enchanter alive",
    [this](std::vector<std::string> &args) {
        int args_size = args.size();
        std::string target_name = "";

        if (args_size > 1) {
          if (Zeal::String::compare_insensitive(args[1], "off")) {
            Zeal::Game::print_chat("AutoCleric disabled.");
            disable();
            return true;
          }

          enable(args[1]);
        }

        return true;
      });
}
