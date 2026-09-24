#include "entity_helper.h"

#include "entity_manager.h"
#include "game_functions.h"
#include "string_util.h"
#include "zeal.h"

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

std::string EntityHelper::get_base_name(const std::string &s) {
  size_t end = s.size();
  while (end > 0 && std::isdigit(static_cast<unsigned char>(s[end - 1]))) --end;
  return s.substr(0, end);
};

EntityHelper::EntityMap EntityHelper::filterNonPhs(const std::vector<std::string> &ph_names) {

  auto response = std::unordered_map<std::string, struct Zeal::GameStructures::Entity *>();

  auto *entity_manager = ZealService::get_instance()->entity_manager.get();
  if (!entity_manager) return response;

  auto entities = entity_manager->GetAll();

  for (auto &entry : entities) {
    if (!entry.second || entry.first.empty()) continue;
    if (entry.second->Type != Zeal::GameEnums::NPC) continue;

    auto entity_name = EntityHelper::get_base_name(entry.first);
    if (std::any_of(ph_names.begin(), ph_names.end(),
        [&entity_name](const std::string &ph_name) {
            return Zeal::String::compare_insensitive(entity_name, ph_name);
        })) {
      response.insert(entry);
    }
  }

  return response;
}