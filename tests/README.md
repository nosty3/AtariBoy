# Host regression tests

These Linux tests compile production modules against small hardware stubs.
They generate synthetic ROM data in a simulated flash image; no game download
or proprietary ROM is required. GCC, G++ and Bash are needed. First run the
firmware build (or `python3 tools/build_atari.py /path/to/mads`) to generate headers.

```sh
bash tests/native/run62.sh
bash tests/native/optimize.sh
```

`run62.sh` covers filesystem access, browser/settings, battery saves, state
round-trips and state management using a synthetic ROM. The historical filename
is retained; this is a regression suite for the current source, not a separate
firmware release. `optimize.sh` covers disk reclamation, preserved data,
invalid-disk rejection and both current and original A8PicoCart FAT layouts.

Outputs are written to the ignored `verification/` directory. Test flash images
must not be committed. The tests use fixed-address `mmap` and are intended for
Linux hosts. They do not emulate RP2040 electrical timing or establish physical
Atari compatibility. Test the final firmware on the target hardware as well.
