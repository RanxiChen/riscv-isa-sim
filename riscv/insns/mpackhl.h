// mpackhl md, ms1, ms2: high(ms1) + low(ms2)
AME_MPACK_LOOP
({
  const auto firstColumns = ame_extract_columns(ms1, halfCols, cols);
  const auto secondColumns = ame_extract_columns(ms2, 0, halfCols);
  ame_pack_columns(dst, firstColumns, secondColumns);
});
