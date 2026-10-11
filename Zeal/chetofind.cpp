#include "chetofind.h"

#include <algorithm>

#include "callbacks.h"
#include "chetofind_data.h"
#include "commands.h"
#include "entity_manager.h"
#include "game_functions.h"
#include "string_util.h"
#include "zeal.h"
#include "zone_map.h"

void ChetoFind::Disable() {
  active = false;
  current_target = {};
  tracked_phs.clear();
  respawn_timer = 0;
  ZealService::get_instance()->zone_map->clear();
}

void ChetoFind::start(const std::string &name) {
  std::string lower_name = name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

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

  setupPathings();

  tracked_phs.clear();
  respawn_timer = 0;
  active = true;
}

void ChetoFind::setupPathings() {
  path_by_grid.clear();
  for (auto &path : current_target.paths) {
    std::vector<Vec3> pts;
    for (auto &wp : path.waypoints) pts.push_back(Vec3(wp.x, wp.y, wp.z));
    path_by_grid[path.grid_id] = {std::move(pts), path.grid_type == 0};
  }

  spawn_to_path_segments.clear();
  for (auto &sp : current_target.spawn_points) {
    auto it = path_by_grid.find(sp.pathgrid);
    if (it == path_by_grid.end() || it->second.points.empty()) continue;
    Vec2 spawn_pos(sp.x, sp.y);
    Vec2 path_start = it->second.points[0].toVec2();
    spawn_to_path_segments.push_back({spawn_pos, path_start});
  }
}

bool ChetoFind::isOnPathOrSpawnSegment(Zeal::GameStructures::Entity *ent) {
  Vec3 pos(ent->Position.x, ent->Position.y, ent->Position.z);
  Vec2 pos2d = pos.toVec2();

  for (auto &[grid_id, info] : path_by_grid) {
    if (VectorHelper::isPointOnPath(info.points, pos, 50.0f, info.circular)) return true;
  }

  for (auto &seg : spawn_to_path_segments) {
    if (VectorHelper::dist2DPointToSegmentSq(pos2d, seg.a, seg.b) <= kToleranceSq) return true;
  }

  return false;
}

void ChetoFind::refreshEntities() {
  auto *entity_manager = ZealService::get_instance()->entity_manager.get();
  if (!entity_manager) return;

  auto now = GetTickCount64();

  // Check if any currently tracked PH has disappeared (died).
  for (auto it = tracked_phs.begin(); it != tracked_phs.end();) {
    auto *ent = entity_manager->Get(it->second.spawn_id);
    if (!ent || ent->StructType != 0x03) {
      if (respawn_timer == 0) {
        respawn_timer = now + (current_target.respawn_seconds * 1000);
        Zeal::Game::print_chat("ChetoFind: PH died. Respawn in %ds...", current_target.respawn_seconds);
      }
      it = tracked_phs.erase(it);
    } else {
      ++it;
    }
  }

  // Scan for new PHs or the target itself.
  auto entities = entity_manager->GetAll();
  for (auto &entry : entities) {
    if (!entry.second || entry.first.empty()) continue;
    if (entry.second->Type != Zeal::GameEnums::NPC) continue;

    auto *ent = entry.second;

    if (Zeal::String::contains(entry.first, current_target.target_name)) {
      Zeal::Game::set_target(ent);
      Zeal::Game::print_chat(USERCOLOR_SHOUT, "ChetoFind: >>> %s FOUND! <<<", current_target.target_name.c_str());
      respawn_timer = 0;
      return;
    }

    if (tracked_phs.count(ent->SpawnId)) continue;

    auto entity_name = EntityHelper::get_base_name(entry.first);
    bool is_ph = std::any_of(
        current_target.ph_names.begin(), current_target.ph_names.end(),
        [&entity_name](const std::string &ph_name) { return Zeal::String::compare_insensitive(entity_name, ph_name); });
    if (!is_ph) continue;

    if (!isOnPathOrSpawnSegment(ent)) continue;

    TrackedPH tracked;
    tracked.spawn_id = ent->SpawnId;
    tracked.last_heading = ent->Heading;
    tracked.last_heading_change = now;
    tracked.stationary = false;
    tracked_phs[ent->SpawnId] = tracked;
  }
}

void ChetoFind::tick() {
  if (!active) return;

  auto now = GetTickCount64();
  if (now - scan_timestamp < 1000) return;
  scan_timestamp = now;

  // Check respawn timer.
  if (respawn_timer != 0 && now >= respawn_timer) {
    Zeal::Game::print_chat("ChetoFind: Respawn window! Scanning for new PH or target...");
    respawn_timer = 0;
  }

  refreshEntities();

  auto *entity_manager = ZealService::get_instance()->entity_manager.get();
  if (!entity_manager) return;

  // Update heading tracking for each PH.
  for (auto &[id, ph] : tracked_phs) {
    auto *ent = entity_manager->Get(ph.spawn_id);
    if (!ent) continue;
    float current_heading = ent->Heading;
    if (current_heading != ph.last_heading) {
      ph.last_heading = current_heading;
      ph.last_heading_change = now;
      ph.stationary = false;
    } else if (!ph.stationary && (now - ph.last_heading_change >= kStationaryThresholdMs)) {
      ph.stationary = true;
    }
  }

  // Render markers on the map.
  auto *zone_map = ZealService::get_instance()->zone_map.get();
  if (!zone_map) return;

  zone_map->clear();
  for (auto &[id, ph] : tracked_phs) {
    if (ph.stationary) continue;
    auto *ent = entity_manager->Get(ph.spawn_id);
    if (!ent) continue;
    zone_map->add_marker(static_cast<int>(ent->Position.x), static_cast<int>(ent->Position.y),
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
                                 Zeal::Game::print_chat("ChetoFind: Disabled.");
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
