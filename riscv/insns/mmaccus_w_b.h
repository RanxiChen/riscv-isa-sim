// mmaccus.w.b md, ms2, ms1
// uint8 ms1 x int8 ms2 -> int32
AME_MMACC_I32_U8_I8
({
  Cij += vecdot(A.row(i), B.row(j));
})
