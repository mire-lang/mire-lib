# Changelog

All notable changes to the mire standard library.

## [1.2.0] - 2026-09-30 (stdin module renamed to `in`, available() fixed)

### Changed

- **`mire::std::input` is now `mire::std::in`.** `in` is a keyword in the
  language, and the module was named `input` only to dodge that collision, which
  left the third standard stream reading as a category rather than a stream next
  to `out` and `err`. Avenys v4.3.2 ([PR #33](https://github.com/mire-lang/Avenys-rust/pull/33))
  accepts the `in` keyword as a module name, a load path segment and a call head,
  so the collision is gone and the module can be named for what it is.

  This is a breaking change to a public module, hence the minor bump. Nothing
  outside this repository's own tests used `mire::std`, and the module is a day
  old, so the cost is close to zero — but it is a rename, so it is not shipping
  as a patch.

  ```mire
  load mire::std::in

  pub fn main: () {
      set line = in::line()
      out::println(line)
  }
  ```

### Fixed

- **`in::available()` under-reported, so a program could block on a line that
  had already been read.** The runtime checked the OS for input with `poll` and
  never consulted its own buffer. Once `in::line()` had pulled a chunk off the
  descriptor, the buffered remainder was invisible: `available()` reported
  nothing, a reader that trusts it waits, and the bytes it is waiting for are
  already in the process. Availability is now answered from the buffer first,
  and the descriptor only when the buffer is empty. Reaching EOF also had to set
  the flag, or `available()` stayed optimistic forever after the input ended.

### Added

- **22 integration cases** in `tests/std_io.sh`, and
  `out::print_f64_no_newline` / `err::print_f64_no_newline` so a float can be
  written without a trailing newline as an integer already could.

### Removed

- The `input` -> `in` workaround in the CHANGELOG's design notes, and with it the
  claim that the flat aliases needed a local to build. They never did. The
  one-line delegations compile as written; the note described a compiler bug
  that was not reproducible, and the locals it recommended are gone.

### Requirements

- Avenys `v4.3.2` (PR #33) for `rt_io_*` and for the `in` keyword as a name.
  The `dasu` flush fix in the same release is what makes a `dasu` and a stream
  write to one destination interleave in call order.

## [1.1.0] - 2026-09-30 (standard streams)

### Added

- **`mire::std`, the three standard streams.** `load mire::std::out`,
  `load mire::std::err` and `load mire::std::in` expose stdout, stderr and
  stdin as a library module rather than leaving every library to reach for libc:

  ```mire
  load mire::std::out
  load mire::std::err
  load mire::std::in

  pub fn main: () {
      set line = in::line()
      out::print("got: ")
      out::println(line)
      err::println("done")
  }
  ```

  `load mire::std` pulls in all three children for code that prefers the flat
  names (`std::print`, `std::read_line`, `std::flush`, ...). The flat names are
  the same functions, not copies, so the two styles cannot drift apart.

  - `out` / `err`: `print`, `println`, `write_n`, `print_i64`,
    `print_i64_no_newline`, `print_f64`, `print_f64_no_newline`, `flush`,
    `failed`, `clear`, `fd`.
  - `in`: `line`, `byte`, `bytes`, `all`, `available`, `is_tty`.

- **`tests/std_io.sh`, 22 integration cases.** The unit tests in
  `tests/std_io.mire` run inside the harness, so they cannot see the process's
  own streams. This script builds probes against the working tree and asserts on
  what actually reaches the file descriptors, covering: binary round-trip
  byte-exactness with embedded NULs, CRLF handling, `available()` reporting
  buffered bytes rather than hiding them, EOF through every entry point, a NUL
  read as `0` while the end of input reads as `-1`, the two output streams
  carrying their own content and nothing else, byte counts that include the
  newline, stdout and stderr interleaving in call order when they share a
  destination, and `is_tty()` answering for a pipe.

  It resolves the library from the checkout it lives in rather than a global
  install, so it tests the working tree.

- **26 unit tests** in `tests/std_io.mire` covering byte counts, the two output
  streams being genuinely distinct, CRLF handling, and EOF-after-drain through
  both the namespaced and the flat entry points.

### Fixed

- **`in::available()` under-reported, so a program could block on a line that
  had already been read.** The runtime checked the OS for input with `poll` and
  never consulted its own buffer. Once `in::line()` had pulled a chunk off the
  descriptor, the buffered remainder was invisible: `available()` reported
  nothing, a reader that trusts it waits, and the bytes it is waiting for are
  already in the process. Availability is now answered from the buffer first,
  and the descriptor only when the buffer is empty. Reaching EOF also had to set
  the flag, or `available()` stayed optimistic forever after the input ended.

### Design notes

- **The module is `in`, matching the `for`/`find` iterator.** `in` is a keyword,
  so this needed the compiler to accept the keyword as a name in three places:
  the module declaration, the load path, and the head of an expression. It does
  in Avenys `v4.3.2` (PR #33). The alternative was `input`, and it was rejected
  for reading as a category rather than a stream, next to `out` and `err`.
- **Writes go through the C `FILE*`, not the raw descriptor.** A program that
  writes from both a `printf` and a stream write otherwise gets output
  interleaved in the wrong order, because the two paths disagree about where the
  file position is. Routing both through stdio makes them interleave in call
  order. This only works because `dasu` flushes after every call, so stdout is
  never sitting in a buffer while stderr — unbuffered — goes straight out.
- **Writes return the byte count, not `0`/`1`.** A caller that cares about a
  short write — a log line, a pipe — otherwise cannot detect one, and finds out
  later as a truncated line.
- **Reads are binary-safe and never truncate at a NUL.** `bytes(n)` returns
  exactly `n` bytes unless input ends first, `all()` returns what is left, and
  `byte()` returns `-1` at end of input, so EOF is distinguishable from a zero
  byte. Lines drop the trailing newline and a preceding `CR`, so CRLF input does
  not leave a stray `\r` on the value.
- **Requires `rt_io_*` from the compiler runtime** (Avenys `v4.3.2`,
  PR #33). The symbols are `rt_`-prefixed, so the `externs = ["rt_*"]`
  allowlist in `[security]` already covers them and strict mode needs no change.
  It also requires the `dasu` flush fix from the same release: a `dasu` and a
  stream write to one destination interleave in call order only if `dasu`
  actually flushes.

## [1.0.0] - 2026-09-22 (first stable release)

### Added

- Stable macros: `assert_eq!`, `assert_ne!`, `trace_i64!` in `core/macros/checks`.
- Updated `core/vec` and `core/mod` with latest fixes.

### Changed

- Bumped library version to 1.0.0.
- Updated `owl.toml` dependencies and macros sections.

## [0.0.7] - 2026-08-05 (strict security mode)

### Changed

- **Manifest enables `mode = "strict"`** (`owl.toml` `[security]`): mire's core
  uses only `rt_*` externs (declared `lib "c"`), so `externs = ["rt_*"]` and
  `extern_libs = ["c"]` cover every helper. All shipped macros (`assert`, `dbg`,
  `panic`, `unreachable`) are allowlisted, so projects depending on mire keep
  working when they also enable strict mode. No library source changed.

## [0.0.6] - 2026-08-01 (map/vec ownership alignment)

### Changed

- **`mire::map` read-only functions take `&anything`**: `len`, `has`,
  `is::empty`, `get::str/i64`, `keys`, `values::i64`, `entries`, `count`,
  `remove` no longer require a concrete `&map[str anything]` type, so passing a
  map does not move it. `set::str/i64` and `merge` now **return the map**
  (the runtime may reallocate the backing storage — same contract as
  `vec::push`). Added `map::count` (alias for `entries`). Keys and str values
  are declared `&str` because the runtime copies them internally.
- **`mire::vec`**: added `set::i64(v, index, value)` (in-place element write via
  `rt_vecs_set_i64`). `get::str` now takes `&vec[str]` (previously `vec[str]` by
  value, which caused "expects Vector, got Ref" at kioto CLI call sites).
- **`mire::str::from::bool`** now takes `:bool` (was `:i64`) and emits
  `"true"`/`"false"` via `rt_bool_to_string`.
- Updated the Kioto compatibility reference to the current `2.4.3` release.

## [0.0.5] - 2026-07-28 (Runtime ABI alignment)

### Fixed

- Corrected `str::pad::left/right` to accept `&str`, matching the current
  runtime ABI instead of passing a scalar `u8` as a pointer.
- Switched `vec::join` to the canonical `rt_strings_join_list` runtime symbol;
  the old list-prefixed symbol remains only as an Avenys compatibility alias.
- Removed the misleading aggregate-entrypoint claim; consumers load exported
  modules explicitly with `load mire::<module>`.
- Synchronized the package and README version with the current release.
- Updated the Kioto compatibility reference to the current `2.4.1` release.

## [0.0.4] - 2026-07-27 (Kioto 2.4.0 compat)

### Changed

- **Kioto 2.4.1** now required for `vec[str]` → `const char **argv`
  marshaling and the new blocking `proc.spawn(cmd, args)` API.
- `mire::fs` module now uses `pal_dir_next_name` instead of the broken
  `pal_dir_next` FFI declaration. Directory iteration is now safe (no
  struct-return ABI mismatch).
- `mire::proc::spawn(cmd, args)` — now executes without shell (uses
  `pal_proc_create` + `PAL_SPAWN_WAIT`). `args` vector is correctly
  passed as individual argv elements.
- `mire::proc::wait(p)` — now uses handle-based `pal_proc_wait(p.handle)`
  instead of the broken PID-based `pal_proc_wait_pid`.
- `mire::fs::join(dir, name, ext)` — now uses `rt_string_concat` instead
  of shadowed `concat` builtin.
- `mire::fs::dir`, `mire::fs::name`, `mire::fs::ext` — fixed `concat`/`substr`
  builtin shadowing by importing all Kioto modules.

## [0.0.3] - 2026-07-26 (Maybe unwrap::or + Section Comments)

### Added

- **`mire::maybe`** — Added `unwrap::or::i64/str/f64/ptr` nested group for unwrap-with-default.
  All 4 C runtime functions (`rt_maybe_unwrap_or_*`) were already declared but had no
  public API. Added section comments for consistency with other modules.

### Changed

- Verified all 6 modules (vec, map, str, arr, result, maybe) compile and link correctly
  with nested function grouping. No parser bug exists — type keywords (`i64`, `str`, etc.)
  are tokenized as `Ident` by the lexer, so `is_member_name_token` handles them correctly.

## [0.0.2] - 2026-07-26 (Nested Function Grouping)

### Changed

All modules rewritten with the new nested function grouping syntax (`parent::child`
via `pub fn parent: () { pub fn child: ... }`). Every existing function is preserved.

- **`mire::map`** — `get::str/i64`, `set::str/i64`, `is::empty`, `values::i64` now
  use nested grouping. `len`, `has`, `keys`, `remove`, `entries`, `merge` remain standalone.
- **`mire::vec`** — `push::i64/str`, `pop::i64`, `get::i64/str`, `first::i64`, `last::i64`,
  `contains::i64`, `index::i64` now use nested grouping. `len`, `remove`, `clear`, `sort`,
  `reverse`, `unique`, `slice`, `flatten`, `concat`, `join` remain standalone.
- **`mire::arr`** — `first::i64`, `last::i64`, `contains::i64`, `index::i64`, `reverse::i64`
  now use nested grouping. `len`, `join` remain standalone.
- **`mire::str`** — `starts::with`, `ends::with`, `pad::left/right`, `from::i64/bool/f64`,
  `to::i64` now use nested grouping. Added `is::empty`. `replace` stays standalone with
  `replace::first` as flat 3-level.
- **`mire::result`** — `ok::i64/str/ptr`, `err::i64/str/ptr/payload`, `is::ok/err`,
  `unwrap::i64/str/f64/ptr/err::str` now use nested grouping. `unwrap::*::or` stays flat.
- **`mire::maybe`** — already migrated in previous commit.

### Added

- `str::is::empty` — returns true if string has zero length

### Removed

- `io` and `math` module exports from `owl.toml` (deprecated since v3.24.2)

## [0.0.1] - 2026-07-21

### Added

- `mire::vec` — vector operations: push, pop, get, remove, clear, concat, sort, reverse, contains, index, slice, flatten, unique, len
- `mire::map` — map/dict operations: get, set, has, keys, values, remove, merge, len
- `mire::str` — string operations: len, upper, lower, trim, replace, contains, starts_with, ends_with, split, join, substr, repeat, index, from_i64, to_i64
- `mire::maybe` — optional type: some, none, is_some, is_none, unwrap, unwrap_or, free
- `mire::result` — result type: ok, err, is_ok, is_err, unwrap, unwrap_or, free
- `mire::arr` — fixed-size array operations: len, first, last, contains, index_of, reverse, join
## Unreleased

- Added native Mire assertion helpers `assert_eq!`, `assert_ne!`, and
  `trace_i64!`, available through the strict macro allowlist.
