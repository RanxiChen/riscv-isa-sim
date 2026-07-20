// msbe64 tr, (rs1), rs2
AME_MATRIX_STORE_B_E64_NORMAL
({
  Memory[base + i * stride + j * elementBytes] = Rij;
})
