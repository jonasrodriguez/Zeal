#include "chainlead.h"

#include "game_functions.h"
#include "zeal.h"
#include "callbacks.h"
#include "commands.h"
#include "string_util.h"

void ChainLead::set_chain(std::string target, int pause, bool yaulp_arg) {
  chain_target = target;
  pause_duration = pause * 100;  // Pause macro is in deciseconds;
  yaulp = yaulp_arg;

  chain.clear();
  auto chain_names = load();
  for (const auto &name : chain_names) {
    chain.push_back({name, -1});
  }
  if (chain.empty()) {
    Zeal::Game::print_chat("No chain members found in ini file.");
    return;
  }
  
  chain_lead = true;
}

void ChainLead::tick() {
  if (!chain_lead) return;

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  if (!Zeal::Game::is_in_game() || !self || !char_info) {
    chain_lead = false;
    return;
  }

  ULONGLONG now = GetTickCount64();

  if (now - last_call_time < pause_duration) return;

  last_call_time = now;
  auto &member = chain[chain_index];
  std::string call = std::format("GO {} - CH a {}{}", member.name, chain_target, yaulp ? " Y" : "");
  Zeal::Game::do_ooc(call);
  chain_index = (chain_index + 1) % chain.size();
}


// Initializes the character dependent filename useed
void ChainLead::initialize_ini_filename() {
  const char *name = Zeal::Game::get_char_info() ? Zeal::Game::get_char_info()->Name : "unknown";
  std::string filename = std::string(name) + "_chain.ini";
  std::filesystem::path file_path = Zeal::Game::get_game_path() / std::filesystem::path(filename);
  ini.set(file_path.string());
}

std::vector<std::string> ChainLead::load() {
  initialize_ini_filename();
  if (!ini.exists("chain", "0")) {
    return {};
  }
  std::vector<std::string> chain;
  for (int i = 0;; ++i) {
    std::string member = ini.getValue<std::string>("chain", std::to_string(i));
    if (member.empty()) break;
    chain.push_back(member);
  }
  return chain;
}

ChainLead::ChainLead(ZealService *zeal) {
  // Disable on zone transitions and character select.
  zeal->callbacks->AddGeneric([this]() { chain_lead = false; }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { chain_lead = false; }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { chain_lead = false; }, callback_type::EnterZone);
  zeal->callbacks->AddGeneric([this]() { tick(); });
  zeal->commands_hook->Add("/chainlead", {"/cl"}, "Manages chain calls. Usage: /chainlead (target | off) pause(int) yaulp(yes|no)",
                           [this](std::vector<std::string> &args) { 
                             if (args.size() > 1) {

                               std::string target;
                               if (Zeal::String::compare_insensitive(args[1], "off")) {
                                 chain_lead = false;
                                 return true;
                               } else {
                                 target = args[1];
                               }

                               int pause = 0;
                               bool yaulp = false;

                               try {
                                 pause = std::stoi(args[2]);
                               } catch (...) {
                                 Zeal::Game::print_chat("Invalid pause duration. Must be an integer.");
                                 return false;
                               }

                               // Get args[3] as boolean for yaulp (if present)
                               if (args.size() == 4) {
                                 if (Zeal::String::compare_insensitive(args[3], "yes")) {
                                   yaulp = true;
                                 } else if (Zeal::String::compare_insensitive(args[3], "no")) {
                                   yaulp = false;
                                 } else {
                                   Zeal::Game::print_chat("Invalid yaulp value. Use yes or no.");
                                   return false;
                                 }
                               }

                               set_chain(target, pause, yaulp);
                               return true;

                             } else {
                               Zeal::Game::print_chat("Invalid usage of /chainlead command. Usage: /chainlead target (target | off) pause(int) yaulp(yes|no)");
                               return false;
                             }
                           });
}

ChainLead::~ChainLead() {};