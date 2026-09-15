#include "chetofind.h"

#include <algorithm>

#include "callbacks.h"
#include "commands.h"
#include "entity_manager.h"
#include "game_functions.h"
#include "string_util.h"
#include "zeal.h"
#include "zone_map.h"

static const D3DCOLOR kAlertColor = D3DCOLOR_XRGB(255, 50, 50);

const std::unordered_map<std::string, FindTarget> &ChetoFind::GetKnownTargets() {
  static const std::unordered_map<std::string, FindTarget> targets = {
      {"quillmane",
       {"Quillmane",
        {"a_lion", "a_lioness", "an_escaped_Splitpaw_gnoll", "an_elephant", "a_mist_wolf", "a_shadow_wolf",
         "centaur_sheltie", "aviak_egret", "centaur_charger", "centaur_foal", "aviak_harrier", "a_cyclops"}}},
  };
  return targets;
}

void ChetoFind::Disable() { stop(); }

void ChetoFind::start(const std::string &name) {
  std::string lower_name = name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

  auto &known = GetKnownTargets();
  auto it = known.find(lower_name);
  if (it == known.end()) {
    Zeal::Game::print_chat("ChetoFind: Unknown target '%s'. Known targets:", name.c_str());
    for (auto &entry : known) Zeal::Game::print_chat("  - %s", entry.second.target_name.c_str());
    return;
  }

  stop();
  current_target = it->second;
  active = true;
  alert_fired = false;

  Zeal::Game::print_chat("ChetoFind: Tracking '%s'. Placeholders:", current_target.target_name.c_str());
  for (auto &ph : current_target.ph_names) Zeal::Game::print_chat("  - %s", ph.c_str());
}

void ChetoFind::stop() {
  if (!active) return;
  active = false;
  alert_fired = false;
  ph_entities.clear();
  current_target = {};
  ZealService::get_instance()->zone_map->clear();
  Zeal::Game::print_chat("ChetoFind: Disabled.");
}

void ChetoFind::tick() {
  if (!active) return;

  auto now = GetTickCount64();

  if (!is_updating && (now - scan_timestamp >= 1000)) {
    scan_timestamp = now;
    marker_index = 0;
    is_updating = true;
    ph_entities.clear();

    auto *entity_manager = ZealService::get_instance()->entity_manager.get();
    if (!entity_manager) return;

    auto entities = entity_manager->GetAll();
    bool target_found = false;

    for (auto &entry : entities) {
      if (!entry.second || entry.first.empty()) continue;
      if (entry.second->Type != Zeal::GameEnums::NPC) continue;

      if (Zeal::String::contains(entry.first, current_target.target_name)) {
        target_found = true;
        ph_entities.insert(ph_entities.begin(), entry.second);

        if (!alert_fired) {
          alert_fired = true;
          Zeal::Game::set_target(entry.second);
          Zeal::Game::print_chat(USERCOLOR_SHOUT, "ChetoFind: >>> %s FOUND! <<<", current_target.target_name.c_str());
          ZealService::get_instance()->zone_map->add_dynamic_label(
              std::string("[") + current_target.target_name + "]", static_cast<int>(entry.second->Position.x),
              static_cast<int>(entry.second->Position.y), 0, kAlertColor);
        }
        continue;
      }

      for (auto &ph_name : current_target.ph_names) {
        if (Zeal::String::compare_insensitive(entry.first, ph_name)) {
          ph_entities.push_back(entry.second);
          break;
        }
      }
    }

    if (!target_found) alert_fired = false;

    ZealService::get_instance()->zone_map->clear();
  }

  if (is_updating) {
    if (marker_index < ph_entities.size()) {
      auto *ent = ph_entities[marker_index++];
      if (ent) {
        bool is_target = Zeal::String::contains(std::string(ent->Name), current_target.target_name);
        std::string label =
            is_target ? std::string("[") + current_target.target_name + "]" : std::string("PH:") + ent->Name;
        ZealService::get_instance()->zone_map->add_marker(static_cast<int>(ent->Position.x),
                                                          static_cast<int>(ent->Position.y), label.c_str(), false);
      }
      return;
    }
    is_updating = false;
  }
}

ChetoFind::ChetoFind(ZealService *zeal) {
  zeal->callbacks->AddGeneric([this]() { Disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { Disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { Disable(); }, callback_type::EnterZone);

  zeal->callbacks->AddGeneric([this]() { tick(); });

  zeal->commands_hook->Add("/chetofind", {"/cfind"}, "Tracks a rare spawn and its placeholders on the map.",
        [this](std::vector<std::string> &args) {
            if (args.size() >= 2) {
                if (Zeal::String::compare_insensitive(args[1], "off")) {
                    stop();
                    return true;
                }
                start(args[1]);
                return true;
            }
            Zeal::Game::print_chat("Usage: /chetofind <target> | off");
            Zeal::Game::print_chat("Known targets:");
            for (auto &entry : GetKnownTargets()) {
              Zeal::Game::print_chat("  - %s", entry.second.target_name.c_str());
            }
            return true;
        });
}

ChetoFind::~ChetoFind() {}
