#include "AutoEnchanter.h"

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

void AutoEnchanter::enable() {
  spell_helper.search_spells(spellset);
  if (spell_helper.missing_spell(spellset)) {
    Zeal::Game::print_chat("AutoEnchanter: Missing \"Boltran\" o \"Color Slant\".");
    auto_enchanter = false;
    return;
  }

  my_pet = Zeal::Game::get_pet();

  auto_enchanter = true;
  Zeal::Game::print_chat("AutoEnchanter habilitado.");
}

void AutoEnchanter::disable() {
  auto_enchanter = false;
  my_pet = nullptr;
  spell_helper.reset_spells(spellset);
}

bool AutoEnchanter::handle_chat_channel(const char *message, int color_index) {
  if (!auto_enchanter || !message) return false;

  std::string target = chat_helper.assist_listener(message, color_index);
  if (!target.empty()) {
    Zeal::Game::print_chat(std::format("AutoEnchanter: Assisting on {}", target));
    Zeal::Game::set_target(nullptr);
    ForwardCommand(std::format("/assist {}", target));
    last_interval_time = GetTickCount64();
    state = Assist;
  }

  return false;
}

void AutoEnchanter::tick() {
  if (!auto_enchanter) return;

  auto now = GetTickCount64();
  if (now - last_interval_time < kCheckIntervalMs) return;
  last_interval_time = now;

  switch (state) {
    case Idle:
      tick_idle();
      break;
    case Assist:
      tick_assist();
      break;
    case Stun:
      tick_stun();
      break;
    case Charm:
      tick_charm();
      break;
    case Break:
      tick_break();
      break;
    default:
      return;
  }
}

void AutoEnchanter::tick_idle() {
  
  // Check charm break !
  if (my_pet && !Zeal::Game::get_pet()) {
    state = Break;
    return;
  }

  // Check if we have a pet
  my_pet = Zeal::Game::get_pet();
}

void AutoEnchanter::tick_assist() {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (self->StandingState != Stance::Stand) {
    self->ChangeStance(Stance::Stand);
  }

  Zeal::GameStructures::Entity *target = Zeal::Game::get_target();
  if (!target) {
    return;
  }
  Zeal::Game::pet_command(Zeal::GameEnums::PetCommand::Attack, target->SpawnId);
}

void AutoEnchanter::tick_break() {
  if (!my_pet) return;
  Vec3 pet_position = my_pet->Position;
  Zeal::Game::set_target(my_pet);

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (!self) return;
  Vec3 player_position = self->Position;

  float distance = player_position.Dist2D(pet_position);
  // If pet is close, stun it, otherwise try to recharm
  if (distance <= STUN_RANGE) {
    Zeal::Game::print_chat("AutoEnchanter: Stun pet %s", my_pet->Name);
    state = Stun;
  } else {
    Zeal::Game::print_chat("AutoEnchanter: Pet %s is too far away, try to recharm", my_pet->Name);
    state = Charm;
  }
}

void AutoEnchanter::tick_stun() {

  if (spell_helper.cast_spell(stun)) {
    Zeal::Game::print_chat("AutoRanger: Stunned!");
    state = Charm;
  }
}

void AutoEnchanter::tick_charm() {
  if (spell_helper.cast_spell(charm)) {
    Zeal::Game::print_chat("AutoRanger: Charmed!");
    state = Idle;
  }
}

AutoEnchanter::AutoEnchanter(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EnterZone);

  // Hook chat channels for assist call
  zeal->chat_hook->add_incoming_chat_callback(
      [this](const char *msg, int color_index) { return handle_chat_channel(msg, color_index); });

  zeal->callbacks->AddGeneric([this]() { tick(); });

  zeal->commands_hook->Add("/autoenchanter", {"/enchanter"}, "Assist and manage pet",
    [this](std::vector<std::string> &args) {
        int args_size = args.size();

        if (args_size > 1) {
            if (Zeal::String::compare_insensitive(args[1], "off")) {
                Zeal::Game::print_chat("AutoEnchanter disabled.");
                disable();
                return true;
            }
        }

        enable();
        return true;
    });
}
