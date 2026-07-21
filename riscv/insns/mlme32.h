// mlme32 md, (rs1) — whole matrix load e32
AME_MATRIX_WHOLE_LDST(uint32_t,
({
  Rij = MMU.load<uint32_t>(
      base + i * rowBytes + j * elementBytes);
}))
