# Running this package's tests

These tests exercise the standard library against the package as the compiler
resolves it, which means they run against the copy in `~/.owl/libs`, not against
the source tree next to this file.

That distinction is the whole difficulty. `~/.owl/libs/mire` currently points at
`mire@1.0.0` while this source tree is 1.2.0, so:

```
cd <this directory>
owl test
```

fails every test with `Package 'mire' has no export 'json'` (or `'bytes'`, or
`'std'`). Those are not test failures. They mean the tests linked the installed
1.0.0 and found none of the modules added since.

## Why a self-dependency does not fix it

The obvious fix is a self-dependency, so that `--lib-dir` points at this source
tree instead of the installed copy. Neither spelling works:

- `mire = { path = "." }` — every `--lib-dir` is a directory *of packages* and the
  compiler appends the package name, so this makes it look for `./mire` inside
  this project, which does not exist.
- `mire = { path = ".." }` — this points at the directory that does contain
  `mire/`, but the Avenys sandbox denies it. `pal_sbx_permit` rejects any path
  that resolves outside the project root, so `fs::exists("..")` is `false` and
  the dependency is reported as `path not found`.

Both are the sandbox working as intended. A package cannot point the compiler at
its own parent directory, and loosening that to accommodate a cache would let any
program write outside its tree.

## What actually works

Install the source tree as the active version, so the tests resolve it the same
way any other project would:

```
# from this directory
mire build code/mod.mire --debug -O 0 \
    --output-dir bin/debug --cache-dir bin/.cache \
    --runtime minimal --artifact shared \
    --c-source code/c/json_parser.c --c-lib m \
    --security-mode strict --extern 'rt_*' --extern 'owl_archive_*' \
    --extern-lib c --lib-dir "$HOME/.owl/libs"

# then make it the version the compiler resolves
ln -sfn "$PWD" ~/.owl/libs/mire@1.2.0
ln -sfn mire@1.2.0 ~/.owl/libs/mire

owl test
```

The symlink is what `owl install` is supposed to do for you. Until it can
(`install::package` is currently a stub that reports "skipped due to compiler
ownership issues"), the two `ln` commands are the manual equivalent. They point
at this directory, so the tests track the working tree.

## Test files

| File | Covers |
|---|---|
| `bytes_test.mire` | raw buffers, endianness, embedded NULs, `bytes::str_len` |
| `json_test.mire` | the JSON parser/serializer wrapper |
| `std_io.mire` | standard streams, with `std_io.sh` driving the shell side |

The suite is small. The bulk of the language's behaviour is covered by the
compiler's own tests in `../avenys` (`owl test` there runs 293 assertions), not
here.
