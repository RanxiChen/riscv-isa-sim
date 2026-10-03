#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace breeze {

enum class BranchType { None, Branch, Jal, Jalr };

struct Prediction {
  BranchType type = BranchType::None;
  bool taken = false;
  uint64_t target = 0;
  uint32_t pht_index = 0;
};

struct BranchResolution {
  bool valid = false;
  uint64_t pc = 0;
  uint64_t target = 0;
  BranchType type = BranchType::None;
  bool taken = false;
  uint32_t pht_index = 0;
  bool prediction_miss = false;
};

// BreezeBTB + BreezePHT + frontend GHR, with the backend's registered BTB
// update. predict() sees state before the edge; tick() advances one edge.
class Predictor {
 public:
  Predictor(unsigned ghr_length = 8, size_t btb_entries = 16,
            unsigned pc_shift = 1);
  Prediction predict(uint64_t pc, unsigned inst_len = 4) const;
  void tick(const BranchResolution& resolution = {});
  void reset();
  uint32_t ghr() const { return ghr_; }

 private:
  struct Entry {
    bool valid = false;
    uint64_t key = 0;
    uint64_t target = 0;
    BranchType type = BranchType::None;
    bool taken = false;
  };
  void update_btb(const BranchResolution& update);

  unsigned ghr_length_;
  unsigned pc_shift_;
  uint32_t mask_;
  uint32_t ghr_ = 0;
  size_t rr_ptr_ = 0;
  std::vector<Entry> btb_;
  std::vector<uint8_t> pht_;
  BranchResolution delayed_btb_;
};

}  // namespace breeze
