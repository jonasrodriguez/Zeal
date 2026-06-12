#include "chainlead.h"

#include <regex>
#include <string>
#include <format>

#include "game_functions.h"
#include "zeal.h"
#include "callbacks.h"
#include "chat.h"
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

  // Calculate idle team for each member, CH duration is always 10 seconds
  int chain_size = chain.size();
  idle_time = pause_duration * chain_size - (100 * 100);

  auto msg = std::format("ChainLead: Chain con {} miembros y {} segundos de pausa deja un margen de {} segundos", 
      chain_size, pause * 0.1, idle_time * 0.001);
  Zeal::Game::print_chat(msg);
  if (idle_time < 1000) {
    Zeal::Game::print_chat("ChainLead: La chain esta demasiado ajustada, considera aumentar la pausa.");
  }

  const char kMarker = 0x12;
  const int kWhoop = 11175;
  auto roll_msg = std::format("Roll chain !! Target: {0:c}0{1:06d}{2}{3:c} Pausa: {4:c}0{5:06d}{6}{7:c}.", 
      kMarker, kWhoop, target, kMarker, kMarker, kWhoop, pause, kMarker);
  send_chat(roll_msg);

  chain_lead = true;
}

void ChainLead::handle_print_chat(const char *message, int color_index) {
  if (!chain_lead || !message) return;

  // Expected format: "Playername says out of character, 'Casteando CH a Target - PlayerMana %'"
  static const std::regex ch_pattern(R"(^(\w+) says out of character, 'Casteando CH a .+ - (\d+) %'$)");
  std::cmatch match;
  if (!std::regex_search(message, match, ch_pattern)) return;

  std::string player_name = match[1].str();
  //std::string player_mana = std::stoi(match[2].str());
}

void ChainLead::send_chat(const std::string &message) {
  Zeal::Game::send_raid_chat(message.c_str());
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

  const char kMarker = 0x12;
  const int kWhoop = 11175;
  std::string call = std::format("GO {} - CH a {} {}",  member.name, chain_target, yaulp ? "yaulp" : "");
  call += std::format(" - GO {0:c}0{1:06d}{2}{3:c} !", kMarker, kWhoop, member.name, kMarker);

  send_chat(call);
  chain_index = (chain_index + 1) % chain.size();
}

// Initializes the character dependent filename useed
void ChainLead::initialize_ini_filename() {
  std::string filename = "chain_lead.ini";
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

  zeal->chat_hook->add_print_chat_callback([this](const char *data, int color_index) { handle_print_chat(data, color_index); });
  zeal->commands_hook->Add("/chainlead", {"/cl"}, "Manages chain calls. Usage: /chainlead (target | off) pause(int) (yaulp | )",
        [this](std::vector<std::string> &args) { 
            if (args.size() > 1) {

            bool yaulp = false;
            // Get args[3] as boolean for yaulp (if present)
            if (args.size() == 4) {
                if (Zeal::String::compare_insensitive(args[3], "yaulp")) {
                yaulp = true;
                }
            }

            std::string target;
            if (Zeal::String::compare_insensitive(args[1], "off")) {
                chain_lead = false;
                return true;
            } else {
                target = args[1];
            }

            int pause = 0;
            try {
                pause = std::stoi(args[2]);
            } catch (...) {
                Zeal::Game::print_chat("Invalid pause duration. Must be an integer.");
                return false;
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