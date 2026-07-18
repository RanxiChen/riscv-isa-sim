// msate32 tr, (rs1), rs2
AME_MATRIX_STORE_A_E32_TRANSPOSED
({
  Memory[base + j * stride + i * elementBytes] = Rij;
})
