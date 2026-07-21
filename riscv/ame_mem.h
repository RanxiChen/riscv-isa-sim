// See LICENSE for license details.
#ifndef _RISCV_AME_MEM_H
#define _RISCV_AME_MEM_H

#include "ame_unit.h"
#include "decode.h"
#include "decode_macros.h"
#include "mmu.h"
#include "trap.h"

#include <cstdint>


#include "ame_macc.h"

// ── element type tokens ────────────────────────────────────────────────────
#define AME_MATRIX_ELEMENT_TYPE_E8  uint8_t
#define AME_MATRIX_ELEMENT_TYPE_E16 uint16_t
#define AME_MATRIX_ELEMENT_TYPE_E32 uint32_t
#define AME_MATRIX_ELEMENT_TYPE_E64 uint64_t

#define AME_MATRIX_ELEMENT_TYPE_(TOKEN) AME_MATRIX_ELEMENT_TYPE_##TOKEN
#define AME_MATRIX_ELEMENT_TYPE(TOKEN)  AME_MATRIX_ELEMENT_TYPE_(TOKEN)

// ── role shape macros ──────────────────────────────────────────────────────
#define AME_MATRIX_ROWS_A(AMU) ((AMU).mtilem->read())
#define AME_MATRIX_COLS_A(AMU) ((AMU).mtilek->read())

#define AME_MATRIX_ROWS_B(AMU) ((AMU).mtilen->read())
#define AME_MATRIX_COLS_B(AMU) ((AMU).mtilek->read())

#define AME_MATRIX_ROWS_C(AMU) ((AMU).mtilem->read())
#define AME_MATRIX_COLS_C(AMU) ((AMU).mtilen->read())

// ── helper: A/B are NOT acc; C IS acc ──────────────────────────────────────
#define A_IS_ACC 0
#define B_IS_ACC 0
#define C_IS_ACC 1

// ── single semantic layer: checks + shape + register + loop + BODY + end ──
#define AME_MATRIX_LDST(ROL, EW, BODY)                                     \
  do {                                                                     \
    ame_require(riscv_ame_assume_ms_enabled ||                             \
                (p->get_isa().has_any_matrix() &&                          \
                 STATE.sstatus->enabled(SSTATUS_MS)), insn);               \
    auto& AMU = p->AMU;                                                    \
    const reg_t rawReg = insn.m_md();                                      \
    if (ROL ## _IS_ACC) {                                                  \
      ame_require(rawReg >= 4 && rawReg < 8, insn);                        \
    } else {                                                               \
      ame_require(rawReg < 4, insn);                                       \
    }                                                                      \
    const reg_t rows = AME_MATRIX_ROWS_##ROL(AMU);                         \
    const reg_t cols = AME_MATRIX_COLS_##ROL(AMU);                         \
    const reg_t base = RS1;                                                \
    const reg_t stride = RS2;                                              \
    const reg_t elementBytes = sizeof(AME_MATRIX_ELEMENT_TYPE(EW));        \
    MatrixReg& R = ROL ## _IS_ACC                                          \
      ? AMU.acc_regs[rawReg - 4]                                           \
      : AMU.tile_regs[rawReg];                                             \
    ame_require(rows <= R.get_rownum(), insn);                             \
    ame_require(cols <= R.get_row_bytes() / elementBytes, insn);           \
    for (reg_t i = 0; i < rows; ++i) {                                     \
      for (reg_t j = 0; j < cols; ++j) {                                   \
        AME_MATRIX_ELEMENT_TYPE(EW)& Rij =                                 \
            R.elt<AME_MATRIX_ELEMENT_TYPE(EW)>(i, j);                       \
        BODY;                                                              \
      }                                                                    \
    }                                                                      \
    AME_END;                                                               \
  } while (0);

// ── whole-register load/store: physical rows, contiguous memory ─────────
#define AME_MATRIX_WHOLE_LDST(EW, BODY)                                    \
  do {                                                                     \
    require_matrix_ms;                                                     \
    auto& AMU = p->AMU;                                                    \
    const reg_t rawReg = insn.m_md();                                      \
    MatrixReg& R = rawReg < 4                                              \
      ? AMU.tile_regs[rawReg]                                              \
      : AMU.acc_regs[rawReg - 4];                                          \
    const reg_t rows = R.get_rownum();                                     \
    const reg_t rowBytes = R.get_row_bytes();                              \
    const reg_t elementBytes = sizeof(EW);                                 \
    ame_require(rowBytes % elementBytes == 0, insn);                       \
    const reg_t cols = rowBytes / elementBytes;                            \
    const reg_t base = RS1;                                                \
    for (reg_t i = 0; i < rows; ++i) {                                     \
      for (reg_t j = 0; j < cols; ++j) {                                   \
        EW& Rij = R.elt<EW>(i, j);                                         \
        BODY;                                                              \
      }                                                                    \
    }                                                                      \
    AME_END;                                                               \
  } while (0);

