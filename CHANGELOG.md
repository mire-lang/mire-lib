# Changelog

All notable changes to the mire standard library.

## [1.1.0] - 2026-09-30 (standard streams)

### Added

- **`mire::std`, the three standard streams.** `load mire::std::out`,
  `load mire::std::err` and `load mire::std::input` expose stdout, stderr and
  stdin as a library module rather than leaving every library to reach for libc:

  ```mire
  load mire::std::out
  load mire::std::err
  load mire::std::input

  pub fn main: () {
      set line = input::line()
      out::print("got: ")
      out::println(line)
      err::println("done")
  }
  ```

  `load mire::std` pulls in all three children for code that prefers the flat
  names (`std::print`, `std::read_line`, `std::flush`, ...). The flat names are
  the same functions, not copies, so the two styles cannot drift apart.

  - `out` / `err`: `print`, `println`, `write_n`, `print_i64`,
    `print_i64_no_newline`, `print_f64`, `flush`, `failed`, `clear`, `fd`.
  - `input`: `line`, `byte`, `bytes`, `all`, `available`, `is_tty`.

- **26 tests** in `tests/std_io.mire` covering byte counts, the two output
  streams being genuinely distinct, CRLF handling, and EOF-after-drain through
  both the namespaced and the flat entry points.

### Design notes

- **The module is `input`, not `in`.** `in` is a reserved keyword in the lexer,
  so `load mire::std::in` cannot parse at all. `input` is the shortest spelling
  that works, and it matches `out` and `err`.
- **Writes go through the C `FILE*`, not the raw descriptor.** A program that
  writes from both a `printf` and a stream write otherwise gets output
  interleaved in the wrong order, because the two paths disagree about where the
  file position is. Routing both through stdio makes them interleave in call
  order.
- **Writes return the byte count, not `0`/`1`.** A caller that cares about a
  short write — a log line, a pipe — otherwise cannot detect one, and finds out
  later as a truncated line.
- **Reads are binary-safe and never truncate at a NUL.** `bytes(n)` returns
  exactly `n` bytes unless input ends first, `all()` returns what is left, and
  `byte()` returns `-1` at end of input, so EOF is distinguishable from a zero
  byte. Lines drop the trailing newline and a preceding `CR`, so CRLF input does
  not leave a stray `\r` on the value.
- **Requires `rt_io_*` from the compiler runtime** (Avenys `v4.3.2`, in
  `pr/4.2.0`). The symbols are `rt_`-prefixed, so the `externs = ["rt_*"]`
  allowlist in `[security]` already covers them and strict mode needs no change.

### Known issues

- The flat aliases in `core/std/mod.mire` route their result through a local
  before returning, which looks redundant and is not. Collapsing one back to a
  single `return out::print(s)` makes the module fail to build with
  `use of undefined value '@std.println'`. The cause is a compiler bug in which
  several one-line delegating functions of the same shape in one loaded module
  drop one another's emitted definition; routing through a local keeps the
  bodies distinct. It reproduces only in a loaded module — the same functions
  in a single self-contained file compile fine. Tracked upstream; the locals
  should be inlined away once it is fixed. The same workaround is why `out::`
  and `err::` bind their stream selector to a `sel` local.

## [1.0.0] - 2026-09-22 (first stable release)

### Added

- Stable macros: `assert_eq!`, `assert_ne!`, `trace_i64!` in `core/macros/checks`.
- Updated `core/vec` and `core/mod` with latest fixes.

### Changed

- Bumped library version to 1.0.0.
- Updated `owl.toml` dependencies and macros sections.

## [0.0.7] - 2026-08-05 (strict security mode)
# Changelog

All notable changes to the mire standard library.

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
