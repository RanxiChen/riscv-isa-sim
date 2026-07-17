// See LICENSE for license details.

#ifndef _RISCV_AME_UNIT_H
#define _RISCV_AME_UNIT_H

#include <array>
#include <cassert>
#include <cstdint>

#include "decode.h"
#include "csrs.h"

class processor_t;

// --- MatrixReg: one two-dimensional AME architectural matrix register ---
//
// MatrixReg owns a single register's storage.  It does not know whether it is
// tr* or acc*; the caller supplies total_bits and row_bits at reset time.
// Access is row-major and intentionally type-erased except for the caller's
// template parameter, matching vectorUnit_t::elt<T>()'s reinterpretation model.
class MatrixReg
{
public:
  MatrixReg() = default;
  MatrixReg(reg_t total_bits, reg_t row_bits) { reset(total_bits, row_bits); }
  ~MatrixReg();

  MatrixReg(const MatrixReg&) = delete;
  MatrixReg& operator=(const MatrixReg&) = delete;

  void reset(reg_t total_bits, reg_t row_bits);

  template<typename T> T* row_ptr(reg_t row) {
    assert(data != nullptr);
    assert(row < rownum);
    assert(row_bytes % sizeof(T) == 0);
    return reinterpret_cast<T*>(reinterpret_cast<char*>(data) + row * row_bytes);
  }

  template<typename T> T& elt(reg_t row, reg_t col) {
    assert(col < row_bytes / sizeof(T));
    return row_ptr<T>(row)[col];
  }

  reg_t get_total_bits() const { return total_bits; }
  reg_t get_row_bits() const { return row_bits; }
  reg_t get_total_bytes() const { return total_bytes; }
  reg_t get_row_bytes() const { return row_bytes; }
  reg_t get_rownum() const { return rownum; }

private:
  void *data = nullptr;
  reg_t total_bits = 0;
  reg_t row_bits = 0;
  reg_t total_bytes = 0;
  reg_t row_bytes = 0;
  reg_t rownum = 0;
};

class ameUnit_t
{
public:
  processor_t* p = nullptr;

  // --- Hardware constants (Chapter 2) ---
  reg_t ELEN = 0;    // max element width in bits
  reg_t TLEN = 0;    // tile register total bits
  reg_t TRLEN = 0;   // tile register bits per row
  reg_t ROWNUM = 0;  // logical rows = TLEN / TRLEN

  // --- Derived hardware constants, matching RVV's vlenb pattern ---
  reg_t TLENB = 0;   // tile register total bytes = TLEN / 8
  reg_t TRLENB = 0;  // tile register row bytes = TRLEN / 8
  reg_t ARLEN = 0;   // accumulation register row bits = ROWNUM * ELEN
  reg_t ALEN = 0;    // accumulation register total bits = ARLEN * ROWNUM
  reg_t ARLENB = 0;  // accumulation register row bytes = ARLEN / 8
  reg_t ALENB = 0;   // accumulation register total bytes = ALEN / 8

  // --- Architectural matrix register storage (Chapter 3.1) ---
  std::array<MatrixReg, 4> tile_regs;
  std::array<MatrixReg, 4> acc_regs;

  // --- xmisa feature bitmap (Chapter 3.2) ---
  //
  // xmisa is AME's local read-only capability declaration.  It mirrors the
  // role of standard misa as a feature bitmap, but does not replace or update
  // standard misa@0x301.  Bits stay clear until the corresponding instruction
  // families are implemented and checked.
  enum xmisa_feature_bit : unsigned {
    XMISA_BIT_MMI4I32   = 0,
    XMISA_BIT_MMI8I32   = 1,
    XMISA_BIT_MMF16F16  = 2,
    XMISA_BIT_MMF32F32  = 3,
    XMISA_BIT_MMF64F64  = 4,
    XMISA_BIT_MMF8F16   = 5,
    XMISA_BIT_MMF8BF16  = 6,  // PDF bit-5 duplicate resolved by table-order shift.
    XMISA_BIT_MMF16F32  = 7,
    XMISA_BIT_MMBF16F32 = 8,
    XMISA_BIT_MMF32F64  = 9,
    XMISA_BIT_MMF8F32   = 10
  };

  static constexpr reg_t xmisa_bit(unsigned bit) { return reg_t(1) << bit; }

  reg_t xmisa_features = 0;

  reg_t get_xmisa() const { return xmisa_features; }
  bool supports_xmisa_feature(reg_t feature_mask) const {
    return (xmisa_features & feature_mask) == feature_mask;
  }

  reg_t xmisa_mfic_mask() const;
  reg_t xmisa_mfew_mask() const;
  reg_t xmisa_miew_mask() const;

  matrix_csr_t_p xmisa = 0;

  // --- URW CSRs (Chapter 3) ---

  // mtilem/mtilen/mtilek — runtime tile shape, updated via msettile* instructions.
  matrix_csr_t_p mtilem = 0;
  matrix_csr_t_p mtilen = 0;
  matrix_csr_t_p mtilek = 0;

  // xmxrm — fixed-point rounding mode (2-bit, RNU/RNE/RDN/ROD).
  matrix_csr_t_p xmxrm = 0;

  // xmsat — fixed-point saturation sticky flag (1-bit).
  matrix_csr_t_p xmsat = 0;

  // xmfflags — floating-point exception flags (5-bit, NX/UF/OF/NV).
  matrix_csr_t_p xmfflags = 0;

