// mlce64 tr, (rs1), rs2
AME_MATRIX_LOAD_C_E64_NORMAL
({
  Rij = Memory[base + i * stride + j * elementBytes];
})
