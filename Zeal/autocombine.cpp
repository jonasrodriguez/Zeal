#include "autocombine.h"

#include <algorithm>
#include <climits>
#include <map>

#include "callbacks.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_packets.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "zeal.h"

// Combine packet payload. Mirrors the server's Combine_Struct (common/eq_packet_structs.h). The server
// re-derives the recipe from the actual container contents, so only worldobjecttype and container_slot
// are strictly required; the remaining fields are populated for completeness / native parity.
#pragma pack(push, 1)
struct CombinePacket {
  uint8_t worldobjecttype;  // Non-zero for world objects (forge, etc); 0 for an inventory container.
  uint8_t unknown001;
  int16_t skill;
  int16_t container_slot;  // Pack slot of the container, or 1000 (SLOT_TRADESKILL) for a world container.
  int16_t iteminslot[10];  // Item ids currently in the container slots.
  int16_t product;
  int16_t difficulty;
  int16_t containerID;  // Item id of the container itself.
};

#pragma pack(pop)

static constexpr int kWorldContainerSlot = 1000;  // EQ::legacy::SLOT_TRADESKILL.

// Finds the pack slot index (0-7) that owns the given container info, or -1 if not a pack container
// (e.g. a world container like a forge).
static int find_container_pack_index(const Zeal::GameStructures::GAMEITEMINFO *container_info) {
  auto char_info = Zeal::Game::get_char_info();
  if (!char_info || !container_info) return -1;
  for (int i = 0; i < GAME_NUM_INVENTORY_PACK_SLOTS; ++i) {
    if (char_info->InventoryPackItem[i] == container_info) return i;
  }
  return -1;
}

bool AutoCombine::is_keep_item(const Zeal::GameStructures::GAMEITEMINFO *item) const {
  if (!item) return false;
  return std::find(active_recipe.keep_item_ids.begin(), active_recipe.keep_item_ids.end(), item->ID) !=
         active_recipe.keep_item_ids.end();
}

// Scans open container windows for a combine-capable one (world container takes priority). Mirrors the
// container detection logic in npc_give.cpp.
Zeal::GameUI::ContainerWnd *AutoCombine::get_active_combine_container() {
  auto container_mgr = Zeal::Game::Windows ? Zeal::Game::Windows->ContainerMgr : nullptr;
  if (!container_mgr) return nullptr;

  // World container (forge, oven, etc) - only one can be open at a time.
  if (container_mgr->pWorldItems) {
    if (container_mgr->pWorldItems->Container.IsOpen &&
        container_mgr->pWorldItems->Container.Combine >= kFirstCombineType) {
      for (int i = 0; i < 0x11; ++i) {
        auto wnd = container_mgr->pPCContainers[i];
        if (wnd && wnd->pContainerInfo == container_mgr->pWorldItems) return wnd->IsVisible ? wnd : nullptr;
      }
    }
    return nullptr;
  }

  // Otherwise look for an open, visible, combine-capable inventory bag.
  for (int i = 0; i < 0x11; ++i) {
    auto wnd = container_mgr->pPCContainers[i];
    if (wnd && wnd->IsVisible && wnd->pContainerInfo && wnd->pContainerInfo->Container.IsOpen &&
        wnd->pContainerInfo->Container.Combine >= kFirstCombineType)
      return wnd;
  }
  return nullptr;
}

// Counts how many complete sets of the recipe's components are currently available in inventory.
int AutoCombine::count_available_sets() {
  if (active_recipe.component_item_ids.empty()) return 0;

  // Tally how many of each required item id exist across all inventory bags/slots.
  std::map<int, int> required;
  for (int id : active_recipe.component_item_ids) required[id]++;

  auto char_info = Zeal::Game::get_char_info();
  if (!char_info) return 0;

  std::map<int, int> available;
  for (int pack = 0; pack < GAME_NUM_INVENTORY_PACK_SLOTS; ++pack) {
    auto slot_info = char_info->InventoryPackItem[pack];
    if (!slot_info) continue;
    available[slot_info->ID]++;  // The pack slot item itself (non-bagged).
    if (slot_info->Type != 1) continue;
    for (int s = 0; s < slot_info->Container.Capacity; ++s) {
      auto item = slot_info->Container.Item[s];
      if (item) available[item->ID]++;
    }
  }

  int sets = INT_MAX;
  for (auto &[id, need] : required) {
    int have = available.count(id) ? available[id] : 0;
    sets = (std::min)(sets, have / need);
  }
  return (sets == INT_MAX) ? 0 : sets;
}

