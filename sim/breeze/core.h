#pragma once

#include "dcache.h"
#include "predictor.h"

#include <cstdint>
#include <deque>
#include <optional>

namespace breeze {

// One architecturally executed instruction supplied on demand by a functional
// engine. The source is called only as the modeled frontend admits a fetch.
struct InstructionEvent {
  uint64_t pc = 0;
  uint64_t next_pc = 0;
  uint32_t instruction = 0;
  unsigned length = 4;
  std::optional<uint64_t> data_address;
};

class InstructionSource {
 public:
  virtual ~InstructionSource() = default;
  virtual bool next(InstructionEvent& event) = 0;
};

class CoreModel {
 public:
  struct Config {
    unsigned frontend_latency = 3;
    unsigned redirect_refill_cycles = 3;
    unsigned home_latency = 6;
    unsigned ghr_length = 8;
    unsigned btb_entries = 16;
    bool compressed = true;
    bool gshare = true;
  };
  CoreModel();
  explicit CoreModel(Config config);
  void reset();
  void tick(InstructionSource& source);
  bool done() const;
  uint64_t cycles() const { return cycles_; }
  uint64_t retired() const { return retired_; }
  const Predictor& predictor() const { return predictor_; }
  const DCache& dcache() const { return dcache_; }

 private:
  struct Slot {
    InstructionEvent event;
    Prediction prediction;
    BranchType actual_type = BranchType::None;
    bool memory = false;
    bool write = false;
    bool mem_requested = false;
  };
  struct PendingFetch {
    Slot slot;
    uint64_t ready_cycle = 0;
  };
  static Slot make_slot(const InstructionEvent& event, const Predictor& predictor,
                        bool gshare);
  static uint64_t static_target(const InstructionEvent& event);

  Config config_;
  Predictor predictor_;
  DCache dcache_;
  uint64_t cycles_ = 0;
  uint64_t retired_ = 0;
  unsigned redirect_bubbles_ = 0;
  bool source_exhausted_ = false;
  std::deque<PendingFetch> fetching_;
  std::deque<Slot> fetch_buffer_;
  std::optional<Slot> id_;
  std::optional<Slot> ex_;
  std::optional<Slot> mem_;
  std::optional<Slot> wb_;
};

}  // namespace breeze
