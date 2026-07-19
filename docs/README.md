# TextCPP

TextCPP is a C++ string utility library for performance-critical code where heap allocations are unacceptable. String data lives inline with the object: on the stack, in a pool, or a contiguous array. Predictable access patterns, no allocator overhead, cache friendly.

---

## Design Goals

- **Zero heap allocation** for string storage
- **Cache-local data** — string bytes live inside the object, not somewhere else on the heap
- **Predictable performance** — no hidden allocator calls, no small-string optimization surprises
- **STL-compatible** — works naturally with `std::string_view`, `std::string`, and standard algorithms
- **Low overhead** — thin wrappers, no virtual dispatch, no reference counting

---

## Requirements

- C++17 or later
- No external dependencies

---

## Classes

### `FixedString<N>`

A compile-time-capacity string stored entirely inline within the object. The buffer is `N` bytes, including the null terminator, so the maximum string length is `N - 1`.

```cpp
#include "fixed_string.h"

FixedString<64> name = "TextCPP";
FixedString<64> copy = name;

if (name == "TextCPP") { ... }

std::cout << name << "\n";
std::string s = name.ToString();
```

**Key properties:**

| Property | Detail |
|---|---|
| Storage | Inline char buffer, no heap |
| Capacity | Compile-time constant `N` (includes null terminator) |
| Max length | `N - 1` characters |
| Null-terminated | Always |
| Copyable | Yes |
| Movable | Yes (same as copy) |

**Member summary:**

| Member | Description |
|---|---|
| `Data[N]` | Raw character buffer (public for aggregate init) |
| `c_str()` | Returns `const char*` to internal buffer |
| `length()` | String length excluding null terminator, O(n) |
| `empty()` | True if first byte is null |
| `Assign(sv)` | Core assignment from `string_view` |
| `ToString()` | Returns a `std::string` (allocates) |
| `Capacity` | `static constexpr size_t`, equals `N` |

**Supported operators:**

- `=` from `const char*`, `std::string`, `std::string_view`
- `==` / `!=` against `FixedString<M>`, `const char*`, `std::string_view`
- `<<` stream output
- `+` concatenation with `const char*`, `std::string_view`, `FixedString<M>` (returns `std::string`)
- Implicit conversion to `std::string_view` and `const char*`

**Truncation behavior:**

In debug builds, `Assign` asserts that the source string fits within the buffer. In release builds, the string is silently truncated to `N - 1` characters. Size your buffers accordingly.

---

## Usage Notes

`FixedString` is best suited for strings whose maximum length is known at design time: identifiers, names, paths, tags, keys, and similar fixed-domain text. It is not intended to replace `std::string` in general-purpose code.

Because the string data is part of the object, arrays and structs of `FixedString` are fully contiguous in memory, which is the primary performance advantage over heap-allocated strings in hot data structures.

Prefer `c_str()` or the implicit `std::string_view` conversion over `ToString()` wherever possible to avoid allocation.

---

## Roadmap

- Additional `FixedString` utilities (trim, find, split, format)
- Low-allocation string builder
- Interning and hashing support
- Additional allocation-free text processing primitives

---

## License

See `LICENSE.txt` for terms.

