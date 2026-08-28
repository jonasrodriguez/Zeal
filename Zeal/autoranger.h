#pragma once

#include <Windows.h>
#include <string>

#include "chat_helper.h"
#include "auto_face.h"

#include "game_structures.h"
#include "spell.h"

class AutoRanger {
 public:
  AutoRanger(class ZealService *zeal);
  ~AutoRanger();

  void enable(bool castSnare);
  void disable();

 private:
  ChatHelper chat_helper;
  AutoFace auto_face;

  enum RangerState { Idle, Assist, Fire, Snare };

  void tick();
  void tick_face();
  void tick_assist();

  void handle_chat(const char *message, int color_index);

  bool auto_ranger = false;
  RangerState state = Idle;

  Spell snare{-1, 512};

  WORD casting_spell_id = kInvalidSpellId;

  ULONGLONG last_interval_time;
  static constexpr DWORD kCheckIntervalMs = 500;
};
