#include "autocleric.h"

#include "callbacks.h"
#include "chat.h"
#include "commands.h"
#include "entity_manager.h"
#include "game_functions.h"
#include "string_util.h"
#include "zeal.h"

void AutoCleric::enable(std::string name) { 

  target_name = name;

  // Get target entity, try to get target's pet if available
  check_target();
  if (!target) {
    Zeal::Game::print_chat("AutoCleric: Target \"%s\" not found.", target_name.c_str());
    auto_cleric = false;
    return;
  }

  int pet_id = target->ActorInfo->PetID;
  if (pet_id) {
    pet = Zeal::Game::get_entity_by_id(pet_id);
  }
  if (!pet) {
    Zeal::Game::print_chat("AutoCleric: Target \"%s\" has no pet.", target_name.c_str());
    auto_cleric = false;
    return;
  }

  spell_helper.search_spells(spellset);
  if (spell_helper.missing_spell(spellset)) {
    Zeal::Game::print_chat("AutoCleric: Missing \"Stun\" or \"Complete Heal\".");
    auto_cleric = false;
    return;
  } 

  Zeal::Game::print_chat("AutoCleric habilitado.");
  auto_cleric = true;
}

void AutoCleric::disable() { 
  target_name = std::string();
  target = nullptr;
  pet = nullptr;
  spell_helper.reset_spells(spellset);
  auto_cleric = false;
}

void AutoCleric::tick() {

  if (!auto_cleric) return;

   auto now = GetTickCount64();
  // Add a 500 ms delay, no need to check every tick
  if (now - last_interval_time < kCheckIntervalMs) return;
  last_interval_time = now;

  // Check it target is still valid
  if (!target) {
    check_target();
    if (!target) {
      Zeal::Game::print_chat("AutoCleric: Target \"%s\" not found.", target_name.c_str());
      disable();
      return;
    }    
  }

  // Does target still have a pet ?
  if (check_pet()) {
    return;
  }

  Zeal::Game::print_chat("AutoCleric: Target \"%s\" has no pet. Charm Break ??", target_name.c_str());
  // Cast Stun on pet, if pet available
  if (pet) {
    Zeal::Game::set_target(pet);
    spell_helper.cast_spell(stun);
  }

  // Disable autocleric for 10 seconds to avoid spamming
  auto_cleric = false;
}

void AutoCleric::check_target() {

  Zeal::GameStructures::Entity *current_ent = Zeal::Game::get_entity_list();
  while (current_ent != nullptr) {
    if (current_ent->Type == Zeal::GameEnums::Player &&
        _strnicmp(current_ent->Name, target_name.c_str(), target_name.length()) == 0) {
      target = current_ent;
      break;
    }
    current_ent = current_ent->Next;
  }
}

bool AutoCleric::check_pet() {

  if (!pet) return false;

  if (pet->PetOwnerSpawnId == 0) {
    return false;
  }

  int pet_id = target->ActorInfo->PetID;
  if (pet_id) {
    pet = Zeal::Game::get_entity_by_id(pet_id);
    return true;
  }

  return false;

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

AutoCleric::~AutoCleric() {}