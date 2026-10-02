# CISLC-O3 module models in the Spike checkout

The uBTB here is an independent C++ implementation of the CISLC-O3 module
contract. It is deliberately separate from Spike's instruction execution and
the nullrvsim implementation. No Spike runtime hook, timing claim, or memory
interaction is enabled yet.

| Model | CISLC-O3 RTL commit | Separate cocotb verification commit |
| --- | --- | --- |
| uBTB | `257df8d62ed2b58d95b66194fa008d71f89a840b` | `2e25a3acb7d52b0d00a5d8177fa3e194be947578` |

The RTL commit, not the cocotb commit, is the behavioral version anchor.
`observe()` is a combinational read. `tick()` applies one rising edge. A query
before the edge sees old state; the same query after it sees trained state.

Build and check without building the full Spike executable:

```sh
cmake -S sim/cislc -B /tmp/cislc-spike-model-build
cmake --build /tmp/cislc-spike-model-build
python3 sim/cislc/check_ubtb.py \
  /tmp/cislc-spike-model-build/cislc_ubtb_runner \
  /path/to/CISLC-O3/sim/cocotb/ubtb/ubtb_model.py
```

The Python oracle is the one used by CISLC-O3 cocotb tests against RTL.
The Spike-side class is checked against the same public-port behavior and
can later be composed into a core model when the SoC RTL integration is ready.
