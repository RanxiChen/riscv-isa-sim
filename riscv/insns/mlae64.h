// mlae64 tr, (rs1), rs2
AME_MATRIX_LOAD_A_E64_NORMAL
({
  Rij = Memory[base + i * stride + j * elementBytes];
})
