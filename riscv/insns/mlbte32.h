// mlbte32 tr, (rs1), rs2
AME_MATRIX_LOAD_B_E32_TRANSPOSED
({
  Rij = Memory[base + j * stride + i * elementBytes];
})
