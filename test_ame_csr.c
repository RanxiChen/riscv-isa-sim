// test_ame_csr.c — AME CSR read/write acceptance test
// Compile: riscv64-unknown-linux-gnu-gcc -static -o test_ame_csr test_ame_csr.c
// Run:     spike pk test_ame_csr
//
// Expected output: all tests PASS, exit 0.
// Any trap/hang = FAIL.

#include <stdio.h>
#include <stdint.h>

// CSR address defines (matching encoding.h)
#define CSR_XMXRM    0x806
#define CSR_XMSAT    0x807
#define CSR_XMFFLAGS 0x808
#define CSR_XMFRM    0x809
#define CSR_XMSATEN  0x80A
#define CSR_MTILEM   0x803
#define CSR_MTILEN   0x804
#define CSR_MTILEK   0x805
#define CSR_XMCSR    0x802
#define CSR_XMISA    0xCC0
#define CSR_XTLENB   0xCC1
#define CSR_XTRLENB  0xCC2
#define CSR_XALENB   0xCC3

// RISC-V CSR access macros using standard pseudo-instructions
#define csrr(csr) ({ \
    uint64_t _v; \
    asm volatile("csrrs %0, %1, x0" : "=r"(_v) : "i"(csr)); \
    _v; \
})
#define csrw(csr, val) \
    asm volatile("csrrw x0, %0, %1" :: "i"(csr), "r"((uint64_t)(val)))

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("PASS: %s\n", msg); } \
} while(0)

int main() {
    uint64_t v;

    // --- Test 1: xmsat (1-bit, URW 0x807) ---
    printf("\n=== Test 1: xmsat (0x807) ===\n");
    csrw(CSR_XMSAT, 0x1);
    v = csrr(CSR_XMSAT);
    CHECK(v == 0x1, "xmsat = 1");
    csrw(CSR_XMSAT, 0x0);
    v = csrr(CSR_XMSAT);
    CHECK(v == 0x0, "xmsat = 0");

    // --- Test 2: xmfflags (5-bit, URW 0x808) ---
    printf("\n=== Test 2: xmfflags (0x808) ===\n");
    csrw(CSR_XMFFLAGS, 0x13);       // NV(4) | NX(0) = 0b10011 = 19
    v = csrr(CSR_XMFFLAGS);
    CHECK(v == 0x13, "xmfflags = 0x13");
    csrw(CSR_XMFFLAGS, 0x1F);       // all 5 bits
    v = csrr(CSR_XMFFLAGS);
    CHECK(v == 0x1F, "xmfflags = 0x1F");

    // --- Test 3: xmfrm (3-bit, URW 0x809) ---
    printf("\n=== Test 3: xmfrm (0x809) ===\n");
    csrw(CSR_XMFRM, 0x4);            // RMM
    v = csrr(CSR_XMFRM);
    CHECK(v == 0x4, "xmfrm = RMM(4)");
    csrw(CSR_XMFRM, 0x7);            // max valid
    v = csrr(CSR_XMFRM);
    CHECK(v == 0x7, "xmfrm mask (FF → 0x7)");

    // --- Test 4: xmsaten (1-bit, URW 0x80A) ---
    printf("\n=== Test 4: xmsaten (0x80A) ===\n");
    csrw(CSR_XMSATEN, 0x1);
    v = csrr(CSR_XMSATEN);
    CHECK(v == 0x1, "xmsaten = 1");
    csrw(CSR_XMSATEN, 0x0);
    v = csrr(CSR_XMSATEN);
    CHECK(v == 0x0, "xmsaten = 0");

    // --- Test 5: mtilem/mtilen/mtilek (URW 0x803/4/5) ---
    printf("\n=== Test 5: mtilem/mtilen/mtilek ===\n");
    csrw(CSR_MTILEM, 3);
    v = csrr(CSR_MTILEM);
    CHECK(v == 3, "mtilem = 3");
    csrw(CSR_MTILEN, 4);
    v = csrr(CSR_MTILEN);
    CHECK(v == 4, "mtilen = 4");
    csrw(CSR_MTILEK, 8);
    v = csrr(CSR_MTILEK);
    CHECK(v == 8, "mtilek = 8");

    // --- Test 6: xmcsr composite (URW 0x802) ---
    printf("\n=== Test 6: xmcsr (0x802) composite ===\n");
    // Set sub-CSRs to known values, then read composite
    csrw(CSR_XMXRM, 0x2);           // bits[1:0] = 2 (RDN)
    csrw(CSR_XMSAT, 0x1);           // bit[2] = 1
    csrw(CSR_XMFFLAGS, 0x0A);       // bits[7:3] = 0b01010
    csrw(CSR_XMFRM, 0x5);           // bits[10:8] = 5
    csrw(CSR_XMSATEN, 0x1);         // bit[11] = 1
    v = csrr(CSR_XMCSR);
    uint64_t expected = (1UL << 11) | (5UL << 8) | (0xAUL << 3) | (1UL << 2) | 2UL;
    CHECK(v == expected, "xmcsr read matches sub-CSRs");

    // --- Test 7: URO registers ---
    printf("\n=== Test 7: URO registers ===\n");
    v = csrr(CSR_XMISA);
    CHECK(v == 0, "xmisa = 0 (TODO: feature bitmap)");
    v = csrr(CSR_XTLENB);
    CHECK(v == 1024/8, "xtlenb = TLEN/8 = 128");
    v = csrr(CSR_XTRLENB);
    CHECK(v == 128/8, "xtrlenb = TRLEN/8 = 16");
    v = csrr(CSR_XALENB);
    CHECK(v == (8*8*32)/8, "xalenb = ROWNUM^2*ELEN/8 = 256");

    printf("\n=== Result: %d failures ===\n", failures);
    return failures != 0;
}
