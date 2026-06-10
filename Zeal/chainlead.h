#pragma once

#include <string>
#include <vector>

#include "io_ini.h"

class ChainLead {
 public:
  ChainLead(class ZealService *zeal);
  ~ChainLead();

  void set_chain(std::string target, int pause, bool yaulp);

 private:
  inline static const std::string CH_CHANNEL = "viejeals";

  struct ChainMember {
    std::string name;
    int mana;
  };

  void tick();
  void handle_print_chat(const char *message, int color_index);  // Scans chat text for CH orders.
  void send_chat(const std::string &message);

  std::vector<ChainMember> chain;
  bool chain_lead = false;
  bool yaulp = false;
  std::string chain_target;
  int pause_duration = 0;
  int chain_index = 0;
  int idle_time = 0;
  ULONGLONG last_call_time = 0;

  // File system for loading chain members
  void initialize_ini_filename();
  std::vector<std::string> load();
  IO_ini ini = IO_ini(".\\chain_lead.ini");  // Filename updated later to per character.
};
