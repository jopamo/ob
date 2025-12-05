# AddressSanitizer and LeakSanitizer for Openbox

## Third‑party library leaks (false positives)

Fontconfig, expat, and other libraries allocate process‑lifetime caches that
LeakSanitizer reports as leaks because it cannot see the global pointers inside
uninstrumented shared libraries.

### Example traces

```
Direct leak of 256 byte(s) in 1 object(s) allocated from:
    #0 malloc
    #1 FcCharSetAddChar (libfontconfig.so.1)
    #2 FcInit (libfontconfig.so.1)
    #3 ...

Indirect leak of 64 byte(s) in 1 object(s) allocated from:
    #0 malloc
    #1 XML_ParseBuffer (libexpat.so.1)
    #2 ...
```

These are **not** Openbox leaks and cannot be fixed in this codebase.

## Suppression file

`lsan.supp` contains patterns to silence these known false positives.

### Usage

```bash
# Run Openbox with leak detection and suppressions
LSAN_OPTIONS="suppressions=lsan.supp:print_suppressions=1" \
  ./build‑meson‑asan/openbox/openbox

# Disable leak detection entirely (keep other ASan checks)
ASAN_OPTIONS=detect_leaks=0 ./build‑meson‑asan/openbox/openbox
```

### Adding new suppressions

1. Run with `print_suppressions=1` to see which patterns match.
2. Add a `leak:` line with a substring from the stack trace (function name,
   library name, etc.).
3. Keep patterns specific enough to avoid suppressing real leaks.

## Running tests with ASan/LSan

```bash
# Unit tests
LSAN_OPTIONS="suppressions=lsan.supp" ./build‑meson‑asan/obt/obt_unittests

# If a test fails due to a leak, first verify whether it's a genuine leak
# (Openbox functions in the allocation stack) or a third‑party false positive.
```

## References

- [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
- [LeakSanitizer](https://clang.llvm.org/docs/LeakSanitizer.html)
- [LSAN suppression format](https://github.com/google/sanitizers/wiki/AddressSanitizerLeakSanitizer#suppressions)
