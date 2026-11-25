# Running this package's tests

The tests run against this working tree, not against an installed copy, by
pointing `--lib-dir` at the directory that *contains* the `mire/` package:

```bash
# from the repository root
mire test tests --lib-dir "$PWD/.."
mire test tests/bytes_test.mire --lib-dir "$PWD/.."
```

`--lib-dir` names a directory of packages and the compiler appends `mire`, so
the parent of this repository is the correct value. This is also what
`tests/std_io.sh` does; it accepts `MIRE_LIB_DIR` to override the location when
the library is installed under `~/.owl/libs`.

## Why not a self-dependency

A `mire = { path = ".." }` entry in `owl.toml` looks equivalent, but the Avenys
sandbox rejects a dependency that resolves outside the project root, so the
dependency is reported as `path not found`. The flag keeps the resolution
explicit without loosening the sandbox. `mire = { path = "." }` does not work
either: the compiler appends `mire`, so it looks for `./mire` inside this
repository.

## Test files

| File | Covers |
|---|---|
| `bytes_test.mire` | raw buffers, endianness, embedded NULs, `bytes::str_len` |
| `std_io.mire` | standard streams; `std_io.sh` drives the shell side |

`std_io.sh` builds small probe programs into a scratch directory, wires stdin to
a controlled pipe, and compares stdout and stderr separately. That is the only
place NUL and CRLF handling is proven end to end, because a `@[test]` function
cannot capture its own streams.

```bash
tests/std_io.sh /path/to/mire
# or
MIRE=/path/to/mire tests/std_io.sh
```

## CI

`.github/workflows/ci.yml` builds Avenys from `migrate/pal-to-mire`, copies
`src/` and `owl.toml` into `~/.owl/libs/mire`, smoke-checks `mire::std` and
`mire::vec`, and runs `tests/std_io.sh`. The language itself is covered by the
compiler's own suite in `../avenys`; these files only cover this package.
