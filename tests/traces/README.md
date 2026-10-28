# Recorded executed-address traces

These files are produced by `s32ref --trace` / `--calls` while the original program runs
the attract mode and the `coin_start` control script. They contain **addresses only**,
not ROM bytes. `tools/recompile.py` uses them by default to add code that the static
analysis cannot find because it is reached through computed jumps (the coin/start path).

Regenerate them with the reference (after building `s32ref`):

```bash
build/bin/s32ref --timing frames --frames 1500 \
  --trace tests/traces/attract.txt --calls tests/traces/attract_calls.txt --rom rom
build/bin/s32ref --timing frames --script tests/controls/coin_start.txt \
  --trace tests/traces/coin_start.txt --calls tests/traces/coin_start_calls.txt --rom rom
```

The set is `spidman` (World); another revision would have different addresses.
