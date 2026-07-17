// mfmacc.h md, ms2, ms1
// fp16 × fp16 -> fp16
AME_MFMACC_H
({
  Cij += vecdot(A.row(i), B.row(j));
})
