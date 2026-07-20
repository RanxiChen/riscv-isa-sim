// mlate64 tr, (rs1), rs2
AME_MATRIX_LOAD_A_E64_TRANSPOSED
({
  Rij = Memory[base + j * stride + i * elementBytes];
})
