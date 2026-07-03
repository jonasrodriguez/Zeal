#include "automage.h"

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

#include "position_helper.h"

void ForwardCommand(std::string cmd);

void AutoMage::enable(bool castNuke, bool castMalo, bool castDS) {
  if (auto_mage) {
	Zeal::Game::print_chat("AutoMage ya estaba habilitado.");
	return;
  }
  auto_mage = true;
  Zeal::Game::print_chat("AutoMage habilitado.");

  search_spells(castNuke, castMalo, castDS);
  if (castNuke && nuke.gem == -1) {
    Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoMage warning: Ancient: Shock of Sun not found in spell gems. Will not cast nuke.");
  }
  if (castMalo && malo.gem == -1) {
    Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoMage warning: Malosini not found in spell gems. Will not cast malo.");
  }
  if (castDS && ds.gem == -1) {
    Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoMage warning: Cadeau of Flame not found in spell gems. Will not cast DS.");
  }
  if (burnout.gem == -1) {
    Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "AutoMage warning: Ancient: Burnout Blaze not found in spell gems. Will not cast burnout.");
  }

  // Join channel "grupete"
  if (Zeal::Game::get_channel_number(CHANNEL.c_str()) == -1) {
    Zeal::Game::do_join(Zeal::Game::get_self(), CHANNEL.c_str());
  }

  // Add some delays for the buffs so they don't overlap
  auto now = GetTickCount64();
  last_burnout_time = now - static_cast<ULONGLONG>(burnout.duration) * 1000 + 15000;
  last_ds_time = now - static_cast<ULONGLONG>(ds.duration) * 1000 + 10000;
}

void AutoMage::disable() { 
  nuke.gem = -1;
  malo.gem = -1;
  ds.gem = -1;
  burnout.gem = -1;
  auto_mage = false; 
}

void AutoMage::search_spells(bool castNuke, bool castMalo, bool castDS) {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  Zeal::GameStructures::SPELLMGR *get_spell_mgr();
  auto *spell_mgr = Zeal::Game::get_spell_mgr();
  for (int i = 0; i < GAME_NUM_SPELL_GEMS; ++i) {
    int spell_id = char_info->MemorizedSpell[i];

    if (castNuke && spell_id == ds.spell_id) {
      ds.gem = i;
    } else if (castNuke && spell_id == nuke.spell_id) {
      nuke.gem = i;
    } else if (castMalo && spell_id == malo.spell_id) {
      malo.gem = i;
    } else if (spell_id == burnout.spell_id) {
      burnout.gem = i;
    }
  }
}

bool AutoMage::handle_chat(const char *message, int color_index) {
  if (!auto_mage || !message) return false;

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

  Zeal::Game::send_to_channel(tag_channel_number, std::format("AutoMage: Pet attack on on {}", target).c_str());

  ForwardCommand(std::format("/assist {}", player));

  state = Assist;

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (self->StandingState != Stance::Stand) {
    self->ChangeStance(Stance::Stand);
  }

  return false;
}

//Zeal::GameStructures::Entity *my_pet = Zeal::GameFunctions::get_pet();
//Zeal::GameStructures::Entity *pet = Zeal::GameFunctions::get_entity_by_parent_id(owner_spawn_id);

void AutoMage::tick() {
  if (!auto_mage) return;

  auto now = GetTickCount64();
  // Add a 500 ms delay, no need to check every tick
  if (now - last_interval_time < kCheckIntervalMs) return;
  last_interval_time = now;

  // Buffs 
  if (now - last_burnout_time > burnout.duration * 1000) {
    buff_pet(now);    
  }

  if (state == Idle) return;

  switch (state) {
    case Assist:
      tick_assist(now);
      break;
    case Malo: 
      tick_malo(now);
      break;
    case Nuke: 
      tick_nuke(now);
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

void AutoMage::tick_assist(ULONGLONG now) {
  Zeal::Game::do_autoattack(false);

  Zeal::GameStructures::Entity *target = Zeal::Game::get_target();
  if (!target) {
    state = Idle;
    return;
  }

  Zeal::Game::pet_command(Zeal::GameEnums::PetCommand::Attack, target->SpawnId);

  // Face the target
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  auto heading = PositionHelper::get_heading(self, target);
  self->Heading = heading;
  Zeal::Game::print_chat("AutoMage: Facing target %s at heading %.2f", target->Name, heading);

  if (malo.gem != -1) {
    state = Malo;
  } else if (nuke.gem != -1) {
    state = Nuke;
  } else {
    state = AboutToSit;
  }
  last_action_time = now;
  casting_spell_id = kInvalidSpellId;

  auto channel_number = Zeal::Game::get_channel_number(CHANNEL.c_str());
  Zeal::Game::send_to_channel(channel_number, std::format("AutoMage: Pet attack on on {}", target->Name).c_str());
}

void AutoMage::tick_malo(ULONGLONG now) {

  // Add delay to avoid agroing (2seconds)
  if ((now - last_action_time) <= 2000) return;
  if (casting_spell_id == kInvalidSpellId) {
    cast_spell(malo);
    return;
  }

  if (Zeal::Game::GetSpellCastingTime() != -1) {
    return;
  }

  // Malod finished casting
  casting_spell_id = kInvalidSpellId;
  last_action_time = now;
  if (nuke.gem != -1) {
    state = Nuke;
  } else {
    state = AboutToSit;
  }
}

void AutoMage::tick_nuke(ULONGLONG now) {
  // Add delay to avoid agroing (2seconds)
  if ((now - last_action_time) <= 2000) return;
  if (casting_spell_id == kInvalidSpellId) {
    cast_spell(nuke);
    return;
  }
  if (Zeal::Game::GetSpellCastingTime() != -1) {
    return;
  }
  // Nuke finished casting
  casting_spell_id = kInvalidSpellId;
  last_action_time = now;
  state = AboutToSit;
}

void AutoMage::buff_pet(ULONGLONG now) {
  if (burnout.gem == -1) return;

  if (casting_spell_id == kInvalidSpellId) {
    cast_spell(burnout);
    return;
  }
  if (Zeal::Game::GetSpellCastingTime() != -1) {
    return;
  }

  casting_spell_id = kInvalidSpellId;
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  self->ChangeStance(Stance::Sit);
  last_burnout_time = now;
}

void AutoMage::cast_spell(const Spell &spell) {
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

AutoMage::AutoMage(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EnterZone);

  // Hook all printed chat lines to match for combat data
  zeal->chat_hook->add_incoming_chat_callback(
      [this](const char *msg, int color_index) { return handle_chat(msg, color_index); });

  // Poll on each main loop iteration (same pattern as autoability).
  zeal->callbacks->AddGeneric([this]() { tick(); });


  zeal->commands_hook->Add("/automage", {"/mage"}, "Assist, send pet nukes and debuffs",
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

          for (int i = 1; i < args_size; ++i) {
            if (Zeal::String::compare_insensitive(args[i], "malo")) {
              castMalo = true;
            } else if (Zeal::String::compare_insensitive(args[i], "nuke")) {
              castNuke = true;
            } else if (Zeal::String::compare_insensitive(args[i], "ds")) {
              castDS = true;
            }
          }
        }

        enable(castNuke, castMalo, castDS);
        return true;
    });
}

AutoMage::~AutoMage() {}
