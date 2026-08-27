#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

#include "game_structures.h"
#include "game_ui.h"
#include "io_ini.h"
#include "zeal_settings.h"

// AutoCombine automates repeated tradeskill combines. It reads recipes from an ini file, locates the
// required items in the player's inventory, moves them into an open combine container, triggers the
// combine, auto-inventories (or deletes) the results, and repeats until the ingredients run out.
class AutoCombine {

 public:
  AutoCombine(class ZealService *zeal);
  ~AutoCombine();

 private:
  // Global slot id conventions (see game_structures.h): 0 = cursor, 22-29 = pack slots.
  static constexpr int kCursorSlotId = 0;

  // Server wire opcode for a tradeskill combine is 0x0541. The client SendMessage layer expects the
  // byte-swapped internal opcode (e.g. wire 0x9240 WearChange == internal 0x4092), so combine is 0x4105.
  static constexpr int kOpTradeSkillCombine = 0x4105;

  // container_slot value the server expects for an in-inventory (non-world) combine container.
  // The server derives everything else from the actual container contents, so only the container
  // location and world object type matter in the packet.
  static constexpr int kMaxComponents = 10;    // Container slot capacity limit.
  static constexpr int kFirstCombineType = 9;  // Container.Combine values >= 9 support combines.

  // How the combine action is delivered to the server.
  enum class TriggerMethod {
    UiButton = 0,  // Simulate a click on the container's Combine button (client builds the packet).
    Packet = 1,    // Build and send OP_TradeSkillCombine directly.
  };

  // A single recipe loaded from the ini file.
  struct Recipe {
    std::string name;                     // Section name in the ini file.
    std::vector<int> component_item_ids;  // Item ids that must be placed into the container each cycle.
    bool delete_result = false;           // If true, destroy the result each cycle instead of keeping it.
    std::vector<int> keep_item_ids;       // Item ids to preserve (never consume) - e.g. reusable molds/tools.
  };

  // State machine phases for a single combine cycle.
  enum class State {
    Idle,         // Not running.
    LoadItems,    // Move the next batch of components into the container.
    Combine,      // Trigger the combine and wait for the server ack.
    WaitAck,      // Waiting for OP_TradeSkillCombine ack (results appear on cursor).
    ClearCursor,  // Auto-inventory or delete whatever is on the cursor.
    NextCycle,    // Decide whether to continue or stop.
  };

  void tick();
  void stop(const char *reason = nullptr);
  bool handle_packet(UINT opcode);  // Detects the combine ack.

  // Recipe file handling.
  void initialize_ini_filename();
  bool load_recipe(const std::string &name);
  std::vector<std::string> list_recipes();

  // Combine helpers.
  Zeal::GameUI::ContainerWnd *get_active_combine_container();
  int count_available_sets();  // How many full combines remain given current inventory.
  bool load_next_batch();      // Moves one full set of components into the container. False if unable.
  bool trigger_combine();      // Fires the combine using the configured method.
  void trigger_combine_button(Zeal::GameUI::ContainerWnd *wnd);
  void trigger_combine_packet(Zeal::GameUI::ContainerWnd *wnd);
  void handle_result_on_cursor();  // Auto-inventory or delete the cursor item.
  bool is_keep_item(const Zeal::GameStructures::GAMEITEMINFO *item) const;

  State state = State::Idle;
  Recipe active_recipe;
  int completed_count = 0;         // Number of successful cycles this run.
  int requested_count = 0;         // 0 = run until out of ingredients.
  ULONGLONG next_action_time = 0;  // Gate for step pacing (GetTickCount64 based).
  ULONGLONG ack_deadline = 0;      // Timeout for a combine ack (GetTickCount64 based).
  bool waiting_for_ack = false;    // Set when a combine was fired, cleared by the ack packet.

  static constexpr int kAckTimeoutMs = 5000;  // Give up waiting for a combine ack after this long.

  // Delay between discrete UI/packet actions to let the client and server stay in sync.
  ZealSetting<int> setting_step_delay_ms = {250, "AutoCombine", "StepDelayMs", false};
  // Trigger method: 0 = UI button click, 1 = direct packet.
  ZealSetting<int> setting_trigger_method = {static_cast<int>(TriggerMethod::UiButton), "AutoCombine", "TriggerMethod",
                                             false};

  IO_ini ini = IO_ini(".\\autocombine.ini");  // Filename updated to per-character on character select.
};