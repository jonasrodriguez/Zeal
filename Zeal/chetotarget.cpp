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
  Zeal::Game::print_chat("ChetoTarget disabled.");
  targets.clear();
  ZealService::get_instance()->zone_map->clear();
}

void ChetoTarget::find_target(const std::string &arg, bool mapa) {

  if (arg.empty()) return;

  auto entity_manager = ZealService::get_instance()->entity_manager.get();

  if (std::all_of(arg.begin(), arg.end(), ::isdigit)) {
    auto target = entity_manager->Get(static_cast<WORD>(std::stoi(arg)));
    if (target) {
      Zeal::Game::do_target(target->Name);
      Zeal::Game::print_chat("ChetoTarget: Targeting entity with ID '%s'.", arg.c_str());
      if (mapa) {
        targets.push_back(target);
      }
    } else {
      Zeal::Game::print_chat("ChetoTarget: No entity found with ID '%s'.", arg.c_str());
    }
  } else {
    auto matches = entity_manager->GetNPCPartialMatches(arg);
    if (matches.size() > 0) {
      auto target = matches[0];
      Zeal::Game::set_target(target);
      if (mapa) {
        targets.assign(matches.begin(), matches.end());
      }
    } else {
      Zeal::Game::print_chat("ChetoTarget: No entities found with name '%s'.", arg.c_str());
      return;
    }
  }
}

void ChetoTarget::find_item(const std::string &item_visual_id) {
  
  if (!std::all_of(item_visual_id.begin(), item_visual_id.end(), ::isdigit)) return;

  auto intvalue = std::atoi(item_visual_id.c_str());    
  auto entities = ZealService::get_instance()->entity_manager.get()->GetAll();

  for (auto entity : entities) {
    if (!entity.second) continue;
    
    if (entity.second->EquipmentPrimaryItemType == intvalue) {
      ZealService::get_instance()->zone_map->add_marker(static_cast<int>(entity.second->Position.x),
                                                        static_cast<int>(entity.second->Position.y),
                                                        entity.second->Name);        
    }
  }
}

void ChetoTarget::tick() {

  if (targets.size() == 0) return;

  // Update map markers each second
   auto now = GetTickCount64();

  if (now - markers_timestamp >= 1000) {
    markers_timestamp = now;
    for (auto target : targets) {
      if (!target) continue;
      ZealService::get_instance()->zone_map->add_marker(
          static_cast<int>(target->Position.x),
          static_cast<int>(target->Position.y),
          target->Name);
    }
  }
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
            bool mapa = false;
            if (args.size() == 3 && Zeal::String::compare_insensitive(args[2], "mapa")) {
              mapa = true;
            }

            if (Zeal::String::compare_insensitive(args[1], "off")) {
              Disable();
              return true;
            }

            if (args[1].size() > 0) {
              find_target(args[1], mapa);
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
