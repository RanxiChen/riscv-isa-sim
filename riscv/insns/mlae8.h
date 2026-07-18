// mlae8 tr, (rs1), rs2 — A normal load e8
AME_MATRIX_LOAD_A_E8_NORMAL
({
  Rij = Memory[base + i * stride + j * elementBytes];
})
