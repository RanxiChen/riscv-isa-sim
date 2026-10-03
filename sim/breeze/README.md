# Breeze model for Spike

This directory contains the first cycle-state modules for the **retained
blocking** Breeze D-cache, GShare predictor and single-hart pipeline shell.
The source baseline is Flow
`8ccdfb261dabdc79381cf66f7792bdabcf204695`; the relevant
`design/src/main/scala/{core,frontend,backend,cache,config}` files have no
tracked differences from the board-verified `bd10119` source. The new,
untracked `BreezePipelinedDCache.scala` is outside this model's scope.

| Model | Flow RTL counterpart | Contract |
| --- | --- | --- |
| `Predictor` | `frontend/BreezeBTB.scala`, `frontend/BreezePHT.scala`, `frontend/BreezeFrontend.scala`, `backend/BreezeBackend.scala` | BTB/PHT/GHR state and one-cycle delayed BTB update |
| `DCache` | `cache/BreezeDCache.scala`, `cache/BreezeCache.scala` | 64 sets, 4 ways, PLRU, blocking CPU hit/refill/eviction/upgrade state timing |
| `CoreModel` | `core/BreezeCore.scala`, `frontend/BreezeFrontend.scala`, `backend/BreezeBackend.scala` | One cycle per `tick()`, three-stage frontend delay, six-entry fetch buffer, single issue backend slots, EX branch resolution, blocking D-cache hold |

`DCache` deliberately abstracts Home service with `home_latency`: no
coherence probes, data values, MMIO, LR/SC/AMO, L2, bus, or DDR behavior is
claimed. It is suitable for early timing experiments only after a live
instruction/memory adapter is attached and its assumptions are checked.
The functional memory remains Spike's responsibility. `InstructionSource` is
queried online as the modeled frontend admits work; no full trace is required
or stored. The first shell uses supplied architectural instruction outcomes
and models wrong-path cost as redirect refill bubbles. It does not yet fetch
or execute speculative instructions, model I-cache/MMU/PTW, CSR dependencies,
FPU/MUL/DIV timing, atomics, or interrupts. Frontend delay and redirect refill
are explicit coarse assumptions, not claims of RTL cycle accuracy.

With `--breeze-model`, Spike's `sim_t::step()` advances this model one cycle
at a time. `SpikeSource` executes one functional instruction when the modeled
frontend requests it, and a Spike memory tracer supplies the first physical
data address. The ordinary Spike path is unchanged when the flag is absent.
This is live coupling, not a pre-generated fixed trace, but Spike's
architectural state advances at modeled fetch rather than modeled commit.
Consequently interrupts, MMIO, self-modifying code, precise faults and
speculative side effects are not faithful yet. A tohost exit can arrive while
modeled pipeline slots are still in flight, so the printed cycle total is a
preliminary estimate. Use it only for bounded bare-metal comparisons after
checking against the corresponding RTL run.

Example after building Spike:

```sh
spike --breeze-model --breeze-ghr=8 --breeze-btb=16 \
  --breeze-home-latency=6 program.elf
spike --breeze-model --breeze-no-gshare program.elf
```

The baseline mode uses the same functional program but disables GShare and
frontend S3 fast branch correction. `--breeze-home-latency` is a fixed Home
response assumption, **not** a bus or DDR timing model. It is intentionally
separate from the branch parameter sweep. The Spike `--instructions` limit is
rejected in this mode because it counts instructions in ordinary Spike and
cycles in this scheduler.

Build the independent contract check:

```sh
cmake -S sim/breeze -B /tmp/breeze-spike-model-build
cmake --build /tmp/breeze-spike-model-build
ctest --test-dir /tmp/breeze-spike-model-build --output-on-failure
```

After building the full Spike binary, run the small bare-metal online smoke
test (100-iteration branch loop, one load/store and a `tohost` exit):

```sh
bash sim/breeze/tests/run_smoke.sh /path/to/spike
```

This verifies exit parity with ordinary Spike and checks that the modeled
GShare path beats the no-predictor baseline on this directed workload. It is
not an FPGA performance validation.

The module is independent of `sim/cislc` (CISLC-O3) and `sim/ddr`.
