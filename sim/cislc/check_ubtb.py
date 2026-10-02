#!/usr/bin/env python3
"""Compare the Spike-side class to the O3 cocotb oracle at each cycle edge."""

import argparse
import random
import subprocess
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("runner", type=Path)
    parser.add_argument("o3_oracle", type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.o3_oracle.parent))
    from ubtb_model import Inputs, Train, UbtbModel

    oracle = UbtbModel(vaddr_bits=64, region_bytes=16,
                       entries=16, tag_bits=12, slots=8)
    rng = random.Random(257)
    cases = [Inputs(rst=True)]
    # Directed cases expose edge ordering, owner slot, saturation, replacement
    # and a folded-tag alias. Random cases exercise their combinations.
    base = 0x1000
    cases += [
        Inputs(query_valid=True, query_pc=base + 2),
        Inputs(query_valid=True, query_pc=base,
               train=Train(valid=True, pc=base, cfi_valid=True,
                           cfi_type=1, cfi_slot=3, target=0x8000)),
        Inputs(query_valid=True, query_pc=base + 8),
        Inputs(query_valid=True, query_pc=base, stall=True,
               train=Train(valid=True, pc=base, br_commit_mask=1 << 3)),
        Inputs(query_valid=True, query_pc=base),
        Inputs(rst=True),
        *[Inputs(train=Train(valid=True, pc=0x5000 + i * 16,
                             cfi_valid=True, cfi_type=2,
                             target=0x9000 + i * 16)) for i in range(17)],
        Inputs(query_valid=True, query_pc=0x5000),
        Inputs(rst=True),
        Inputs(train=Train(valid=True, pc=base, cfi_valid=True,
                           cfi_type=2, target=0xA000)),
        Inputs(query_valid=True, query_pc=base ^ (1 << 4) ^ (1 << 16)),
    ]
    choices = [0x2000 + i * 16 for i in range(32)]
    choices += [v ^ (1 << 4) ^ (1 << 16) for v in choices[:8]]
    for _ in range(1200):
        kind = rng.randrange(1, 4)
        slot = rng.randrange(8)
        cases.append(Inputs(
            rst=rng.random() < 0.02,
            query_valid=rng.random() < 0.8,
            query_pc=rng.choice(choices) + rng.randrange(8) * 2,
            stall=rng.random() < 0.12,
            train=Train(valid=rng.random() < 0.55,
                        pc=rng.choice(choices),
                        br_commit_mask=(1 << rng.randrange(8))
                        if rng.random() < 0.5 else 0,
                        br_taken_mask=1 << slot if kind == 1 else 0,
                        cfi_valid=rng.random() < 0.45,
                        cfi_slot=slot, cfi_type=kind,
                        ras_action=rng.randrange(3),
                        target=0x80000000 + rng.randrange(2048) * 2),
        ))

    def flatten(obs):
        hit, ready, p, lookup, hit_inc = obs
        return tuple(map(int, (hit, ready, p.region_base, p.entry_slot,
            p.br_mask, p.jal_mask, p.cfi_valid, p.cfi_slot, p.cfi_type,
            p.ras_action, p.raw_pred_taken, p.target_missing,
            p.target, p.next_pc, lookup, hit_inc)))

    expected, input_lines = [], []
    for case in cases:
        t = case.train
        input_lines.append(" ".join(map(str, map(int, (
            case.rst, case.query_valid, case.query_pc, case.stall,
            t.valid, t.pc, t.br_commit_mask, t.br_taken_mask,
            t.cfi_valid, t.cfi_slot, t.cfi_type, t.ras_action,
            t.target)))) + "\n")
        expected.append(flatten(oracle.visible(case)))
        oracle.tick(case)
        expected.append(flatten(oracle.visible(case)))

    run = subprocess.run([str(args.runner), "64", "16", "16", "12"],
                         input="".join(input_lines), text=True,
                         capture_output=True, check=True)
    observed = [tuple(map(int, line.split())) for line in run.stdout.splitlines()]
    assert len(observed) == len(expected), (len(observed), len(expected))
    for index, (got, want) in enumerate(zip(observed, expected)):
        assert got == want, (index // 2, "post" if index % 2 else "pre",
                             cases[index // 2], got, want)
    print(f"PASS: {len(cases)} cycles, pre/post edge")


if __name__ == "__main__":
    main()
