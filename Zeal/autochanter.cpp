#include "autochanter.h"

#include <string>
#include <format>
#include <regex>

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

void AutoChanter::enable() {
  if (auto_chanter) {
    Zeal::Game::print_chat("AutoChanter ya estaba habilitado.");
    return;
  }
  auto_chanter = true;
  Zeal::Game::print_chat("AutoChanter habilitado.");

  search_spells();
  if (stun.gem == -1) {
    Zeal::Game::print_chat("AutoChanter warning: Color Slant not found in spell gems. Will not cast stun.");
  }
  if (charm.gem == -1) {
    Zeal::Game::print_chat("AutoChanter warning: Boltran's Agacerie not found in spell gems. Will not cast charm.");
  }

  // Join channel "grupete"
  if (Zeal::Game::get_channel_number(CHANNEL.c_str()) == -1) {
    Zeal::Game::do_join(Zeal::Game::get_self(), CHANNEL.c_str());
  }
}

void AutoChanter::disable() {
  auto_chanter = false;
  my_pet = nullptr;
  stun.gem = -1;
  charm.gem = -1;
}

void AutoChanter::search_spells() {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  Zeal::GameStructures::SPELLMGR *get_spell_mgr();
  auto *spell_mgr = Zeal::Game::get_spell_mgr();
  for (int i = 0; i < GAME_NUM_SPELL_GEMS; ++i) {
    int spell_id = char_info->MemorizedSpell[i];

    if (spell_id == stun.spell_id) {
      stun.gem = i;
    } else if (spell_id == charm.spell_id) {
      charm.gem = i;
    }
  }
}

void AutoChanter::handle_print(const char *message, int color_index) {
  if (!auto_chanter || !message) return;

  std::string charm_off = "Your charm spell has worn off.";

  // On charm break, target pet and prepare to stun
  if (std::string(message) == charm_off) {
    Zeal::Game::print_chat("AutoChanter: Charm has worn off, returning to Idle state.");
    Zeal::Game::set_target(my_pet);
    state = Stun;
  }
}

bool AutoChanter::handle_chat_channel(const char *message, int color_index) {
  if (!auto_chanter || !message) return false;

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

  state = Assist;

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (self->StandingState != Stance::Stand) {
    self->ChangeStance(Stance::Stand);
  }

  return false;
}

void AutoChanter::tick() {
  if (!auto_chanter) return;

  auto now = GetTickCount64();
  // Add a 500 ms delay, no need to check every tick
  if (now - last_interval_time < kCheckIntervalMs) return;
  last_interval_time = now;

  // Check if we have a pet, if we don't keep old pet pointer just in case
  auto pet = Zeal::Game::get_pet();
  if (pet) {
    my_pet = pet;
  }

  if (state == Idle) return;

  switch (state) {
    case Assist:
      tick_assist(now);
      break;
    case Stun:
      tick_stun(now);
      break;
    case AboutToSit:
      break;
    case Idle:
    default:
      return;
  }
}

void AutoChanter::tick_assist(ULONGLONG now) {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (self->StandingState != Stance::Stand) {
    self->ChangeStance(Stance::Stand);
  }
}

void AutoChanter::tick_stun(ULONGLONG now) {
  // Try to calculate distance between player and pet
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  auto player_position = self->Position;
  auto pet_position = my_pet->Position;

  float distance = player_position.Dist2D(pet_position);
  Zeal::Game::print_chat("AutoChanter: Distance to pet is %.2f", distance);
  // TODO: If its far away try to tash before stunning

  if (distance <= STUN_RANGE + 20) {
    if (casting_spell_id == kInvalidSpellId) {
      cast_spell(stun);
      return;
    }

    if (Zeal::Game::GetSpellCastingTime() != -1) {
      return;
    }

    // Stun finished casting
    casting_spell_id = kInvalidSpellId;
    state = Idle;
  }
}

void AutoChanter::cast_spell(const Spell &spell) {
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

AutoChanter::AutoChanter(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EnterZone);

  // Hook chat channels for assist call
  zeal->chat_hook->add_incoming_chat_callback(
      [this](const char *msg, int color_index) { return handle_chat_channel(msg, color_index); });

  // Hook all printed chat for charm break detection
  zeal->chat_hook->add_print_chat_callback(
      [this](const char *data, int color_index) { handle_print(data, color_index); });

  zeal->callbacks->AddGeneric([this]() { tick(); });

  zeal->commands_hook->Add("/autochanter", {"/chanter"}, "Assist and manage pet",
    [this](std::vector<std::string> &args) {
        int args_size = args.size();

        if (args_size > 1) {
        if (Zeal::String::compare_insensitive(args[1], "off")) {
            Zeal::Game::print_chat("AutoChanter disabled.");
            disable();
            return true;
        }

        if (Zeal::String::compare_insensitive(args[1], "help")) {
            Zeal::Game::print_chat("Uso: /autochanter");
            return true;
        }
        }

        enable();
        return true;
    });
}

AutoChanter::~AutoChanter() {}