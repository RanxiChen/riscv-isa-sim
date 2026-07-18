// mfmacc.s.bf16 md, ms2, ms1
AME_MFMACC_S_BF16_BF16
({
  Cij += vecdot(A.row(i), B.row(j));
})
