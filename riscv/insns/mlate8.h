// mlate8 tr, (rs1), rs2 — A transposed load e8
AME_MATRIX_LOAD_A_E8_TRANSPOSED
({
  Rij = Memory[base + j * stride + i * elementBytes];
})
