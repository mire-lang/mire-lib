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
