// msate64 tr, (rs1), rs2
AME_MATRIX_STORE_A_E64_TRANSPOSED
({
  Memory[base + j * stride + i * elementBytes] = Rij;
})
