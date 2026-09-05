#include "autoranger.h"

#include <format>
#include <regex>

#include "callbacks.h"
#include "chat.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "zeal.h"

#include "autofire.h"

void ForwardCommand(std::string cmd);

void AutoRanger::enable(bool castSnare, bool castBuff) {
  cast_snare = castSnare;
  auto_ranger = true; 
  state = Idle;

  spell_helper.search_spells(spellset);
  if (spell_helper.missing_spell(spellset)) {
    Zeal::Game::print_chat("AutoRanger: Missing \"Esnare\", \"Eagle Eye\" or \"Nature's Precision\".");

    if (castSnare && snare.gem == -1) {
      disable();
      return;
    }
    if (castBuff && (eagle.gem == -1 || precision.gem == -1)) {
      disable();
      return;
    }
  }

  Zeal::Game::print_chat(std::format("AutoRanger habilitado {} {}", castSnare ? "con snare" : "", castBuff ? "con buffs" : ""));
}

void AutoRanger::disable() {
  state = Idle;
  auto_ranger = false;
  spell_helper.reset_spells(spellset);
}

void AutoRanger::handle_chat(const char *message, int color_index) {
  if (!auto_ranger || !message) return;

  auto target = chat_helper.assist_listener(message, color_index);
  if (!target.empty()) {
    Zeal::Game::print_chat(std::format("AutoRanger: Assisting on {}", target));
    Zeal::Game::set_target(nullptr);
    ForwardCommand(std::format("/assist {}", target));
    last_interval_time = GetTickCount64();
    state = Face;
  }
}

void AutoRanger::tick() {
  if (!auto_ranger) return;

  auto now = GetTickCount64();
  if (now - last_interval_time < kCheckIntervalMs) return;
  last_interval_time = now;

  // Check buffs fading if idle
  if (state == Idle) {
    check_buffs();
  }

  switch (state) {
    case Face:
      tick_face();
      break;
    case Snare:
      tick_snare();
      break;
    case Fire:
      tick_auto_fire();
      break;
    case Buff:
      tick_buff();
      break;
    case Idle:
    default:
      return;
  }
}

void AutoRanger::tick_face() { 
  if (!Zeal::Game::get_target()) {
    return;
  }
  auto_face.face_target();
  if (cast_snare) {
    state = Snare;    
  } else {
    state = Fire;
  }
}

void AutoRanger::tick_snare() {
  if (spell_helper.cast_spell(snare)) {
    Zeal::Game::print_chat("AutoRanger: Esnared!");
    state = Fire;
  }
}

void AutoRanger::tick_auto_fire() {
  ZealService::get_instance()->autofire->SetAutoFire(true, true);
  state = Idle;
}

void AutoRanger::tick_buff() {
  if (spell_helper.cast_spell(buff)) {
    state = Idle;
  }
}

void AutoRanger::check_buffs() {
  auto spell = SpellHelper::get_fading_buff({eagle, precision});
  if (spell.spell_id != -1) {
    Zeal::Game::print_chat("AutoRanger: Rebuffing...");
    Zeal::Game::set_target(Zeal::Game::get_self());
    buff = spell;
    state = Buff;
  }
}

AutoRanger::AutoRanger(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { disable(); }, callback_type::EnterZone);

  // Hook all printed chat lines to match for combat data
  zeal->chat_hook->add_print_chat_callback(
      [this](const char *msg, int color_index) { return handle_chat(msg, color_index); });

  // Poll on each main loop iteration (same pattern as autoability).
  zeal->callbacks->AddGeneric([this]() { tick(); });

  zeal->commands_hook->Add("/autoranger", {"/ranger"}, "Assist, autofire and snare",
    [this](std::vector<std::string> &args) {
        int args_size = args.size();
        bool castSnare = false;
        bool castBuff = false;

        if (args_size > 1) {
            if (Zeal::String::compare_insensitive(args[1], "off")) {
                Zeal::Game::print_chat("AutoRanger disabled.");
                disable();
                return true;
            }

            for (int i = 1; i < args_size; ++i) {
                if (Zeal::String::compare_insensitive(args[i], "snare")) {
                    castSnare = true;
                }
                if (Zeal::String::compare_insensitive(args[i], "buffs")) {
                  castBuff = true;
                }
            }
        }

        enable(castSnare, castBuff);
        return true;
    });
}

AutoRanger::~AutoRanger() {}
