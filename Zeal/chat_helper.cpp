#include "chat_helper.h"

#include "game_functions.h"

void ChatHelper::join_channel() {
  if (Zeal::Game::get_channel_number(CHANNEL.c_str()) == -1) {
	Zeal::Game::do_join(Zeal::Game::get_self(), CHANNEL.c_str());
  }
}

std::string ChatHelper::assist_listener(const char *message, int color_index) { 

  if (!message) return std::string();

  std::cmatch match;
  if (!std::regex_search(message, match, ASSIST_PATTTERN)) return std::string();
  
  return match[1].str();
}
