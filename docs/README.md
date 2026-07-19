# xstd

Minimal, header-only replacements for the C++ standard library types every
project ends up needing (vector, string), plus a few things the STL
doesn't have at all.

## Why this exists

The standard library isn't yours to change. Whatever std::vector and
std::string do, however they're laid out, is up to whichever compiler and
STL implementation happens to be in use. You can't teach std::vector that
a type is safe to memcpy during growth instead of doing a move+destroy per
element. You can't change its failure behavior. You can't add a type it
doesn't have. None of that is available to you, because it isn't your
code.

xstd is source, not a system header. Whoever's using it owns the layout,
the growth strategy, the failure policy, and everything else the STL
normally decides for you. Those things can be changed, because it's just
a header sitting in the project.


## What's in here

- vector_base(T) - shared allocation, storage, and iteration mechanics.
  Not meant to be used directly. vector and ordered_vector both build on
  it so neither duplicates the same low-level logic.
- vector(T) - dynamic array. Doubling growth, arbitrary-position insert
  and erase, memcpy-relocates trivially-relocatable types during growth
  instead of move+destroy per element.
- ordered_vector(T) - vector that stays sorted at all times. Duplicates
  allowed. O(log N) lookup via binary search, O(N) insert and erase. No
  push_back and no positional insert exist on this type, so the sort
  order can't be broken through its own API. Trivially relocatable types
  shift via memmove instead of a per-element loop.
- string - dynamic, heap-backed string. No small-string optimization on
  purpose, see fixed_string below. Always null-terminated.
- fixed_string(N) - fixed-capacity, stack or inline string. No allocation,
  ever. Truncates past N-1 characters with a debug-only assert. Named
  aliases for common sizes.
- A trivially-relocatable trait. Inherit TriviallyRelocatable on a type
  to opt it in, or it's picked up automatically for anything trivially
  copyable. Backs the memcpy/memmove fast paths in vector and
  ordered_vector.
- FNV-1a hashing. A std::hash specialization for xstd::string so it works
  as a key in hash-based containers.

## Rules for anything added to xstd

- No STL containers internally. That's the entire point.
- Bounds and misuse checks are debug-only asserts, not runtime-checked in
  release, unless a function says otherwise.
- Always write xstd:: at call sites. Don't `using namespace xstd`.
- Anything meant to be read from outside C++ needs a plain-data struct
  form, with no members that aren't themselves plain data, all the way
  down.