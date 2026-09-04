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
  if (casting_spell_id == kInvalidSpellId) {
    cast(spell);
    return false;
  }

  if (Zeal::Game::GetSpellCastingTime() != -1) {
    return false;
  }

  casting_spell_id = kInvalidSpellId;
  return true;
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