#include "spell_helper.h"

#include "game_functions.h"
#include <unordered_map>

void SpellHelper::search_spells(SpellSet &spellset) {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  std::unordered_map<int, Spell *> spell_by_id;
  spell_by_id.reserve(spellset.size());
  for (auto &spell : spellset) {
    spell_by_id[spell->spell_id] = spell;
  }

  for (int i = 0; i < GAME_NUM_SPELL_GEMS; ++i) {
    int spell_id = char_info->MemorizedSpell[i];
    auto it = spell_by_id.find(spell_id);
    if (it != spell_by_id.end()) {
      it->second->gem = i;
    }
  }
}

bool SpellHelper::cast_spell(const Spell &spell) {

  switch (state) {
    case Idle:
      return about_to_cast(spell);
    case CheckCasting:
      return check_casting(spell);
    case Casting:
      return casting();
  }
}

bool SpellHelper::about_to_cast(const Spell &spell) {
  Zeal::Game::print_chat("About to cast !");
  if (spell.gem == -1) {
    Zeal::Game::print_chat("Spell not found in spell gems.");
    return true;
  }

  cast(spell);
  casting_started_timestamp = GetTickCount64();
  retry_count++;
  return false;
}

bool SpellHelper::check_casting(const Spell &spell) {
  if (Zeal::Game::GetSpellCastingTime() != -1) {
    casting_visible_timestamp = GetTickCount64();
    if ((casting_visible_timestamp - casting_started_timestamp) > 500) {
      Zeal::Game::print_chat("Check casting -> All good, reset retries!");

      retry_count = 0;
      state = Casting;
    }
  } else {
    Zeal::Game::print_chat("Check casting -> Error casting, retry !");
    retry_count++;
    state = Idle;
    if (retry_count > max_retries) {
      Zeal::Game::print_chat("Cancelando casteo, demasiadas retries (Estas demasiado lejos?)");
      casting_spell_id = kInvalidSpellId;
      retry_count = 0;
      state = Idle;
      return true;
    }
  }

  return false;
}

bool SpellHelper::casting() {
  if (Zeal::Game::GetSpellCastingTime() != -1) {
    Zeal::Game::print_chat("Casting still...");
    return false; // Nothing to do, still casting
  } else {         
    // Finish casting, clear state and reset retry count
    Zeal::Game::print_chat("Finished casting !");
    casting_spell_id = kInvalidSpellId;
    retry_count = 0;
    state = Idle;

    return true;
  }
}


Spell SpellHelper::get_missing_or_fading_buff(const std::vector<Spell> &buffs) {

  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  std::unordered_map<int, Zeal::GameStructures::_GAMEBUFFINFO *> active_buffs;
  for (int i = 0; i < char_info->GetMaxBuffs(); i++) {
    auto *buff_info = char_info->GetBuff(i);
    if (buff_info) {
      active_buffs[buff_info->SpellId] = buff_info;
    }
  }

  // Check if buff is fading
  for (const auto &buff : buffs) {
    auto it = active_buffs.find(buff.spell_id);
    if (it != active_buffs.end()) {
      auto duration = it->second->Ticks * 6;  // Convert ticks to seconds
      if (duration <= kFadingBuffThreshold) {
        return buff;
      }
    } else {
      // Buff is missing, recast
      return buff;
    }
  }

  return Spell{-1, -1, -1};
}

void SpellHelper::cast(const Spell &spell) {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  auto fizzle = self->ActorInfo->FizzleTimeout;
  auto gameTime = Zeal::Game::get_display()->GameTimeMs;
  // Check if gem is ready before trying to cast
  if (fizzle > gameTime) {
    return;
  }
  if (self->ActorInfo->RecastTimeout[spell.gem] > gameTime) {
    return;
  }

  if (char_info->cast(spell.gem, char_info->MemorizedSpell[spell.gem], 0, -1)) {
    casting_spell_id = char_info->MemorizedSpell[spell.gem];
  }
}

void SpellHelper::reset_spells(SpellSet &spells) {

  for (auto &spell : spells) {
    spell->gem = -1;
  }
}

bool SpellHelper::missing_spell(const SpellSet &spells) {
  for (auto &spell : spells) {
    if (spell->gem == -1) {
      return true;
    }
  }
  return false;
}