#include <riscv/ame_macc.h>
#include <riscv/encoding.h>
#include <riscv/processor.h>
#include <riscv/sim.h>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
#include <sstream>

static std::vector<std::pair<reg_t, abstract_mem_t *>> make_mems(const std::vector<mem_cfg_t> &layout)
{
  std::vector<std::pair<reg_t, abstract_mem_t *>> mems;
  mems.reserve(layout.size());
  for (const auto &cfg : layout)
    mems.push_back(std::make_pair(cfg.get_base(), new mem_t(cfg.get_size())));
  return mems;
}

static float32_t f32_from_float(float value) {
  float32_t out; std::memcpy(&out, &value, sizeof(out)); return out;
}
static float float_from_f32(float32_t value) {
  float out; std::memcpy(&out, &value, sizeof(out)); return out;
}
static float64_t f64_from_double(double value) {
  float64_t out; std::memcpy(&out, &value, sizeof(out)); return out;
}
static double double_from_f64(float64_t value) {
  double out; std::memcpy(&out, &value, sizeof(out)); return out;
}

template<typename T>
static bool raw_eq(T a, T b) {
  uint64_t va, vb;
  std::memcpy(&va, &a, sizeof(T)); std::memcpy(&vb, &b, sizeof(T));
  return va == vb;
}

static int g_errors = 0;
static std::ostringstream g_fail_log;
static void check(bool cond, const char* file, int line, const char* msg) {
  if (!cond) { g_errors++; g_fail_log << "FAIL " << file << ":" << line << " — " << msg << "\n"; }
}
#define CHECK(cond, msg) check((cond), __FILE__, __LINE__, msg)

