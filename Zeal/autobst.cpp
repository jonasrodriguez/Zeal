#include <format>
#include <regex>

#include "autobst.h"

#include "callbacks.h"
#include "chat.h"
#include "chatfilter.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_ui.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "tag_arrows.h"
#include "target_ring.h"
#include "zeal.h"

void ForwardCommand(std::string cmd);

void AutoBst::enable() {
  if (auto_bst) {
    Zeal::Game::print_chat("AutoBst ya estaba habilitado.");
    return;
  }
  auto_bst = true;
  Zeal::Game::print_chat("AutoBst habilitado.");

  search_spells();
  if (slow.gem == -1) {
    Zeal::Game::print_chat("AutoBst warning: Sha's Lethargy not found in spell gems. Will not cast slow.");
  }
  if (pethaste.gem == -1) {
    Zeal::Game::print_chat("AutoBst warning: Sha's Ferocity not found in spell gems. Will not cast pet haste.");
  }

  // Join channel "grupete"
  if (Zeal::Game::get_channel_number(CHANNEL.c_str()) == -1) {
    Zeal::Game::do_join(Zeal::Game::get_self(), CHANNEL.c_str());
  }

  // Add some delays for the buffs so they don't overlap
  auto now = GetTickCount64();
  last_pet_haste_time = now - static_cast<ULONGLONG>(pethaste.duration) * 1000 + 15000;
}

void AutoBst::disable() {
  slow.gem = -1;
  pethaste.gem = -1;
  auto_bst = false;
}

void AutoBst::search_spells() {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  Zeal::GameStructures::SPELLMGR *get_spell_mgr();
  auto *spell_mgr = Zeal::Game::get_spell_mgr();
  for (int i = 0; i < GAME_NUM_SPELL_GEMS; ++i) {
    int spell_id = char_info->MemorizedSpell[i];

    if (spell_id == slow.spell_id) {
      slow.gem = i;
    } else if (spell_id == pethaste.spell_id) {
      pethaste.gem = i;
    }
  }
}

bool AutoBst::handle_chat(const char *message, int color_index) {
  if (!auto_bst || !message) return false;

  auto tag_channel_number = Zeal::Game::get_channel_number(CHANNEL.c_str());
  if (tag_channel_number < 0) return false;

  // Only scan the expected response channel (if not joined, channel will be -1 and bail out above).
  if ((USERCOLOR_CHAT_1 + tag_channel_number) != color_index &&
      (USERCOLOR_ECHO_CHAT_1 + tag_channel_number) != color_index)
    return false;

  // Expected format: "Player tells Grupete:1, 'Assist me on Target'"
  static const std::regex go_pattern(R"((\S+) tells Grupete:\d+, 'Assist me on ([^']+)')", std::regex_constants::icase);
  std::cmatch match;
  if (!std::regex_search(message, match, go_pattern)) return false;

  std::string player = match[1].str();
  std::string target = match[2].str();

  Zeal::Game::print_chat("AutoMage: Assisting %s on %s", player.c_str(), target.c_str());

  ForwardCommand(std::format("/assist {}", player));

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (self->StandingState != Stance::Stand) {
    self->ChangeStance(Stance::Stand);
  }

  state = Assist;

  return false;
}

void AutoBst::tick() {
  if (!auto_bst) return;

  auto now = GetTickCount64();
  // Add a 500 ms delay, no need to check every tick
  if (now - last_interval_time < kCheckIntervalMs) return;
  last_interval_time = now;

  // Recast pet haste

  if (state == Idle) return;

  switch (state) {
    case Assist:
      tick_assist(now);
      break;
    case Slow:
      tick_slow(now);
      break;
    case Nuke:
      break;
    case AboutToSit: {
      // Wait last action timer to avoid agroing
      if ((now - last_action_time) <= kLastActionIntervalMs) return;
      Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
      self->ChangeStance(Stance::Sit);
      state = Idle;
    } break;
    case Idle:
    default:
      return;
  }
}

void AutoBst::tick_assist(ULONGLONG now) {
  Zeal::Game::do_autoattack(false);
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();

  if (cast_slow) {
  } else {
  }
}

void AutoBst::tick_slow(ULONGLONG now) {

  // Add delay to avoid agroing (2seconds)
  if ((now - last_action_time) <= 2000) return;
  if (casting_spell_id == kInvalidSpellId) {
    cast_spell(slow);
    return;
  }

  if (Zeal::Game::GetSpellCastingTime() != -1) {
    return;
  }
}

void AutoBst::buff_pet(ULONGLONG now) {
  if (pethaste.gem == -1) return;

  if (casting_spell_id == kInvalidSpellId) {
    cast_spell(pethaste);
    return;
  }
  if (Zeal::Game::GetSpellCastingTime() != -1) {
    return;
  }

  casting_spell_id = kInvalidSpellId;
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  self->ChangeStance(Stance::Sit);
  last_pet_haste_time = now;
}

void AutoBst::cast_spell(const Spell &spell) {
  if (spell.gem == -1) return;

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  auto fizzle = self->ActorInfo->FizzleTimeout;
  auto gameTime = Zeal::Game::get_display()->GameTimeMs;
  // Check if gem is ready before trying to cast
  if (fizzle > gameTime) {
    return;
  }

  if (char_info->cast(spell.gem, char_info->MemorizedSpell[spell.gem], 0, -1)) {
    casting_spell_id = char_info->MemorizedSpell[spell.gem];
  }
}

AutoBst::AutoBst(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EnterZone);

  // Hook all printed chat lines to match for combat data
  zeal->chat_hook->add_incoming_chat_callback(
      [this](const char *msg, int color_index) { return handle_chat(msg, color_index); });

  // Poll on each main loop iteration (same pattern as autoability).
  zeal->callbacks->AddGeneric([this]() { tick(); });

  zeal->commands_hook->Add("/autobst", {"/bst"}, "Assist, send pet and slow",
                           [this](std::vector<std::string> &args) {
                             int args_size = args.size();
                             bool castNuke = false;
                             bool castMalo = false;
                             bool castDS = false;

                             if (args_size > 1) {
                               if (Zeal::String::compare_insensitive(args[1], "off")) {
                                 Zeal::Game::print_chat("AutoMage disabled.");
                                 disable();
                                 return true;
                               }

                               if (Zeal::String::compare_insensitive(args[1], "help")) {
                                 Zeal::Game::print_chat("Uso: /automage malo nuke ds");
                                 return true;
                               }
                             }
                             enable();
                             return true;
                           });
}

AutoBst::~AutoBst() {}
