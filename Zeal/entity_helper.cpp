#include "entity_helper.h"

#include "game_functions.h"

Zeal::GameStructures::Entity *EntityHelper::get_player_by_name(const std::string &name) {

  Zeal::GameStructures::Entity *current_ent = Zeal::Game::get_entity_list();
  while (current_ent != nullptr) {
    if (current_ent->Type == Zeal::GameEnums::Player 
        && _strnicmp(current_ent->Name, name.c_str(), name.length()) == 0) {
      return current_ent;
    }
    current_ent = current_ent->Next;
  }

  return nullptr;
}

Zeal::GameStructures::Entity *EntityHelper::get_pet_by_owner(Zeal::GameStructures::Entity *owner) {
  if (!owner) {
    return nullptr;
  }

  int pet_id = owner->ActorInfo->PetID;
  if (pet_id) {
    return Zeal::Game::get_entity_by_id(pet_id);
  }
  return nullptr;
}