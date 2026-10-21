#pragma once

#include "game_structures.h"

#include <string>
#include <regex>

class ChatHelper {

 public:

  void join_channel();
  static std::string assist_listener(const std::string &message);
  static std::string enrage_listener(const std::string &message);

 private:
  inline static const std::string CHANNEL = "grupete";

  // Expected formats:
  //   "PlayerName tells Grupete:1, 'Assist me on TargetName'"
  //   "PlayerName tells the group, 'Assist me on TargetName'"
  //   "PlayerName tells the raid, 'Assist me on TargetName'"
  inline static const std::regex ASSIST_PATTTERN {R"((\S+) tells .*?Assist me on)", std::regex_constants::icase};

  // Expected formats:
  // "Mobname has become ENRAGED."
  inline static const std::regex ENRAGE_PATTTERN{R"((\S+) has become ENRAGED\.)", std::regex_constants::icase};

};