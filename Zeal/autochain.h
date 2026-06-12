#pragma once

#include <string>

#include "zeal.h"
#include "game_structures.h"

class AutoChain {
 public:
  AutoChain(class ZealService *zeal);
  ~AutoChain();

  void enable();
  void disable();

 private:
  static constexpr int COMPLETE_HEALING_ID = 13;
  static constexpr int YAULP_V_ID = 2326;
  static constexpr int CH_MANA = 400;
  inline static const std::string CH_CHANNEL = "viejeals";

  enum CHState { Idle, WaitingChannel, AboutToCast, CheckCasting, Casting, AboutToLand, FinishCasting, Yaulp, RetryCasting };

  void handle_print_chat(const char *message, int color_index);  // Scans chat text for CH orders.
  void tick();

  void tick_about_to_cast();
  void tick_check_casting();
  void tick_casting();
  void tick_about_to_land();
  void tick_finish_casting();
  void tick_cast_yaulp();
  void tick_retry_casting();

  void send_chat(const std::string &message);

  void search_spells();              // Searches for CH and Yaulp V in the spell gems and updates gem indices.
  bool is_gem_ready(int gem_index);  // Check if spell it's ready for castting

  bool autochain = false;
  std::string ch_target;
  bool yaulp = false;
  int ch_gem = -1;
  int yaulp_gem = -1;
  CHState state = Idle;

  int channel_num = -1;						// Channel number for "viejeals" if it exists, otherwise -1.

  int retry_count = 0;                      // Tracks unsuccessful song casts.
  int max_retries = 2;						// Max retries before giving asking for skip.
  WORD casting_spell_id = kInvalidSpellId;  // Current spell being cast. Is only a valid id while casting.
  ULONGLONG current_timestamp = 0;			// Now
  ULONGLONG start_ch_cast_timestamp = 0;    // Timestamp of when the cast started
  ULONGLONG end_ch_cast_timestamp = 0;		// Timestamp of when the cast ended
  ULONGLONG casting_visible_timestamp = 0;  // Timestamp of when the casting bar became visible
};