#pragma once

#include <array>
#include <cstdint>

namespace breeze {

// Timing model of the retained BreezeDCache blocking CPU path. It models tag,
// MESI permission, PLRU and the RTL state transitions. Home/bus/DDR service is
// represented by a configurable response delay, not a DRAM model.
class DCache {
 public:
  enum class State {
    Idle, Lookup, StoreHitWrite, PutReq, PutWait, RefillReq,
    RefillWait, RefillInstall, UpgradeReq, UpgradeWait, Respond
  };
  struct Config {
    unsigned home_latency = 6;
    bool load_grants_exclusive = true;
  };
  struct Request {
    uint64_t address = 0;
    bool write = false;
  };
  struct Response {
    bool valid = false;
    bool hit = false;
  };

  DCache();
  explicit DCache(Config config);
  void reset();
  bool request(Request request);
  Response response() const;
  void tick();
  State state() const { return state_; }
  bool idle() const { return state_ == State::Idle && !pending_; }

 private:
  struct Way {
    bool valid = false;
    bool exclusive = false;
    bool dirty = false;
    uint32_t tag = 0;
  };
  struct Set {
    std::array<Way, 4> ways{};
    uint8_t plru = 0;
  };
  static unsigned set_index(uint64_t address) { return (address >> 5) & 63; }
  static uint32_t tag(uint64_t address) { return (address >> 11) & 0x1fffff; }
  static uint8_t touch(uint8_t plru, unsigned way);
  static unsigned victim(const Set& set);

  Config config_;
  std::array<Set, 64> sets_{};
  State state_ = State::Idle;
  Request request_{};
  bool pending_ = false;
  bool hit_ = false;
  unsigned selected_way_ = 0;
  unsigned remaining_ = 0;
};

}  // namespace breeze
