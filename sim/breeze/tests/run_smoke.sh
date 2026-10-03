#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/built/spike" >&2
  exit 2
fi

spike_bin=$1
cc_bin=${RISCV_GCC:-riscv64-linux-gnu-gcc}
test_dir=$(cd "$(dirname "$0")" && pwd)
build_dir=$(mktemp -d /tmp/breeze-spike-smoke.XXXXXX)
elf=$build_dir/smoke.elf

"$cc_bin" -nostdlib -static -no-pie -march=rv64gc -mabi=lp64 \
  -Wl,--build-id=none -Wl,-N -Wl,-Ttext=0x80000000 -Wl,-e,_start \
  -o "$elf" "$test_dir/smoke.S"

"$spike_bin" "$elf"
gshare_output=$("$spike_bin" --breeze-model "$elf" 2>&1)
baseline_output=$("$spike_bin" --breeze-model --breeze-no-gshare "$elf" 2>&1)
gshare_cycles=$(printf '%s\n' "$gshare_output" | sed -n 's/^Breeze coarse model: \([0-9][0-9]*\) core cycles$/\1/p')
baseline_cycles=$(printf '%s\n' "$baseline_output" | sed -n 's/^Breeze coarse model: \([0-9][0-9]*\) core cycles$/\1/p')
if [[ -z $gshare_cycles || -z $baseline_cycles ]]; then
  echo "missing Breeze cycle report" >&2
  exit 1
fi
if (( gshare_cycles >= baseline_cycles )); then
  echo "expected GShare cycles below baseline: $gshare_cycles vs $baseline_cycles" >&2
  exit 1
fi
printf 'plain Spike exit: PASS\nGShare cycles: %s\nBaseline cycles: %s\n' \
  "$gshare_cycles" "$baseline_cycles"
