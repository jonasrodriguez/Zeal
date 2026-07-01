#include "autowarrior.h"

#include <format>
#include <regex>

#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

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
  kick_btn = nullptr;
  clickies_btn = nullptr;
  taunt_btn = nullptr;

  const std::string kick_label = "Kick";
  const std::string clickies_label = "Clickies";
  const std::string taunt_label = "Taunt";

  auto* hb_wnd = Zeal::Game::Windows->HotButton;
  if (!hb_wnd) {
    Zeal::Game::print_chat("AutoMelee: Error, barra de hotbuttons innacesible.");
    return false;
  }

  for (int slot = 0; slot < 10; ++slot) {
    Zeal::GameUI::BasicWnd* btn = hb_wnd->GetChildItem("HB_Button" + std::to_string(slot + 1), false);
    if (!btn) continue;

    const char* text = btn->Text.CastToCharPtr();
    if (!text) continue;

    if (Zeal::String::compare_insensitive(std::string(text), kick_label)) {
      kick_btn = btn;
      kick_slot = slot;
    }
    if (Zeal::String::compare_insensitive(std::string(text), clickies_label)) {
      clickies_btn = btn;
      clickies_slot = slot;
    }
    if (Zeal::String::compare_insensitive(std::string(text), taunt_label)) {
      taunt_btn = btn;
      taunt_slot = slot;
    }
  }

  if (!kick_btn) {
    Zeal::Game::print_chat("AutoMelee: Error, hotbutton 'Kick' no encontrado.");
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
  static const std::regex enraged_pattern(R"(^(\S+) has become ENRAGED)");
  std::smatch match;
  if (!std::regex_search(message, match, enraged_pattern)) return;

  const std::string name = match[1].str();
  if (name.empty()) return;
  Zeal::Game::print_chat("AutoMelee: Cuidado ! Enrage de " + name);
  Zeal::GameStructures::Entity* target = Zeal::Game::get_target();
  if (target) {
    if (Zeal::String::compare_insensitive(name, target->Name)) {
      Zeal::Game::print_chat("AutoMelee: Enrage mob and target are the same");
      state = State::Enraged;
    }
  }
}

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

  // Kick !
  if (now - last_interval_time >= kCheckIntervalMs) {
    if (kick_btn && !kick_btn->Checked) {
      reinterpret_cast<void(__fastcall*)(Zeal::GameUI::SidlWnd*, int, int, int)>(0x4209bd)(
          reinterpret_cast<Zeal::GameUI::SidlWnd*>(kick_btn), 0, kick_slot, 0);
      return;
    }
    if (taunt && taunt_btn && !taunt_btn->Checked) {
      reinterpret_cast<void(__fastcall*)(Zeal::GameUI::SidlWnd*, int, int, int)>(0x4209bd)(
          reinterpret_cast<Zeal::GameUI::SidlWnd*>(kick_btn), 0, kick_slot, 0);
      return;
    }
  }

  if (clickies && now - last_use_item_time >= kUseItemRetryMs) {
    last_use_item_time = now;

    if (clickies_btn) {
      reinterpret_cast<void(__fastcall*)(Zeal::GameUI::SidlWnd*, int, int, int)>(0x4209bd)(
          reinterpret_cast<Zeal::GameUI::SidlWnd*>(clickies_btn), 0, clickies_slot, 0);
    }
  }
}
