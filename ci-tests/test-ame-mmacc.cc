#include <riscv/ame_macc.h>
#include <riscv/encoding.h>
#include <riscv/processor.h>
#include <riscv/sim.h>
#include <cassert>
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

  AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMI8I32);

  auto reset_regs = [&]() {
    for (auto &reg : AMU.tile_regs) reg.reset(AMU.TLEN, AMU.TRLEN);
    for (auto &reg : AMU.acc_regs) reg.reset(AMU.ALEN, AMU.ARLEN);
  };
  auto set_222 = [&]() { AMU.set_mtilem(2); AMU.set_mtilen(2); AMU.set_mtilek(2); };
  auto set_111 = [&]() { AMU.set_mtilem(1); AMU.set_mtilen(1); AMU.set_mtilek(1); };
  auto set_112 = [&]() { AMU.set_mtilem(1); AMU.set_mtilen(1); AMU.set_mtilek(2); };

  auto mk_insn = [](uint32_t match) -> insn_bits_t {
    return match | (insn_bits_t(4)<<7) | (insn_bits_t(0)<<15) | (insn_bits_t(1)<<20);
  };
  auto mk_acc0_tr1_tr0 = [](uint32_t match) -> insn_t {
    return insn_t(match | (4u << 7) | (0u << 15) | (1u << 20));
  };

  // ================================================================
  // 11.2  Four signedness + 2x2x2
  // ================================================================
  auto set_2x2x2_data = [&]() {
    set_222();
    AMU.xmsaten->write_raw(0);
    AMU.xmsat->write_raw(0);
    reset_regs();
    // A (tr0) raw rows
    AMU.tile_regs[0].elt<int8_t>(0,0) = (int8_t)0xfe;
    AMU.tile_regs[0].elt<int8_t>(0,1) = (int8_t)0x03;
    AMU.tile_regs[0].elt<int8_t>(1,0) = (int8_t)0x80;
    AMU.tile_regs[0].elt<int8_t>(1,1) = (int8_t)0x7f;
    // B (tr1) raw rows
    AMU.tile_regs[1].elt<int8_t>(0,0) = (int8_t)0xfd;
    AMU.tile_regs[1].elt<int8_t>(0,1) = (int8_t)0x04;
    AMU.tile_regs[1].elt<int8_t>(1,0) = (int8_t)0xff;
    AMU.tile_regs[1].elt<int8_t>(1,1) = (int8_t)0x80;
    // C (acc0) initial
    AMU.acc_regs[0].elt<int32_t>(0,0) = 5;
    AMU.acc_regs[0].elt<int32_t>(0,1) = -7;
    AMU.acc_regs[0].elt<int32_t>(1,0) = 11;
    AMU.acc_regs[0].elt<int32_t>(1,1) = -13;
  };

  // mmacc.w.b: int8 x int8 -> int32
  {
    set_2x2x2_data();
    execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,0) == 23,     "mmacc.w.b C[0,0]");
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,1) == -389,   "mmacc.w.b C[0,1]");
    CHECK(AMU.acc_regs[0].elt<int32_t>(1,0) == 903,    "mmacc.w.b C[1,0]");
    CHECK(AMU.acc_regs[0].elt<int32_t>(1,1) == -16141, "mmacc.w.b C[1,1]");
    CHECK(AMU.get_xmsat() == 0, "mmacc.w.b xmsat==0");
  }

  // mmaccu.w.b: uint8 x uint8 -> int32
  {
    set_2x2x2_data();
    execute_mmaccu_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACCU_W_B));
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,0) == 64279, "mmaccu.w.b C[0,0]");
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,1) == 65147, "mmaccu.w.b C[0,1]");
    CHECK(AMU.acc_regs[0].elt<int32_t>(1,0) == 32903, "mmaccu.w.b C[1,0]");
    CHECK(AMU.acc_regs[0].elt<int32_t>(1,1) == 48883, "mmaccu.w.b C[1,1]");
    CHECK(AMU.get_xmsat() == 0, "mmaccu.w.b xmsat==0");
  }

  // mmaccus.w.b: uint8 ms1 x int8 ms2 -> int32
  {
    set_2x2x2_data();
    execute_mmaccus_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACCUS_W_B));
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,0) == -745,  "mmaccus.w.b C[0,0]");
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,1) == -645,  "mmaccus.w.b C[0,1]");
    CHECK(AMU.acc_regs[0].elt<int32_t>(1,0) == 135,   "mmaccus.w.b C[1,0]");
    CHECK(AMU.acc_regs[0].elt<int32_t>(1,1) == -16397,"mmaccus.w.b C[1,1]");
    CHECK(AMU.get_xmsat() == 0, "mmaccus.w.b xmsat==0");
  }

  // mmaccsu.w.b: int8 ms1 x uint8 ms2 -> int32
  {
    set_2x2x2_data();
    execute_mmaccsu_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACCSU_W_B));
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,0) == -489,   "mmaccsu.w.b C[0,0]");
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,1) == -133,   "mmaccsu.w.b C[0,1]");
    CHECK(AMU.acc_regs[0].elt<int32_t>(1,0) == -31865, "mmaccsu.w.b C[1,0]");
    CHECK(AMU.acc_regs[0].elt<int32_t>(1,1) == -16397, "mmaccsu.w.b C[1,1]");
    CHECK(AMU.get_xmsat() == 0, "mmaccsu.w.b xmsat==0");
  }

  // ================================================================
  // 11.3  Final-only saturation / cancellation (mmacc.w.b only)
  // ================================================================
  {
    set_112(); AMU.xmsaten->write_raw(1); AMU.xmsat->write_raw(0); reset_regs();
    AMU.acc_regs[0].elt<int32_t>(0,0) = INT32_MAX;
    AMU.tile_regs[0].elt<int8_t>(0,0) = 100;
    AMU.tile_regs[0].elt<int8_t>(0,1) = 100;
    AMU.tile_regs[1].elt<int8_t>(0,0) = 1;
    AMU.tile_regs[1].elt<int8_t>(0,1) = -1;

    execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,0) == INT32_MAX, "cancellation C==INT32_MAX");
    CHECK(AMU.get_xmsat() == 0, "cancellation xmsat==0");
  }

  // ================================================================
  // 11.4  Positive / negative / unsigned saturation
  // ================================================================
  // signed positive saturation
  {
    set_111(); AMU.xmsaten->write_raw(1); AMU.xmsat->write_raw(0); reset_regs();
    AMU.acc_regs[0].elt<int32_t>(0,0) = INT32_MAX;
    AMU.tile_regs[0].elt<int8_t>(0,0) = 127;
    AMU.tile_regs[1].elt<int8_t>(0,0) = 127;

    execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,0) == INT32_MAX, "pos-sat C==INT32_MAX");
    CHECK((AMU.get_xmsat() & 1) == 1, "pos-sat xmsat==1");
  }

  // signed negative saturation
  {
    set_111(); AMU.xmsaten->write_raw(1); AMU.xmsat->write_raw(0); reset_regs();
    AMU.acc_regs[0].elt<int32_t>(0,0) = INT32_MIN;
    AMU.tile_regs[0].elt<int8_t>(0,0) = -128;
    AMU.tile_regs[1].elt<int8_t>(0,0) = 127;

    execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,0) == INT32_MIN, "neg-sat C==INT32_MIN");
    CHECK((AMU.get_xmsat() & 1) == 1, "neg-sat xmsat==1");
  }

  // unsigned target-range proof
  {
    set_111(); AMU.xmsaten->write_raw(1); AMU.xmsat->write_raw(0); reset_regs();
    AMU.acc_regs[0].elt<int32_t>(0,0) = INT32_MAX;
    AMU.tile_regs[0].elt<uint8_t>(0,0) = 1;
    AMU.tile_regs[1].elt<uint8_t>(0,0) = 1;

    execute_mmaccu_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACCU_W_B));
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,0) == INT32_MAX, "unsigned target C==INT32_MAX");
    CHECK((AMU.get_xmsat() & 1) == 1, "unsigned target xmsat==1");
  }

  // ================================================================
  // 11.5  Wrap (xmsaten=0)
  // ================================================================
  // positive overflow wrap
  {
    set_111(); AMU.xmsaten->write_raw(0); AMU.xmsat->write_raw(0); reset_regs();
    AMU.acc_regs[0].elt<int32_t>(0,0) = INT32_MAX;
    AMU.tile_regs[0].elt<int8_t>(0,0) = 127;
    AMU.tile_regs[1].elt<int8_t>(0,0) = 127;

    execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    int32_t result = AMU.acc_regs[0].elt<int32_t>(0,0);
    uint32_t raw;
    std::memcpy(&raw, &result, sizeof(raw));
    // exact = 2147499776 = 0x80003F00 -> low32 = 0x80003F00 -> int32 = -2147467520
    CHECK(raw == 0x80003F00u, "wrap-pos raw32==0x80003F00");
    CHECK(result == -2147467520, "wrap-pos written int32==-2147467520");
    CHECK(AMU.get_xmsat() == 0, "wrap-pos xmsat==0");
  }

  // negative overflow wrap
  {
    set_111(); AMU.xmsaten->write_raw(0); AMU.xmsat->write_raw(0); reset_regs();
    AMU.acc_regs[0].elt<int32_t>(0,0) = INT32_MIN;
    AMU.tile_regs[0].elt<int8_t>(0,0) = -128;
    AMU.tile_regs[1].elt<int8_t>(0,0) = 127;

    execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    int32_t result = AMU.acc_regs[0].elt<int32_t>(0,0);
    uint32_t raw;
    std::memcpy(&raw, &result, sizeof(raw));
    // exact = -2147499904 = 0xFFFFFFFF7FFFC080 -> low32 = 0x7FFFC080 -> int32 = 2147467392
    CHECK(raw == 0x7FFFC080u, "wrap-neg raw32==0x7FFFC080");
    CHECK(result == 2147467392, "wrap-neg written int32==2147467392");
    CHECK(AMU.get_xmsat() == 0, "wrap-neg xmsat==0");
  }

  // ================================================================
  // 11.6  xmsat sticky
  // ================================================================
  {
    // Step 1: real saturation -> xmsat=1
    set_111(); AMU.xmsaten->write_raw(1); AMU.xmsat->write_raw(0); reset_regs();
    AMU.acc_regs[0].elt<int32_t>(0,0) = INT32_MAX;
    AMU.tile_regs[0].elt<int8_t>(0,0) = 127;
    AMU.tile_regs[1].elt<int8_t>(0,0) = 127;
    execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    CHECK((AMU.get_xmsat() & 1) == 1, "sticky step1 xmsat==1");

    // Step 2: normal result, don't clear xmsat -> stays 1
    AMU.acc_regs[0].elt<int32_t>(0,0) = 0;
    AMU.tile_regs[0].elt<int8_t>(0,0) = 1;
    AMU.tile_regs[1].elt<int8_t>(0,0) = 1;
    execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,0) == 1, "sticky step2 C==1");
    CHECK((AMU.get_xmsat() & 1) == 1, "sticky step2 xmsat==1");

    // Step 3: wrap, xmsaten=0, should NOT clear xmsat
    AMU.xmsaten->write_raw(0);
    AMU.acc_regs[0].elt<int32_t>(0,0) = INT32_MAX;
    AMU.tile_regs[0].elt<int8_t>(0,0) = 127;
    AMU.tile_regs[1].elt<int8_t>(0,0) = 127;
    execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    CHECK((AMU.get_xmsat() & 1) == 1, "sticky step3 xmsat==1");

    // Step 4: software clears xmsat -> normal result keeps 0
    AMU.xmsat->write_raw(0);
    AMU.xmsaten->write_raw(1);
    AMU.acc_regs[0].elt<int32_t>(0,0) = 0;
    AMU.tile_regs[0].elt<int8_t>(0,0) = 1;
    AMU.tile_regs[1].elt<int8_t>(0,0) = 1;
    execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,0) == 1, "sticky step4 C==1");
    CHECK(AMU.get_xmsat() == 0, "sticky step4 xmsat==0");
  }

  // ================================================================
  // 11.7  Inactive zeroing
  // ================================================================
  {
    AMU.set_mtilem(1); AMU.set_mtilen(1); AMU.set_mtilek(1);
    AMU.xmsaten->write_raw(0); AMU.xmsat->write_raw(0);
    reset_regs();
    // Fill entire acc0 with non-zero
    const reg_t acc_cols = AMU.acc_regs[0].get_row_bytes() / sizeof(int32_t);
    for (reg_t i = 0; i < AMU.get_rownum(); ++i)
      for (reg_t j = 0; j < acc_cols; ++j)
        AMU.acc_regs[0].elt<int32_t>(i, j) = 0xDEADBEEF;

    AMU.tile_regs[0].elt<int8_t>(0,0) = 3;
    AMU.tile_regs[1].elt<int8_t>(0,0) = 7;
    AMU.acc_regs[0].elt<int32_t>(0,0) = 0;

    execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    CHECK(AMU.acc_regs[0].elt<int32_t>(0,0) == 21, "inactive C[0,0]==21");

    // Check all other elements are zero
    for (reg_t i = 0; i < AMU.get_rownum(); ++i) {
      for (reg_t j = 0; j < acc_cols; ++j) {
        if (i >= 1 || j >= 1) {
          int32_t v = AMU.acc_regs[0].elt<int32_t>(i, j);
          CHECK(v == 0, "inactive element zero");
        }
      }
    }
  }

  // ================================================================
  // 11.8  Legality
  // ================================================================
  auto safe_state = [&]() {
    set_222(); AMU.xmsaten->write_raw(0); AMU.xmsat->write_raw(0); reset_regs();
    AMU.ELEN = 64;
    AMU.xmisa_features |= ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMI8I32);
  };
  auto expect_illegal = [&](const char* n, auto fn) {
    bool caught = false;
    try { fn(); } catch (const trap_illegal_instruction&) { caught = true; }
    CHECK(caught, n);
  };

  // md=0 illegal
  safe_state();
  expect_illegal("md=0", [&](){
    execute_mmacc_w_b(p, insn_t(MATCH_MMACC_W_B | (0u<<7) | (0u<<15) | (1u<<20)));
  });

  // md=3 illegal
  safe_state();
  expect_illegal("md=3", [&](){
    execute_mmacc_w_b(p, insn_t(MATCH_MMACC_W_B | (3u<<7) | (0u<<15) | (1u<<20)));
  });

  // md=8 illegal
  safe_state();
  expect_illegal("md=8", [&](){
    execute_mmacc_w_b(p, insn_t(MATCH_MMACC_W_B | (8u<<7) | (0u<<15) | (1u<<20)));
  });

  // ms1=4 illegal
  safe_state();
  expect_illegal("ms1=4", [&](){
    execute_mmacc_w_b(p, insn_t(MATCH_MMACC_W_B | (4u<<7) | (4u<<15) | (1u<<20)));
  });

  // ms2=4 illegal
  safe_state();
  expect_illegal("ms2=4", [&](){
    execute_mmacc_w_b(p, insn_t(MATCH_MMACC_W_B | (4u<<7) | (0u<<15) | (4u<<20)));
  });

  // Feature MMI8I32 off
  safe_state();
  { reg_t s = AMU.xmisa_features;
    AMU.xmisa_features &= ~ameUnit_t::xmisa_bit(ameUnit_t::XMISA_BIT_MMI8I32);
    expect_illegal("MMI8I32 off", [&](){
      execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    });
    AMU.xmisa_features = s; }

  // ELEN < 32
  safe_state();
  { reg_t s = AMU.ELEN; AMU.ELEN = 16;
    expect_illegal("ELEN<32", [&](){
      execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    });
    AMU.ELEN = s; }

  // M > ROWNUM
  safe_state();
  { AMU.set_mtilem(5);
    expect_illegal("M>4", [&](){
      execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    });
    AMU.set_mtilem(2); }

  // N > ROWNUM
  safe_state();
  { AMU.set_mtilen(5);
    expect_illegal("N>4", [&](){
      execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    });
    AMU.set_mtilen(2); }

  // K > TRLEN/8 (TRLEN=128, so K > 16)
  safe_state();
  { AMU.set_mtilek(17);
    expect_illegal("K>16", [&](){
      execute_mmacc_w_b(p, mk_acc0_tr1_tr0(MATCH_MMACC_W_B));
    });
    AMU.set_mtilek(2); }

  if (g_errors == 0)
    std::cout << "4 normative integer mmacc instructions passed\n";
  else {
    std::cout << g_fail_log.str();
    std::cout << g_errors << " FAILURE(S)\n";
  }
  return g_errors;
}