// ── execute wrapper declarations (48) ──────────────────────────────────────
#define AME_MATRIX_DECL_EXECUTE(NAME)                                          \
  void execute_##NAME(processor_t* p, insn_t insn);

// A normal load
AME_MATRIX_DECL_EXECUTE(mlae8)
AME_MATRIX_DECL_EXECUTE(mlae16)
AME_MATRIX_DECL_EXECUTE(mlae32)
AME_MATRIX_DECL_EXECUTE(mlae64)
// B normal load
AME_MATRIX_DECL_EXECUTE(mlbe8)
AME_MATRIX_DECL_EXECUTE(mlbe16)
AME_MATRIX_DECL_EXECUTE(mlbe32)
AME_MATRIX_DECL_EXECUTE(mlbe64)
// C normal load
AME_MATRIX_DECL_EXECUTE(mlce8)
AME_MATRIX_DECL_EXECUTE(mlce16)
AME_MATRIX_DECL_EXECUTE(mlce32)
AME_MATRIX_DECL_EXECUTE(mlce64)
// A normal store
AME_MATRIX_DECL_EXECUTE(msae8)
AME_MATRIX_DECL_EXECUTE(msae16)
AME_MATRIX_DECL_EXECUTE(msae32)
AME_MATRIX_DECL_EXECUTE(msae64)
// B normal store
AME_MATRIX_DECL_EXECUTE(msbe8)
AME_MATRIX_DECL_EXECUTE(msbe16)
AME_MATRIX_DECL_EXECUTE(msbe32)
AME_MATRIX_DECL_EXECUTE(msbe64)
// C normal store
AME_MATRIX_DECL_EXECUTE(msce8)
AME_MATRIX_DECL_EXECUTE(msce16)
AME_MATRIX_DECL_EXECUTE(msce32)
AME_MATRIX_DECL_EXECUTE(msce64)
// A transposed load
AME_MATRIX_DECL_EXECUTE(mlate8)
AME_MATRIX_DECL_EXECUTE(mlate16)
AME_MATRIX_DECL_EXECUTE(mlate32)
AME_MATRIX_DECL_EXECUTE(mlate64)
// B transposed load
AME_MATRIX_DECL_EXECUTE(mlbte8)
AME_MATRIX_DECL_EXECUTE(mlbte16)
AME_MATRIX_DECL_EXECUTE(mlbte32)
AME_MATRIX_DECL_EXECUTE(mlbte64)
// C transposed load
AME_MATRIX_DECL_EXECUTE(mlcte8)
AME_MATRIX_DECL_EXECUTE(mlcte16)
AME_MATRIX_DECL_EXECUTE(mlcte32)
AME_MATRIX_DECL_EXECUTE(mlcte64)
// A transposed store
AME_MATRIX_DECL_EXECUTE(msate8)
AME_MATRIX_DECL_EXECUTE(msate16)
AME_MATRIX_DECL_EXECUTE(msate32)
AME_MATRIX_DECL_EXECUTE(msate64)
// B transposed store
AME_MATRIX_DECL_EXECUTE(msbte8)
AME_MATRIX_DECL_EXECUTE(msbte16)
AME_MATRIX_DECL_EXECUTE(msbte32)
AME_MATRIX_DECL_EXECUTE(msbte64)
// C transposed store
AME_MATRIX_DECL_EXECUTE(mscte8)
AME_MATRIX_DECL_EXECUTE(mscte16)
AME_MATRIX_DECL_EXECUTE(mscte32)
AME_MATRIX_DECL_EXECUTE(mscte64)
// Whole load/store
AME_MATRIX_DECL_EXECUTE(mlme8)
AME_MATRIX_DECL_EXECUTE(mlme16)
AME_MATRIX_DECL_EXECUTE(mlme32)
AME_MATRIX_DECL_EXECUTE(mlme64)
AME_MATRIX_DECL_EXECUTE(msme8)
AME_MATRIX_DECL_EXECUTE(msme16)
AME_MATRIX_DECL_EXECUTE(msme32)
AME_MATRIX_DECL_EXECUTE(msme64)

#undef AME_MATRIX_DECL_EXECUTE

#endif
