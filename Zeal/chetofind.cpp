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

void ChetoFind::Disable() {
  active = false;
  current_target = {};
  targetList.clear();
  ZealService::get_instance()->zone_map->clear();
  Zeal::Game::print_chat("ChetoFind: Disabled.");
}

void ChetoFind::start(const std::string &name) {
  std::string lower_name = name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

  // Get data from target
  auto target = getTargetData(lower_name);
  if (!target) {
    Zeal::Game::print_chat("ChetoFind: Unknown target '%s'. Known targets:", name.c_str());
    for (auto &entry : targets_data) Zeal::Game::print_chat("  - %s", entry.second.target_name.c_str());
    return;
  }
  current_target = target.value();

  Zeal::Game::print_chat("ChetoFind: Tracking '%s'. Placeholders:", current_target.target_name.c_str());
  for (auto &ph : current_target.ph_names) Zeal::Game::print_chat("  - %s", ph.c_str());
  Zeal::Game::print_chat("ChetoFind: %d spawn points, %d patrol paths loaded.", current_target.spawn_points.size(),
                         current_target.paths.size());

  // Get current target or PH list
  ph_list = EntityHelper::filterNonPhs(current_target.ph_names);

  setupPathings();
  filterPHs();

  active = true;
}

void ChetoFind::setupPathings() {

  // Build a lookup from grid_id -> {waypoints, is_circular}.
  path_by_grid.clear();
  for (auto &path : current_target.paths) {
    std::vector<Vec3> pts;
    for (auto &wp : path.waypoints) pts.push_back(Vec3(wp.x, wp.y, wp.z));
    path_by_grid[path.grid_id] = {std::move(pts), path.grid_type == 0};
  }

  // Build spawn-to-path segments: each spawn point connects to waypoint[0] of its assigned path.
  spawn_to_path_segments.clear();
  for (auto &sp : current_target.spawn_points) {
    auto it = path_by_grid.find(sp.pathgrid);
    if (it == path_by_grid.end() || it->second.points.empty()) continue;
    Vec2 spawn_pos(sp.x, sp.y);
    Vec2 path_start = it->second.points[0].toVec2();
    spawn_to_path_segments.push_back({spawn_pos, path_start});
  }
}

void ChetoFind::filterPHs() {
  const size_t initial_size = ph_list.size();
  std::unordered_map<std::string, struct Zeal::GameStructures::Entity *> filtered;
  for (auto &entry : ph_list) {
    auto *ent = entry.second;
    Vec3 pos(ent->Position.x, ent->Position.y, ent->Position.z);
    Vec2 pos2d = pos.toVec2();

    // Remove stationary NPCs - Not sure about this one..
    /* if (ent->MovementSpeed == 0.0f) {
      continue;
    }*/

    bool match = false;
    // Check if this PH is walking along any patrol path.
    for (auto &[grid_id, info] : path_by_grid) {
      if (VectorHelper::isPointOnPath(info.points, pos, 50.0f, info.circular)) {
        match = true;
        break;
      }
    }
    // Check if this PH is walking from a spawn point to the start of its path.
    if (!match) {
      for (auto &seg : spawn_to_path_segments) {
        if (VectorHelper::dist2DPointToSegmentSq(pos2d, seg.a, seg.b) <= tolerance_sq) {
          match = true;
          break;
        }
      }
    }
    if (match) {
      filtered.insert(entry);
    }
  }

  ph_list = filtered;
  if (initial_size != ph_list.size()) {
    Zeal::Game::print_chat("ChetoFind: Fitering PHs de %d a %d", initial_size, ph_list.size());
  }
}

void ChetoFind::tick() {
  if (!active) return;

  auto now = GetTickCount64();
  if (now - scan_timestamp < 1000) return;
  scan_timestamp = now;

  filterPHs();

  auto *zone_map = ZealService::get_instance()->zone_map.get();
  if (!zone_map) return;

  zone_map->clear();
  for (auto &entry : ph_list) {
    auto *ent = entry.second;
    zone_map->add_marker(
        static_cast<int>(ent->Position.x), 
        static_cast<int>(ent->Position.y),                         
        (std::string("[") + ent->Name + "]").c_str(), false);
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
            Disable();
            return true;
        }
        start(args[1]);
        return true;
        }
        Zeal::Game::print_chat("Usage: /chetofind <target> | off");
        Zeal::Game::print_chat("Known targets: Quillmane");
        return true;
    });
}

ChetoFind::~ChetoFind() {}
