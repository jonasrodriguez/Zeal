#include "autocleric.h"

#include <format>
#include <string>
#include <regex>

#include "callbacks.h"
#include "chat.h"
#include "commands.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

void AutoCleric::enable() { 
  search_spells();

  if (ch_gem == -1) {
    Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoCleric cannot be enabled: Complete Healing not found in spell gems.");
    return;
  }
  if (yaulp_gem == -1) {
    Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoCleric warning: Yaulp V not found in spell gems. Will cast CH without yaulping.");
  }

  autocleric = true;
}

void AutoCleric::disable() { 
  yaulp_gem = -1;
  ch_gem = -1;
  autocleric = false; 
}

void AutoCleric::search_spells() {

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  Zeal::GameStructures::SPELLMGR *get_spell_mgr();
  auto *spell_mgr = Zeal::Game::get_spell_mgr();
  for (int i = 0; i < GAME_NUM_SPELL_GEMS; ++i) {
    int spell_id = char_info->MemorizedSpell[i];

    if (spell_id == COMPLETE_HEALING_ID) {
      ch_gem = i;
    } else if (spell_id == YAULP_V_ID) {
      yaulp_gem = i;
    }
  }
}

bool AutoCleric::is_gem_ready(int gem_index) {
  bool invalid_index = gem_index < 0 || gem_index >= GAME_NUM_SPELL_GEMS;
  auto self = Zeal::Game::get_self();
  auto actor_info = self ? self->ActorInfo : nullptr;
  auto char_info = Zeal::Game::get_char_info();
  auto display = Zeal::Game::get_display();
  if (invalid_index || !self || !actor_info || !char_info || !display) return true;  // Default to true.

  int game_time = display->GameTimeMs;
  int spell_id = char_info->MemorizedSpell[gem_index];
  if (spell_id != kInvalidSpellId && actor_info->RecastTimeout[gem_index] > game_time) return false;

  return true;
}

// Inspects every incoming chat line for a "Go - <MyName> on <Target>" order.
void AutoCleric::handle_print_chat(const char *data, int color_index) {
  if (!autocleric || !data) return;

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (!self) return;

  // Expected format: "Go - PlayerName on TargetName"
  static const std::regex go_pattern(R"(^Go - (\S+) on (\S+)$)");
  std::cmatch match;
  if (!std::regex_match(data, match, go_pattern)) return;

  std::string player_name = match[1].str();
  std::string target_name = match[2].str();

  Zeal::Game::print_chat(USERCOLOR_SPELLS, "AutoCleric: Received CH order for player '%s' and target '%s'.", player_name.c_str(), target_name.c_str());

  // Only react if the player name matches us.
  if (!Zeal::String::compare_insensitive(player_name, self->Name)) return;

  Zeal::Game::print_chat(USERCOLOR_SPELLS, "AutoCleric: Order matches our name. Queuing CH cast on target '%s'.", target_name.c_str());

  /*Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (!self) return;

  // Expected format: "Go - PlayerName on TargetName"
  std::string msg(data);
  const std::string prefix = "Go - ";
  size_t prefix_pos = msg.find(prefix);
  if (prefix_pos == std::string::npos) return;

  size_t name_start = prefix_pos + prefix.size();
  const std::string on_sep = " on ";
  size_t on_pos = msg.find(on_sep, name_start);
  if (on_pos == std::string::npos) return;

  std::string player_name = msg.substr(name_start, on_pos - name_start);
  std::string target_name = msg.substr(on_pos + on_sep.size());

  // Strip any trailing whitespace or newline characters.
  while (!target_name.empty() &&
         (target_name.back() == ' ' || target_name.back() == '\r' || target_name.back() == '\n'))
    target_name.pop_back();
  while (!player_name.empty() && (player_name.back() == ' '))
    player_name.pop_back();

  // Only react if the player name matches us.
  if (!Zeal::String::compare_insensitive(player_name, self->Name)) return;

  // Order matches this character — queue the cast and acknowledge.
  heal_target = target_name;
  cast_pending = true;
  order_received_time = GetTickCount64();
  last_attempt_time = 0;  // Allow the tick() to fire immediately.

  Zeal::Game::do_say(false, "Casting complete heal on %s", heal_target.c_str());
  Zeal::Game::print_chat(USERCOLOR_SPELLS, "AutoCleric: CH order received for %s.", heal_target.c_str());*/
}

void AutoCleric::tick() {
  /* if (!autocleric)
    return;
  if (!is_active) return;

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  if (!Zeal::Game::is_in_game() || !self || !char_info) {
    setEnabled(false, -1, -1, false);
    return;
  }

  if (!cast_pending) return;

  attempt_cast();*/
}

AutoCleric::AutoCleric(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EnterZone);

  // Poll on each main loop iteration.
  zeal->callbacks->AddGeneric([this]() { tick(); });

  // Hook all printed chat lines to watch for CH orders (mirrors the Triggers pattern).
  zeal->chat_hook->add_print_chat_callback(
      [this](const char *data, int color_index) { handle_print_chat(data, color_index); });

  // Register the /autocleric command.
  zeal->commands_hook->Add(
      "/autocleric", {"/ac"},
      "Auto-casts CH on 'Go' orders. Usage: /autocleric (toggle, on, off)",
      [this](std::vector<std::string> &args) {
        bool force_on = (args.size() == 2 && args[1] == "on");
        bool force_off = (args.size() == 2 && args[1] == "off");
        bool enable_ac = force_on ? true : force_off ? false : !autocleric;
        (enable_ac) ? enable() : disable();
        return true;
      });
}

AutoCleric::~AutoCleric() {}