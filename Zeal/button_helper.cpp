#include "button_helper.h"

#include "game_addresses.h"
#include "game_functions.h"
#include "string_util.h"

bool ButtonHelper::find_hotbuttons(Buttons& buttons) {

  auto* hb_wnd = Zeal::Game::Windows->HotButton;
  if (!hb_wnd) {
    Zeal::Game::print_chat("Zeal: Error, barra de hotbuttons innacesible.");
    return false;
  }

  for (int slot = 0; slot < 10; ++slot) {
    Zeal::GameUI::BasicWnd* btn = hb_wnd->GetChildItem("HB_Button" + std::to_string(slot + 1), false);
    if (!btn) continue;

    const char* text = btn->Text.CastToCharPtr();
    if (!text) continue;

    for (auto button : buttons) {
      if (Zeal::String::compare_insensitive(std::string(text), button->label)) {
        button->button = btn;
        button->slot = slot;
        break;
      }
    }
  }


  return true;
}

void ButtonHelper::click_button(Button& button) {
  if (!button.button || button.slot < 0) return;

  reinterpret_cast<void(__fastcall*)(Zeal::GameUI::SidlWnd*, int, int, int)>(0x4209bd)(
      reinterpret_cast<Zeal::GameUI::SidlWnd*>(button.button), 0, button.slot, 0);
}

void ButtonHelper::reset_hotbuttons(Buttons& buttons) {
  for (auto button : buttons) {
    button->button = nullptr;
    button->slot = -1;
  }
}