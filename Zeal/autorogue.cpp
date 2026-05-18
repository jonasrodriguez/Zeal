#include "autorogue.h"

#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

// Forward declaration from commands.cpp - executes a command bypassing the detour.
void ForwardCommand(std::string cmd);

bool AutoRogue::start() {
  state = State::On;
  last_interval_time = 0;
  last_use_item_time = 0;
  last_hide_attempt_time = 0;
  return find_hotbuttons();
}

bool AutoRogue::find_hotbuttons() {
  backstab_btn = nullptr;
  hide_btn = nullptr;

  const std::string backstab_label = "Backstab";
  const std::string hide_label = "Hide";

  auto* hb_wnd = Zeal::Game::Windows->HotButton;
  if (!hb_wnd) {
    Zeal::Game::print_chat("AutoRogue: hotbutton bar not available.");
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
  }

  if (!backstab_btn) {
    Zeal::Game::print_chat("AutoRogue: hotbutton 'Backstab' not found.");
    return false;
  }
  if (!hide_btn) {
    Zeal::Game::print_chat("AutoRogue: hotbutton 'Hide' not found.");
    return false;
  }
  return true;
}

void AutoRogue::tick() {
  if (!Zeal::Game::is_in_game()) return;

  ULONGLONG now = GetTickCount64();

  switch (state) {
    case State::On:
      handle_combat(now);
      break;
    case State::StartHide:
      Zeal::Game::do_autoattack(false);
      last_hide_attempt_time = now;
      state = State::Hide;
      break;
    case State::Hide:
      if (now - last_hide_attempt_time >= kHideAttemptMs) {
        reinterpret_cast<void(__fastcall*)(Zeal::GameUI::SidlWnd*, int, int, int)>(0x4209bd)(
            reinterpret_cast<Zeal::GameUI::SidlWnd*>(hide_btn), 0, hide_slot, 0);
        state = State::FinishHide;
        last_hide_attempt_time = now;
      }
      break;
    case State::FinishHide:
      if (now - last_hide_attempt_time >= kHideAttemptMs) {
        Zeal::Game::do_autoattack(true);
        state = State::On;
      }
      break;
  }
}

void AutoRogue::handle_combat(ULONGLONG now) {
  if (!Zeal::Game::get_target() || !Zeal::Game::is_autoattacking()) return;

  if (now - last_interval_time >= kCheckIntervalMs) {
    last_interval_time = now;

    if (backstab_btn && !backstab_btn->Checked) {
      reinterpret_cast<void(__fastcall*)(Zeal::GameUI::SidlWnd*, int, int, int)>(0x4209bd)(
          reinterpret_cast<Zeal::GameUI::SidlWnd*>(backstab_btn), 0, backstab_slot, 0);
      return;
    }

    if (hide_btn && !hide_btn->Checked) {
      state = State::StartHide;
      return;
    }
  }

  if (clickies && now - last_use_item_time >= kUseItemRetryMs) {
    last_use_item_time = now;
    if (!clickies_btn) {
      ForwardCommand("/use Ring of Dain Frostreaver IV");
    }
  }
}