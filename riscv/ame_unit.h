// See LICENSE for license details.

#ifndef _RISCV_AME_UNIT_H
#define _RISCV_AME_UNIT_H

#include <cstdint>

#include "decode.h"
#include "csrs.h"

class processor_t;

class ameUnit_t
{
public:
  processor_t* p = nullptr;

  // --- Hardware constants (Chapter 2) ---
  reg_t ELEN = 0;   // max element width in bits
  reg_t TLEN = 0;   // tile register total bits
  reg_t TRLEN = 0;  // tile register bits per row
  reg_t ROWNUM = 0; // logical rows = TLEN / TRLEN

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

  // --- Accessors (for use by AME instructions) ---
  reg_t get_xmxrm()   { return xmxrm->read(); }
  reg_t get_xmfrm()   { return xmfrm->read(); }
  reg_t get_xmsat()   { return xmsat->read(); }
  reg_t get_xmfflags(){ return xmfflags->read(); }
  reg_t get_xmsaten() { return xmsaten->read(); }
  reg_t get_tlen()    { return TLEN; }
  reg_t get_treln()   { return TRLEN; }
  reg_t get_elen()    { return ELEN; }
  reg_t get_rownum()  { return ROWNUM; }

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
};

#endif
