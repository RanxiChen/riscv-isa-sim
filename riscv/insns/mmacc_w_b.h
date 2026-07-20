// mmacc.w.b md, ms2, ms1
// int8 x int8 -> int32
AME_MMACC_I32_I8_I8
({
  Cij += vecdot(A.row(i), B.row(j));
})
