// msbe16 tr, (rs1), rs2
AME_MATRIX_STORE_B_E16_NORMAL
({
  Memory[base + i * stride + j * elementBytes] = Rij;
})
