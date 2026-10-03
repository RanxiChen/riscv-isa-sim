#include "core.h"

#include <stdexcept>

namespace breeze {

namespace {
int64_t sign_extend(uint64_t value, unsigned bits) {
  return static_cast<int64_t>(value << (64 - bits)) >> (64 - bits);
}
}  // namespace

CoreModel::CoreModel() : CoreModel(Config{}) {}

CoreModel::CoreModel(Config config)
    : config_(config), predictor_(config.ghr_length, config.btb_entries,
                                   config.compressed ? 1 : 2),
      dcache_({config.home_latency, true}) {
  if (config.frontend_latency == 0)
    throw std::invalid_argument("frontend latency must be positive");
  reset();
}

void CoreModel::reset() {
  predictor_.reset();
  dcache_.reset();
  cycles_ = 0;
  retired_ = 0;
  redirect_bubbles_ = 0;
  source_exhausted_ = false;
  fetching_.clear();
  fetch_buffer_.clear();
  id_.reset();
  ex_.reset();
  mem_.reset();
  wb_.reset();
}

uint64_t CoreModel::static_target(const InstructionEvent& event) {
  const uint32_t inst = event.instruction;
  int64_t offset = 0;
  if (event.length == 2 && (inst & 3) == 1 && (inst >> 13) == 5) {
    const uint32_t imm = (((inst >> 12) & 1) << 11) |
                         (((inst >> 11) & 1) << 4) |
                         (((inst >> 9) & 3) << 8) |
                         (((inst >> 8) & 1) << 10) |
                         (((inst >> 7) & 1) << 6) |
                         (((inst >> 6) & 1) << 7) |
                         (((inst >> 3) & 7) << 1) |
                         (((inst >> 2) & 1) << 5);
    offset = sign_extend(imm, 12);
  } else if (event.length == 2 && (inst & 3) == 1 &&
             ((inst >> 13) == 6 || (inst >> 13) == 7)) {
    const uint32_t imm = (((inst >> 12) & 1) << 8) |
                         (((inst >> 10) & 3) << 3) |
                         (((inst >> 5) & 3) << 6) |
                         (((inst >> 3) & 3) << 1) |
                         (((inst >> 2) & 1) << 5);
    offset = sign_extend(imm, 9);
  } else if ((inst & 0x7f) == 0x6f) {
    const uint32_t imm = ((inst >> 31) << 20) |
                         (((inst >> 12) & 0xff) << 12) |
                         (((inst >> 20) & 1) << 11) |
                         (((inst >> 21) & 0x3ff) << 1);
    offset = sign_extend(imm, 21);
  } else if ((inst & 0x7f) == 0x63) {
    const uint32_t imm = (((inst >> 31) & 1) << 12) |
                         (((inst >> 7) & 1) << 11) |
                         (((inst >> 25) & 0x3f) << 5) |
                         (((inst >> 8) & 0xf) << 1);
    offset = sign_extend(imm, 13);
  }
  return event.pc + offset;
}

CoreModel::Slot CoreModel::make_slot(const InstructionEvent& event,
                                     const Predictor& predictor, bool gshare) {
  Slot slot;
  slot.event = event;
  slot.prediction = gshare ? predictor.predict(event.pc, event.length)
                           : Prediction{};
  if (!gshare) slot.prediction.target = event.pc + event.length;
  const uint32_t opcode = event.instruction & 0x7f;
  if (event.length == 4 && opcode == 0x63) slot.actual_type = BranchType::Branch;
  else if (event.length == 4 && opcode == 0x6f) slot.actual_type = BranchType::Jal;
  else if (event.length == 4 && opcode == 0x67) slot.actual_type = BranchType::Jalr;
  if (event.length == 2) {
    const unsigned quadrant = event.instruction & 3;
    const unsigned funct3 = (event.instruction >> 13) & 7;
    const unsigned rs1 = (event.instruction >> 7) & 31;
    const unsigned rs2 = (event.instruction >> 2) & 31;
    if (quadrant == 1 && funct3 == 5) slot.actual_type = BranchType::Jal;
    else if (quadrant == 1 && (funct3 == 6 || funct3 == 7))
      slot.actual_type = BranchType::Branch;
    else if (quadrant == 2 && funct3 == 4 && rs1 != 0 && rs2 == 0)
      slot.actual_type = BranchType::Jalr;
  }
  // BreezeFrontend S3 mini-decode corrects direct JAL and BR type/target.
  if (gshare && slot.actual_type == BranchType::Jal) {
    slot.prediction.type = BranchType::Jal;
    slot.prediction.taken = true;
    slot.prediction.target = static_target(event);
  } else if (gshare && slot.actual_type == BranchType::Branch) {
    slot.prediction.type = BranchType::Branch;
    if (slot.prediction.taken)
      slot.prediction.target = static_target(event);
  } else if (gshare && slot.actual_type == BranchType::Jalr) {
    slot.prediction.type = BranchType::Jalr;
  }
  slot.memory = event.data_address.has_value();
  slot.write = (event.length == 4 && (opcode == 0x23 || opcode == 0x27)) ||
               (event.length == 2 && ((event.instruction & 3) == 0 ||
                                      (event.instruction & 3) == 2) &&
                (((event.instruction >> 13) & 7) == 6 ||
                 ((event.instruction >> 13) & 7) == 7));
  return slot;
}

void CoreModel::tick(InstructionSource& source) {
  ++cycles_;
  // All state transitions below represent the same rising edge. The
  // functional source is queried only when the frontend has room.
  if (wb_) {
    ++retired_;
    wb_.reset();
  }

  dcache_.tick();
  bool mem_complete = false;
  if (mem_) {
    if (!mem_->memory) {
      mem_complete = true;
    } else if (!mem_->mem_requested) {
      mem_->mem_requested = dcache_.request(
          {*mem_->event.data_address, mem_->write});
    } else if (dcache_.response().valid) {
      mem_complete = true;
    }
  }
  if (mem_complete && !wb_) {
    wb_ = std::move(mem_);
    mem_.reset();
  }

  BranchResolution resolution;
  if (ex_ && !mem_) {
    auto& slot = *ex_;
    if (slot.actual_type != BranchType::None) {
      const bool actual_taken =
          slot.event.next_pc != slot.event.pc + slot.event.length;
      const bool mismatch = slot.prediction.taken != actual_taken ||
          (actual_taken && slot.prediction.target != slot.event.next_pc);
      const uint64_t branch_target = slot.actual_type == BranchType::Branch
          ? static_target(slot.event) : slot.event.next_pc;
      resolution = {true, slot.event.pc, branch_target,
                    slot.actual_type, actual_taken,
                    slot.prediction.pht_index, mismatch};
      if (mismatch) redirect_bubbles_ = config_.redirect_refill_cycles;
    }
    mem_ = std::move(ex_);
    ex_.reset();
  }
  if (config_.gshare) predictor_.tick(resolution);

  // The source supplies only architecturally correct-path instructions.
  // Hold these younger slots during recovery; a future speculative frontend
  // can replace this with actual wrong-path fetch and flush behavior.
  if (redirect_bubbles_ == 0) {
    if (!ex_ && id_) {
      ex_ = std::move(id_);
      id_.reset();
    }
    if (!id_ && !fetch_buffer_.empty()) {
      id_ = std::move(fetch_buffer_.front());
      fetch_buffer_.pop_front();
    }
  }
  while (!fetching_.empty() && fetching_.front().ready_cycle <= cycles_ &&
         fetch_buffer_.size() < 6) {
    fetch_buffer_.push_back(std::move(fetching_.front().slot));
    fetching_.pop_front();
  }
  if (redirect_bubbles_) {
    --redirect_bubbles_;
  } else if (!source_exhausted_ && fetching_.size() + fetch_buffer_.size() < 6) {
    InstructionEvent event;
    if (source.next(event)) {
      fetching_.push_back({make_slot(event, predictor_, config_.gshare),
                           cycles_ + config_.frontend_latency});
    } else {
      source_exhausted_ = true;
    }
  }
}

bool CoreModel::done() const {
  return source_exhausted_ && fetching_.empty() && fetch_buffer_.empty() &&
         !id_ && !ex_ && !mem_ && !wb_;
}

}  // namespace breeze
