#include "chetotarget.h"

#include "callbacks.h"
#include "commands.h"
#include "entity_manager.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "zeal.h"
#include "zone_map.h"

void ChetoTarget::Disable() {
  targets.clear();
  ZealService::get_instance()->zone_map->clear();
}

void ChetoTarget::find_target(const std::string &nombre) {

  if (nombre.empty()) return;
  if (!std::all_of(nombre.begin(), nombre.end(), ::isdigit)) return;

  auto entity_manager = ZealService::get_instance()->entity_manager.get();
  if (!entity_manager) {
    Zeal::Game::print_chat("ChetoTarget: Entity manager not available.");
    return;
  }

  auto entities = entity_manager->GetAll();
  if (entities.empty()) {
    Zeal::Game::print_chat("ChetoTarget: No entities found in the current zone.");
    return;
  }

  std::vector<Zeal::GameStructures::Entity *> found;

  for (auto entity : entities) {

    if (entity.first.empty()) continue;

    if (Zeal::String::contains(entity.first, nombre)) {
      found.push_back(entity.second);
    }
  }

  if (found.empty()) {
    Zeal::Game::print_chat("ChetoTarget: No se han encontrado entidades '%s'.", nombre.c_str());
  }

  targets.insert(targets.end(), found.begin(), found.end());
  auto first = found.front();
  Zeal::Game::do_target(first->Name);
  if (found.size() == 1) {
    Zeal::Game::print_chat("ChetoTarget: Se ha encontrado '%s'.", first->Name);
  } else {
    Zeal::Game::print_chat("ChetoTarget: Se han encontrado %d entidades coincidentes. Se ha seleccionado la primera.", found.size());
  } 
}

void ChetoTarget::find_item(const std::string &item_visual_id) {
  
  if (!std::all_of(item_visual_id.begin(), item_visual_id.end(), ::isdigit)) return;

  auto intvalue = std::atoi(item_visual_id.c_str());    
  auto entities = ZealService::get_instance()->entity_manager.get()->GetAll();

  Zeal::Game::print_chat("ChetoTarget: Looking for item with visual id '%d'.", intvalue);

  for (auto entity : entities) {
    if (!entity.second) continue;
    
    if (entity.second->EquipmentPrimaryItemType == intvalue) {

      Zeal::Game::print_chat("ChetoTarget: Found NPC %s with item '%d'.", entity.second->Name, intvalue);
      targets.push_back(entity.second);
    }
  }
}

void ChetoTarget::tick() {
  auto now = GetTickCount64();

  if (!is_updating && (now - markers_timestamp >= 1000)) {
    markers_timestamp = now;
    target_index = 0;
    is_updating = true;
  }

  if (is_updating) {
    if (target_index < targets.size()) {
      auto target = targets[target_index++];
      if (target) {
          ZealService::get_instance()->zone_map->add_marker(static_cast<int>(target->Position.x),
                                                            static_cast<int>(target->Position.y),
                                                            target->Name,
                                                            false);
      }
    }
    // Processing complete for this single tick -> function exits here
    return;
  }

  // 3. Reached the end of targets vector, stop updating until next second trigger
  is_updating = false;
}

ChetoTarget::ChetoTarget(ZealService *zeal) {

  zeal->callbacks->AddGeneric([this]() { Disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { Disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { Disable(); }, callback_type::EnterZone);

  zeal->callbacks->AddGeneric([this]() { tick(); });

  // Register the /chetotarget command.
  zeal->commands_hook->Add("/chetotarget", {"/ct"}, "Tries to target far away target.",
    [this](std::vector<std::string> &args) {
        if (args.size() >= 2) {

            if (Zeal::String::compare_insensitive(args[1], "off")) {
              Disable();
              return true;
            }

            if (args[1].size() > 0) {
              find_target(args[1]);
              return true;
            }

            return true;
        }

        Zeal::Game::print_chat("Uso: /chetotarget name | id | off <mapa | >");
        return true;
    });

  zeal->commands_hook->Add("/chetobusca", {"/cb"}, "Tries to find enemy holding an item.",
    [this](std::vector<std::string> &args) {
        if (args.size() == 2) {
            if (Zeal::String::compare_insensitive(args[1], "off")) {
                Disable();
                return true;
            }
            find_item(args[1]);
            return true;
        }

        Zeal::Game::print_chat("Uso: /chetobusca itemId");
        return true;
    });

}

ChetoTarget::~ChetoTarget() {};
