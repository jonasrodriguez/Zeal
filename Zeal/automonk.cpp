#include "automonk.h"

#include "game_addresses.h"
#include "game_functions.h"
#include "game_packets.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

#include "chat_helper.h"

// Forward declaration from commands.cpp - executes a command bypassing the detour.
void ForwardCommand(std::string cmd);

bool AutoMonk::start(const std::vector<std::string>& arguments) {
  state = State::On;
  clickies = false;
  for (const auto& arg : arguments) {
    if (Zeal::String::compare_insensitive(arg, "clickies")) {
      clickies = true;
      break;
    }
  }

  return find_hotbuttons();
}

bool AutoMonk::find_hotbuttons() {
  if (!ButtonHelper::find_hotbuttons(buttons)) {
    Zeal::Game::print_chat("AutoMonk: Error, no se pudieron encontrar los hotbuttons.");
    return false;
  }

  if (!kick_button.button) {
    Zeal::Game::print_chat("AutoMonk: Error, hotbutton 'Flying Kick' no encontrado.");
    return false;
  }

  return true;
}

void AutoMonk::handle_chat(const std::string& message) {
  if (state != State::On) return;
  if (!Zeal::Game::get_target() || !Zeal::Game::is_autoattacking()) return;

  auto enraged = ChatHelper::enrage_listener(message);
  if (enraged.empty()) return;

  auto target = Zeal::Game::get_target();

  Zeal::Game::print_chat("AutoMonk: Enraged message detected: %s", enraged.c_str());

  // Compare enraged with target name
  if (Zeal::String::compare_insensitive(target->Name, enraged)) {
    Zeal::Game::print_chat("AutoMonk: Your target %s enraged, carefull !", enraged.c_str());
    target_enraged = enraged;
  }
}

void AutoMonk::handle_damage(const Zeal::Packets::Damage_Struct* dmg) {
  // if (state != State::On || !dmg || dmg->damage <= 0) return;  // Only landed hits count.
  if (state != State::On || !dmg) return;  // Count missed hits too

  auto* self = Zeal::Game::get_self();
  if (!self || dmg->target != self->SpawnId) return;  // Not us taking the hit.
  if (dmg->source == self->SpawnId) return;           // Ignore self-inflicted damage.

  // Only react to a mob (NPC) hitting us, not players or environmental damage.
  auto* source = Zeal::Game::get_entity_by_id(dmg->source);
  if (!source || source->Type != Zeal::GameEnums::NPC) return;

  hit_by_mob = true;
}

void AutoMonk::tick() {
  if (!Zeal::Game::is_in_game()) return;

  if (!Zeal::Game::get_target() || !Zeal::Game::is_autoattacking()) {
    hit_by_mob = false;
    target_enraged.clear();

    return;
  }

  // A mob hit us stop auto attacking && FD
  if (!target_enraged.empty() && hit_by_mob) {
    Zeal::Game::print_chat("AutoMonk: Tu target %s esta ENRAGED y te esta pegando !", target_enraged.c_str());

    hit_by_mob = false;
    target_enraged.clear();
    Zeal::Game::do_autoattack(false);

    if (fd_button.button && !fd_button.button->Checked) {
      ButtonHelper::click_button(fd_button);
    }

    return;
  }

  ULONGLONG now = GetTickCount64();

  // Flying Kick
  if (now - last_interval_time >= kCheckIntervalMs) {
    if (kick_button.button && !kick_button.button->Checked) {
      ButtonHelper::click_button(kick_button);
      return;
    }
  }

  // Epic Fists
  if (now - last_epic_time_time >= kUseEpicMs) {
    last_epic_time_time = now;
    int slot = Zeal::Game::find_use_item_by_name("Celestial Fists", true);
    // Use celestial fist hotbutton if available, otherwise try with useitem command.
    if (slot != -1) {
      Zeal::GameStructures::GAMEITEMINFO* out_item = nullptr;
      Zeal::Game::use_item(slot, false, &out_item);
    } else {
      ForwardCommand("/use Celestial Fists");
    }
  }

  if (clickies && now - last_use_item_time >= kUseItemRetryMs) {
    last_use_item_time = now;
    if (clickies_button.button) {
      ButtonHelper::click_button(clickies_button);
    } else {
      ForwardCommand("/use Ring of Dain Frostreaver IV");
    }
  }
}

