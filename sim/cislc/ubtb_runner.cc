#include "ubtb.h"

#include <iostream>

using namespace cislc_model;

static void emit(const UbtbView& v) {
  const auto& p = v.prediction;
  std::cout << v.hit << ' ' << v.train_ready << ' '
            << p.region_base << ' ' << p.entry_slot << ' '
            << p.br_mask << ' ' << p.jal_mask << ' '
            << p.cfi_valid << ' ' << p.cfi_slot << ' '
            << static_cast<unsigned>(p.cfi) << ' '
            << static_cast<unsigned>(p.ras) << ' '
            << p.taken << ' ' << p.target_missing << ' '
            << p.target << ' ' << p.next_pc << ' '
            << v.lookup_increment << ' ' << v.hit_increment << '\n';
}

int main(int argc, char** argv) {
  try {
    if (argc != 5) return 2;
    Ubtb btb({static_cast<unsigned>(std::stoul(argv[1])),
              static_cast<unsigned>(std::stoul(argv[2])),
              static_cast<unsigned>(std::stoul(argv[3])),
              static_cast<unsigned>(std::stoul(argv[4]))});
    uint64_t reset, query, pc, stall, train, region, branches, taken;
    uint64_t cfi_valid, owner_slot, cfi, ras, target;
    while (std::cin >> reset >> query >> pc >> stall >> train >> region
                    >> branches >> taken >> cfi_valid >> owner_slot
                    >> cfi >> ras >> target) {
      if (cfi > 3 || ras > 3) return 2;
      UbtbCycle cycle;
      cycle.reset = reset;
      cycle.query_valid = query;
      cycle.pc = pc;
      cycle.stall = stall;
      cycle.training = {bool(train), region, branches, taken,
                        bool(cfi_valid), static_cast<unsigned>(owner_slot),
                        static_cast<Cfi>(cfi), static_cast<Ras>(ras), target};
      emit(btb.observe(cycle));
      btb.tick(cycle);
      emit(btb.observe(cycle));
    }
    return std::cin.eof() ? 0 : 2;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
