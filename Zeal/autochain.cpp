#include <format>
#include <regex>
#include <string>

#include "autochain.h"
#include "callbacks.h"
#include "chat.h"
#include "commands.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "tick.h"
#include "zeal.h"

void AutoChain::enable(bool do_print) {
  search_spells();

  if (ch_gem == -1) {
    Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoChain cannot be enabled: Complete Healing not found in spell gems.");
    return;
  }
  if (yaulp_gem == -1) {
    Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoChain warning: Yaulp V not found in spell gems. Will cast CH without yaulping.");
  }

  if (do_print) {
    Zeal::Game::print_chat("AutoChain enabled.");
  }

  ch_target.clear();
  autochain = true;
}

void AutoChain::disable(bool do_print) {

  ch_target.clear();
  yaulp_gem = -1;
  ch_gem = -1;
  autochain = false;

  if (do_print) {
    Zeal::Game::print_chat("AutoChain disabled.");
  }
}

void AutoChain::search_spells() {
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

bool AutoChain::is_gem_ready(int gem_index) {
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

void AutoChain::handle_print_chat(const char *message, int color_index) {

  if (!autochain || !message) return;

  Zeal::Game::print_debug("AutoChain: Received chat message: %s", message);

  // Expected format: "GO PlayerName - CH a TargetName"
  static const std::regex go_pattern(R"(.*GO (\S+) - CH a ([^'\s]+))");
  std::cmatch match;
  if (!std::regex_search(message, match, go_pattern)) return;

  std::string player_name = match[1].str();
  std::string target_name = match[2].str();

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (!self) return;

  if (!Zeal::String::compare_insensitive(player_name, self->Name)) return;

  state = AboutToCast;
  ch_target = target_name;
}

void AutoChain::tick() {
  if (!autochain) return;

  ULONGLONG current_timestamp = GetTickCount64();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  switch (state) { 
    case AboutToCast: {
      Zeal::Game::do_ooc("Casteando CH a " + ch_target + "(%n)");
      Zeal::GameStructures::Entity *target = Zeal::Game::get_player_partial_name(ch_target.c_str());
      if (!target) {
        Zeal::Game::print_chat("AutoChain: Target '{}' not found.", ch_target);
        state = Idle;
        return;
      }
      Zeal::Game::set_target(target);

      if (char_info->cast(ch_gem, char_info->MemorizedSpell[ch_gem], 0, -1)) {
        casting_spell_id = char_info->MemorizedSpell[ch_gem];
      }
      start_ch_cast_timestamp = current_timestamp;
      state = CheckCasting;
      break;
    }
    case CheckCasting:
      if (Zeal::Game::GetSpellCastingTime() != -1) {
        if ((current_timestamp - start_ch_cast_timestamp) > 500) {
          retry_count = 0;
          state = Casting;
        }
      } else {
        Zeal::Game::print_chat("AutoChain: Failed to cast CH on {}. Retrying...", ch_target);
        Zeal::Game::do_ooc("Fizzle ??");
        retry_count++;
        state = RetryCasting;
      }
      break;
    case Casting:
      if (Zeal::Game::GetSpellCastingTime() != -1) {
        casting_visible_timestamp = current_timestamp;
        // Check if spell is halfway (5000)
        if ((current_timestamp - start_ch_cast_timestamp) == 5000) {
          Zeal::Game::do_ooc("CH al 50%");
        } else if ((current_timestamp - start_ch_cast_timestamp) > 9000) {  // About to land CH (9 seconds)          
          state = AboutToLand;
        }
      } else {
        Zeal::Game::do_ooc("CH Interrumpido. SKIP ME!");
        state = Idle;
      }
      break;
    case AboutToLand: {
      if (Zeal::Game::GetSpellCastingTime() != -1) {
        // If target HP is over 75%, cancel CH by ducking
        auto target = Zeal::Game::get_target();
        if (target && target->HpCurrent > (target->HpMax * 0.75)) {
          Zeal::Game::do_ooc(ch_target + " por encima del 75%. Cancelo CH");
          Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
          self->ChangeStance(Stance::Sit);
          state = Idle;
        }
      } else {
        Zeal::Game::print_chat("AutoChain: CH landed on {}.", ch_target);
        Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
        self->ChangeStance(Stance::Sit);
        state = Idle;
      }
    }
      break;
    case RetryCasting:
      if (retry_count > max_retries) {
        Zeal::Game::do_ooc("No puedo castear CH. SKIP ME!");
        retry_count = 0;
        state = Idle;
      } else {
        state = AboutToCast;
      }
      break;
    case Idle:
    default:
      return;
  }
}

 /* Zeal::GameStructures::Entity get_target(std::string name) { /* 
    
      Zeal::GameStructures::Entity *current = Zeal::Game::get_entity_list();
  while (current != nullptr) {
    if (current->Type == Zeal::GameEnums::NPC ||
        (current->Type == Zeal::GameEnums::Player &&
         std::none_of(allies.begin(), allies.end(), [current](auto entity) { return current == entity; }))) {
      entities.push_back(current);
    }
    current = current->Next;
  } 
  */

AutoChain::AutoChain(ZealService *zeal) {

  // Hook all printed chat lines to watch for CH orders (mirrors the Triggers pattern).
  zeal->chat_hook->add_print_chat_callback(
      [this](const char *data, int color_index) { handle_print_chat(data, color_index); });

  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { disable(false); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { disable(false); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { disable(false); }, callback_type::EnterZone);

  // Poll on each main loop iteration.
  zeal->callbacks->AddGeneric([this]() { tick(); });

  // Register the /autocleric command.
  zeal->commands_hook->Add("/autochain", {"/ac"}, "Auto-casts CH on 'Go' orders. Usage: /autochain (toggle, on, off)",
                           [this](std::vector<std::string> &args) {
                             bool force_on = (args.size() == 2 && args[1] == "on");
                             bool force_off = (args.size() == 2 && args[1] == "off");
                             bool enable_ac = force_on ? true : force_off ? false : !autochain;
                             (enable_ac) ? enable() : disable();
                             return true;
                           });
}

AutoChain::~AutoChain() {}