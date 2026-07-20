// msate16 tr, (rs1), rs2
AME_MATRIX_STORE_A_E16_TRANSPOSED
({
  Memory[base + j * stride + i * elementBytes] = Rij;
})
