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

void ForwardCommand(std::string cmd);

void AutoChain::enable() {
  search_spells();

  if (ch_gem == -1) {
    Zeal::Game::print_chat("AutoChain: ERROR ! Complete Healing no esta memorizado.");
    return;
  }
  if (yaulp_gem == -1) {
    Zeal::Game::print_chat("AutoChain: Yaulp V no esta memorizado, no se podra usar durante la chain.");
  }

  Zeal::Game::print_chat("AutoChain enabled.");

  ch_target.clear();
  autochain = true;
}

void AutoChain::disable() {

  ch_target.clear();
  yaulp_gem = -1;
  ch_gem = -1;
  autochain = false;
}

void AutoChain::handle_print_chat(const char *message, int color_index) {
  if (!autochain || !message) return;

  // Expected format: "GO PlayerName - CH a TargetName yaulp."
  static const std::regex go_pattern(R"(.*GO (\S+) - CH a ([^'\s]+)( yaulp)?)");
  std::cmatch match;
  if (!std::regex_search(message, match, go_pattern)) return;

  std::string player_name = match[1].str();
  std::string target_name = match[2].str();
  yaulp = match[3].matched;

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (!self) return;

  if (!Zeal::String::compare_insensitive(player_name, self->Name)) return;

  if (state != Idle) {
    send_chat("Casteando el CH anterior. SKIP ME!");
    return;
  }

  ch_target = target_name;

  state = AboutToCast;
}

void AutoChain::send_chat(const std::string &message) {
  Zeal::Game::send_raid_chat(message.c_str());
}

void AutoChain::tick() {
  if (!autochain) return;

  if (state == Idle) return;

  current_timestamp = GetTickCount64();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  switch (state) {
    case AboutToCast:
      tick_about_to_cast();
      break;
    case CheckCasting:
      tick_check_casting();
      break;
    case Casting:
      tick_casting();
      break;
    case AboutToLand:
      tick_about_to_land();
      break;
    case FinishCasting:
      tick_finish_casting();
      break;
    case Yaulp:
      tick_cast_yaulp();
      break;
    case RetryCasting:
      tick_retry_casting();
      break;
    case Idle:
    default:
      return;
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

void AutoChain::tick_about_to_cast() {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();

  // Check if have enough mana
  if (char_info->mana() < CH_MANA) {
    send_chat("No tengo mana para CH. SKIP ME!");
    state = Idle;
    return;
  } else if (char_info->mana() < CH_MANA * 1.5) {
    send_chat("Atencion ! Ultimo CH !!");
  }

  auto fizzle = self->ActorInfo->FizzleTimeout;
  auto gameTime = Zeal::Game::get_display()->GameTimeMs;
  // Check if CH gem is ready before trying to cast
  if (fizzle > gameTime) {
    return;
  }

  // Try to use links into the message
  const char kMarker = 0x12;
  const int kChId = 13;
  const int kWhoop = 11175;
  auto ch_msg = std::format("Casteando {0:c}0{1:06d}CH{2:c} a {3:c}0{4:06d}{5}{6:c} - %n mana.",
      kMarker, kChId, kMarker,kMarker, kWhoop, ch_target, kMarker);

  send_chat(ch_msg);
  Zeal::GameStructures::Entity *target = Zeal::Game::get_player_partial_name(ch_target.c_str());
  if (!target) {
    send_chat(std::format("No puedo encontrar a {}. SKIP ME!", ch_target));
    state = Idle;
    return;
  }
  Zeal::Game::set_target(target);

  if (char_info->cast(ch_gem, char_info->MemorizedSpell[ch_gem], 0, -1)) {
    casting_spell_id = char_info->MemorizedSpell[ch_gem];
  }
  start_ch_cast_timestamp = current_timestamp;
  state = CheckCasting;
}

void AutoChain::tick_check_casting() {
  if (Zeal::Game::GetSpellCastingTime() != -1) {
    casting_visible_timestamp = current_timestamp;
    if ((casting_visible_timestamp - start_ch_cast_timestamp) > 500) {
      retry_count = 0;
      state = Casting;
    }
  } else {
    retry_count++;
    state = RetryCasting;
  }
}

void AutoChain::tick_casting() {
  if (Zeal::Game::GetSpellCastingTime() != -1) {
    casting_visible_timestamp = current_timestamp;
    if ((current_timestamp - start_ch_cast_timestamp) > 9500) {  // About to land CH (9.5 seconds)
      state = AboutToLand;
    }
  } else {
    send_chat("CH Interrumpido. SKIP ME!");
    state = Idle;
  }
}

void AutoChain::tick_about_to_land() {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (Zeal::Game::GetSpellCastingTime() != -1) {
    // If target HP is over 75%, cancel CH by ducking
    Zeal::GameStructures::Entity *target = Zeal::Game::get_player_partial_name(ch_target.c_str());
    if (target && target->HpCurrent > (target->HpMax * 0.75)) {
      self->ChangeStance(Stance::Sit);
      state = Idle;  
    }
  } else { // Finish casting
    end_ch_cast_timestamp = current_timestamp;
    state = FinishCasting;
  }
}

void AutoChain::tick_finish_casting() {

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  bool casting_active = self->ActorInfo->CastingSpellId != kInvalidSpellId;
  bool server_ack_cast = (self->ActorInfo->CastingSpellGemNumber == 0xff);

  if (casting_active && !server_ack_cast) {
    return;
  }


  // Check if we need to cast yaulp
  if (yaulp) {
    bool has_yaulp = false;
    Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
    // Check if self has yaulp buff, if not cast it
    for (size_t i = 0; i < GAME_NUM_BUFFS; i++) {
      Zeal::GameStructures::_GAMEBUFFINFO *buff = char_info->GetBuff(i);
      if (buff && buff->BuffType != 0 && buff->SpellId == YAULP_V_ID) {
        has_yaulp = true;
      }
    }
    state = has_yaulp ? Idle : Yaulp;
  } else {
    self->ChangeStance(Stance::Sit);
    state = Idle;
  }
}

void AutoChain::tick_cast_yaulp() {

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();

  auto fizzle = self->ActorInfo->FizzleTimeout;
  auto gameTime = Zeal::Game::get_display()->GameTimeMs;

  // Check if yaulp gem is ready before trying to cast
  if (fizzle > gameTime) {
    return;
  }

  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();  
  if (char_info->cast(yaulp_gem, char_info->MemorizedSpell[yaulp_gem], 0, -1)) {
    casting_spell_id = char_info->MemorizedSpell[yaulp_gem];
  }
  state = Idle;
}

void AutoChain::tick_retry_casting() {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (retry_count > max_retries) {
    send_chat("No puedo castear CH. SKIP ME!");
    retry_count = 0;
    state = Idle;
    self->ChangeStance(Stance::Sit);
  } else {
    state = AboutToCast;
  }
}


AutoChain::AutoChain(ZealService *zeal) {

  // Hook all printed chat lines to watch for CH orders (mirrors the Triggers pattern).
  zeal->chat_hook->add_print_chat_callback(
      [this](const char *data, int color_index) { handle_print_chat(data, color_index); });

  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EnterZone);

  // Poll on each main loop iteration.
  zeal->callbacks->AddGeneric([this]() { tick(); });

  // Register the /autocleric command.
  zeal->commands_hook->Add("/autochain", {"/chain"}, "Auto-casts CH on 'Go' orders. Usage: /autochain (toggle, on, off)",
        [this](std::vector<std::string> &args) {                
            if (args.size() == 2 && args[1] == "off") {
                disable();
                return true;
            }
            enable();
            return true;
        });
}

AutoChain::~AutoChain() {}