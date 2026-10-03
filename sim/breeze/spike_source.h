#pragma once

#include "core.h"
#include "riscv/memtracer.h"

#include <cstddef>
#include <cstdint>
#include <optional>

class processor_t;

namespace breeze {

// Pulls one architecturally executed instruction from Spike when CoreModel's
// frontend asks for it. Captures the first physical data access of that step.
class SpikeSource final : public InstructionSource, public memtracer_t {
 public:
  explicit SpikeSource(processor_t* processor);
  bool next(InstructionEvent& event) override;
  bool take_tohost_store(uint64_t tohost_address);
  bool interested_in_range(uint64_t begin, uint64_t end,
                           access_type type) override;
  void trace(uint64_t addr, size_t bytes, access_type type) override;
  void clean_invalidate(uint64_t addr, size_t bytes,
                        bool clean, bool inval) override;

 private:
  processor_t* processor_;
  std::optional<uint64_t> data_address_;
  std::optional<uint64_t> store_address_;
};

}  // namespace breeze
