#include <riscv/ame_macc.h>
#include <riscv/encoding.h>
#include <riscv/processor.h>
#include <riscv/sim.h>

#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

static std::vector<std::pair<reg_t, abstract_mem_t *>> make_mems(const std::vector<mem_cfg_t> &layout)
{
  std::vector<std::pair<reg_t, abstract_mem_t *>> mems;
  mems.reserve(layout.size());
  for (const auto &cfg : layout)
    mems.push_back(std::make_pair(cfg.get_base(), new mem_t(cfg.get_size())));
  return mems;
}

static float64_t f64_from_double(double value)
{
  float64_t out;
  static_assert(sizeof(out) == sizeof(value));
  std::memcpy(&out, &value, sizeof(out));
  return out;
}

static double double_from_f64(float64_t value)
{
  double out;
  static_assert(sizeof(out) == sizeof(value));
  std::memcpy(&out, &value, sizeof(out));
  return out;
}

static void expect_eq(double got, double expected)
{
  if (got != expected) {
    std::cerr << "got " << got << ", expected " << expected << "\n";
    std::abort();
  }
}

int main()
{
  cfg_t cfg;
  cfg.isa = "RV64IMAFDCV";
  std::vector<device_factory_sargs_t> plugin_devices;
  std::vector<std::string> htif_args;
  debug_module_config_t dm_config = {.progbufsize = 2,
                                     .max_sba_data_width = 0,
                                     .require_authentication = false,
                                     .abstract_rti = 0,
                                     .support_hasel = true,
                                     .support_abstract_csr_access = true,
                                     .support_abstract_fpr_access = true,
                                     .support_haltgroups = true,
                                     .support_impebreak = true};
  std::vector<std::pair<reg_t, abstract_mem_t *>> mems = make_mems(cfg.mem_layout);
  sim_t sim(&cfg, false, mems, plugin_devices, false, htif_args, dm_config,
            nullptr, true, nullptr, false, nullptr, std::nullopt);

  processor_t *p = sim.get_core(0);
  auto &AMU = p->AMU;
  assert(AMU.get_elen() >= 64);

  AMU.set_mtilem(2);
  AMU.set_mtilen(2);
  AMU.set_mtilek(2);
  AMU.xmfrm->write_raw(0);     // RNE
  AMU.xmfflags->write_raw(0);

  AMU.tile_regs[0].elt<float64_t>(0, 0) = f64_from_double(1.0);
  AMU.tile_regs[0].elt<float64_t>(0, 1) = f64_from_double(2.0);
  AMU.tile_regs[0].elt<float64_t>(1, 0) = f64_from_double(3.0);
  AMU.tile_regs[0].elt<float64_t>(1, 1) = f64_from_double(4.0);

  AMU.tile_regs[1].elt<float64_t>(0, 0) = f64_from_double(5.0);
  AMU.tile_regs[1].elt<float64_t>(0, 1) = f64_from_double(6.0);
  AMU.tile_regs[1].elt<float64_t>(1, 0) = f64_from_double(7.0);
  AMU.tile_regs[1].elt<float64_t>(1, 1) = f64_from_double(8.0);

  for (reg_t i = 0; i < AMU.acc_regs[0].get_rownum(); ++i)
    for (reg_t j = 0; j < AMU.acc_regs[0].get_row_bytes() / sizeof(float64_t); ++j)
      AMU.acc_regs[0].elt<float64_t>(i, j) = f64_from_double(10.0);

  const insn_bits_t bits = MATCH_MFMACC_D | (insn_bits_t(4) << 7) |
                           (insn_bits_t(0) << 15) | (insn_bits_t(1) << 20);
  execute_mfmacc_d(p, insn_t(bits));

  expect_eq(double_from_f64(AMU.acc_regs[0].elt<float64_t>(0, 0)), 27.0);
  expect_eq(double_from_f64(AMU.acc_regs[0].elt<float64_t>(0, 1)), 33.0);
  expect_eq(double_from_f64(AMU.acc_regs[0].elt<float64_t>(1, 0)), 49.0);
  expect_eq(double_from_f64(AMU.acc_regs[0].elt<float64_t>(1, 1)), 63.0);
  assert(AMU.get_xmfflags() == 0);

  for (reg_t i = 0; i < AMU.acc_regs[0].get_rownum(); ++i) {
    for (reg_t j = 0; j < AMU.acc_regs[0].get_row_bytes() / sizeof(float64_t); ++j) {
      if (i >= 2 || j >= 2)
        expect_eq(double_from_f64(AMU.acc_regs[0].elt<float64_t>(i, j)), 0.0);
    }
  }

  std::cout << "mfmacc.d internal test passed\n";
}
