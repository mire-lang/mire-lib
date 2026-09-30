#!/usr/bin/env bash
# Integration tests for mire::std.
#
# The @[test] functions in tests/std_io.mire can only assert on return values,
# because a test cannot capture its own stdout without testing the harness, and
# because `mire test` hands the program whatever stdin happens to be. That leaves
# the interesting half of a stream API untested: whether the bytes coming back
# are the bytes that went in, and whether available() agrees with the reads.
#
# So this script does the part only an outside observer can do. It builds small
# probe programs into a scratch directory, runs them with stdin wired to a pipe
# whose behaviour the test controls, captures stdout and stderr separately, and
# compares. It is the only place the NUL and CRLF handling is actually proven.
#
# Usage:  tests/std_io.sh [path-to-mire]
# Exits non-zero if any case fails.

set -uo pipefail

MIRE="${1:-${MIRE:-mire}}"
if ! command -v "$MIRE" >/dev/null 2>&1 && [ ! -x "$MIRE" ]; then
    echo "error: mire binary not found: $MIRE" >&2
    exit 2
fi

# The library under test is resolved from this working tree, not from a global
# install, so the tests always exercise the checked-out code. The loader wants a
# directory that *contains* packages, so it gets the parent of this repository.
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIB_DIR="$(dirname "$REPO_ROOT")"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/mire-stdio-XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

pass=0
fail=0

ok()   { pass=$((pass+1)); printf '  \033[32mok\033[0m   %s\n' "$1"; }
bad()  { fail=$((fail+1)); printf '  \033[31mFAIL\033[0m %s\n' "$1"; [ -n "${2:-}" ] && printf '         %s\n' "$2"; }

# check NAME EXPECTED ACTUAL
check() {
    if [ "$2" = "$3" ]; then ok "$1"; else bad "$1" "expected [$2], got [$3]"; fi
}

# build NAME <<'EOF' ... EOF   -> probe source on stdin, prints its path
build() {
    local name="$1"
    mkdir -p "$WORK/$name"
    cat > "$WORK/$name/main.mire"
    if ! "$MIRE" build "$WORK/$name/main.mire" --debug -O0 \
            --lib-dir "$LIB_DIR" --no-analysis-cache >"$WORK/$name/build.log" 2>&1; then
        bad "build $name" "$(tail -5 "$WORK/$name/build.log" | tr '\n' ' ')"
        return 1
    fi
    printf '%s' "$WORK/$name/bin/debug/main"
}

# feed WRITER-COMMAND PROBE  -> run probe with that stdin
feed() { bash -c "$1" | "$2"; }

echo "mire::std integration tests"
echo "  mire:   $MIRE"
echo "  repo:   $REPO_ROOT"
echo "  libdir: $LIB_DIR"
echo "  scratch: $WORK"
echo

# ── 1. binary round-trip, NULs included ─────────────────────────────────────
# A managed string can hold NULs, so a round trip through stdin and back out to
# stdout has to be byte-exact. cmp is the oracle; nothing in here inspects text.
P=$(build nuls <<'EOF'
load mire::str
load mire::std
pub fn main: () {
    set data = in::all()
    out::write_n(data str::len(data))
}
EOF
) || { echo; echo "aborting: cannot build probes"; exit 1; }

PAYLOAD="$WORK/payload.bin"
python3 -c "import sys; sys.stdout.buffer.write(b'AB\x00CD\x00\x00EF\n')" > "$PAYLOAD"
GOT="$WORK/got.bin"
if [ -x "$P" ]; then
    "$P" < "$PAYLOAD" > "$GOT"
    if cmp -s "$PAYLOAD" "$GOT"; then
        ok "binary round-trip is byte-exact (embedded NULs preserved)"
    else
        bad "binary round-trip is byte-exact" "$(cmp "$PAYLOAD" "$GOT" 2>&1 | head -2 | tr '\n' ' ')"
    fi
    # 'AB\0CD\0\0EF\n' is 10 bytes: 9 of content plus the trailing newline.
    check "round-trip byte count" "10" "$(wc -c < "$GOT" | tr -d ' ')"
