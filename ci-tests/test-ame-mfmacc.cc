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
static float32_t f32_from_float(float value)
{
  float32_t out;
  static_assert(sizeof(out) == sizeof(value));
  std::memcpy(&out, &value, sizeof(out));
  return out;
}
static float float_from_f32(float32_t value)
{
  float out;
  static_assert(sizeof(out) == sizeof(value));
  std::memcpy(&out, &value, sizeof(out));
  return out;
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
static void expect_eq(float got, float expected)
{
  if (got != expected) {
    std::cerr << "got " << got << ", expected " << expected << "\n";
    std::abort();
  }
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
  std::vector<std::string> htif_args{"/bin/true"};
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

  // --- common fixture --------------------------------------------------------
  AMU.ELEN = 64;
  AMU.TLEN = 512;
  AMU.TRLEN = 128;
  AMU.ROWNUM = AMU.TLEN / AMU.TRLEN;
  AMU.TLENB = AMU.TLEN / 8;
  AMU.TRLENB = AMU.TRLEN / 8;
  AMU.ARLEN = AMU.ROWNUM * AMU.ELEN;
  AMU.ALEN = AMU.ARLEN * AMU.ROWNUM;
  AMU.ARLENB = AMU.ARLEN / 8;
  AMU.ALENB = AMU.ALEN / 8;

  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF16F16);
  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF32F32);
  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF64F64);

  AMU.set_mtilem(2);
  AMU.set_mtilen(2);
  AMU.set_mtilek(2);
  AMU.xmfrm->write_raw(0);     // RNE

  // ==========================================================================
  // fp16 test
  // ==========================================================================
  {
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.xmfflags->write_raw(0);

    // A = [[1,2],[3,4]]
    AMU.tile_regs[0].elt<float16_t>(0, 0) = float16_t{0x3c00};
    AMU.tile_regs[0].elt<float16_t>(0, 1) = float16_t{0x4000};
    AMU.tile_regs[0].elt<float16_t>(1, 0) = float16_t{0x4200};
    AMU.tile_regs[0].elt<float16_t>(1, 1) = float16_t{0x4400};
    // B = [[5,6],[7,8]]  (transposed layout: B.row(j))
    AMU.tile_regs[1].elt<float16_t>(0, 0) = float16_t{0x4500};
    AMU.tile_regs[1].elt<float16_t>(0, 1) = float16_t{0x4600};
    AMU.tile_regs[1].elt<float16_t>(1, 0) = float16_t{0x4700};
    AMU.tile_regs[1].elt<float16_t>(1, 1) = float16_t{0x4800};
    // C active init = 10
    for (reg_t i = 0; i < AMU.acc_regs[0].get_rownum(); ++i)
      for (reg_t j = 0; j < AMU.acc_regs[0].get_row_bytes() / sizeof(float16_t); ++j)
        AMU.acc_regs[0].elt<float16_t>(i, j) = float16_t{0x4900};

    const insn_bits_t bits_h = MATCH_MFMACC_H | (insn_bits_t(4) << 7) |
                               (insn_bits_t(0) << 15) | (insn_bits_t(1) << 20);
    execute_mfmacc_h(p, insn_t(bits_h));

    // expected: [[27,33],[49,63]]
    assert(AMU.acc_regs[0].elt<float16_t>(0, 0).v == 0x4ec0);
    assert(AMU.acc_regs[0].elt<float16_t>(0, 1).v == 0x5020);
    assert(AMU.acc_regs[0].elt<float16_t>(1, 0).v == 0x5220);
    assert(AMU.acc_regs[0].elt<float16_t>(1, 1).v == 0x53e0);
    assert(AMU.get_xmfflags() == 0);

    // inactive region must be zero
    const reg_t acc_rows_h = AMU.acc_regs[0].get_rownum();
    const reg_t acc_cols_h = AMU.acc_regs[0].get_row_bytes() / sizeof(float16_t);
    for (reg_t i = 0; i < acc_rows_h; ++i) {
      for (reg_t j = 0; j < acc_cols_h; ++j) {
        if (i >= 2 || j >= 2)
          assert(AMU.acc_regs[0].elt<float16_t>(i, j).v == 0x0000);
      }
    }
  }

  // ==========================================================================
  // fp32 test
  // ==========================================================================
  {
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.xmfflags->write_raw(0);

    // A = [[1,2],[3,4]]
    AMU.tile_regs[0].elt<float32_t>(0, 0) = f32_from_float(1.0f);
    AMU.tile_regs[0].elt<float32_t>(0, 1) = f32_from_float(2.0f);
    AMU.tile_regs[0].elt<float32_t>(1, 0) = f32_from_float(3.0f);
    AMU.tile_regs[0].elt<float32_t>(1, 1) = f32_from_float(4.0f);
    // B = [[5,6],[7,8]]
    AMU.tile_regs[1].elt<float32_t>(0, 0) = f32_from_float(5.0f);
    AMU.tile_regs[1].elt<float32_t>(0, 1) = f32_from_float(6.0f);
    AMU.tile_regs[1].elt<float32_t>(1, 0) = f32_from_float(7.0f);
    AMU.tile_regs[1].elt<float32_t>(1, 1) = f32_from_float(8.0f);
    // C active init = 10
    for (reg_t i = 0; i < AMU.acc_regs[0].get_rownum(); ++i)
      for (reg_t j = 0; j < AMU.acc_regs[0].get_row_bytes() / sizeof(float32_t); ++j)
        AMU.acc_regs[0].elt<float32_t>(i, j) = f32_from_float(10.0f);

    const insn_bits_t bits_s = MATCH_MFMACC_S | (insn_bits_t(4) << 7) |
                               (insn_bits_t(0) << 15) | (insn_bits_t(1) << 20);
    execute_mfmacc_s(p, insn_t(bits_s));

    expect_eq(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0, 0)), 27.0f);
    expect_eq(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0, 1)), 33.0f);
    expect_eq(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1, 0)), 49.0f);
    expect_eq(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1, 1)), 63.0f);
    assert(AMU.get_xmfflags() == 0);

    const reg_t acc_rows_s = AMU.acc_regs[0].get_rownum();
    const reg_t acc_cols_s = AMU.acc_regs[0].get_row_bytes() / sizeof(float32_t);
    for (reg_t i = 0; i < acc_rows_s; ++i) {
      for (reg_t j = 0; j < acc_cols_s; ++j) {
        if (i >= 2 || j >= 2)
          expect_eq(float_from_f32(AMU.acc_regs[0].elt<float32_t>(i, j)), 0.0f);
      }
    }
  }

  // ==========================================================================
  // fp64 test
  // ==========================================================================
  {
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
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

    const insn_bits_t bits_d = MATCH_MFMACC_D | (insn_bits_t(4) << 7) |
                               (insn_bits_t(0) << 15) | (insn_bits_t(1) << 20);
    execute_mfmacc_d(p, insn_t(bits_d));

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
  }

  // ==========================================================================
  // rounding mode tests: RNE vs RUP (1×1×1, half-ULP tie)
  // ==========================================================================
  AMU.set_mtilem(1);
  AMU.set_mtilen(1);
  AMU.set_mtilek(1);

  // --- fp16 rounding ---------------------------------------------------------
  {
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.xmfflags->write_raw(0);
    AMU.xmfrm->write_raw(0); // RNE

    // C=1.0, A=1.0, B=half_ULP (2^-11)
    AMU.acc_regs[0].elt<float16_t>(0, 0) = float16_t{0x3c00};
    AMU.tile_regs[0].elt<float16_t>(0, 0) = float16_t{0x3c00};
    AMU.tile_regs[1].elt<float16_t>(0, 0) = float16_t{0x1000};

    const insn_bits_t bits_h = MATCH_MFMACC_H | (insn_bits_t(4) << 7) |
                               (insn_bits_t(0) << 15) | (insn_bits_t(1) << 20);
    execute_mfmacc_h(p, insn_t(bits_h));
    // RNE: tie to even → 1.0 stays
    assert(AMU.acc_regs[0].elt<float16_t>(0, 0).v == 0x3c00);
    assert(AMU.get_xmfflags() == 0);

    // RUP: round up → nextUp
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.xmfflags->write_raw(0);
    AMU.xmfrm->write_raw(3); // RUP

    AMU.acc_regs[0].elt<float16_t>(0, 0) = float16_t{0x3c00};
    AMU.tile_regs[0].elt<float16_t>(0, 0) = float16_t{0x3c00};
    AMU.tile_regs[1].elt<float16_t>(0, 0) = float16_t{0x1000};

    execute_mfmacc_h(p, insn_t(bits_h));
    assert(AMU.acc_regs[0].elt<float16_t>(0, 0).v == 0x3c01);
  }

  // --- fp32 rounding ---------------------------------------------------------
  {
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.xmfflags->write_raw(0);
    AMU.xmfrm->write_raw(0); // RNE

    AMU.acc_regs[0].elt<float32_t>(0, 0) = f32_from_float(1.0f);
    AMU.tile_regs[0].elt<float32_t>(0, 0) = f32_from_float(1.0f);
    AMU.tile_regs[1].elt<float32_t>(0, 0) = f32_from_float(1.0f);
    *reinterpret_cast<uint32_t *>(&AMU.tile_regs[1].elt<float32_t>(0, 0)) = 0x33800000;

    const insn_bits_t bits_s = MATCH_MFMACC_S | (insn_bits_t(4) << 7) |
                               (insn_bits_t(0) << 15) | (insn_bits_t(1) << 20);
    execute_mfmacc_s(p, insn_t(bits_s));
    assert(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0, 0)) == 1.0f);
    assert(AMU.get_xmfflags() == 0);

    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.xmfflags->write_raw(0);
    AMU.xmfrm->write_raw(3); // RUP

    AMU.acc_regs[0].elt<float32_t>(0, 0) = f32_from_float(1.0f);
    AMU.tile_regs[0].elt<float32_t>(0, 0) = f32_from_float(1.0f);
    *reinterpret_cast<uint32_t *>(&AMU.tile_regs[1].elt<float32_t>(0, 0)) = 0x33800000;

    execute_mfmacc_s(p, insn_t(bits_s));
    // nextUp: 0x3f800001
    assert(*reinterpret_cast<uint32_t *>(&AMU.acc_regs[0].elt<float32_t>(0, 0)) == 0x3f800001);
  }

  // --- fp64 rounding ---------------------------------------------------------
  {
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.xmfflags->write_raw(0);
    AMU.xmfrm->write_raw(0); // RNE

    AMU.acc_regs[0].elt<float64_t>(0, 0) = f64_from_double(1.0);
    AMU.tile_regs[0].elt<float64_t>(0, 0) = f64_from_double(1.0);
    *reinterpret_cast<uint64_t *>(&AMU.tile_regs[1].elt<float64_t>(0, 0)) = 0x3ca0000000000000ULL;

    const insn_bits_t bits_d = MATCH_MFMACC_D | (insn_bits_t(4) << 7) |
                               (insn_bits_t(0) << 15) | (insn_bits_t(1) << 20);
    execute_mfmacc_d(p, insn_t(bits_d));
    assert(double_from_f64(AMU.acc_regs[0].elt<float64_t>(0, 0)) == 1.0);
    assert(AMU.get_xmfflags() == 0);

    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.xmfflags->write_raw(0);
    AMU.xmfrm->write_raw(3); // RUP

    AMU.acc_regs[0].elt<float64_t>(0, 0) = f64_from_double(1.0);
    AMU.tile_regs[0].elt<float64_t>(0, 0) = f64_from_double(1.0);
    *reinterpret_cast<uint64_t *>(&AMU.tile_regs[1].elt<float64_t>(0, 0)) = 0x3ca0000000000000ULL;

    execute_mfmacc_d(p, insn_t(bits_d));
    assert(*reinterpret_cast<uint64_t *>(&AMU.acc_regs[0].elt<float64_t>(0, 0)) == 0x3ff0000000000001ULL);
  }

  // ==========================================================================
  // NV + sticky xmfflags test (0 × Inf + 0 → NV, then 1 × 1 + 0 → NV sticky)
  // ==========================================================================

  // --- fp16 NV ---------------------------------------------------------------
  {
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.xmfflags->write_raw(0);
    AMU.xmfrm->write_raw(0); // RNE

    AMU.acc_regs[0].elt<float16_t>(0, 0) = float16_t{0x0000};
    AMU.tile_regs[0].elt<float16_t>(0, 0) = float16_t{0x0000};
    AMU.tile_regs[1].elt<float16_t>(0, 0) = float16_t{0x7c00};

    const insn_bits_t bits_h = MATCH_MFMACC_H | (insn_bits_t(4) << 7) |
                               (insn_bits_t(0) << 15) | (insn_bits_t(1) << 20);
    execute_mfmacc_h(p, insn_t(bits_h));
    assert(AMU.get_xmfflags() & ameUnit_t::XMFFLAG_NV);

    // sticky: clean 1×1+0, NV must persist
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.acc_regs[0].elt<float16_t>(0, 0) = float16_t{0x0000};
    AMU.tile_regs[0].elt<float16_t>(0, 0) = float16_t{0x3c00};
    AMU.tile_regs[1].elt<float16_t>(0, 0) = float16_t{0x3c00};

    execute_mfmacc_h(p, insn_t(bits_h));
    assert(AMU.get_xmfflags() & ameUnit_t::XMFFLAG_NV);
  }

  // --- fp32 NV ---------------------------------------------------------------
  {
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.xmfflags->write_raw(0);
    AMU.xmfrm->write_raw(0); // RNE

    AMU.acc_regs[0].elt<float32_t>(0, 0) = f32_from_float(0.0f);
    AMU.tile_regs[0].elt<float32_t>(0, 0) = f32_from_float(0.0f);
    *reinterpret_cast<uint32_t *>(&AMU.tile_regs[1].elt<float32_t>(0, 0)) = 0x7f800000;

    const insn_bits_t bits_s = MATCH_MFMACC_S | (insn_bits_t(4) << 7) |
                               (insn_bits_t(0) << 15) | (insn_bits_t(1) << 20);
    execute_mfmacc_s(p, insn_t(bits_s));
    assert(AMU.get_xmfflags() & ameUnit_t::XMFFLAG_NV);

    // sticky: clean 1×1+0, NV must persist
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.acc_regs[0].elt<float32_t>(0, 0) = f32_from_float(0.0f);
    AMU.tile_regs[0].elt<float32_t>(0, 0) = f32_from_float(1.0f);
    AMU.tile_regs[1].elt<float32_t>(0, 0) = f32_from_float(1.0f);

    execute_mfmacc_s(p, insn_t(bits_s));
    assert(AMU.get_xmfflags() & ameUnit_t::XMFFLAG_NV);
  }

  // --- fp64 NV ---------------------------------------------------------------
  {
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.xmfflags->write_raw(0);
    AMU.xmfrm->write_raw(0); // RNE

    AMU.acc_regs[0].elt<float64_t>(0, 0) = f64_from_double(0.0);
    AMU.tile_regs[0].elt<float64_t>(0, 0) = f64_from_double(0.0);
    *reinterpret_cast<uint64_t *>(&AMU.tile_regs[1].elt<float64_t>(0, 0)) = 0x7ff0000000000000ULL;

    const insn_bits_t bits_d = MATCH_MFMACC_D | (insn_bits_t(4) << 7) |
                               (insn_bits_t(0) << 15) | (insn_bits_t(1) << 20);
    execute_mfmacc_d(p, insn_t(bits_d));
    assert(AMU.get_xmfflags() & ameUnit_t::XMFFLAG_NV);

    // sticky: clean 1×1+0, NV must persist
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
    AMU.acc_regs[0].elt<float64_t>(0, 0) = f64_from_double(0.0);
    AMU.tile_regs[0].elt<float64_t>(0, 0) = f64_from_double(1.0);
    AMU.tile_regs[1].elt<float64_t>(0, 0) = f64_from_double(1.0);

    execute_mfmacc_d(p, insn_t(bits_d));
    assert(AMU.get_xmfflags() & ameUnit_t::XMFFLAG_NV);
  }

  // restore xmfrm and tile shape
  AMU.xmfrm->write_raw(0); // RNE
  AMU.set_mtilem(2);
  AMU.set_mtilen(2);
  AMU.set_mtilek(2);

  std::cout << "mfmacc.h/s/d internal test passed\n";
}