int main()
{
  cfg_t cfg; cfg.isa = "RV64IMAFDCV";
  std::vector<device_factory_sargs_t> plugin_devices;
  std::vector<std::string> htif_args{"/bin/true"};
  debug_module_config_t dm_config = {};
  auto mems = make_mems(cfg.mem_layout);
  sim_t sim(&cfg, false, mems, plugin_devices, false, htif_args, dm_config,
            nullptr, true, nullptr, false, nullptr, std::nullopt);
  processor_t *p = sim.get_core(0);
  auto &AMU = p->AMU;

  AMU.ELEN = 64; AMU.TLEN = 512; AMU.TRLEN = 128;
  AMU.ROWNUM = AMU.TLEN / AMU.TRLEN;
  AMU.TLENB = AMU.TLEN / 8; AMU.TRLENB = AMU.TRLEN / 8;
  AMU.ARLEN = AMU.ROWNUM * AMU.ELEN; AMU.ALEN = AMU.ARLEN * AMU.ROWNUM;
  AMU.ARLENB = AMU.ARLEN / 8; AMU.ALENB = AMU.ALEN / 8;

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
    for (auto &reg : AMU.tile_regs) reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs) reg.reset(AMU.ALEN, AMU.ARLEN);
  };
  auto set_222 = [&]() { AMU.set_mtilem(2); AMU.set_mtilen(2); AMU.set_mtilek(2); };

  auto mk_insn = [](uint32_t match) -> insn_bits_t {
    return match | (insn_bits_t(4)<<7) | (insn_bits_t(0)<<15) | (insn_bits_t(1)<<20);
  };
  auto mk_full = [](uint32_t match, reg_t md, reg_t ms1, reg_t ms2) -> insn_bits_t {
    return match | (insn_bits_t(md)<<7) | (insn_bits_t(ms1)<<15) | (insn_bits_t(ms2)<<20);
  };

  // mfmacc.h 2x2x2
  {
    set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<float16_t>(0,0)=float16_t{0x3c00}; AMU.tile_regs[0].elt<float16_t>(0,1)=float16_t{0x4000};
    AMU.tile_regs[0].elt<float16_t>(1,0)=float16_t{0x4200}; AMU.tile_regs[0].elt<float16_t>(1,1)=float16_t{0x4400};
    AMU.tile_regs[1].elt<float16_t>(0,0)=float16_t{0x4500}; AMU.tile_regs[1].elt<float16_t>(0,1)=float16_t{0x4600};
    AMU.tile_regs[1].elt<float16_t>(1,0)=float16_t{0x4700}; AMU.tile_regs[1].elt<float16_t>(1,1)=float16_t{0x4800};
    AMU.acc_regs[0].elt<float16_t>(0,0)=float16_t{0x4900}; AMU.acc_regs[0].elt<float16_t>(0,1)=float16_t{0x4980};
    AMU.acc_regs[0].elt<float16_t>(1,0)=float16_t{0x4a00}; AMU.acc_regs[0].elt<float16_t>(1,1)=float16_t{0x4a80};
    execute_mfmacc_h(p, insn_t(mk_insn(MATCH_MFMACC_H)));
    CHECK(float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(0,0))) == 27.0f, "mfmacc.h C[0,0]");
    CHECK(float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(0,1))) == 34.0f, "mfmacc.h C[0,1]");
    CHECK(float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(1,0))) == 51.0f, "mfmacc.h C[1,0]");
    CHECK(float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(1,1))) == 66.0f, "mfmacc.h C[1,1]");
  }
  // mfmacc.s 2x2x2
  {
    set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<float32_t>(0,0)= f32_from_float(1.0f); AMU.tile_regs[0].elt<float32_t>(0,1)= f32_from_float(2.0f);
    AMU.tile_regs[0].elt<float32_t>(1,0)= f32_from_float(3.0f); AMU.tile_regs[0].elt<float32_t>(1,1)= f32_from_float(4.0f);
    AMU.tile_regs[1].elt<float32_t>(0,0)= f32_from_float(5.0f); AMU.tile_regs[1].elt<float32_t>(0,1)= f32_from_float(6.0f);
    AMU.tile_regs[1].elt<float32_t>(1,0)= f32_from_float(7.0f); AMU.tile_regs[1].elt<float32_t>(1,1)= f32_from_float(8.0f);
    AMU.acc_regs[0].elt<float32_t>(0,0)= f32_from_float(10.0f); AMU.acc_regs[0].elt<float32_t>(0,1)= f32_from_float(11.0f);
    AMU.acc_regs[0].elt<float32_t>(1,0)= f32_from_float(12.0f); AMU.acc_regs[0].elt<float32_t>(1,1)= f32_from_float(13.0f);
    execute_mfmacc_s(p, insn_t(mk_insn(MATCH_MFMACC_S)));
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0,0)) == 27.0f, "mfmacc.s C[0,0]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0,1)) == 34.0f, "mfmacc.s C[0,1]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1,0)) == 51.0f, "mfmacc.s C[1,0]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1,1)) == 66.0f, "mfmacc.s C[1,1]");
  }
  // mfmacc.d 2x2x2
  {
    set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<float64_t>(0,0)= f64_from_double(1.0); AMU.tile_regs[0].elt<float64_t>(0,1)= f64_from_double(2.0);
    AMU.tile_regs[0].elt<float64_t>(1,0)= f64_from_double(3.0); AMU.tile_regs[0].elt<float64_t>(1,1)= f64_from_double(4.0);
    AMU.tile_regs[1].elt<float64_t>(0,0)= f64_from_double(5.0); AMU.tile_regs[1].elt<float64_t>(0,1)= f64_from_double(6.0);
    AMU.tile_regs[1].elt<float64_t>(1,0)= f64_from_double(7.0); AMU.tile_regs[1].elt<float64_t>(1,1)= f64_from_double(8.0);
    AMU.acc_regs[0].elt<float64_t>(0,0)= f64_from_double(10.0); AMU.acc_regs[0].elt<float64_t>(0,1)= f64_from_double(11.0);
    AMU.acc_regs[0].elt<float64_t>(1,0)= f64_from_double(12.0); AMU.acc_regs[0].elt<float64_t>(1,1)= f64_from_double(13.0);
    execute_mfmacc_d(p, insn_t(mk_insn(MATCH_MFMACC_D)));
    CHECK(double_from_f64(AMU.acc_regs[0].elt<float64_t>(0,0)) == 27.0, "mfmacc.d C[0,0]");
    CHECK(double_from_f64(AMU.acc_regs[0].elt<float64_t>(0,1)) == 34.0, "mfmacc.d C[0,1]");
    CHECK(double_from_f64(AMU.acc_regs[0].elt<float64_t>(1,0)) == 51.0, "mfmacc.d C[1,0]");
    CHECK(double_from_f64(AMU.acc_regs[0].elt<float64_t>(1,1)) == 66.0, "mfmacc.d C[1,1]");
  }
  // mfmacc.s.h 2x2x2
  {
    set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<float16_t>(0,0)=float16_t{0x3c00}; AMU.tile_regs[0].elt<float16_t>(0,1)=float16_t{0x4000};
    AMU.tile_regs[0].elt<float16_t>(1,0)=float16_t{0x4200}; AMU.tile_regs[0].elt<float16_t>(1,1)=float16_t{0x4400};
    AMU.tile_regs[1].elt<float16_t>(0,0)=float16_t{0x4500}; AMU.tile_regs[1].elt<float16_t>(0,1)=float16_t{0x4600};
    AMU.tile_regs[1].elt<float16_t>(1,0)=float16_t{0x4700}; AMU.tile_regs[1].elt<float16_t>(1,1)=float16_t{0x4800};
    AMU.acc_regs[0].elt<float32_t>(0,0)= f32_from_float(10.0f); AMU.acc_regs[0].elt<float32_t>(0,1)= f32_from_float(11.0f);
    AMU.acc_regs[0].elt<float32_t>(1,0)= f32_from_float(12.0f); AMU.acc_regs[0].elt<float32_t>(1,1)= f32_from_float(13.0f);
    execute_mfmacc_s_h(p, insn_t(mk_insn(MATCH_MFMACC_S_H)));
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0,0)) == 27.0f, "mfmacc.s.h C[0,0]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0,1)) == 34.0f, "mfmacc.s.h C[0,1]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1,0)) == 51.0f, "mfmacc.s.h C[1,0]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1,1)) == 66.0f, "mfmacc.s.h C[1,1]");
  }
  // mfmacc.s.bf16 2x2x2
  {
    set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<bfloat16_t>(0,0)=bfloat16_t{0x3f80}; AMU.tile_regs[0].elt<bfloat16_t>(0,1)=bfloat16_t{0x4000};
    AMU.tile_regs[0].elt<bfloat16_t>(1,0)=bfloat16_t{0x4040}; AMU.tile_regs[0].elt<bfloat16_t>(1,1)=bfloat16_t{0x4080};
    AMU.tile_regs[1].elt<bfloat16_t>(0,0)=bfloat16_t{0x40a0}; AMU.tile_regs[1].elt<bfloat16_t>(0,1)=bfloat16_t{0x40c0};
    AMU.tile_regs[1].elt<bfloat16_t>(1,0)=bfloat16_t{0x40e0}; AMU.tile_regs[1].elt<bfloat16_t>(1,1)=bfloat16_t{0x4100};
    AMU.acc_regs[0].elt<float32_t>(0,0)= f32_from_float(10.0f); AMU.acc_regs[0].elt<float32_t>(0,1)= f32_from_float(11.0f);
    AMU.acc_regs[0].elt<float32_t>(1,0)= f32_from_float(12.0f); AMU.acc_regs[0].elt<float32_t>(1,1)= f32_from_float(13.0f);
    execute_mfmacc_s_bf16(p, insn_t(mk_insn(MATCH_MFMACC_S_BF16)));
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0,0)) == 27.0f, "mfmacc.s.bf16 C[0,0]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0,1)) == 34.0f, "mfmacc.s.bf16 C[0,1]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1,0)) == 51.0f, "mfmacc.s.bf16 C[1,0]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1,1)) == 66.0f, "mfmacc.s.bf16 C[1,1]");
  }
  // mfmacc.d.s 2x2x2
  {
    set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<float32_t>(0,0)= f32_from_float(1.0f); AMU.tile_regs[0].elt<float32_t>(0,1)= f32_from_float(2.0f);
    AMU.tile_regs[0].elt<float32_t>(1,0)= f32_from_float(3.0f); AMU.tile_regs[0].elt<float32_t>(1,1)= f32_from_float(4.0f);
    AMU.tile_regs[1].elt<float32_t>(0,0)= f32_from_float(5.0f); AMU.tile_regs[1].elt<float32_t>(0,1)= f32_from_float(6.0f);
    AMU.tile_regs[1].elt<float32_t>(1,0)= f32_from_float(7.0f); AMU.tile_regs[1].elt<float32_t>(1,1)= f32_from_float(8.0f);
    AMU.acc_regs[0].elt<float64_t>(0,0)= f64_from_double(10.0); AMU.acc_regs[0].elt<float64_t>(0,1)= f64_from_double(11.0);
    AMU.acc_regs[0].elt<float64_t>(1,0)= f64_from_double(12.0); AMU.acc_regs[0].elt<float64_t>(1,1)= f64_from_double(13.0);
    execute_mfmacc_d_s(p, insn_t(mk_insn(MATCH_MFMACC_D_S)));
    CHECK(double_from_f64(AMU.acc_regs[0].elt<float64_t>(0,0)) == 27.0, "mfmacc.d.s C[0,0]");
    CHECK(double_from_f64(AMU.acc_regs[0].elt<float64_t>(0,1)) == 34.0, "mfmacc.d.s C[0,1]");
    CHECK(double_from_f64(AMU.acc_regs[0].elt<float64_t>(1,0)) == 51.0, "mfmacc.d.s C[1,0]");
    CHECK(double_from_f64(AMU.acc_regs[0].elt<float64_t>(1,1)) == 66.0, "mfmacc.d.s C[1,1]");
  }
  // mfmacc.h.e4 2x2x2
  {
    set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<e4m3_t>(0,0)=e4m3_t{0x38}; AMU.tile_regs[0].elt<e4m3_t>(0,1)=e4m3_t{0x40};
    AMU.tile_regs[0].elt<e4m3_t>(1,0)=e4m3_t{0x44}; AMU.tile_regs[0].elt<e4m3_t>(1,1)=e4m3_t{0x48};
    AMU.tile_regs[1].elt<e4m3_t>(0,0)=e4m3_t{0x4a}; AMU.tile_regs[1].elt<e4m3_t>(0,1)=e4m3_t{0x4c};
    AMU.tile_regs[1].elt<e4m3_t>(1,0)=e4m3_t{0x4e}; AMU.tile_regs[1].elt<e4m3_t>(1,1)=e4m3_t{0x50};
    AMU.acc_regs[0].elt<float16_t>(0,0)=float16_t{0x4900}; AMU.acc_regs[0].elt<float16_t>(0,1)=float16_t{0x4980};
    AMU.acc_regs[0].elt<float16_t>(1,0)=float16_t{0x4a00}; AMU.acc_regs[0].elt<float16_t>(1,1)=float16_t{0x4a80};
    execute_mfmacc_h_e4(p, insn_t(mk_insn(MATCH_MFMACC_H_E4)));
    CHECK(float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(0,0))) == 27.0f, "mfmacc.h.e4 C[0,0]");
    CHECK(float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(0,1))) == 34.0f, "mfmacc.h.e4 C[0,1]");
    CHECK(float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(1,0))) == 51.0f, "mfmacc.h.e4 C[1,0]");
    CHECK(float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(1,1))) == 66.0f, "mfmacc.h.e4 C[1,1]");
  }
  // mfmacc.h.e5 2x2x2
  {
    set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<e5m2_t>(0,0)=e5m2_t{0x3c}; AMU.tile_regs[0].elt<e5m2_t>(0,1)=e5m2_t{0x40};
    AMU.tile_regs[0].elt<e5m2_t>(1,0)=e5m2_t{0x42}; AMU.tile_regs[0].elt<e5m2_t>(1,1)=e5m2_t{0x44};
    AMU.tile_regs[1].elt<e5m2_t>(0,0)=e5m2_t{0x45}; AMU.tile_regs[1].elt<e5m2_t>(0,1)=e5m2_t{0x46};
    AMU.tile_regs[1].elt<e5m2_t>(1,0)=e5m2_t{0x47}; AMU.tile_regs[1].elt<e5m2_t>(1,1)=e5m2_t{0x48};
    AMU.acc_regs[0].elt<float16_t>(0,0)=float16_t{0x4900}; AMU.acc_regs[0].elt<float16_t>(0,1)=float16_t{0x4980};
    AMU.acc_regs[0].elt<float16_t>(1,0)=float16_t{0x4a00}; AMU.acc_regs[0].elt<float16_t>(1,1)=float16_t{0x4a80};
    execute_mfmacc_h_e5(p, insn_t(mk_insn(MATCH_MFMACC_H_E5)));
    CHECK(float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(0,0))) == 27.0f, "mfmacc.h.e5 C[0,0]");
    CHECK(float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(0,1))) == 34.0f, "mfmacc.h.e5 C[0,1]");
    CHECK(float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(1,0))) == 51.0f, "mfmacc.h.e5 C[1,0]");
    CHECK(float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<float16_t>(1,1))) == 66.0f, "mfmacc.h.e5 C[1,1]");
  }
  // mfmacc.bf16.e4 2x2x2
  {
    set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<e4m3_t>(0,0)=e4m3_t{0x38}; AMU.tile_regs[0].elt<e4m3_t>(0,1)=e4m3_t{0x40};
    AMU.tile_regs[0].elt<e4m3_t>(1,0)=e4m3_t{0x44}; AMU.tile_regs[0].elt<e4m3_t>(1,1)=e4m3_t{0x48};
    AMU.tile_regs[1].elt<e4m3_t>(0,0)=e4m3_t{0x4a}; AMU.tile_regs[1].elt<e4m3_t>(0,1)=e4m3_t{0x4c};
    AMU.tile_regs[1].elt<e4m3_t>(1,0)=e4m3_t{0x4e}; AMU.tile_regs[1].elt<e4m3_t>(1,1)=e4m3_t{0x50};
    AMU.acc_regs[0].elt<bfloat16_t>(0,0)=bfloat16_t{0x4120}; AMU.acc_regs[0].elt<bfloat16_t>(0,1)=bfloat16_t{0x4130};
    AMU.acc_regs[0].elt<bfloat16_t>(1,0)=bfloat16_t{0x4140}; AMU.acc_regs[0].elt<bfloat16_t>(1,1)=bfloat16_t{0x4150};
    execute_mfmacc_bf16_e4(p, insn_t(mk_insn(MATCH_MFMACC_BF16_E4)));
    CHECK(float_from_f32(bf16_to_f32(AMU.acc_regs[0].elt<bfloat16_t>(0,0))) == 27.0f, "mfmacc.bf16.e4 C[0,0]");
    CHECK(float_from_f32(bf16_to_f32(AMU.acc_regs[0].elt<bfloat16_t>(0,1))) == 34.0f, "mfmacc.bf16.e4 C[0,1]");
    CHECK(float_from_f32(bf16_to_f32(AMU.acc_regs[0].elt<bfloat16_t>(1,0))) == 51.0f, "mfmacc.bf16.e4 C[1,0]");
    CHECK(float_from_f32(bf16_to_f32(AMU.acc_regs[0].elt<bfloat16_t>(1,1))) == 66.0f, "mfmacc.bf16.e4 C[1,1]");
  }
  // mfmacc.bf16.e5 2x2x2
  {
    set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<e5m2_t>(0,0)=e5m2_t{0x3c}; AMU.tile_regs[0].elt<e5m2_t>(0,1)=e5m2_t{0x40};
    AMU.tile_regs[0].elt<e5m2_t>(1,0)=e5m2_t{0x42}; AMU.tile_regs[0].elt<e5m2_t>(1,1)=e5m2_t{0x44};
    AMU.tile_regs[1].elt<e5m2_t>(0,0)=e5m2_t{0x45}; AMU.tile_regs[1].elt<e5m2_t>(0,1)=e5m2_t{0x46};
    AMU.tile_regs[1].elt<e5m2_t>(1,0)=e5m2_t{0x47}; AMU.tile_regs[1].elt<e5m2_t>(1,1)=e5m2_t{0x48};
    AMU.acc_regs[0].elt<bfloat16_t>(0,0)=bfloat16_t{0x4120}; AMU.acc_regs[0].elt<bfloat16_t>(0,1)=bfloat16_t{0x4130};
    AMU.acc_regs[0].elt<bfloat16_t>(1,0)=bfloat16_t{0x4140}; AMU.acc_regs[0].elt<bfloat16_t>(1,1)=bfloat16_t{0x4150};
    execute_mfmacc_bf16_e5(p, insn_t(mk_insn(MATCH_MFMACC_BF16_E5)));
    CHECK(float_from_f32(bf16_to_f32(AMU.acc_regs[0].elt<bfloat16_t>(0,0))) == 27.0f, "mfmacc.bf16.e5 C[0,0]");
    CHECK(float_from_f32(bf16_to_f32(AMU.acc_regs[0].elt<bfloat16_t>(0,1))) == 34.0f, "mfmacc.bf16.e5 C[0,1]");
    CHECK(float_from_f32(bf16_to_f32(AMU.acc_regs[0].elt<bfloat16_t>(1,0))) == 51.0f, "mfmacc.bf16.e5 C[1,0]");
    CHECK(float_from_f32(bf16_to_f32(AMU.acc_regs[0].elt<bfloat16_t>(1,1))) == 66.0f, "mfmacc.bf16.e5 C[1,1]");
  }
  // mfmacc.s.e4 2x2x2
  {
    set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<e4m3_t>(0,0)=e4m3_t{0x38}; AMU.tile_regs[0].elt<e4m3_t>(0,1)=e4m3_t{0x40};
    AMU.tile_regs[0].elt<e4m3_t>(1,0)=e4m3_t{0x44}; AMU.tile_regs[0].elt<e4m3_t>(1,1)=e4m3_t{0x48};
    AMU.tile_regs[1].elt<e4m3_t>(0,0)=e4m3_t{0x4a}; AMU.tile_regs[1].elt<e4m3_t>(0,1)=e4m3_t{0x4c};
    AMU.tile_regs[1].elt<e4m3_t>(1,0)=e4m3_t{0x4e}; AMU.tile_regs[1].elt<e4m3_t>(1,1)=e4m3_t{0x50};
    AMU.acc_regs[0].elt<float32_t>(0,0)= f32_from_float(10.0f); AMU.acc_regs[0].elt<float32_t>(0,1)= f32_from_float(11.0f);
    AMU.acc_regs[0].elt<float32_t>(1,0)= f32_from_float(12.0f); AMU.acc_regs[0].elt<float32_t>(1,1)= f32_from_float(13.0f);
    execute_mfmacc_s_e4(p, insn_t(mk_insn(MATCH_MFMACC_S_E4)));
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0,0)) == 27.0f, "mfmacc.s.e4 C[0,0]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0,1)) == 34.0f, "mfmacc.s.e4 C[0,1]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1,0)) == 51.0f, "mfmacc.s.e4 C[1,0]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1,1)) == 66.0f, "mfmacc.s.e4 C[1,1]");
  }
  // mfmacc.s.e5 2x2x2
  {
    set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<e5m2_t>(0,0)=e5m2_t{0x3c}; AMU.tile_regs[0].elt<e5m2_t>(0,1)=e5m2_t{0x40};
    AMU.tile_regs[0].elt<e5m2_t>(1,0)=e5m2_t{0x42}; AMU.tile_regs[0].elt<e5m2_t>(1,1)=e5m2_t{0x44};
    AMU.tile_regs[1].elt<e5m2_t>(0,0)=e5m2_t{0x45}; AMU.tile_regs[1].elt<e5m2_t>(0,1)=e5m2_t{0x46};
    AMU.tile_regs[1].elt<e5m2_t>(1,0)=e5m2_t{0x47}; AMU.tile_regs[1].elt<e5m2_t>(1,1)=e5m2_t{0x48};
    AMU.acc_regs[0].elt<float32_t>(0,0)= f32_from_float(10.0f); AMU.acc_regs[0].elt<float32_t>(0,1)= f32_from_float(11.0f);
    AMU.acc_regs[0].elt<float32_t>(1,0)= f32_from_float(12.0f); AMU.acc_regs[0].elt<float32_t>(1,1)= f32_from_float(13.0f);
    execute_mfmacc_s_e5(p, insn_t(mk_insn(MATCH_MFMACC_S_E5)));
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0,0)) == 27.0f, "mfmacc.s.e5 C[0,0]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0,1)) == 34.0f, "mfmacc.s.e5 C[0,1]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1,0)) == 51.0f, "mfmacc.s.e5 C[1,0]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1,1)) == 66.0f, "mfmacc.s.e5 C[1,1]");
  }
  // Inactive zeroing (mfmacc.s)
  {
    AMU.set_mtilem(1); AMU.set_mtilen(1); AMU.set_mtilek(2);
    AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.acc_regs[0].elt<float32_t>(1,0)=f32_from_float(99.0f);
    AMU.acc_regs[0].elt<float32_t>(0,1)=f32_from_float(99.0f);
    AMU.acc_regs[0].elt<float32_t>(1,1)=f32_from_float(99.0f);
    AMU.tile_regs[0].elt<float32_t>(0,0)=f32_from_float(1.0f);
    AMU.tile_regs[0].elt<float32_t>(0,1)=f32_from_float(2.0f);
    AMU.tile_regs[1].elt<float32_t>(0,0)=f32_from_float(3.0f);
    AMU.tile_regs[1].elt<float32_t>(0,1)=f32_from_float(4.0f);
    execute_mfmacc_s(p, insn_t(mk_insn(MATCH_MFMACC_S)));
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0,0))==11.0f, "inactive C[0,0]");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1,0))==0.0f, "inactive row");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(0,1))==0.0f, "inactive col");
    CHECK(float_from_f32(AMU.acc_regs[0].elt<float32_t>(1,1))==0.0f, "inactive corner");
  }
  set_222();

  // Tie rounding mfmacc.s.h (a=0x4248 b=0x2FFE c=0x3DCCCCCD)
  {
    AMU.set_mtilem(1); AMU.set_mtilen(1); AMU.set_mtilek(1);
    float32_t rne_exp{0x3efc00f3}, rup_exp{0x3efc00f4};
    assert(rne_exp.v != rup_exp.v);
    // RNE
    AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<float16_t>(0,0)=float16_t{0x4248};
    AMU.tile_regs[1].elt<float16_t>(0,0)=float16_t{0x2ffe};
    AMU.acc_regs[0].elt<float32_t>(0,0)=float32_t{0x3dcccccd};
    execute_mfmacc_s_h(p, insn_t(mk_insn(MATCH_MFMACC_S_H)));
    CHECK(raw_eq(AMU.acc_regs[0].elt<float32_t>(0,0), rne_exp), "tie RNE raw");
    CHECK((AMU.get_xmfflags()&ameUnit_t::XMFFLAG_NX)!= 0u, "tie RNE NX");
    // RUP
    AMU.xmfrm->write_raw(3); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<float16_t>(0,0)=float16_t{0x4248};
    AMU.tile_regs[1].elt<float16_t>(0,0)=float16_t{0x2ffe};
    AMU.acc_regs[0].elt<float32_t>(0,0)=float32_t{0x3dcccccd};
    execute_mfmacc_s_h(p, insn_t(mk_insn(MATCH_MFMACC_S_H)));
    CHECK(raw_eq(AMU.acc_regs[0].elt<float32_t>(0,0), rup_exp), "tie RUP raw");
    CHECK((AMU.get_xmfflags()&ameUnit_t::XMFFLAG_NX)!= 0u, "tie RUP NX");
  }

  // xmfflags sticky: sNaN->NV, NV persists, inexact->NX, reserved bit
  {
    AMU.set_mtilem(1); AMU.set_mtilen(1); AMU.set_mtilek(1);
    // (a) sNaN -> NV
    AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<bfloat16_t>(0,0)=bfloat16_t{0x7f91}; // BF16 sNaN
    AMU.tile_regs[1].elt<bfloat16_t>(0,0)=bfloat16_t{0x3f80};
    AMU.acc_regs[0].elt<float32_t>(0,0)=f32_from_float(0.0f);
    execute_mfmacc_s_bf16(p, insn_t(mk_insn(MATCH_MFMACC_S_BF16)));
    CHECK((AMU.get_xmfflags()&ameUnit_t::XMFFLAG_NV)!= 0u, "sNaN->NV");
    float32_t r=AMU.acc_regs[0].elt<float32_t>(0,0);
    CHECK(((r.v>>23)&0xFF)==0xFF&&(r.v&0x7FFFFF)!= 0u, "sNaN result qNaN");
    // (b) NV sticky
    reset_regs();
    AMU.tile_regs[0].elt<bfloat16_t>(0,0)=bfloat16_t{0x3f80};
    AMU.tile_regs[1].elt<bfloat16_t>(0,0)=bfloat16_t{0x3f80};
    AMU.acc_regs[0].elt<float32_t>(0,0)=f32_from_float(0.0f);
    execute_mfmacc_s_bf16(p, insn_t(mk_insn(MATCH_MFMACC_S_BF16)));
    CHECK((AMU.get_xmfflags()&ameUnit_t::XMFFLAG_NV)!= 0u, "NV sticky");
    // (c) inexact -> NX
    AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<float16_t>(0,0)=float16_t{0x4200}; // 3.0
    AMU.tile_regs[1].elt<float16_t>(0,0)=float16_t{0x3555}; // ~0.333
    AMU.acc_regs[0].elt<float16_t>(0,0)=float16_t{0x3c00}; // 1.0
    softfloat_exceptionFlags=0; softfloat_roundingMode=softfloat_round_near_even;
    float16_t h_exp=f16_mulAdd(float16_t{0x4200},float16_t{0x3555},float16_t{0x3c00});
    bool exp_nx=(softfloat_exceptionFlags&softfloat_flag_inexact)!= 0u;
    softfloat_exceptionFlags=0;
    execute_mfmacc_h(p, insn_t(mk_insn(MATCH_MFMACC_H)));
    CHECK(raw_eq(AMU.acc_regs[0].elt<float16_t>(0,0),h_exp),"inexact f16 bits");
    if(exp_nx) CHECK((AMU.get_xmfflags()&ameUnit_t::XMFFLAG_NX)!= 0u,"inexact->NX");
    // (d) DZ reserved not set
    AMU.xmfflags->write_raw(0); reset_regs();
    AMU.tile_regs[0].elt<float16_t>(0,0)=float16_t{0x3c00};
    AMU.tile_regs[1].elt<float16_t>(0,0)=float16_t{0x3c00};
    execute_mfmacc_h(p, insn_t(mk_insn(MATCH_MFMACC_H)));
    CHECK((AMU.get_xmfflags()&ameUnit_t::XMFFLAG_RESERVED)==0,"DZ reserved=0");
  }
  set_222();


  // FP8 special values — 6 handlers, each: pos normal, neg normal, subnormal, Inf/max, NaN, NaN*NaN+NV
  // E4M3 (h.e4, bf16.e4, s.e4): NaN=0x7F only, no Inf, no sNaN. Max normal=0x7E.
  // E5M2 (h.e5, bf16.e5, s.e5): Inf=0x7C, qNaN=0x7E, sNaN=0x7D.
  auto fp8_test = [&](auto set_t0, auto set_t1, auto set_c, auto exec, auto get_res,
                       const char* label) {
    AMU.set_mtilem(1); AMU.set_mtilen(1); AMU.set_mtilek(1);
    AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    set_t0(); set_t1(); set_c();
    exec();
    get_res(label);
  };

  // ----- mfmacc.h.e4 -----
  {
    using AT = e4m3_t; using CT = float16_t;
    auto exec = [&](){ execute_mfmacc_h_e4(p, insn_t(mk_insn(MATCH_MFMACC_H_E4))); };
    auto getf = [&](){ return float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<CT>(0,0))); };
    auto getv = [&](){ return AMU.acc_regs[0].elt<CT>(0,0); };

    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x38}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x40}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() == 2.0f, l); }, "h.e4 pos normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0xb8}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x40}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() == -2.0f, l); }, "h.e4 neg normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x01}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x38}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() != 0.0f, l); }, "h.e4 subnormal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7e}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x38}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getv().v == 0x5F00, l); }, "h.e4 max normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7f}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x38}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(((getv().v>>10)&0x1F)==0x1F&&(getv().v&0x3FF)!=0, l); }, "h.e4 NaN->qNaN");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7f}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x7f}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){
               CHECK(((getv().v>>10)&0x1F)==0x1F&&(getv().v&0x3FF)!=0, l);
               // E4M3 NaN is quiet-only, no NV from quiet NaN operands
             }, "h.e4 NaN*NaN->qNaN");
  }

  // ----- mfmacc.h.e5 -----
  {
    using AT = e5m2_t; using CT = float16_t;
    auto exec = [&](){ execute_mfmacc_h_e5(p, insn_t(mk_insn(MATCH_MFMACC_H_E5))); };
    auto getf = [&](){ return float_from_f32(f16_to_f32(AMU.acc_regs[0].elt<CT>(0,0))); };
    auto getv = [&](){ return AMU.acc_regs[0].elt<CT>(0,0); };

    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x3c}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x40}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() == 2.0f, l); }, "h.e5 pos normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0xbc}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x40}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() == -2.0f, l); }, "h.e5 neg normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x01}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x3c}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() != 0.0f, l); }, "h.e5 subnormal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7c}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x3c}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK((getv().v&0x7FFF)==0x7C00, l); }, "h.e5 Inf");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7e}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x3c}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(((getv().v>>10)&0x1F)==0x1F&&(getv().v&0x3FF)!=0, l); }, "h.e5 qNaN->qNaN");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7d}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x3c}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){
               CHECK(((getv().v>>10)&0x1F)==0x1F&&(getv().v&0x3FF)!=0, l);
               CHECK((AMU.get_xmfflags() & ameUnit_t::XMFFLAG_NV) != 0, "h.e5 sNaN NV");
             }, "h.e5 sNaN->qNaN+NV");
  }

  // ----- mfmacc.bf16.e4 -----
  {
    using AT = e4m3_t; using CT = bfloat16_t;
    auto exec = [&](){ execute_mfmacc_bf16_e4(p, insn_t(mk_insn(MATCH_MFMACC_BF16_E4))); };
    auto getf = [&](){ return float_from_f32(bf16_to_f32(AMU.acc_regs[0].elt<CT>(0,0))); };
    auto getv = [&](){ return AMU.acc_regs[0].elt<CT>(0,0); };

    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x38}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x40}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() == 2.0f, l); }, "bf16.e4 pos normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0xb8}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x40}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() == -2.0f, l); }, "bf16.e4 neg normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x01}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x38}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() != 0.0f, l); }, "bf16.e4 subnormal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7e}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x38}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getv().v == 0x43E0, l); }, "bf16.e4 max normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7f}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x38}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(((getv().v>>7)&0xFF)==0xFF, l); }, "bf16.e4 NaN->qNaN");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7f}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x7f}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){
               CHECK(((getv().v>>7)&0xFF)==0xFF, l);
               // E4M3 NaN is quiet-only, no NV from quiet NaN operands
             }, "bf16.e4 NaN*NaN->qNaN");
  }

  // ----- mfmacc.bf16.e5 -----
  {
    using AT = e5m2_t; using CT = bfloat16_t;
    auto exec = [&](){ execute_mfmacc_bf16_e5(p, insn_t(mk_insn(MATCH_MFMACC_BF16_E5))); };
    auto getf = [&](){ return float_from_f32(bf16_to_f32(AMU.acc_regs[0].elt<CT>(0,0))); };
    auto getv = [&](){ return AMU.acc_regs[0].elt<CT>(0,0); };

    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x3c}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x40}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() == 2.0f, l); }, "bf16.e5 pos normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0xbc}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x40}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() == -2.0f, l); }, "bf16.e5 neg normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x01}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x3c}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() != 0.0f, l); }, "bf16.e5 subnormal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7c}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x3c}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK((getv().v&0x7F80)==0x7F80, l); }, "bf16.e5 Inf");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7e}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x3c}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(((getv().v>>7)&0xFF)==0xFF, l); }, "bf16.e5 qNaN->qNaN");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7d}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x3c}; AMU.acc_regs[0].elt<CT>(0,0)=CT{0x0000}; },
             [](){}, [](){}, exec, [&](const char* l){
               CHECK(((getv().v>>7)&0xFF)==0xFF, l);
               CHECK((AMU.get_xmfflags() & ameUnit_t::XMFFLAG_NV) != 0, "bf16.e5 sNaN NV");
             }, "bf16.e5 sNaN->qNaN+NV");
  }

  // ----- mfmacc.s.e4 -----
  {
    using AT = e4m3_t; using CT = float32_t;
    auto exec = [&](){ execute_mfmacc_s_e4(p, insn_t(mk_insn(MATCH_MFMACC_S_E4))); };
    auto getf = [&](){ return float_from_f32(AMU.acc_regs[0].elt<CT>(0,0)); };
    auto getv = [&](){ return AMU.acc_regs[0].elt<CT>(0,0); };

    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x38}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x40}; AMU.acc_regs[0].elt<CT>(0,0)=f32_from_float(0.0f); },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() == 2.0f, l); }, "s.e4 pos normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0xb8}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x40}; AMU.acc_regs[0].elt<CT>(0,0)=f32_from_float(0.0f); },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() == -2.0f, l); }, "s.e4 neg normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x01}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x38}; AMU.acc_regs[0].elt<CT>(0,0)=f32_from_float(0.0f); },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getv().v != 0u, l); }, "s.e4 subnormal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7e}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x38}; AMU.acc_regs[0].elt<CT>(0,0)=f32_from_float(0.0f); },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getv().v == 0x43E00000u, l); }, "s.e4 max normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7f}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x38}; AMU.acc_regs[0].elt<CT>(0,0)=f32_from_float(0.0f); },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(((getv().v>>23)&0xFF)==0xFF&&(getv().v&0x7FFFFF)!=0u, l); }, "s.e4 NaN->qNaN");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7f}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x7f}; AMU.acc_regs[0].elt<CT>(0,0)=f32_from_float(0.0f); },
             [](){}, [](){}, exec, [&](const char* l){
               CHECK(((getv().v>>23)&0xFF)==0xFF&&(getv().v&0x7FFFFF)!=0u, l);
               // E4M3 NaN is quiet-only, no NV from quiet NaN operands
             }, "s.e4 NaN*NaN->qNaN");
  }

  // ----- mfmacc.s.e5 -----
  {
    using AT = e5m2_t; using CT = float32_t;
    auto exec = [&](){ execute_mfmacc_s_e5(p, insn_t(mk_insn(MATCH_MFMACC_S_E5))); };
    auto getf = [&](){ return float_from_f32(AMU.acc_regs[0].elt<CT>(0,0)); };
    auto getv = [&](){ return AMU.acc_regs[0].elt<CT>(0,0); };

    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x3c}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x40}; AMU.acc_regs[0].elt<CT>(0,0)=f32_from_float(0.0f); },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() == 2.0f, l); }, "s.e5 pos normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0xbc}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x40}; AMU.acc_regs[0].elt<CT>(0,0)=f32_from_float(0.0f); },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getf() == -2.0f, l); }, "s.e5 neg normal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x01}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x3c}; AMU.acc_regs[0].elt<CT>(0,0)=f32_from_float(0.0f); },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(getv().v != 0u, l); }, "s.e5 subnormal");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7c}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x3c}; AMU.acc_regs[0].elt<CT>(0,0)=f32_from_float(0.0f); },
             [](){}, [](){}, exec, [&](const char* l){ CHECK((getv().v&0x7F800000)==0x7F800000u, l); }, "s.e5 Inf");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7e}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x3c}; AMU.acc_regs[0].elt<CT>(0,0)=f32_from_float(0.0f); },
             [](){}, [](){}, exec, [&](const char* l){ CHECK(((getv().v>>23)&0xFF)==0xFF&&(getv().v&0x7FFFFF)!=0u, l); }, "s.e5 qNaN->qNaN");
    fp8_test([&](){ AMU.tile_regs[0].elt<AT>(0,0)=AT{0x7d}; AMU.tile_regs[1].elt<AT>(0,0)=AT{0x3c}; AMU.acc_regs[0].elt<CT>(0,0)=f32_from_float(0.0f); },
             [](){}, [](){}, exec, [&](const char* l){
               CHECK(((getv().v>>23)&0xFF)==0xFF&&(getv().v&0x7FFFFF)!=0u, l);
               CHECK((AMU.get_xmfflags() & ameUnit_t::XMFFLAG_NV) != 0, "s.e5 sNaN NV");
             }, "s.e5 sNaN->qNaN+NV");
  }


  // Legality
  auto safe_state = [&]() { set_222(); AMU.xmfrm->write_raw(0); AMU.xmfflags->write_raw(0); reset_regs();
    AMU.ELEN=64; AMU.xmisa_features|=ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF16F16); };
  auto expect_illegal = [&](const char* n, auto fn) {
    bool caught=false; try { fn(); } catch(const trap_illegal_instruction&){ caught=true; } CHECK(caught,n); };

  // md=0 illegal
  safe_state();
  expect_illegal("md=0 illegal", [&](){ execute_mfmacc_h(p, insn_t(mk_full(MATCH_MFMACC_H,0,0,1))); });

  // md=4 legal
  safe_state();
  { AMU.tile_regs[0].elt<float16_t>(0,0)=float16_t{0x3c00}; AMU.tile_regs[1].elt<float16_t>(0,0)=float16_t{0x3c00};
    execute_mfmacc_h(p, insn_t(mk_full(MATCH_MFMACC_H,4,0,1))); }

  // ms1=4 illegal
  safe_state();
  expect_illegal("ms1=4 illegal", [&](){ execute_mfmacc_h(p, insn_t(mk_full(MATCH_MFMACC_H,4,4,1))); });

  // ms2=4 illegal
  safe_state();
  expect_illegal("ms2=4 illegal", [&](){ execute_mfmacc_h(p, insn_t(mk_full(MATCH_MFMACC_H,4,0,4))); });

  // Feature MMF16F32 off
  safe_state();
  { reg_t s=AMU.xmisa_features; AMU.xmisa_features&=~ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF16F32);
    expect_illegal("MMF16F32 off", [&](){ execute_mfmacc_s_h(p, insn_t(mk_insn(MATCH_MFMACC_S_H))); });
    AMU.xmisa_features=s; }

  // ELEN < 64 for fp64
  safe_state();
  { reg_t s=AMU.ELEN; AMU.ELEN=32;
    expect_illegal("ELEN<64", [&](){ execute_mfmacc_d(p, insn_t(mk_insn(MATCH_MFMACC_D))); });
    AMU.ELEN=s; }

  // K > TRLEN/bits
  safe_state();
  { AMU.set_mtilek(9);
    expect_illegal("K>8", [&](){ execute_mfmacc_h(p, insn_t(mk_insn(MATCH_MFMACC_H))); });
    AMU.set_mtilek(2); }

  // xmfrm=5
  safe_state();
  { AMU.xmfrm->write_raw(5);
    expect_illegal("xmfrm=5", [&](){ execute_mfmacc_h(p, insn_t(mk_insn(MATCH_MFMACC_H))); });
    AMU.xmfrm->write_raw(0); }

  // M > ROWNUM
  safe_state();
  { AMU.set_mtilem(5);
    expect_illegal("M>4", [&](){ execute_mfmacc_h(p, insn_t(mk_insn(MATCH_MFMACC_H))); });
    AMU.set_mtilem(2); }

  // N > ROWNUM
  safe_state();
  { AMU.set_mtilen(5);
    expect_illegal("N>4", [&](){ execute_mfmacc_h(p, insn_t(mk_insn(MATCH_MFMACC_H))); });
    AMU.set_mtilen(2); }

  // Feature groups: MMF64F64, MMF32F64, MMF8F16, MMF8BF16, MMF8F32, MMBF16F32
  { safe_state(); reg_t s=AMU.xmisa_features; AMU.xmisa_features&=~ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF64F64);
    expect_illegal("MMF64F64 off", [&](){ execute_mfmacc_d(p, insn_t(mk_insn(MATCH_MFMACC_D))); }); AMU.xmisa_features=s; }
  { safe_state(); reg_t s=AMU.xmisa_features; AMU.xmisa_features&=~ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF32F64);
    expect_illegal("MMF32F64 off", [&](){ execute_mfmacc_d_s(p, insn_t(mk_insn(MATCH_MFMACC_D_S))); }); AMU.xmisa_features=s; }
  { safe_state(); reg_t s=AMU.xmisa_features; AMU.xmisa_features&=~ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF8F16);
    expect_illegal("MMF8F16 off", [&](){ execute_mfmacc_h_e4(p, insn_t(mk_insn(MATCH_MFMACC_H_E4))); }); AMU.xmisa_features=s; }
  { safe_state(); reg_t s=AMU.xmisa_features; AMU.xmisa_features&=~ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF8BF16);
    expect_illegal("MMF8BF16 off", [&](){ execute_mfmacc_bf16_e4(p, insn_t(mk_insn(MATCH_MFMACC_BF16_E4))); }); AMU.xmisa_features=s; }
  { safe_state(); reg_t s=AMU.xmisa_features; AMU.xmisa_features&=~ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMF8F32);
    expect_illegal("MMF8F32 off", [&](){ execute_mfmacc_s_e4(p, insn_t(mk_insn(MATCH_MFMACC_S_E4))); }); AMU.xmisa_features=s; }
  { safe_state(); reg_t s=AMU.xmisa_features; AMU.xmisa_features&=~ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMBF16F32);
    expect_illegal("MMBF16F32 off", [&](){ execute_mfmacc_s_bf16(p, insn_t(mk_insn(MATCH_MFMACC_S_BF16))); }); AMU.xmisa_features=s; }

  if (g_errors == 0)
    std::cout << "12 normative floating mfmacc instructions passed\n";
  else {
    std::cout << g_fail_log.str();
    std::cout << g_errors << " FAILURE(S)\n";
  }
  return g_errors;
}