fi

# ── 2. CRLF ─────────────────────────────────────────────────────────────────
# A line read must not leave the CR behind, or every value read from a Windows
# file gains a stray \r that compares unequal to the same value written locally.
P=$(build crlf <<'EOF'
load mire::str
load mire::std
pub fn main: () {
    set a = in::line()
    set b = in::line()
    dasu(str::len(a))
    dasu(str::len(b))
    dasu(a)
    dasu(b)
}
EOF
) || true
if [ -x "$P" ]; then
    OUT=$(printf 'alpha\r\nbeta\r\n' | "$P" | tr '\n' '|')
    check "CRLF: first line length" "5" "$(echo "$OUT" | cut -d'|' -f1)"
    check "CRLF: second line length" "4" "$(echo "$OUT" | cut -d'|' -f2)"
    check "CRLF: no CR leaks into the value" "alpha|beta|" "$(echo "$OUT" | cut -d'|' -f3-5)"
fi

# ── 3. available() must see the process's own buffer ────────────────────────
# The regression this pins down. getchar() pulls a whole chunk off the pipe into
# stdio's buffer, so after one line() the descriptor can be empty while four
# bytes are already in hand. Polling the descriptor alone reported 0, and a drain
# loop written as `while available() > 0` dropped the second line silently.
#
# The writer deliberately stays open with nothing more to send, so there is no
# POLLHUP to make the answer accidentally right.
P=$(build avail <<'EOF'
load mire::str
load mire::std
pub fn main: () {
    set a = in::line()
    dasu(str::len(a))
    dasu(in::available())
    set rest = in::all()
    dasu(str::len(rest))
}
EOF
) || true
if [ -x "$P" ]; then
    OUT=$( { printf 'one\ntwo\n'; sleep 2; } | "$P" | tr '\n' '|' )
    check "available(): first line read" "3" "$(echo "$OUT" | cut -d'|' -f1)"
    check "available(): buffered bytes are reported, not hidden" "1" "$(echo "$OUT" | cut -d'|' -f2)"
    check "available(): the bytes are really there" "4" "$(echo "$OUT" | cut -d'|' -f3)"
fi

# ── 4. a drain loop loses nothing ───────────────────────────────────────────
# The user-visible consequence of 3, asserted the way a program would hit it.
P=$(build drain <<'EOF'
load mire::str
load mire::std
pub fn main: () {
    set total = 0
    while in::available() > 0 {
        set chunk = in::all()
        set total = total + str::len(chunk)
    }
    dasu(total)
}
EOF
) || true
if [ -x "$P" ]; then
    OUT=$( { printf 'alpha\nbeta\ngamma\n'; sleep 2; } | "$P" | tr -d '\n' )
    check "drain loop sees every byte" "16" "$OUT"
fi

# ── 5. end of input is distinguishable from data ────────────────────────────
P=$(build eof <<'EOF'
load mire::str
load mire::std
pub fn main: () {
    set l = in::line()
    dasu(str::len(l))
    dasu(in::byte())
    dasu(in::available())
    set rest = in::all()
    dasu(str::len(rest))
}
EOF
) || true
if [ -x "$P" ]; then
    OUT=$("$P" < /dev/null | tr '\n' '|')
    check "EOF: line is empty" "0" "$(echo "$OUT" | cut -d'|' -f1)"
    check "EOF: byte is -1, not 0" "-1" "$(echo "$OUT" | cut -d'|' -f2)"
    check "EOF: available is 0" "0" "$(echo "$OUT" | cut -d'|' -f3)"
    check "EOF: all is empty" "0" "$(echo "$OUT" | cut -d'|' -f4)"
fi

