// msate8 tr, (rs1), rs2 — A transposed store e8
AME_MATRIX_STORE_A_E8_TRANSPOSED
({
  Memory[base + j * stride + i * elementBytes] = Rij;
})
