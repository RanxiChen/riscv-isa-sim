// See LICENSE for license details.
#include "ame_mem.h"
#include "processor.h"

// ── A normal load ──────────────────────────────────────────────────────────
#define DEF_LOAD_A(NUM)                                                         \
void execute_mlae##NUM(processor_t* p, insn_t insn) {                           \
  AME_MATRIX_LDST(A, E##NUM, {                                                \
    Rij = MMU.load<uint##NUM##_t>(base + i * stride + j * elementBytes);                        \
  });                                                                         \
}
DEF_LOAD_A(8)
DEF_LOAD_A(16)
DEF_LOAD_A(32)
DEF_LOAD_A(64)
#undef DEF_LOAD_A

// ── B normal load ──────────────────────────────────────────────────────────
#define DEF_LOAD_B(NUM)                                                         \
void execute_mlbe##NUM(processor_t* p, insn_t insn) {                           \
  AME_MATRIX_LDST(B, E##NUM, {                                                \
    Rij = MMU.load<uint##NUM##_t>(base + i * stride + j * elementBytes);                        \
  });                                                                         \
}
DEF_LOAD_B(8)
DEF_LOAD_B(16)
DEF_LOAD_B(32)
DEF_LOAD_B(64)
#undef DEF_LOAD_B

// ── C normal load ──────────────────────────────────────────────────────────
#define DEF_LOAD_C(NUM)                                                         \
void execute_mlce##NUM(processor_t* p, insn_t insn) {                           \
  AME_MATRIX_LDST(C, E##NUM, {                                                \
    Rij = MMU.load<uint##NUM##_t>(base + i * stride + j * elementBytes);                        \
  });                                                                         \
}
DEF_LOAD_C(8)
DEF_LOAD_C(16)
DEF_LOAD_C(32)
DEF_LOAD_C(64)
#undef DEF_LOAD_C

// ── A normal store ─────────────────────────────────────────────────────────
#define DEF_STORE_A(NUM)                                                        \
void execute_msae##NUM(processor_t* p, insn_t insn) {                           \
  AME_MATRIX_LDST(A, E##NUM, {                                                \
    MMU.store<uint##NUM##_t>(base + i * stride + j * elementBytes, Rij);                        \
  });                                                                         \
}
DEF_STORE_A(8)
DEF_STORE_A(16)
DEF_STORE_A(32)
DEF_STORE_A(64)
#undef DEF_STORE_A

// ── B normal store ─────────────────────────────────────────────────────────
#define DEF_STORE_B(NUM)                                                        \
void execute_msbe##NUM(processor_t* p, insn_t insn) {                           \
  AME_MATRIX_LDST(B, E##NUM, {                                                \
    MMU.store<uint##NUM##_t>(base + i * stride + j * elementBytes, Rij);                        \
  });                                                                         \
}
DEF_STORE_B(8)
DEF_STORE_B(16)
DEF_STORE_B(32)
DEF_STORE_B(64)
#undef DEF_STORE_B

// ── C normal store ─────────────────────────────────────────────────────────
#define DEF_STORE_C(NUM)                                                        \
void execute_msce##NUM(processor_t* p, insn_t insn) {                           \
  AME_MATRIX_LDST(C, E##NUM, {                                                \
    MMU.store<uint##NUM##_t>(base + i * stride + j * elementBytes, Rij);                        \
  });                                                                         \
}
DEF_STORE_C(8)
DEF_STORE_C(16)
DEF_STORE_C(32)
DEF_STORE_C(64)
#undef DEF_STORE_C

// ── A transposed load ──────────────────────────────────────────────────────
#define DEF_LOAD_AT(NUM)                                                        \
void execute_mlate##NUM(processor_t* p, insn_t insn) {                          \
  AME_MATRIX_LDST(A, E##NUM, {                                                \
    Rij = MMU.load<uint##NUM##_t>(base + j * stride + i * elementBytes);                        \
  });                                                                         \
}
DEF_LOAD_AT(8)
DEF_LOAD_AT(16)
DEF_LOAD_AT(32)
DEF_LOAD_AT(64)
#undef DEF_LOAD_AT

