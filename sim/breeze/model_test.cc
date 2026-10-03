#include "dcache.h"
#include "core.h"
#include "predictor.h"

#include <cassert>
#include <cstdint>
#include <vector>

using breeze::BranchResolution;
using breeze::BranchType;
using breeze::DCache;
using breeze::Predictor;

class VectorSource : public breeze::InstructionSource {
 public:
  explicit VectorSource(std::vector<breeze::InstructionEvent> events)
      : events_(std::move(events)) {}
  bool next(breeze::InstructionEvent& event) override {
    if (position_ == events_.size()) return false;
    event = events_[position_++];
    return true;
  }
 private:
  std::vector<breeze::InstructionEvent> events_;
  size_t position_ = 0;
};

static unsigned wait_response(DCache& cache) {
  unsigned ticks = 0;
  while (!cache.response().valid && ticks < 1000) {
    cache.tick();
    ++ticks;
  }
  assert(cache.response().valid);
  return ticks;
}

static void predictor_contract() {
  Predictor predictor;
  const uint64_t pc = 0x80001002;
  const uint64_t target = 0x80001100;
  const auto cold = predictor.predict(pc);
  assert(cold.type == BranchType::None && !cold.taken);
  // Frontend mini-decode tags an actual BR even on BTB miss. PHT sees the
  // S1 snapshot index; GHR shifts in the EXE resolution cycle.
  BranchResolution update{true, pc, target, BranchType::Branch,
                          true, cold.pht_index, true};
  predictor.tick(update);
  assert(predictor.ghr() == 1);
  assert(predictor.predict(pc).type == BranchType::None);
  predictor.tick();
  auto warm = predictor.predict(pc);
  assert(warm.type == BranchType::Branch && warm.target == pc + 4);
  // A second taken resolution trains the index selected by current GHR.
  update.pht_index = warm.pht_index;
  predictor.tick(update);
  predictor.tick();
  warm = predictor.predict(pc);
  assert(warm.type == BranchType::Branch);
  predictor.reset();
  assert(predictor.ghr() == 0 && predictor.predict(pc).type == BranchType::None);
}

static void dcache_hit_contract() {
  DCache cache({2, true});
  assert(cache.request({0x80000000, false}));
  const auto miss_ticks = wait_response(cache);
  assert(!cache.response().hit);
  assert(miss_ticks >= 6);
  assert(!cache.request({0x80000000, false}));
  cache.tick();  // Respond -> Idle
  assert(cache.request({0x80000000, false}));
  assert(wait_response(cache) == 2);  // Idle/Lookup -> Respond
  assert(cache.response().hit);
  cache.tick();
  assert(cache.request({0x80000000, true}));
  assert(wait_response(cache) == 3);  // Idle/Lookup/StoreHitWrite -> Respond
  assert(cache.response().hit);
}

static void dcache_eviction_contract() {
  DCache cache({1, true});
  // Five different tags in the same set force one Home Put before refill.
  unsigned fifth_ticks = 0;
  for (uint64_t i = 0; i < 5; ++i) {
    if (i) cache.tick();
    assert(cache.request({0x80000000 + (i << 11), false}));
    const auto ticks = wait_response(cache);
    if (i == 4) fifth_ticks = ticks;
  }
  assert(fifth_ticks > 6);
}

static void core_cycle_contract() {
  // Load miss followed by a load hit to the same line, then a taken JAL.
  const std::vector<breeze::InstructionEvent> program = {
      {0x1000, 0x1004, 0x00002083, 4, 0x80000000},
      {0x1004, 0x1008, 0x00002103, 4, 0x80000008},
      {0x1008, 0x1010, 0x0080006f, 4, std::nullopt},
      {0x1010, 0x1014, 0x00000013, 4, std::nullopt},
  };
  VectorSource source(program);
  breeze::CoreModel model;
  for (unsigned i = 0; i < 1000 && !model.done(); ++i) model.tick(source);
  assert(model.done());
  assert(model.retired() == 4);
  assert(model.cycles() > model.retired());
  VectorSource baseline_source(program);
  breeze::CoreModel::Config baseline_config;
  baseline_config.gshare = false;
  breeze::CoreModel baseline(baseline_config);
  for (unsigned i = 0; i < 1000 && !baseline.done(); ++i)
    baseline.tick(baseline_source);
  assert(baseline.done() && baseline.retired() == 4);
  assert(baseline.cycles() > model.cycles());
}

int main() {
  predictor_contract();
  dcache_hit_contract();
  dcache_eviction_contract();
  core_cycle_contract();
}
