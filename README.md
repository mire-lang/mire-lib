# mire v2.0.0-beta

Standard library for the [Mire](https://github.com/mire-lang) programming language.

## Modules

| Module | Description |
|--------|-------------|
| `mire::std` | Standard streams: `in` (stdin), `out` (stdout), `err` (stderr) |
| `mire::vec` | Vector operations (`push::i64`, `get`, `set`, `len`, `sort`, `binary::search`, etc.) |
| `mire::map` | Map/dictionary operations (`get::i64`, `set::i64`, `has`, `keys`, `values`, `count`) |
| `mire::str` | String operations (`len`, `upper`, `lower`, `split`, `join`, `pad`, `from::bool`, etc.) |
| `mire::maybe` | Optional handles (`some`, `none`, `is`, `unwrap`, `unwrap::or`, `free`) |
| `mire::result` | Result handles (`ok`, `err`, `is`, `unwrap`, `unwrap::or`, `free`) |
| `mire::arr` | Fixed-size array operations (`new`, `get`, `set`, `len`) |
| `mire::bytes` | Raw byte buffers and binary framing (`alloc`, `get_u8`, `set_u8`, `to_str`, ...) |
| `mire::aux::pal` | Platform Abstraction Layer bindings (`rt_io_*`, `pal_time_*`, ...) |

## Usage

```toml
# owl.toml
[dependencies]
    mire = "2.0.0-beta"
```

```mire
load mire::vec
load mire::map

pub fn main: () {
    set v = [] :vec[i64] mut
    set v = vec::push::i64(v 42)
    set n = vec::len(v)
    use dasu(n) // "1"

    set m = [] :map[str i64] mut
    set m = map::set::i64(m "key" 100)
    set val = map::get::i64(m "key")
    use dasu(val) // "100"
}
```

## Design

- **Nested function grouping**: `push::i64`, `get::i64`, `set::i64` — type suffixes for polymorphic operations. Overloads live under their type: `vec::pop::i64(...)`, `vec::with::capacity::i64(...)`.
- **Module constants use `cons`**: Mire has no `pub const`; each stream module defines its own local `cons SEL_OUT = 1 :i64` / `cons SEL_ERR = 2 :i64` (matching the runtime's `rt_io_*` selector convention: 1 = stdout, 2 = stderr, 3 = stdin).
- **Independent package**: does not require kioto as a dependency.
- **Explicit module loading**: use `load mire::<module>`; each intermediate segment resolves to a directory listed in `[exports]` of `owl.toml`.
- **PAL-backed externs**: `mire::aux::pal` declares real runtime symbols (`rt_io_write`, `rt_io_read_all`, `pal_time_monotonic_ns`, ...). No fictitious `io_*` names.
- **Standard streams live in `mire::std`**: `in`, `out` and `err`, or the flat `std::print` / `std::read_line` / `std::flush` forms. Reads are binary-safe and never truncate at a NUL: `byte()` returns `-1` at end of input, so EOF is distinguishable from a zero byte. `available()` answers from the runtime's own buffer first.
- **Growing collections return the new value**: `vec::push`, `map::set`, and `map::merge` may reallocate the backing storage, so they return the collection and you reassign (`set v = vec::push::i64(v 42)`). Read-only functions (`len`, `get`, `has`, `keys`, `values`, ...) take `&anything` and never move the value.
- **`str::from::bool`** takes `:bool` and produces `"true"`/`"false"`.
- **Padding uses strings**: `str::pad::left/right` accept a `&str` pad value, matching the runtime ABI and allowing multi-byte padding sequences.
- **Owned optional/result handles**: values returned by `maybe` and `result` are runtime-owned pointers and must be released with the matching `free` function after their last use.
- **No `version()`**: version is tracked in `owl.toml` and the lockfile.

## Building

```bash
owl build
owl test
```

## License

Part of the Mire ecosystem. See the main project for license details.
