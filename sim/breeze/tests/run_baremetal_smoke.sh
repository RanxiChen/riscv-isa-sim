#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 /path/to/spike [output-directory]" >&2
  exit 2
fi

spike_bin=$1
output_dir=${2:-/tmp/breeze-spike-baremetal-smoke}
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
elf="$output_dir/baremetal_smoke.elf"

mkdir -p -- "$output_dir"
riscv64-unknown-elf-gcc \
  -nostdlib -nostartfiles -static \
  -march=rv64gc -mabi=lp64 -mcmodel=medany \
  -Wl,-T,"$script_dir/baremetal_smoke.ld" \
  -o "$elf" "$script_dir/baremetal_smoke.S"

"$spike_bin" --isa=rv64gc "$elf"
echo "BAREMETAL_SMOKE_PASS: $elf"
