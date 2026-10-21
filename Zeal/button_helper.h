#pragma once

#include <string>
#include <vector>

#include "game_ui.h"

struct Button {
  std::string label;
  int slot = -1;
  Zeal::GameUI::BasicWnd *button = nullptr;
};

using Buttons = std::vector<Button*>;

class ButtonHelper {

 public:
  static bool find_hotbuttons(Buttons& buttons);
  static void reset_hotbuttons(Buttons& buttons);
  static void click_button(Button& button);

  static constexpr const char* backstab_label = "Backstab";
  static constexpr const char* hide_label = "Hide";
  static constexpr const char* evade_label = "Evade";
  static constexpr const char* clickies_label = "Clickies";
};