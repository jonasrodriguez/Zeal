#include "automonk.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

// Forward declaration from commands.cpp - executes a command bypassing the detour.
void ForwardCommand(std::string cmd);

bool AutoMonk::start(bool click) {
  state = State::On;
  clickies = click;
  return find_hotbuttons();
}

bool AutoMonk::find_hotbuttons() {
  kick_btn = nullptr;
  clickies_btn = nullptr;

  const std::string kick_label = "Flying Kick";
  const std::string epic_label = "Celestial Fists";
  const std::string clickies_label = "Clickies";

  auto* hb_wnd = Zeal::Game::Windows->HotButton;
  if (!hb_wnd) {
    Zeal::Game::print_chat("AutoMelee: hotbutton bar not available.");
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
  }

  if (!kick_btn) {
    Zeal::Game::print_chat("AutoMelee: hotbutton 'Fyling Kick' not found.");
    return false;
  }

  epic_slot = Zeal::Game::find_use_item_by_name(epic_label, true);

  return true;
}

void AutoMonk::tick() {
  if (!Zeal::Game::is_in_game()) return;
  if (!Zeal::Game::get_target() || !Zeal::Game::is_autoattacking()) return;

  ULONGLONG now = GetTickCount64();

  if (now - last_interval_time >= kCheckIntervalMs) {
    if (kick_btn && !kick_btn->Checked) {
      reinterpret_cast<void(__fastcall*)(Zeal::GameUI::SidlWnd*, int, int, int)>(0x4209bd)(
          reinterpret_cast<Zeal::GameUI::SidlWnd*>(kick_btn), 0, kick_slot, 0);
      return;
    }
  }

  // Epic Fists
  if (now - last_epic_time_time >= kUseEpicMs) {
    last_epic_time_time = now;
    // Use celestial fist hotbutton if available, otherwise try with useitem command.
    if (epic_slot != -1) {
      Zeal::GameStructures::GAMEITEMINFO* out_item = nullptr;
      Zeal::Game::use_item(epic_slot, false, &out_item);
    } else {
      ForwardCommand("/use Celestial Fists");
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
