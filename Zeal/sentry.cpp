#include "sentry.h"

#include "callbacks.h"
#include "chat.h"
#include "commands.h"
#include "game_functions.h"
#include "string_util.h"
#include "zeal.h"

#include <string> 

void Sentry::enable(bool targets) {
  if (!load() && targets) {
    Zeal::Game::print_chat("Sentry: No se encontraron objetivos en el archivo ini.");
    return;
  }

  // Join channel "grupete"
  if (Zeal::Game::get_channel_number(CHANNEL.c_str()) == -1) {
    Zeal::Game::do_join(Zeal::Game::get_self(), CHANNEL.c_str());
  }

  Zeal::Game::print_chat("Sentry habilitado.");
  state = Idle;
  target = nullptr;
  sentry = true;
  all_targets = targets;
}

void Sentry::disable() { 
  state = Idle;
  target = nullptr;
  sentry = false; 
}

void Sentry::tick() {
  if (!sentry) return;

  ULONGLONG current_time = GetTickCount64();
  if (current_time - last_interval_time < kCheckIntervalMs) {
    return;
  }
  last_interval_time = current_time;

  switch (state) {
    case Idle:
    default:
      search_targets();
      break;
    case Attacking:
      // Keep checking it target still exists and is alive
      if (!target || target->HpCurrent <= 0) {
        Zeal::Game::print_chat("Sentry: Target is dead or missing. Returning to idle.");
        state = Idle;
        target = nullptr;
      }
      break;
  }
}

void Sentry::search_targets() {
  auto visible_entities = Zeal::Game::get_world_visible_actor_list(kMaxDist, true);

  auto pet = Zeal::Game::get_pet();

  for (auto entity : visible_entities) {
    if (!entity || entity->Type != Zeal::GameEnums::NPC) continue;
    if (pet && entity->SpawnId == pet->SpawnId) continue;

    Zeal::Game::print_chat("Sentry: Found entity %s with spawn id %d", entity->Name, entity->SpawnId);

    std::string entity_name = entity->Name;
    std::transform(entity_name.begin(), entity_name.end(), entity_name.begin(), ::tolower);

    for (auto name : targets) {
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);
      if (entity_name.find(name) != std::string::npos) {
        target = entity;
        state = Attacking;
        Zeal::Game::set_target(entity);

        Zeal::Game::pet_command(Zeal::GameEnums::PetCommand::Attack, target->SpawnId);

        auto channel_number = Zeal::Game::get_channel_number(CHANNEL.c_str());
        Zeal::Game::send_to_channel(channel_number, std::format("Sentry: Pet attack on on {}", target->Name).c_str());

        return;
      }
    }
  }
}

void Sentry::initialize_ini_filename() {
  std::string filename = "sentry.ini";
  std::filesystem::path file_path = Zeal::Game::get_game_path() / std::filesystem::path(filename);
  ini.set(file_path.string());
}

bool Sentry::load() {
  initialize_ini_filename();
  if (!ini.exists("targets", "0")) {
    return false;
  }
  for (int i = 0;; ++i) {
    std::string member = ini.getValue<std::string>("targets", std::to_string(i));
    if (member.empty()) break;
    targets.push_back(member);
  }
  return true;
}


Sentry::Sentry(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EnterZone);

  // Poll on each main loop iteration
  zeal->callbacks->AddGeneric([this]() { tick(); });

  // Register the /sentry command.
  zeal->commands_hook->Add("/sentry", {"/sen"},
        "Auto camps an area and sends pets to kill mobs nerby",
        [this](std::vector<std::string> &args) {
            if (args.size() > 2) {
                if (Zeal::String::compare_insensitive(args[1], "off")) {
                    disable();
                    return true;
                }
                if (Zeal::String::compare_insensitive(args[1], "all")) {
                    enable();
                    return true;
                }
            }

            enable();                             
            return true;
        });
}

Sentry::~Sentry() {}