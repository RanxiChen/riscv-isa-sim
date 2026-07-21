// mlme64 md, (rs1) — whole matrix load e64
AME_MATRIX_WHOLE_LDST(uint64_t,
({
  Rij = MMU.load<uint64_t>(
      base + i * rowBytes + j * elementBytes);
}))