// Moves one full set of components from inventory onto the container slots. The container should be
// empty when this is called (start of a cycle). Returns false if any component could not be placed.
bool AutoCombine::load_next_batch() {
  auto wnd = get_active_combine_container();
  if (!wnd || !wnd->pContainerInfo) return false;

  auto char_info = Zeal::Game::get_char_info();
  if (!char_info || char_info->CursorItem) return false;  // Cursor must be empty to move items.

  const int num_slots = wnd->pContainerInfo->Container.Capacity;
  std::vector<int> occupied_container_slots;

  for (size_t c = 0; c < active_recipe.component_item_ids.size() && c < static_cast<size_t>(num_slots); ++c) {
    int item_id = active_recipe.component_item_ids[c];

    // Locate the component in inventory. find_item_in_inventory only searches pack slots and bags, so
    // items already sitting in the (world or bag) combine container are not re-picked.
    int from_slot = Zeal::Game::find_item_in_inventory(item_id, false);
    if (from_slot < 0) {
      stop("could not locate a required component in inventory");
      return false;
    }

    // Pick up the component onto the cursor.
    if (!Zeal::Game::move_item(from_slot, kCursorSlotId, 1, 0) || !char_info->CursorItem) {
      stop("failed to pick up a component");
      return false;
    }

    // The container must be able to hold an item of this size.
    if (wnd->pContainerInfo->Container.SizeCapacity < char_info->CursorItem->Size) {
      Zeal::Game::GameInternal::auto_inventory(char_info, &char_info->CursorItem, 0);
      stop("a component is too large for this container");
      return false;
    }

    // Find the first empty container slot to drop it into.
    int placed = -1;
    for (int i = 0; i < num_slots; ++i) {
      auto invslot = wnd->pSlotWnds[i];
      if (invslot && invslot->invSlot && !invslot->invSlot->Item &&
          std::find(occupied_container_slots.begin(), occupied_container_slots.end(), i) ==
              occupied_container_slots.end()) {
        Zeal::Game::move_item(kCursorSlotId, invslot->SlotID, 0, 1);
        placed = i;
        break;
      }
    }

    if (placed < 0 || char_info->CursorItem) {
      // Could not place; put the item back to avoid stranding it on the cursor.
      Zeal::Game::GameInternal::auto_inventory(char_info, &char_info->CursorItem, 0);
      stop("no free container slot for a component");
      return false;
    }
    occupied_container_slots.push_back(placed);
  }

  return !occupied_container_slots.empty();
}

// Method A: simulate a native left-click on the container's Combine button so the client builds and
// sends the combine packet itself. Safest path - mirrors normal play and honors client-side checks.
void AutoCombine::trigger_combine_button(Zeal::GameUI::ContainerWnd *wnd) {
  if (!wnd || !wnd->pCombine) return;
  wnd->pCombine->LeftClickDown(0, 0);
  wnd->pCombine->LeftClickUp(0, 0);
}

// Method B: build the Combine_Struct and send OP_TradeSkillCombine directly.
void AutoCombine::trigger_combine_packet(Zeal::GameUI::ContainerWnd *wnd) {
  if (!wnd || !wnd->pContainerInfo) return;

  CombinePacket pkt = {};
  auto container_info = wnd->pContainerInfo;

  int pack_index = find_container_pack_index(container_info);
  if (pack_index >= 0) {
    // Inventory container: worldobjecttype 0, container_slot is the pack slot id (22-29).
    pkt.worldobjecttype = 0;
    pkt.container_slot = static_cast<int16_t>(GAME_PACKS_SLOTS_START + pack_index);
    pkt.containerID = static_cast<int16_t>(container_info->ID);
  } else {
    // World container (forge, etc): server keys off SLOT_TRADESKILL and its own object type.
    pkt.worldobjecttype = container_info->Container.Combine;
    pkt.container_slot = kWorldContainerSlot;
    pkt.containerID = 0;
  }

  // Fill in the item ids currently sitting in the container slots.
  const int num_slots = std::min<int>(container_info->Container.Capacity, kMaxComponents);
  for (int i = 0; i < num_slots; ++i) {
    auto item = container_info->Container.Item[i];
    pkt.iteminslot[i] = item ? static_cast<int16_t>(item->ID) : 0;
  }

  Zeal::Game::send_message(kOpTradeSkillCombine, reinterpret_cast<int *>(&pkt), sizeof(pkt), 0);
}

bool AutoCombine::trigger_combine() {
  auto wnd = get_active_combine_container();
  if (!wnd) {
    stop("combine container closed");
    return false;
  }

  if (static_cast<TriggerMethod>(setting_trigger_method.get()) == TriggerMethod::Packet)
    trigger_combine_packet(wnd);
  else
    trigger_combine_button(wnd);

  waiting_for_ack = true;
  ack_deadline = GetTickCount64() + kAckTimeoutMs;
  return true;
}

