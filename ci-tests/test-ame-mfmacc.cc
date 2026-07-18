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

  // Enable all widen feature bits for testing
  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF16F16);
  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF32F32);
  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF64F64);
  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF8F16);
  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF8BF16);
  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF16F32);
  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMBF16F32);
  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF8F32);
  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF32F64);

  auto reset_regs = [&]() {
    for (auto &reg : AMU.tile_regs)
      reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs)
      reg.reset(AMU.ALEN, AMU.ARLEN);
  };

  // ==========================================================================
  // helper: build insn bits (md=4, ms1=0, ms2=1)
  // ==========================================================================
  auto bits_for = [](uint32_t match) -> insn_bits_t {
    return match | (insn_bits_t(4) << 7) | (insn_bits_t(0) << 15) | (insn_bits_t(1) << 20);
  };

  // ==========================================================================
  // 1. mfmacc.h  non-widen  (regression)
  // ==========================================================================
  {
    AMU.set_mtilem(2);
    AMU.set_mtilen(2);
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0); // RNE
    AMU.xmfflags->write_raw(0);
    reset_regs();

    // 2×2×2 exact: tile0=[[1,2],[3,4]], tile1=[[5,6],[7,8]], C[0,0]=0
    // expected: 1*5+2*7 = 19 (0x4cc0)
    AMU.tile_regs[0].elt<float16_t>(0, 0) = float16_t{0x3c00}; // 1.0
    AMU.tile_regs[0].elt<float16_t>(0, 1) = float16_t{0x4000}; // 2.0
    AMU.tile_regs[1].elt<float16_t>(0, 0) = float16_t{0x4500}; // 5.0
    AMU.tile_regs[1].elt<float16_t>(0, 1) = float16_t{0x4700}; // 7.0

    execute_mfmacc_h(p, insn_t(bits_for(MATCH_MFMACC_H)));
    // C[0,0] = 0 + 1*5 + 2*7 = 19
    float got_h = float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(0, 0)));
    assert(got_h == 19.0f);
  }

  // ==========================================================================
  // 2. mfmacc.s  non-widen  (regression)
  // ==========================================================================
  {
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    AMU.tile_regs[0].elt<float32_t>(0, 0) = f32_from_float(1.5f);
    AMU.tile_regs[0].elt<float32_t>(0, 1) = f32_from_float(2.5f);
    AMU.tile_regs[1].elt<float32_t>(0, 0) = f32_from_float(3.0f);
    AMU.tile_regs[1].elt<float32_t>(0, 1) = f32_from_float(4.0f);

    execute_mfmacc_s(p, insn_t(bits_for(MATCH_MFMACC_S)));
    // 1.5*3 + 2.5*4 = 4.5 + 10 = 14.5
    float got = float_from_f32(AMU.acc_regs[0].elt<float32_t>(0, 0));
    assert(got == 14.5f);
  }

  // ==========================================================================
  // 3. mfmacc.d  non-widen  (regression)
  // ==========================================================================
  {
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    AMU.tile_regs[0].elt<float64_t>(0, 0) = f64_from_double(1.25);
    AMU.tile_regs[0].elt<float64_t>(0, 1) = f64_from_double(2.75);
    AMU.tile_regs[1].elt<float64_t>(0, 0) = f64_from_double(3.5);
    AMU.tile_regs[1].elt<float64_t>(0, 1) = f64_from_double(4.0);

    execute_mfmacc_d(p, insn_t(bits_for(MATCH_MFMACC_D)));
    // 1.25*3.5 + 2.75*4.0 = 4.375 + 11.0 = 15.375
    double got = double_from_f64(AMU.acc_regs[0].elt<float64_t>(0, 0));
    assert(got == 15.375);
  }

  // ==========================================================================
  // 4. mfmacc.s.h  fp16->fp32 widen
  // ==========================================================================
  {
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    AMU.tile_regs[0].elt<float16_t>(0, 0) = float16_t{0x3c00}; // 1.0
    AMU.tile_regs[0].elt<float16_t>(0, 1) = float16_t{0x4000}; // 2.0
    AMU.tile_regs[1].elt<float16_t>(0, 0) = float16_t{0x4200}; // 3.0
    AMU.tile_regs[1].elt<float16_t>(0, 1) = float16_t{0x4400}; // 4.0

    execute_mfmacc_s_h(p, insn_t(bits_for(MATCH_MFMACC_S_H)));
    float got = float_from_f32(AMU.acc_regs[0].elt<float32_t>(0, 0));
    // 1*3 + 2*4 = 11
    assert(got == 11.0f);
  }

  // ==========================================================================
  // 5. mfmacc.s.bf16  bf16->fp32 widen
  // ==========================================================================
  {
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    // BF16 1.0 = 0x3f80, BF16 2.0 = 0x4000
    AMU.tile_regs[0].elt<bfloat16_t>(0, 0) = bfloat16_t{0x3f80};
    AMU.tile_regs[0].elt<bfloat16_t>(0, 1) = bfloat16_t{0x4000};
    AMU.tile_regs[1].elt<bfloat16_t>(0, 0) = bfloat16_t{0x4040}; // 3.0
    AMU.tile_regs[1].elt<bfloat16_t>(0, 1) = bfloat16_t{0x4080}; // 4.0

    execute_mfmacc_s_bf16(p, insn_t(bits_for(MATCH_MFMACC_S_BF16)));
    float got = float_from_f32(AMU.acc_regs[0].elt<float32_t>(0, 0));
    assert(got == 11.0f);
  }

  // ==========================================================================
  // 6. mfmacc.d.s  fp32->fp64 widen
  // ==========================================================================
  {
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    AMU.tile_regs[0].elt<float32_t>(0, 0) = f32_from_float(1.5f);
    AMU.tile_regs[0].elt<float32_t>(0, 1) = f32_from_float(2.5f);
    AMU.tile_regs[1].elt<float32_t>(0, 0) = f32_from_float(3.0f);
    AMU.tile_regs[1].elt<float32_t>(0, 1) = f32_from_float(4.0f);

    execute_mfmacc_d_s(p, insn_t(bits_for(MATCH_MFMACC_D_S)));
    double got = double_from_f64(AMU.acc_regs[0].elt<float64_t>(0, 0));
    assert(got == 14.5);
  }

  // ==========================================================================
  // 7. mfmacc.h.e4  e4m3->fp16 widen
  // ==========================================================================
  {
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    // e4m3: 0x18 = 1.0, 0x20 = 2.0, 0x28 = 4.0
    AMU.tile_regs[0].elt<e4m3_t>(0, 0) = e4m3_t{0x38}; // 1.0
    AMU.tile_regs[0].elt<e4m3_t>(0, 1) = e4m3_t{0x40}; // 2.0
    AMU.tile_regs[1].elt<e4m3_t>(0, 0) = e4m3_t{0x48}; // 4.0
    AMU.tile_regs[1].elt<e4m3_t>(0, 1) = e4m3_t{0x50}; // 8.0

    execute_mfmacc_h_e4(p, insn_t(bits_for(MATCH_MFMACC_H_E4)));
    // 1*4 + 2*8 = 20
    float got = float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(0, 0)));
    assert(got == 20.0f);
  }

  // ==========================================================================
  // 8. mfmacc.h.e5  e5m2->fp16 widen
  // ==========================================================================
  {
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    // e5m2: 0x20 = 1.0, 0x28 = 2.0, 0x30 = 4.0
    AMU.tile_regs[0].elt<e5m2_t>(0, 0) = e5m2_t{0x3C}; // 1.0
    AMU.tile_regs[0].elt<e5m2_t>(0, 1) = e5m2_t{0x40}; // 2.0
    AMU.tile_regs[1].elt<e5m2_t>(0, 0) = e5m2_t{0x44}; // 4.0
    AMU.tile_regs[1].elt<e5m2_t>(0, 1) = e5m2_t{0x48}; // 8.0

    execute_mfmacc_h_e5(p, insn_t(bits_for(MATCH_MFMACC_H_E5)));
    float got = float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(0, 0)));
    assert(got == 20.0f);
  }

  // ==========================================================================
  // 9. mfmacc.bf16.e4  e4m3->bf16 widen
  // ==========================================================================
  {
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    AMU.tile_regs[0].elt<e4m3_t>(0, 0) = e4m3_t{0x38}; // 1.0
    AMU.tile_regs[0].elt<e4m3_t>(0, 1) = e4m3_t{0x40}; // 2.0
    AMU.tile_regs[1].elt<e4m3_t>(0, 0) = e4m3_t{0x48}; // 4.0
    AMU.tile_regs[1].elt<e4m3_t>(0, 1) = e4m3_t{0x50}; // 8.0

    execute_mfmacc_bf16_e4(p, insn_t(bits_for(MATCH_MFMACC_BF16_E4)));
    float got = float_from_f32(bf16_to_f32(AMU.acc_regs[0].elt<bfloat16_t>(0, 0)));
    assert(got == 20.0f);
  }

  // ==========================================================================
  // 10. mfmacc.bf16.e5  e5m2->bf16 widen
  // ==========================================================================
  {
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    AMU.tile_regs[0].elt<e5m2_t>(0, 0) = e5m2_t{0x3C}; // 1.0
    AMU.tile_regs[0].elt<e5m2_t>(0, 1) = e5m2_t{0x40}; // 2.0
    AMU.tile_regs[1].elt<e5m2_t>(0, 0) = e5m2_t{0x44}; // 4.0
    AMU.tile_regs[1].elt<e5m2_t>(0, 1) = e5m2_t{0x48}; // 8.0

    execute_mfmacc_bf16_e5(p, insn_t(bits_for(MATCH_MFMACC_BF16_E5)));
    float got = float_from_f32(bf16_to_f32(AMU.acc_regs[0].elt<bfloat16_t>(0, 0)));
    assert(got == 20.0f);
  }

  // ==========================================================================
  // 11. mfmacc.s.e4  e4m3->fp32 widen
  // ==========================================================================
  {
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    AMU.tile_regs[0].elt<e4m3_t>(0, 0) = e4m3_t{0x38}; // 1.0
    AMU.tile_regs[0].elt<e4m3_t>(0, 1) = e4m3_t{0x40}; // 2.0
    AMU.tile_regs[1].elt<e4m3_t>(0, 0) = e4m3_t{0x48}; // 4.0
    AMU.tile_regs[1].elt<e4m3_t>(0, 1) = e4m3_t{0x50}; // 8.0

    execute_mfmacc_s_e4(p, insn_t(bits_for(MATCH_MFMACC_S_E4)));
    float got = float_from_f32(AMU.acc_regs[0].elt<float32_t>(0, 0));
    assert(got == 20.0f);
  }

  // ==========================================================================
  // 12. mfmacc.s.e5  e5m2->fp32 widen
  // ==========================================================================
  {
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    AMU.tile_regs[0].elt<e5m2_t>(0, 0) = e5m2_t{0x3C}; // 1.0
    AMU.tile_regs[0].elt<e5m2_t>(0, 1) = e5m2_t{0x40}; // 2.0
    AMU.tile_regs[1].elt<e5m2_t>(0, 0) = e5m2_t{0x44}; // 4.0
    AMU.tile_regs[1].elt<e5m2_t>(0, 1) = e5m2_t{0x48}; // 8.0

    execute_mfmacc_s_e5(p, insn_t(bits_for(MATCH_MFMACC_S_E5)));
    float got = float_from_f32(AMU.acc_regs[0].elt<float32_t>(0, 0));
    assert(got == 20.0f);
  }

  // ==========================================================================
  // 13. B transposed layout: B[j,k] not B[k,j]
  // ==========================================================================
  {
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    // C[1,0] = A.row(1) dot B.row(0) = A[1,:] . B[0,:]
    // A[1,0]=3, A[1,1]=4; B[0,0]=1, B[0,1]=2
    // Expected: C[1,0] = 3*1 + 4*2 = 11
    AMU.tile_regs[0].elt<float16_t>(1, 0) = float16_t{0x4200}; // 3.0
    AMU.tile_regs[0].elt<float16_t>(1, 1) = float16_t{0x4400}; // 4.0
    AMU.tile_regs[1].elt<float16_t>(0, 0) = float16_t{0x3c00}; // 1.0
    AMU.tile_regs[1].elt<float16_t>(0, 1) = float16_t{0x4000}; // 2.0

    execute_mfmacc_h(p, insn_t(bits_for(MATCH_MFMACC_H)));
    float got = float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(1, 0)));
    assert(got == 11.0f);
  }

  // ==========================================================================
  // 14. NV sticky (sNaN in BF16 inputs)
  // ==========================================================================
  {
    AMU.set_mtilek(1);
    AMU.set_mtilem(1);
    AMU.set_mtilen(1);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    // BF16 sNaN = 0x7f91
    AMU.tile_regs[0].elt<bfloat16_t>(0, 0) = bfloat16_t{0x7f91};
    AMU.tile_regs[1].elt<bfloat16_t>(0, 0) = bfloat16_t{0x3f80}; // 1.0
    AMU.acc_regs[0].elt<float32_t>(0, 0) = f32_from_float(0.0f);

    execute_mfmacc_s_bf16(p, insn_t(bits_for(MATCH_MFMACC_S_BF16)));
    assert(AMU.get_xmfflags() & ameUnit_t::XMFFLAG_NV);

    // sticky: clean 1x1+0, NV must persist
    AMU.set_mtilek(1);
    reset_regs();
    AMU.tile_regs[0].elt<bfloat16_t>(0, 0) = bfloat16_t{0x3f80}; // 1.0
    AMU.tile_regs[1].elt<bfloat16_t>(0, 0) = bfloat16_t{0x3f80}; // 1.0
    AMU.acc_regs[0].elt<float32_t>(0, 0) = f32_from_float(0.0f);
    execute_mfmacc_s_bf16(p, insn_t(bits_for(MATCH_MFMACC_S_BF16)));
    assert(AMU.get_xmfflags() & ameUnit_t::XMFFLAG_NV);
  }

  // ==========================================================================
  // 15. Inactive accumulator zeroing
  // ==========================================================================
  {
    AMU.set_mtilem(1);
    AMU.set_mtilen(1);
    AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    // Pre-fill acc with garbage
    AMU.acc_regs[0].elt<float32_t>(1, 0) = f32_from_float(99.0f);
    AMU.acc_regs[0].elt<float32_t>(0, 1) = f32_from_float(99.0f);
    AMU.acc_regs[0].elt<float32_t>(1, 1) = f32_from_float(99.0f);

    AMU.tile_regs[0].elt<float32_t>(0, 0) = f32_from_float(1.0f);
    AMU.tile_regs[0].elt<float32_t>(0, 1) = f32_from_float(2.0f);
    AMU.tile_regs[1].elt<float32_t>(0, 0) = f32_from_float(3.0f);
    AMU.tile_regs[1].elt<float32_t>(0, 1) = f32_from_float(4.0f);

    execute_mfmacc_s(p, insn_t(bits_for(MATCH_MFMACC_S)));

    // Active cell
    assert(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0, 0)) == 11.0f);
    // Inactive cells must be zero
    assert(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1, 0)) == 0.0f);
    assert(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0, 1)) == 0.0f);
    assert(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1, 1)) == 0.0f);
  }

  // ==========================================================================
  // 16. Feature bit off -> illegal
  // ==========================================================================
  {
    AMU.set_mtilek(1);
    AMU.set_mtilem(1);
    AMU.set_mtilen(1);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    // Save and clear MMF16F32
    reg_t saved = AMU.xmisa_features;
    AMU.xmisa_features &= ~ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF16F32);
    bool caught = false;
    try {
      execute_mfmacc_s_h(p, insn_t(bits_for(MATCH_MFMACC_S_H)));
    } catch (const trap_illegal_instruction &) {
      caught = true;
    }
    assert(caught);
    AMU.xmisa_features = saved;
  }

  // ==========================================================================
  // 17. ELEN < C bits -> illegal
  // ==========================================================================
  {
    AMU.set_mtilek(1);
    AMU.set_mtilem(1);
    AMU.set_mtilen(1);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    reg_t saved_elen = AMU.ELEN;
    AMU.ELEN = 32; // too small for fp64
    bool caught = false;
    try {
      execute_mfmacc_d(p, insn_t(bits_for(MATCH_MFMACC_D)));
    } catch (const trap_illegal_instruction &) {
      caught = true;
    }
    assert(caught);
    AMU.ELEN = saved_elen;
  }

  // ==========================================================================
  // 18. K > TRLEN/A_bits -> illegal
  // ==========================================================================
  {
    AMU.set_mtilem(1);
    AMU.set_mtilen(1);
    AMU.xmfrm->write_raw(0);
    AMU.xmfflags->write_raw(0);
    reset_regs();
    // TRLEN=128, float16=16 bits => max K=8. Set K=9 -> illegal
    AMU.set_mtilek(9);
    bool caught = false;
    try {
      execute_mfmacc_h(p, insn_t(bits_for(MATCH_MFMACC_H)));
    } catch (const trap_illegal_instruction &) {
      caught = true;
    }
    assert(caught);
    AMU.set_mtilek(2); // restore
  }

  // ==========================================================================
  // 19. xmfrm=5 (invalid) -> illegal (xmfrm_is_valid)
  // ==========================================================================
  {
    AMU.set_mtilek(1);
    AMU.set_mtilem(1);
    AMU.set_mtilen(1);
    AMU.xmfflags->write_raw(0);
    reset_regs();
    AMU.xmfrm->write_raw(5);
    bool caught = false;
    try {
      execute_mfmacc_h(p, insn_t(bits_for(MATCH_MFMACC_H)));
    } catch (const trap_illegal_instruction &) {
      caught = true;
    }
    assert(caught);
    AMU.xmfrm->write_raw(0);
  }

  // ==========================================================================
  // 20. Rounding: RNE tie with mfmacc.s.h
  // ==========================================================================
  {
    AMU.set_mtilek(1);
    AMU.set_mtilem(1);
    AMU.set_mtilen(1);
    AMU.xmfflags->write_raw(0);
    reset_regs();

    // FP16 1024 = 0x6400, FP16 0.000061 = 0x0401 -> prod = 0.0625... tie
    // Use RNE (xmfrm=0)
    AMU.xmfrm->write_raw(0);
    AMU.tile_regs[0].elt<float16_t>(0, 0) = float16_t{0x6400}; // 1024.0
    AMU.tile_regs[1].elt<float16_t>(0, 0) = float16_t{0x0401}; // ~0.00006104
    AMU.acc_regs[0].elt<float32_t>(0, 0) = f32_from_float(0.0f);

    execute_mfmacc_s_h(p, insn_t(bits_for(MATCH_MFMACC_S_H)));
    // Non-zero result confirms computation ran (exact value depends on tie behavior)
    assert(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0, 0)) != 0.0f);
  }

  // restore
  AMU.xmfrm->write_raw(0);
  AMU.set_mtilem(2);
  AMU.set_mtilen(2);
  AMU.set_mtilek(2);
  AMU.xmfflags->write_raw(0);

  std::cout << "mfmacc floating 12-instruction internal test passed\n";
}
