#include "predictor.h"

#include <stdexcept>

namespace breeze {

Predictor::Predictor(unsigned ghr_length, size_t btb_entries,
                     unsigned pc_shift)
    : ghr_length_(ghr_length), pc_shift_(pc_shift),
      mask_(0), btb_(btb_entries) {
  if (ghr_length < 2 || ghr_length > 16 || btb_entries == 0 ||
      (pc_shift != 1 && pc_shift != 2))
    throw std::invalid_argument("invalid Breeze predictor geometry");
  mask_ = (uint32_t{1} << ghr_length) - 1;
  pht_.assign(size_t{1} << ghr_length, 1);
}

void Predictor::reset() {
  ghr_ = 0;
  rr_ptr_ = 0;
  btb_.assign(btb_.size(), {});
  pht_.assign(pht_.size(), 1);
  delayed_btb_ = {};
}

Prediction Predictor::predict(uint64_t pc, unsigned inst_len) const {
  Prediction out;
  out.target = pc + inst_len;
  out.pht_index = static_cast<uint32_t>(((pc >> pc_shift_) & mask_) ^ ghr_);
  for (const auto& entry : btb_) {
    if (!entry.valid || entry.key != (pc >> pc_shift_)) continue;
    out.type = entry.type;
    if (entry.type == BranchType::Jal || entry.type == BranchType::Jalr)
      out.taken = true;
    else if (entry.type == BranchType::Branch)
      out.taken = (pht_[out.pht_index] & 2) != 0;
    if (out.taken) out.target = entry.target;
    break;
  }
  return out;
}

void Predictor::update_btb(const BranchResolution& update) {
  const uint64_t key = update.pc >> pc_shift_;
  size_t slot = btb_.size();
  for (size_t i = 0; i < btb_.size(); ++i) {
    if (btb_[i].valid && btb_[i].key == key) {
      slot = i;
      break;
    }
  }
  if (slot == btb_.size()) {
    for (size_t i = 0; i < btb_.size(); ++i) {
      if (!btb_[i].valid) {
        slot = i;
        break;
      }
    }
    if (slot == btb_.size()) {
      slot = rr_ptr_;
      rr_ptr_ = (rr_ptr_ + 1) % btb_.size();
    }
  }
  btb_[slot] = {true, key, update.target, update.type, update.taken};
}

void Predictor::tick(const BranchResolution& resolution) {
  // The BTB request crosses one register boundary; PHT/GHR update in EXE.
  if (delayed_btb_.valid) update_btb(delayed_btb_);
  delayed_btb_ = {};
  if (!resolution.valid) return;
  switch (resolution.type) {
    case BranchType::Branch: {
      auto& counter = pht_.at(resolution.pht_index & mask_);
      if (resolution.taken) {
        if (counter < 3) ++counter;
      } else if (counter > 0) {
        --counter;
      }
      ghr_ = ((ghr_ << 1) | resolution.taken) & mask_;
      delayed_btb_ = resolution;
      break;
    }
    case BranchType::Jal:
      delayed_btb_ = resolution;
      break;
    case BranchType::Jalr:
      if (resolution.prediction_miss) delayed_btb_ = resolution;
      break;
    case BranchType::None:
      break;
  }
}

}  // namespace breeze