// Handles whatever the combine left on the cursor: either preserve it (auto-inventory) or, if the
// recipe is flagged delete_result, destroy it to save bag space (skill-grinding).
void AutoCombine::handle_result_on_cursor() {
  auto char_info = Zeal::Game::get_char_info();
  if (!char_info || !char_info->CursorItem) return;

  // Never destroy a keep-item even if delete_result is set (e.g. a returned reusable mold/tool).
  if (active_recipe.delete_result && !is_keep_item(char_info->CursorItem)) {
    Zeal::Game::destroy_held();
    return;
  }

  if (Zeal::Game::can_inventory_item(char_info->CursorItem)) {
    Zeal::Game::GameInternal::auto_inventory(char_info, &char_info->CursorItem, 0);
  } else {
    stop("cursor item cannot be auto-inventoried (bags full?)");
  }
}

// Detects the server's combine acknowledgement (0-length OP_TradeSkillCombine) which signals the
// client is free to transact again and any result is now on the cursor.
bool AutoCombine::handle_packet(UINT opcode) {
  if (state == State::Idle) return false;
  if (opcode == kOpTradeSkillCombine) waiting_for_ack = false;
  return false;  // Never consume; let the client process normally.
}

void AutoCombine::tick() {
  if (state == State::Idle) return;

  if (!Zeal::Game::is_in_game()) {
    stop();
    return;
  }

  // Pace discrete actions using the configured step delay.
  ULONGLONG now = GetTickCount64();
  if (now < next_action_time) return;
  int delay = setting_step_delay_ms.get();
  if (delay < 50) delay = 50;  // Safety floor to avoid flooding the server.

  auto char_info = Zeal::Game::get_char_info();
  if (!char_info) return;

  switch (state) {
    case State::LoadItems: {
      if (requested_count > 0 && completed_count >= requested_count) {
        stop(nullptr);
        Zeal::Game::print_chat(USERCOLOR_LOOT, "AutoCombine: completed %d requested combine(s).", completed_count);
        return;
      }
      if (count_available_sets() <= 0) {
        stop(nullptr);
        Zeal::Game::print_chat(USERCOLOR_LOOT, "AutoCombine: out of ingredients after %d combine(s).", completed_count);
        return;
      }
      if (!load_next_batch()) return;  // stop() already called on failure.
      state = State::Combine;
      next_action_time = now + delay;
      break;
    }
    case State::Combine: {
      if (char_info->CursorItem) return;  // Wait for cursor to clear before combining.
      if (!trigger_combine()) return;
      state = State::WaitAck;
      next_action_time = now + delay;
      break;
    }
    case State::WaitAck: {
      if (waiting_for_ack) {
        if (now >= ack_deadline) {
          stop("timed out waiting for combine acknowledgement");
          return;
        }
        return;  // Still waiting for the server ack.
      }
      state = State::ClearCursor;
      next_action_time = now + delay;
      break;
    }
    case State::ClearCursor: {
      if (char_info->CursorItem) {
        handle_result_on_cursor();
        next_action_time = now + delay;
        if (state == State::Idle) return;  // handle_result_on_cursor may have stopped us.
        return;                            // Loop back next tick to clear any remaining cursor item.
      }
      completed_count++;
      state = State::NextCycle;
      next_action_time = now + delay;
      break;
    }
    case State::NextCycle: {
      state = State::LoadItems;
      next_action_time = now + delay;
      break;
    }
    default:
      break;
  }
}

void AutoCombine::stop(const char *reason) {
  if (state != State::Idle && reason)
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "AutoCombine stopped: %s.", reason);
  state = State::Idle;
  waiting_for_ack = false;
  next_action_time = 0;
}

// Builds the per-character recipe ini filename (e.g. Soandso_autocombine.ini).
void AutoCombine::initialize_ini_filename() {
  auto self = Zeal::Game::get_self();
  std::string name = (self && self->Name[0]) ? self->Name : "";
  std::string filename = name.empty() ? "autocombine.ini" : (name + "_autocombine.ini");
  std::filesystem::path file_path = Zeal::Game::get_game_path() / std::filesystem::path(filename);
  ini.set(file_path.string());
}

std::vector<std::string> AutoCombine::list_recipes() {
  initialize_ini_filename();
  return ini.getSectionNames();
}

