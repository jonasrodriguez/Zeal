#include "chetofind.h"

#include <algorithm>

#include "callbacks.h"
#include "commands.h"
#include "entity_manager.h"
#include "game_functions.h"
#include "string_util.h"
#include "zeal.h"
#include "zone_map.h"

#include "chetofind_data.h"
#include "entity_helper.h"


static const D3DCOLOR kAlertColor = D3DCOLOR_XRGB(255, 50, 50);

const std::unordered_map<std::string, FindTarget> &ChetoFind::GetKnownTargets() {
  static const std::unordered_map<std::string, FindTarget> targets = {
      {"quillmane", MakeQuillmaneTarget()},
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
    stop();
    return;
  }

  current_target = it->second;
  active = true;

  Zeal::Game::print_chat("ChetoFind: Tracking '%s'. Placeholders:", current_target.target_name.c_str());
  for (auto &ph : current_target.ph_names) Zeal::Game::print_chat("  - %s", ph.c_str());
  Zeal::Game::print_chat("ChetoFind: %d spawn points, %d patrol paths loaded.", current_target.spawn_points.size(),
                         current_target.paths.size());
}

void ChetoFind::stop() {
  active = false;
  current_target = {};
  ZealService::get_instance()->zone_map->clear();
  Zeal::Game::print_chat("ChetoFind: Disabled.");
}

void ChetoFind::tick() {
  if (!active) return;

  auto now = GetTickCount64();
  if (now - scan_timestamp < 1000) return;
  scan_timestamp = now;

  auto *entity_manager = ZealService::get_instance()->entity_manager.get();
  if (!entity_manager) return;

  auto *zone_map = ZealService::get_instance()->zone_map.get();
  if (!zone_map) return;

  // Convert spawn paths to Vec3 lists once per scan.
  std::vector<std::vector<Vec3>> path_points;
  for (auto &path : current_target.paths) {
    std::vector<Vec3> pts;
    for (auto &wp : path.waypoints) pts.push_back(Vec3(wp.x, wp.y, wp.z));
    path_points.push_back(pts);
  }

  zone_map->clear();

  auto entities = entity_manager->GetAll();

  for (auto &entry : entities) {
    if (!entry.second || entry.first.empty()) continue;
    if (entry.second->Type != Zeal::GameEnums::NPC) continue;

    auto *ent = entry.second;

    // On every iteration check if target is up
    if (Zeal::String::contains(entry.first, current_target.target_name)) {
      Zeal::Game::set_target(ent);
      Zeal::Game::print_chat("ChetoFind: >>> %s FOUND! <<<", current_target.target_name.c_str());
      zone_map->add_marker(static_cast<int>(ent->Position.x), static_cast<int>(ent->Position.y),
                           (std::string("[") + current_target.target_name + "]").c_str(), false);
      break;
    }

    auto entity_name = EntityHelper::get_base_name(entry.first);

    // Filter out entities that are not placeholders for the target.
    if (!std::any_of(current_target.ph_names.begin(), current_target.ph_names.end(),
                     [&entity_name](const std::string &ph_name) {
                       return Zeal::String::compare_insensitive(entity_name, ph_name);
                     })) {
      continue;
    }

    // Filter out entities that are stationary
    if (ent->MovementSpeed == 0.0f) {
      continue;
    }

    // Check if this PH is near any spawn point.
    /* for (auto &sp : current_target.spawn_points) {
      float dx = ent->Position.x - sp.x;
      float dy = ent->Position.y - sp.y;
      if (dx * dx + dy * dy < 50.0f * 50.0f) {
        zone_map->add_marker(static_cast<int>(ent->Position.x), 
            static_cast<int>(ent->Position.y),            
            (std::string("[") + ent->Name + "]").c_str(), 
            false);
      }
    }*/

    // Check if this PH is walking along any patrol path.
    Vec3 pos(ent->Position.x, ent->Position.y, ent->Position.z);
    for (auto &pts : path_points) {
      if (VectorHelper::isPointOnPath(pts, pos, 50.0f)) {
        zone_map->add_marker(static_cast<int>(ent->Position.x), 
            static_cast<int>(ent->Position.y),                             
            (std::string("[") + ent->Name + "]").c_str(), 
            false);
      }
    }
  }


    /*// Check if this is a known placeholder.
    bool is_ph = false;
    for (auto &ph_name : current_target.ph_names) {
      if (Zeal::String::compare_insensitive(entry.first, ph_name)) {
        is_ph = true;
        break;
      }
    }
    if (!is_ph) continue;

    // Check if this PH is near any spawn point.
    bool at_spawn = false;
    for (auto &sp : current_target.spawn_points) {
      float dx = ent->Position.x - sp.x;
      float dy = ent->Position.y - sp.y;
      if (dx * dx + dy * dy < 50.0f * 50.0f) {
        at_spawn = true;
        break;
      }
    }

    // Check if this PH is walking along any patrol path.
    bool on_path = false;
    Vec3 pos(ent->Position.x, ent->Position.y, ent->Position.z);
    for (auto &pts : path_points) {
      if (VectorHelper::isPointOnPath(pts, pos, 50.0f)) {
        on_path = true;
        break;
      }
    }

    std::string label;
    if (on_path)
      label = std::string("PH*:") + ent->Name;
    else if (at_spawn)
      label = std::string("PH:") + ent->Name;
    else
      label = std::string("??:") + ent->Name;

    zone_map->add_marker(static_cast<int>(ent->Position.x), static_cast<int>(ent->Position.y), label.c_str(), false);
  }

  if (!target_found) alert_fired = false;*/
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
                             for (auto &entry : GetKnownTargets())
                               Zeal::Game::print_chat("  - %s", entry.second.target_name.c_str());
                             return true;
                           });
}

ChetoFind::~ChetoFind() {}
