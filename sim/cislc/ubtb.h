#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace cislc_model {

// Behavioral contract: CISLC-O3 rtl/frontend/ubtb.sv at
// 257df8d62ed2b58d95b66194fa008d71f89a840b.
// The cocotb reference source at 2e25a3acb7d52b0d00a5d8177fa3e194be947578
// is verification provenance, not the RTL version.
struct UbtbParameters {
  unsigned address_bits = 64;
  unsigned region_bytes = 16;
  unsigned entry_count = 16;
  unsigned tag_bits = 12;
};

enum class Cfi : unsigned { none = 0, branch = 1, jal = 2, jalr = 3 };
enum class Ras : unsigned { none = 0, push = 1, pop = 2 };

struct UbtbTraining {
  bool valid = false;
  uint64_t region_base = 0;
  uint64_t committed_branches = 0;
  uint64_t taken_branches = 0; // RTL input, unused by this uBTB organization.
  bool cfi_valid = false;
  unsigned cfi_slot = 0;
  Cfi cfi = Cfi::none;
  Ras ras = Ras::none;
  uint64_t target = 0;
};

struct UbtbCycle {
  bool reset = false;
  bool query_valid = false;
  uint64_t pc = 0;
  bool stall = false;
  UbtbTraining training;
};

struct Prediction {
  uint64_t region_base = 0;
  unsigned entry_slot = 0;
  uint64_t br_mask = 0;
  uint64_t jal_mask = 0;
  bool cfi_valid = false;
  unsigned cfi_slot = 0;
  Cfi cfi = Cfi::none;
  Ras ras = Ras::none;
  bool taken = false;
  bool target_missing = false;
  uint64_t target = 0;
  uint64_t next_pc = 0;
};

struct UbtbView {
  bool hit = false;
  bool train_ready = true;
  Prediction prediction;
  unsigned lookup_increment = 0;
  unsigned hit_increment = 0;
};

class Ubtb {
public:
  explicit Ubtb(UbtbParameters parameters = {});

  // Combinational view of current state, followed separately by one edge.
  UbtbView observe(const UbtbCycle& cycle) const;
  void tick(const UbtbCycle& cycle);
  unsigned tag(uint64_t address) const;

private:
  struct Line {
    unsigned tag = 0;
    uint64_t branches = 0;
    uint64_t jals = 0;
    bool owner = false;
    unsigned owner_slot = 0;
    Cfi cfi = Cfi::none;
    Ras ras = Ras::none;
    uint64_t target = 0;
    unsigned confidence = 0;
  };

  UbtbParameters cfg_;
  unsigned shift_ = 0;
  uint64_t address_mask_ = 0;
  uint64_t branch_mask_ = 0;
  std::vector<std::optional<Line>> entries_;
  unsigned next_victim_ = 0;

  uint64_t mask_address(uint64_t address) const { return address & address_mask_; }
};

} // namespace cislc_model