# ── 6. NUL byte is data, not a terminator ───────────────────────────────────
# byte() must return 0 for a real NUL and -1 for end of input, otherwise binary
# input cannot be walked.
P=$(build nulbyte <<'EOF'
load mire::std
pub fn main: () {
    // 'A\0B' is three bytes, so the fourth read is the one that reports the end.
    set a = in::byte()
    set b = in::byte()
    set c = in::byte()
    set d = in::byte()
    dasu(a)
    dasu(b)
    dasu(c)
    dasu(d)
}
EOF
) || true
if [ -x "$P" ]; then
    OUT=$(printf 'A\0B' | "$P" | tr '\n' '|')
    check "byte(): reads the NUL as 0" "0" "$(echo "$OUT" | cut -d'|' -f2)"
    check "byte(): the byte after the NUL" "66" "$(echo "$OUT" | cut -d'|' -f3)"
    check "byte(): then -1 at end" "-1" "$(echo "$OUT" | cut -d'|' -f4)"
fi

# ── 7. stdout and stderr are separate channels ──────────────────────────────
P=$(build fds <<'EOF'
load mire::std
pub fn main: () {
    dasu(out::fd())
    dasu(err::fd())
    out::println("to-stdout")
    err::println("to-stderr")
}
EOF
) || true
if [ -x "$P" ]; then
    "$P" > "$WORK/o.txt" 2> "$WORK/e.txt"
    # dasu writes to stdout, so both fds are reported there; the point of this
    # case is that each stream carries its own content and nothing else.
    check "out::fd() is 1" "1" "$(sed -n '1p' "$WORK/o.txt")"
    check "err::fd() is 2" "2" "$(sed -n '2p' "$WORK/o.txt")"
    check "stdout carries only stdout" "to-stdout" "$(sed -n '3p' "$WORK/o.txt")"
    check "stderr carries only stderr" "to-stderr" "$(sed -n '1p' "$WORK/e.txt")"
    check "stderr does not leak into stdout" "3" "$(wc -l < "$WORK/o.txt" | tr -d ' ')"
fi

# ── 8. a write reports how many bytes it wrote ──────────────────────────────
P=$(build counts <<'EOF'
load mire::std
pub fn main: () {
    // The text goes to stderr and only the returned counts to stdout, so stdout
    // holds nothing but the numbers under assertion. print() writes no newline
    # but still reports the bytes, which is what makes this interleaving awkward
    # to read by eye.
    err::print("abc")
    dasu(out::println("de"))
    err::write_n("abcdef", 3)
    dasu(out::write_n("abcdef", 3))
    err::print_i64(42)
    dasu(out::print_i64(42))
    err::print_f64_no_newline(2.5)
    dasu(out::print_f64_no_newline(2.5))
    dasu(out::print(""))
    out::flush()
}
EOF
) || true
if [ -x "$P" ]; then
    OUT=$("$P" 2>/dev/null | tr '\n' ' ' | xargs)
    check "write byte counts include the newline" "3 3 3 3 3 0" "$OUT"
    # The literal text must reach stderr, proving the counts were not produced
    # by swallowing the write.
    check "the written text is not swallowed" "abcdefabc2.542" \
        "$("$P" 2>&1 >/dev/null | tr -d '\n')"
fi

# ── 9. interleaving is preserved when both streams share a destination ──────
# This is the reason writes go through the stdio FILE* rather than the raw
# descriptor: a printf and a stream write to the same pipe must not reorder.
P=$(build interleave <<'EOF'
load mire::std
pub fn main: () {
    dasu(1)
    err::println("two")
    dasu(3)
    err::println("four")
    out::flush()
    err::flush()
}
EOF
) || true
if [ -x "$P" ]; then
    OUT=$("$P" 2>&1 | tr '\n' ' ')
    check "stdout and stderr interleave in call order" "1 two 3 four " "$OUT"
fi

# ── 10. is_tty follows the actual stdin ─────────────────────────────────────
P=$(build tty <<'EOF'
load mire::std
pub fn main: () {
    // is_tty returns a bool; dasu takes an i64, so the branch is explicit.
    if in::is_tty() { dasu(1) } else { dasu(0) }
}
EOF
) || true
if [ -x "$P" ]; then
    check "is_tty() is false on a pipe" "0" "$(printf '' | "$P" | tr -d '\n')"
fi

echo
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
