#!/usr/bin/env python3
"""Deterministic interface and timing-trend checks; no CPU or payload model."""
import csv
import json
import pathlib
import subprocess
import sys
import tempfile

exe, config, root = map(pathlib.Path, sys.argv[1:])
root.mkdir(parents=True, exist_ok=True)
run = pathlib.Path(tempfile.mkdtemp(prefix='run-', dir=root))
small = run / 'small-queue.ini'
small.write_text(config.read_text().replace('trans_queue_size = 32', 'trans_queue_size = 4'))
results = {}
cases = [
    ('single', 'stream', 1, 1, 1, 2),
    ('serial', 'stream', 256, 1, 1, 2),
    ('parallel', 'stream', 256, 32, 4, 2),
    ('hit', 'row-hit', 64, 1, 1, 2),
    ('conflict', 'row-conflict', 64, 1, 1, 2),
    ('saturation', 'stream', 1024, 256, 16, 2),
    ('duplicate', 'duplicate', 64, 32, 4, 2),
    ('mixed', 'mixed', 128, 32, 4, 2),
    ('delay', 'stream', 1, 1, 1, 12),
]
for name, pattern, count, cap, width, delay in cases:
    out = run / name
    out.mkdir()
    cmd = [str(exe), str(small if name == 'saturation' else config), str(out),
           pattern, str(count), str(cap), str(width), str(delay)]
    with (out / 'stdout.log').open('w') as log:
        subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=60)
    summary = json.loads((out / 'summary.json').read_text())
    with (out / 'requests.csv').open() as f:
        rows = list(csv.DictReader(f))
    assert len(rows) == count and {int(r['id']) for r in rows} == set(range(count)), name
    assert summary['read_ready'] + summary['write_buffer_ack'] == count, name
    assert summary['peak_inflight'] <= cap, name
    for r in rows:
        assert int(r['callback_tick']) >= int(r['accepted_tick']), (name, r)
        assert int(r['response_tick']) == int(r['callback_tick']) + delay, (name, r)
        assert r['event'] == ('write_buffer_ack' if int(r['write']) else 'read_ready')
    if name in ('hit', 'conflict'):
        assert len({(r['channel'], r['rank'], r['bankgroup'], r['bank']) for r in rows}) == 1
        assert len({r['row'] for r in rows}) == (1 if name == 'hit' else 2)
    # Independent library counters must agree with adapter callback counts.
    stats = json.loads((out / 'dramsim3.json').read_text())
    channels = list(stats.values()) if isinstance(stats, dict) and 'num_reads_done' not in stats else [stats]
    assert sum(c['num_reads_done'] for c in channels) == summary['read_ready'], name
    assert sum(c['num_writes_done'] for c in channels) == summary['write_buffer_ack'], name
    if name == 'mixed':
        # Unique write addresses: ensure commands were actually issued during tail.
        assert sum(c['num_write_cmds'] for c in channels) == summary['write_buffer_ack'], name
    results[name] = summary
assert results['parallel']['completion_ticks'] < results['serial']['completion_ticks']
assert results['hit']['completion_ticks'] < results['conflict']['completion_ticks']
assert results['saturation']['backend_rejections'] > 0
assert results['serial']['master_cap_stall_ticks'] > 0
assert results['duplicate']['read_ready'] == 64
assert results['delay']['completion_ticks'] - results['single']['completion_ticks'] == 10
(run / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
print('PASS 9 scenarios:', run)
