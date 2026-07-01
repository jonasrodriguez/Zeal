#include "autorogue.h"

#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

// Forward declaration from commands.cpp - executes a command bypassing the detour.
void ForwardCommand(std::string cmd);

bool AutoRogue::start(const std::vector<std::string>& arguments) {
  state = State::On;
  clickies = false;
  hides = false;
  for (const auto& arg : arguments) {
    if (Zeal::String::compare_insensitive(arg, "clickies")) {
      clickies = true;
      break;
    }
    if (Zeal::String::compare_insensitive(arg, "hide")) {
      hides = true;
      break;
    }
  }
  Zeal::Game::print_chat("AutoRogue: Clickies: %s, Hide: %s", clickies ? "ON" : "OFF", hides ? "ON" : "OFF");
  return find_hotbuttons();
}

bool AutoRogue::find_hotbuttons() {
  backstab_btn = nullptr;
  hide_btn = nullptr;

  const std::string backstab_label = "Backstab";
  const std::string hide_label = "Hide";
  const std::string evade_label = "Evade";
  const std::string clickies_label = "Clickies";

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

    if (Zeal::String::compare_insensitive(std::string(text), backstab_label)) {
      backstab_btn = btn;
      backstab_slot = slot;
    }
    if (Zeal::String::compare_insensitive(std::string(text), hide_label)) {
      hide_btn = btn;
      hide_slot = slot;
    }
    if (Zeal::String::compare_insensitive(std::string(text), evade_label)) {
      evade_btn = btn;
      evade_slot = slot;
    }
    if (Zeal::String::compare_insensitive(std::string(text), clickies_label)) {
      clickies_btn = btn;
      clickies_slot = slot;
    }
  }

  if (!backstab_btn) {
    Zeal::Game::print_chat("AutoRogue: Error, hotbutton 'Backstab' no encontrado.");
    return false;
  }

  return true;
}

void AutoRogue::tick() {
  if (!Zeal::Game::is_in_game()) return;
  
  // Skip process while stunned
  Zeal::GameStructures::GAMECHARINFO* char_info = Zeal::Game::get_char_info();
  if (!char_info || char_info->StunnedState) {
    return;
  }

  ULONGLONG now = GetTickCount64();

  if (!Zeal::Game::get_target() || !Zeal::Game::is_autoattacking()) return;

  if (now - last_interval_time >= kCheckIntervalMs) {
    last_interval_time = now;

    if (backstab_btn && !backstab_btn->Checked) {
      reinterpret_cast<void(__fastcall*)(Zeal::GameUI::SidlWnd*, int, int, int)>(0x4209bd)(
          reinterpret_cast<Zeal::GameUI::SidlWnd*>(backstab_btn), 0, backstab_slot, 0);
      return;
    }

    if (hides && hide_btn && !hide_btn->Checked) {
      // Do evade if avialable, its just faster
      if (evade_btn) {
        reinterpret_cast<void(__fastcall*)(Zeal::GameUI::SidlWnd*, int, int, int)>(0x4209bd)(
            reinterpret_cast<Zeal::GameUI::SidlWnd*>(evade_btn), 0, evade_slot, 0);
        return;
      }

      return;
    }
  }

  if (clickies && now - last_use_item_time >= kUseItemRetryMs) {
    last_use_item_time = now;
    if (clickies_btn) {
      reinterpret_cast<void(__fastcall*)(Zeal::GameUI::SidlWnd*, int, int, int)>(0x4209bd)(
          reinterpret_cast<Zeal::GameUI::SidlWnd*>(clickies_btn), 0, clickies_slot, 0);
    } else {
      ForwardCommand("/use Ring of Dain Frostreaver IV");
    }
  }  
}