// Loads a recipe section. Expected ini format:
//   [MyRecipe]
//   Items=1001,1001,1002    ; item ids to place into the container each cycle (repeats allowed)
//   DeleteResult=TRUE       ; optional, destroys the result each cycle
//   Keep=1050               ; optional, item ids to never consume/destroy (reusable molds/tools)
bool AutoCombine::load_recipe(const std::string &name) {
  initialize_ini_filename();

  if (!ini.exists(name, "Items")) {
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "AutoCombine: recipe [%s] not found or missing Items=.",
                           name.c_str());
    return false;
  }

  Recipe recipe;
  recipe.name = name;

  std::string items = ini.getValue<std::string>(name, "Items");
  for (auto &tok : Zeal::String::split(items, ",")) {
    int id = 0;
    if (!tok.empty() && Zeal::String::tryParse(tok, &id, true) && id > 0) recipe.component_item_ids.push_back(id);
  }

  if (recipe.component_item_ids.empty()) {
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "AutoCombine: recipe [%s] has no valid item ids.", name.c_str());
    return false;
  }

  if (ini.exists(name, "DeleteResult")) recipe.delete_result = ini.getValue<bool>(name, "DeleteResult");

  if (ini.exists(name, "Keep")) {
    for (auto &tok : Zeal::String::split(ini.getValue<std::string>(name, "Keep"), ",")) {
      int id = 0;
      if (!tok.empty() && Zeal::String::tryParse(tok, &id, true) && id > 0) recipe.keep_item_ids.push_back(id);
    }
  }

  active_recipe = recipe;
  return true;
}

AutoCombine::AutoCombine(ZealService *zeal) {
  zeal->callbacks->AddGeneric([this]() { tick(); });
  zeal->callbacks->AddGeneric([this]() { stop(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { stop(); }, callback_type::DeactivateUI);
  zeal->callbacks->AddPacket([this](UINT opcode, char *buffer, UINT len) { return handle_packet(opcode); },
                             callback_type::WorldMessage);

  zeal->commands_hook->Add(
      "/autocombine", {"/ac"}, "Automates repeated tradeskill combines from recipes in <name>_autocombine.ini.",
      [this](std::vector<std::string> &args) {
        if (args.size() >= 2 && args[1] == "stop") {
          if (state == State::Idle)
            Zeal::Game::print_chat("AutoCombine: not running.");
          else
            stop("cancelled by user");
          return true;
        }

        if (args.size() >= 2 && args[1] == "list") {
          auto recipes = list_recipes();
          if (recipes.empty()) {
            Zeal::Game::print_chat("AutoCombine: no recipes found. Create %s_autocombine.ini.",
                                   Zeal::Game::get_self() ? Zeal::Game::get_self()->Name : "<character>");
          } else {
            Zeal::Game::print_chat("AutoCombine recipes:");
            for (auto &r : recipes) Zeal::Game::print_chat("  %s", r.c_str());
          }
          return true;
        }

        if (args.size() >= 2 && args[1] == "method") {
          if (args.size() >= 3) {
            int m = 0;
            if (Zeal::String::tryParse(args[2], &m) && (m == 0 || m == 1)) setting_trigger_method.set(m);
          }
          Zeal::Game::print_chat("AutoCombine trigger method: %s",
                                 static_cast<TriggerMethod>(setting_trigger_method.get()) == TriggerMethod::Packet
                                     ? "1 (direct packet)"
                                     : "0 (UI button)");
          return true;
        }

        if (args.size() >= 2 && args[1] == "delay") {
          int d = 0;
          if (args.size() >= 3 && Zeal::String::tryParse(args[2], &d)) setting_step_delay_ms.set(d);
          Zeal::Game::print_chat("AutoCombine step delay: %d ms", setting_step_delay_ms.get());
          return true;
        }

        if (args.size() < 2) {
          Zeal::Game::print_chat("Usage: /autocombine <recipe> [count] | stop | list | method [0|1] | delay [ms]");
          return true;
        }

        if (state != State::Idle) {
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE,
                                 "AutoCombine is already running. Use /autocombine stop first.");
          return true;
        }

        if (!Zeal::Game::is_in_game()) return true;

        // Require an open combine container before starting.
        if (!get_active_combine_container()) {
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE,
                                 "AutoCombine: open a combine container (or bag) before starting.");
          return true;
        }

        if (!load_recipe(args[1])) return true;

        requested_count = 0;
        if (args.size() >= 3) {
          int c = 0;
          if (Zeal::String::tryParse(args[2], &c) && c > 0) requested_count = c;
        }

        int available = count_available_sets();
        if (available <= 0) {
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "AutoCombine: not enough ingredients for recipe [%s].",
                                 args[1].c_str());
          return true;
        }

        completed_count = 0;
        waiting_for_ack = false;
        next_action_time = 0;
        state = State::LoadItems;
        Zeal::Game::print_chat(USERCOLOR_LOOT, "AutoCombine started: [%s], %s%d set(s) available, method %d.",
                               active_recipe.name.c_str(), requested_count ? "" : "up to ",
                               requested_count ? requested_count : available, setting_trigger_method.get());
        return true;
      });
}

AutoCombine::~AutoCombine() {}