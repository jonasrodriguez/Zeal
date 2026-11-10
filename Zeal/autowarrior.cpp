#include "autowarrior.h"

#include <format>
#include <regex>

#include "game_addresses.h"
#include "game_functions.h"
#include "game_packets.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

#include "chat_helper.h"

// Forward declaration from commands.cpp - executes a command bypassing the detour.
void ForwardCommand(std::string cmd);

bool AutoWarrior::start(const std::vector<std::string>& arguments) {
  state = State::On;
  clickies = false;
  taunt = false;
  for (const auto& arg : arguments) {
    if (Zeal::String::compare_insensitive(arg, "clickies")) {
      clickies = true;
      break;
    }
    if (Zeal::String::compare_insensitive(arg, "taunt")) {
      taunt = true;
      break;
    }
  }
  Zeal::Game::print_chat("AutoWarrior: Clickies: %s, Taunt: %s", clickies ? "ON" : "OFF", taunt ? "ON" : "OFF");
  return find_hotbuttons();
}

bool AutoWarrior::find_hotbuttons() {

  if (!ButtonHelper::find_hotbuttons(buttons)) {
    Zeal::Game::print_chat("AutoWarrior: Error, no se pudieron encontrar los hotbuttons.");
    return false;
  }

  if (!kick_button.button) {
    Zeal::Game::print_chat("AutoWarrior: Error, hotbutton 'Kick' no encontrado.");
    return false;
  }

  return true;
}

void AutoWarrior::handle_chat(const std::string &message) {

  // Check disciplines messages
  static const std::string defensive = "You assume a defensive fighting style.";
  static const std::string evasive = "You assume an evasive fighting style.";
  static const std::string discOff = "You return to your normal fighting style.";

  if (message == defensive  || message == evasive) {
    discipline_start_time = GetTickCount64();
    discipline = message == defensive ? Discipline::Defensive : Discipline::Evasive;
    return;
  } else if (message == discOff) {
    discipline = Discipline::Off;
    return;
  }

  //Pattern: Someone has become ENRAGED
  auto enraged = ChatHelper::enrage_listener(message);
  if (enraged.empty()) return;

  auto target = Zeal::Game::get_target();

  Zeal::Game::print_chat("AutoWarrior: Enraged message detected: %s", enraged.c_str());

  // Compare enraged with target name
  if (Zeal::String::compare_insensitive(target->Name, enraged)) {
    Zeal::Game::print_chat("AutoWarrior: Your target %s enraged, carefull !", enraged.c_str());
    state = State::Enraged;
  }
}

void AutoWarrior::handle_damage(const Zeal::Packets::Damage_Struct* dmg) { return; }

void AutoWarrior::tick() {
  if (!Zeal::Game::is_in_game()) return;
  if (!Zeal::Game::get_target() || !Zeal::Game::is_autoattacking()) return;
  if (state == State::Off) return;

  if (state == State::Enraged) {
    Zeal::Game::print_chat("AutoMelee: Target ENRAGED, dejando de atacar");
    Zeal::Game::do_autoattack(false);
    state = State::On;
  }

  // Skip process while stunned
  Zeal::GameStructures::GAMECHARINFO* char_info = Zeal::Game::get_char_info();
  if (!char_info || char_info->StunnedState) {
    return;
  }

  ULONGLONG now = GetTickCount64();

  if (discipline_start_time != 0 && (discipline == Discipline::Evasive || discipline == Discipline::Defensive)) {
    if (now - discipline_start_time >= 150000) {
      auto msg = std::format("Quedan 30 segundos de {} !!", discipline == Discipline::Defensive ? "defensive" : "evasive");
      Zeal::Game::do_gsay(msg);
      Zeal::Game::send_raid_chat(msg);
      discipline_start_time = 0;
    }
    return;
  }

  if (discipline == Discipline::Off) {
    Zeal::Game::do_gsay("Discipline is OFF !");
    Zeal::Game::send_raid_chat("Discipline is OFF !");
    discipline = Discipline::None;
    return;
  }

  if (now - last_interval_time >= kCheckIntervalMs) {
    if (kick_button.button && !kick_button.button->Checked) {
      ButtonHelper::click_button(kick_button);
      return;
    }
    if (taunt && taunt_button.button && !taunt_button.button->Checked) {
      ButtonHelper::click_button(taunt_button);
      return;
    }
  }

  if (clickies && now - last_use_item_time >= kUseItemRetryMs) {
    last_use_item_time = now;

    if (clickies_button.button) {
      ButtonHelper::click_button(clickies_button);
    }
  }
}
