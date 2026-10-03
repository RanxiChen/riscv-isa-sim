#include "spike_source.h"

#include "riscv/mmu.h"
#include "riscv/processor.h"

namespace breeze {

SpikeSource::SpikeSource(processor_t* processor) : processor_(processor) {
  processor_->get_mmu()->register_memtracer(this);
}

bool SpikeSource::next(InstructionEvent& event) {
  const uint64_t pc = processor_->get_state()->pc;
  uint32_t raw = 0;
  unsigned length = 4;
  try {
    auto fetched = processor_->get_mmu()->load_insn(pc);
    raw = static_cast<uint32_t>(fetched.insn.bits());
    length = insn_length(raw);
  } catch (const trap_t&) {
    // Spike itself will take the fetch exception in step(1).
  }
  data_address_.reset();
  store_address_.reset();
  processor_->step(1);
  event = {pc, processor_->get_state()->pc, raw, length, data_address_};
  return true;
}

bool SpikeSource::take_tohost_store(uint64_t tohost_address) {
  if (tohost_address == 0 || !store_address_ ||
      *store_address_ != tohost_address)
    return false;
  store_address_.reset();
  return true;
}

bool SpikeSource::interested_in_range(uint64_t, uint64_t, access_type type) {
  return type == LOAD || type == STORE;
}

void SpikeSource::trace(uint64_t addr, size_t, access_type type) {
  if ((type == LOAD || type == STORE) && !data_address_)
    data_address_ = addr;
  if (type == STORE) store_address_ = addr;
}

void SpikeSource::clean_invalidate(uint64_t, size_t, bool, bool) {}

}  // namespace breeze
