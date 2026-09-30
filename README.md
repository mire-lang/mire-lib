# mire v1.1.0

Standard library for the [Mire](https://github.com/mire-lang) programming language.

## Modules

| Module | Description |
|--------|-------------|
| `mire::std` | Standard streams: `input` (stdin), `out` (stdout), `err` (stderr) |
| `mire::vec` | Vector operations (`push::i64`, `get`, `set`, `len`, `sort`, etc.) |
| `mire::map` | Map/dictionary operations (`get::i64`, `set::i64`, `has`, `keys`, `values`, `count`) |
| `mire::str` | String operations (`len`, `upper`, `lower`, `split`, `join`, `pad`, `from::bool`, etc.) |
| `mire::maybe` | Optional handles (`some`, `none`, `is`, `unwrap`, `unwrap::or`, `free`) |
| `mire::result` | Result handles (`ok`, `err`, `is`, `unwrap`, `unwrap::or`, `free`) |
| `mire::arr` | Fixed-size array operations (`new`, `get`, `set`, `len`) |

## Usage

```toml
# owl.toml
[dependencies]
    mire = "1.1.0"
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

- **Nested function grouping**: `push::i64`, `get::i64`, `set::i64` — type suffixes for polymorphic operations
- **Independent package**: does not require kioto as a dependency
- **Explicit module loading**: use `load mire::<module>`; the package entry is
  intentionally not an aggregate re-export.
- **Thin wrappers**: each function wraps a single runtime C function (`rt_*`)
- **Standard streams live in `mire::std`**: `input`, `out` and `err`, or the
  flat `std::print` / `std::read_line` / `std::flush` forms. The module is
  `input` rather than `in` because `in` is a reserved keyword and
  `load mire::std::in` does not parse. Writes go through the C `FILE*` rather
  than the raw descriptor so that stream writes and `printf`-based output
  interleave in call order. Reads are binary-safe and never truncate at a NUL:
  `byte()` returns `-1` at end of input, so EOF is distinguishable from a zero
  byte. Writes return the number of bytes written, not `0`/`1`, so a short
  write is detectable.
- **Growing collections return the new value**: `vec::push`, `map::set`, and
  `map::merge` may reallocate the backing storage, so they return the collection
  and you reassign (`set v = vec::push::i64(v 42)`). Read-only functions
  (`len`, `get`, `has`, `keys`, `values`, ...) take `&anything` and never move
  the value. `vec::set::i64` writes an element in place.
- **`str::from::bool`** takes `:bool` and produces `"true"`/`"false"`.
- **Padding uses strings**: `str::pad::left/right` accept a `&str` pad value,
  matching the runtime ABI and allowing multi-byte padding sequences.
- **Owned optional/result handles**: values returned by `maybe` and `result`
  are runtime-owned pointers and must be released with the matching `free`
  function after their last use.
- **No version()**: version is tracked in `owl.toml` and the lockfile

## Building

```bash
owl build
owl test
```

## License

Part of the Mire ecosystem. See the main project for license details.