  // xmfrm — floating-point rounding mode (3-bit, RNE/RTZ/RDN/RUP/RMM).
  matrix_csr_t_p xmfrm = 0;

  // xmsaten — saturation enable for int/fp8 MMA (1-bit).
  matrix_csr_t_p xmsaten = 0;

  // --- CSR semantic placeholders (Chapter 3.4-3.9) ---
  //
  // These names document CSR encodings for future instruction semantics.  They
  // intentionally do not implement rounding, saturation, exception accrual, or
  // invalid-mode traps yet.
  static constexpr reg_t XMXRM_MASK = 0x3;
  static constexpr reg_t XMSAT_MASK = 0x1;
  static constexpr reg_t XMFFLAGS_MASK = 0x1F;
  static constexpr reg_t XMFRM_MASK = 0x7;
  static constexpr reg_t XMSATEN_MASK = 0x1;

  enum xmxrm_rounding_mode : reg_t {
    XMXRM_RNU = 0,  // round-to-nearest-up
    XMXRM_RNE = 1,  // round-to-nearest-even
    XMXRM_RDN = 2,  // round-down / truncate
    XMXRM_ROD = 3   // round-to-odd
  };

  enum xmfrm_rounding_mode : reg_t {
    XMFRM_RNE = 0,
    XMFRM_RTZ = 1,
    XMFRM_RDN = 2,
    XMFRM_RUP = 3,
    XMFRM_RMM = 4
    // Values 5-7 are invalid by spec; future FP AME instructions will trap.
  };

  enum xmfflags_flag : reg_t {
    XMFFLAG_NX       = 1 << 0,
    XMFFLAG_UF       = 1 << 1,
    XMFFLAG_OF       = 1 << 2,
    XMFFLAG_RESERVED = 1 << 3,
    XMFFLAG_NV       = 1 << 4
  };

  bool xmfrm_is_valid(reg_t mode) const { return mode <= XMFRM_RMM; }
  bool xmsaten_enabled() { return (get_xmsaten() & XMSATEN_MASK) != 0; }
  bool xmsat_set() { return (get_xmsat() & XMSAT_MASK) != 0; }

  // --- Accessors (for use by AME instructions) ---
  reg_t get_xmxrm()   { return xmxrm->read(); }
  reg_t get_xmfrm()   { return xmfrm->read(); }
  reg_t get_xmsat()   { return xmsat->read(); }
  reg_t get_xmfflags(){ return xmfflags->read(); }
  reg_t get_xmsaten() { return xmsaten->read(); }
  xmxrm_rounding_mode get_xmxrm_mode() { return static_cast<xmxrm_rounding_mode>(get_xmxrm() & XMXRM_MASK); }
  xmfrm_rounding_mode get_xmfrm_mode() { return static_cast<xmfrm_rounding_mode>(get_xmfrm() & XMFRM_MASK); }
  reg_t get_tlen()    { return TLEN; }
  reg_t get_trlen()   { return TRLEN; }
  reg_t get_treln()   { return TRLEN; } // Backward-compatible typo alias.
  reg_t get_elen()    { return ELEN; }
  reg_t get_rownum()  { return ROWNUM; }
  reg_t get_tlenb()   { return TLENB; }
  reg_t get_trlenb()  { return TRLENB; }
  reg_t get_arlen()   { return ARLEN; }
  reg_t get_alen()    { return ALEN; }
  reg_t get_arlenb()  { return ARLENB; }
  reg_t get_alenb()   { return ALENB; }

  void set_mtilem(reg_t value);
  void set_mtilek(reg_t value);
  void set_mtilen(reg_t value);

  void reset();

  ameUnit_t() {}
  ~ameUnit_t() {}
};

// --- ame_mcsr_t: composite CSR packing 5 sub-CSRs (spec 3.4) ---
//
// xmcsr layout (bits):
//   11:       xmsaten
//   10:8      xmfrm
//   7:3       xmfflags
//   2:        xmsat
//   1:0       xmxrm
//
// This is a bespoke composite (composite_csr_t only supports 2 sub-CSRs).
//
class ame_mcsr_t : public csr_t
{
public:
  ame_mcsr_t(processor_t* const proc, const reg_t addr,
             matrix_csr_t_p xmxrm, matrix_csr_t_p xmsat,
             matrix_csr_t_p xmfflags, matrix_csr_t_p xmfrm,
             matrix_csr_t_p xmsaten);

  virtual void verify_permissions(insn_t insn, bool write) const override;
  virtual reg_t read() const noexcept override;

protected:
  virtual bool unlogged_write(const reg_t val) noexcept override;

private:
  matrix_csr_t_p _xmxrm;
  matrix_csr_t_p _xmsat;
  matrix_csr_t_p _xmfflags;
  matrix_csr_t_p _xmfrm;
  matrix_csr_t_p _xmsaten;

  // xmcsr field positions and masks (Chapter 3.4).
  static const unsigned XMXRM_LSB   = 0;
  static const unsigned XMSAT_LSB   = 2;
  static const unsigned XMFFLAGS_LSB = 3;
  static const unsigned XMFRM_LSB   = 8;
  static const unsigned XMSATEN_LSB = 11;

  static constexpr reg_t XMXRM_FIELD_MASK   = 0x3;
  static constexpr reg_t XMSAT_FIELD_MASK   = 0x1;
  static constexpr reg_t XMFFLAGS_FIELD_MASK = 0x1F;
  static constexpr reg_t XMFRM_FIELD_MASK   = 0x7;
  static constexpr reg_t XMSATEN_FIELD_MASK = 0x1;
};

#endif
