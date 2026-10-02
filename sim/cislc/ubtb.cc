#include "ubtb.h"

#include <stdexcept>

namespace cislc_model {

Ubtb::Ubtb(UbtbParameters parameters) : cfg_(parameters) {
  if (!cfg_.address_bits || cfg_.address_bits > 64 ||
      cfg_.region_bytes < 2 || (cfg_.region_bytes & (cfg_.region_bytes - 1)) ||
      cfg_.region_bytes / 2 > 64 || cfg_.entry_count < 2 ||
      !cfg_.tag_bits || cfg_.tag_bits > 32)
    throw std::invalid_argument("invalid uBTB parameters");
  for (unsigned bytes = cfg_.region_bytes; bytes > 1; bytes >>= 1) ++shift_;
  if (shift_ >= cfg_.address_bits)
    throw std::invalid_argument("uBTB region exceeds address width");
  address_mask_ = cfg_.address_bits == 64 ? UINT64_MAX :
      (uint64_t{1} << cfg_.address_bits) - 1;
  const unsigned slots = cfg_.region_bytes / 2;
  branch_mask_ = slots == 64 ? UINT64_MAX : (uint64_t{1} << slots) - 1;
  entries_.resize(cfg_.entry_count);
}

unsigned Ubtb::tag(uint64_t address) const {
  uint64_t number = mask_address(address) >> shift_;
  unsigned folded = 0;
  for (unsigned i = 0; i < cfg_.address_bits - shift_; ++i, number >>= 1)
    folded ^= unsigned(number & 1) << (i % cfg_.tag_bits);
  return folded;
}

UbtbView Ubtb::observe(const UbtbCycle& cycle) const {
  UbtbView view;
  view.train_ready = !cycle.reset;
  if (cycle.reset || cycle.stall || !cycle.query_valid) return view;

  const uint64_t pc = mask_address(cycle.pc);
  Prediction& pred = view.prediction;
  pred.region_base = pc & ~(uint64_t(cfg_.region_bytes) - 1);
  pred.entry_slot = (pc >> 1) % (cfg_.region_bytes / 2);
  pred.next_pc = mask_address(pred.region_base + cfg_.region_bytes);
  view.lookup_increment = 1;

  for (const auto& slot : entries_) {
    if (!slot || slot->tag != tag(pred.region_base)) continue;
    const Line& line = *slot;
    view.hit = true;
    view.hit_increment = 1;
    pred.br_mask = line.branches;
    pred.jal_mask = line.jals;
    const bool use_target = line.owner && line.owner_slot >= pred.entry_slot &&
        (line.cfi == Cfi::jal || line.cfi == Cfi::jalr ||
         (line.cfi == Cfi::branch && line.confidence >= 2));
    if (use_target) {
      pred.cfi_valid = true;
      pred.cfi_slot = line.owner_slot;
      pred.cfi = line.cfi;
      pred.ras = line.ras;
      pred.taken = true;
      pred.target = line.target;
      pred.next_pc = line.target;
    }
    break;
  }
  return view;
}

void Ubtb::tick(const UbtbCycle& cycle) {
  if (cycle.reset) {
    for (auto& entry : entries_) entry.reset();
    next_victim_ = 0;
    return;
  }
  const UbtbTraining& train = cycle.training;
  if (!train.valid || !((train.committed_branches & branch_mask_) ||
                        (train.cfi_valid && train.cfi != Cfi::none))) return;
  if (train.cfi_valid && train.cfi != Cfi::none &&
      train.cfi_slot >= cfg_.region_bytes / 2)
    throw std::invalid_argument("uBTB training slot out of range");

  const unsigned key = tag(train.region_base);
  unsigned index = cfg_.entry_count;
  bool found = false;
  for (unsigned i = 0; i < entries_.size(); ++i)
    if (entries_[i] && entries_[i]->tag == key) {
      index = i;
      found = true;
      break;
    }
  if (!found) {
    for (unsigned i = 0; i < entries_.size(); ++i)
      if (!entries_[i]) { index = i; break; }
    if (index == cfg_.entry_count) {
      index = next_victim_;
      next_victim_ = (next_victim_ + 1) % cfg_.entry_count;
    }
  }

  Line line = found ? *entries_[index] : Line{};
  line.tag = key;
  line.branches |= train.committed_branches & branch_mask_;
  if (train.cfi_valid && train.cfi != Cfi::none) {
    if (train.cfi == Cfi::branch) {
      line.branches |= uint64_t{1} << train.cfi_slot;
      line.confidence = found && line.owner && line.cfi == Cfi::branch &&
                        line.owner_slot == train.cfi_slot ?
                        (line.confidence < 3 ? line.confidence + 1 : 3) : 2;
    } else {
      line.confidence = 0;
    }
    if (train.cfi == Cfi::jal) line.jals |= uint64_t{1} << train.cfi_slot;
    line.owner = true;
    line.owner_slot = train.cfi_slot;
    line.cfi = train.cfi;
    line.ras = train.ras;
    line.target = mask_address(train.target);
  } else if (found && line.owner && line.cfi == Cfi::branch &&
             (train.committed_branches & (uint64_t{1} << line.owner_slot)) &&
             line.confidence) {
    --line.confidence;
  }
  entries_[index] = line;
}

} // namespace cislc_model
