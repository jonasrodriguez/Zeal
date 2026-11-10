#include "chat_helper.h"

#include "game_functions.h"

void ChatHelper::join_channel() {
  if (Zeal::Game::get_channel_number(CHANNEL.c_str()) == -1) {
	Zeal::Game::do_join(Zeal::Game::get_self(), CHANNEL.c_str());
  }
}

std::string ChatHelper::assist_listener(const std::string &message) { 

  if (message.empty()) return std::string();

  std::cmatch match;
  if (!std::regex_search(message.c_str(), match, ASSIST_PATTTERN)) return std::string();
  
  return match[1].str();
}

std::string ChatHelper::enrage_listener(const std::string& message) {
  if (message.empty()) return std::string();

  std::cmatch match;
  if (!std::regex_search(message.c_str(), match, ENRAGE_PATTERN)) return std::string();

  Zeal::Game::print_chat("ChatHelper: Enraged message detected: %s", match[0].str().c_str());

  return match[0].str();
}