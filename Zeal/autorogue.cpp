#include "autorogue.h"

#include "game_addresses.h"
#include "game_functions.h"

#include "string_util.h"
#include "zeal.h"

#include "chat_helper.h"

// Forward declaration from commands.cpp - executes a command bypassing the detour.
void ForwardCommand(std::string cmd);

bool AutoRogue::start(const std::vector<std::string>& arguments) {
  state = State::On;
  clickies = false;
  hides = false;
  hit_by_mob = false;
  for (const auto& arg : arguments) {
    if (Zeal::String::compare_insensitive(arg, "clickies")) {
      clickies = true;
      break;
    }
    if (Zeal::String::compare_insensitive(arg, "hide")) {
      hides = true;
      break;
    }
    if (Zeal::String::compare_insensitive(arg, "stop")) {
      stop_on_hit = true;
      break;
    }
  }
  Zeal::Game::print_chat("AutoRogue: Clickies: %s, Hide: %s, Stop on Hit: %s", clickies ? "ON" : "OFF", hides ? "ON" : "OFF", stop_on_hit);
  return find_hotbuttons();
}

bool AutoRogue::find_hotbuttons() {
  
  if (!ButtonHelper::find_hotbuttons(buttons)) {
    Zeal::Game::print_chat("AutoRogue: Error, no se pudieron encontrar los hotbuttons.");
    return false;
  }

  if (!backstab_button.button) {
    Zeal::Game::print_chat("AutoRogue: Error, hotbutton 'Backstab' no encontrado.");
    return false;
  }

  return true;
}

void AutoRogue::handle_chat(const std::string& message) {

  if (!Zeal::Game::get_target() || !Zeal::Game::is_autoattacking()) return;

  auto enraged = ChatHelper::enrage_listener(message);
  if (enraged.empty()) return;
  
  auto target = Zeal::Game::get_target();

  Zeal::Game::print_chat("AutoRogue: Enraged message detected: %s", enraged.c_str());

  // Compare enraged with target name
  if (Zeal::String::compare_insensitive(target->Name, enraged)) {
    Zeal::Game::print_chat("AutoRogue: Your target %s enraged, carefull !", enraged.c_str());
    target_enraged = target;
  }
}

void AutoRogue::tick() {
  if (!Zeal::Game::is_in_game()) return;

  if (!Zeal::Game::get_target() || !Zeal::Game::is_autoattacking()) {
    hit_by_mob = false;
    target_enraged = nullptr;

    return;
  }
  
  // Skip process while stunned
  Zeal::GameStructures::GAMECHARINFO* char_info = Zeal::Game::get_char_info();
  if (!char_info || char_info->StunnedState) {
    return;
  }

  ULONGLONG now = GetTickCount64();

  // A mob hit us stop auto attacking and press hide
  if (target_enraged && hit_by_mob || stop_on_hit && hit_by_mob) {
    Zeal::Game::print_chat("AutoRogue: Hit by mob, stopping autoattack and pressing hide.");
    hit_by_mob = false;
    Zeal::Game::do_autoattack(false);
    if (hide_button.button && !hide_button.button->Checked) {
      ButtonHelper::click_button(hide_button);
    }
    return;
  }

  if (now - last_interval_time >= kCheckIntervalMs) {
    last_interval_time = now;

    if (backstab_button.button && !backstab_button.button->Checked) {
      ButtonHelper::click_button(backstab_button);
      return;
    }

    if (hides && hide_button.button && !hide_button.button->Checked) {
      // Do evade if avialable, its just faster
      if (evade_button.button) {
        ButtonHelper::click_button(evade_button);
        return;
      }
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

void AutoRogue::handle_damage(const Zeal::Packets::Damage_Struct* dmg) {
  if (state != State::On || !dmg || dmg->damage <= 0) return;  // Only landed hits count.

  auto* self = Zeal::Game::get_self();
  if (!self || dmg->target != self->SpawnId) return;  // Not us taking the hit.
  if (dmg->source == self->SpawnId) return;           // Ignore self-inflicted damage.

  // Only react to a mob (NPC) hitting us, not players or environmental damage.
  auto* source = Zeal::Game::get_entity_by_id(dmg->source);
  if (!source || source->Type != Zeal::GameEnums::NPC) return;

  hit_by_mob = true;
}