// ── B transposed load ──────────────────────────────────────────────────────
#define DEF_LOAD_BT(NUM)                                                        \
void execute_mlbte##NUM(processor_t* p, insn_t insn) {                          \
  AME_MATRIX_LDST(B, E##NUM, {                                                \
    Rij = MMU.load<uint##NUM##_t>(base + j * stride + i * elementBytes);                        \
  });                                                                         \
}
DEF_LOAD_BT(8)
DEF_LOAD_BT(16)
DEF_LOAD_BT(32)
DEF_LOAD_BT(64)
#undef DEF_LOAD_BT

// ── C transposed load ──────────────────────────────────────────────────────
#define DEF_LOAD_CT(NUM)                                                        \
void execute_mlcte##NUM(processor_t* p, insn_t insn) {                          \
  AME_MATRIX_LDST(C, E##NUM, {                                                \
    Rij = MMU.load<uint##NUM##_t>(base + j * stride + i * elementBytes);                        \
  });                                                                         \
}
DEF_LOAD_CT(8)
DEF_LOAD_CT(16)
DEF_LOAD_CT(32)
DEF_LOAD_CT(64)
#undef DEF_LOAD_CT

// ── A transposed store ─────────────────────────────────────────────────────
#define DEF_STORE_AT(NUM)                                                       \
void execute_msate##NUM(processor_t* p, insn_t insn) {                          \
  AME_MATRIX_LDST(A, E##NUM, {                                                \
    MMU.store<uint##NUM##_t>(base + j * stride + i * elementBytes, Rij);                        \
  });                                                                         \
}
DEF_STORE_AT(8)
DEF_STORE_AT(16)
DEF_STORE_AT(32)
DEF_STORE_AT(64)
#undef DEF_STORE_AT

// ── B transposed store ─────────────────────────────────────────────────────
#define DEF_STORE_BT(NUM)                                                       \
void execute_msbte##NUM(processor_t* p, insn_t insn) {                          \
  AME_MATRIX_LDST(B, E##NUM, {                                                \
    MMU.store<uint##NUM##_t>(base + j * stride + i * elementBytes, Rij);                        \
  });                                                                         \
}
DEF_STORE_BT(8)
DEF_STORE_BT(16)
DEF_STORE_BT(32)
DEF_STORE_BT(64)
#undef DEF_STORE_BT

// ── C transposed store ─────────────────────────────────────────────────────
#define DEF_STORE_CT(NUM)                                                       \
void execute_mscte##NUM(processor_t* p, insn_t insn) {                          \
  AME_MATRIX_LDST(C, E##NUM, {                                                \
    MMU.store<uint##NUM##_t>(base + j * stride + i * elementBytes, Rij);                        \
  });                                                                         \
}
DEF_STORE_CT(8)
DEF_STORE_CT(16)
DEF_STORE_CT(32)
DEF_STORE_CT(64)
#undef DEF_STORE_CT

// ── Whole matrix load ─────────────────────────────────────────────────────
#define DEF_WHOLE_LOAD(NUM)                                                   \
void execute_mlme##NUM(processor_t* p, insn_t insn) {                         \
  AME_MATRIX_WHOLE_LDST(uint##NUM##_t, ({                                    \
    Rij = MMU.load<uint##NUM##_t>(                                           \
        base + i * rowBytes + j * elementBytes);                             \
  }));                                                                       \
}
DEF_WHOLE_LOAD(8)
DEF_WHOLE_LOAD(16)
DEF_WHOLE_LOAD(32)
DEF_WHOLE_LOAD(64)
#undef DEF_WHOLE_LOAD

// ── Whole matrix store ────────────────────────────────────────────────────
#define DEF_WHOLE_STORE(NUM)                                                  \
void execute_msme##NUM(processor_t* p, insn_t insn) {                         \
  AME_MATRIX_WHOLE_LDST(uint##NUM##_t, ({                                    \
    MMU.store<uint##NUM##_t>(                                                \
        base + i * rowBytes + j * elementBytes, Rij);                        \
  }));                                                                       \
}
DEF_WHOLE_STORE(8)
DEF_WHOLE_STORE(16)
DEF_WHOLE_STORE(32)
DEF_WHOLE_STORE(64)
#undef DEF_WHOLE_STORE
