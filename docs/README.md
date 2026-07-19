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

### Issues with the Standard Library
- std::string's small-string optimization means almost every operation
  starts with a branch: is this string using its inline buffer or a heap
  allocation? That branch has to be predicted correctly to be free, and
  with strings of mixed or unpredictable sizes flowing through real code,
  it doesn't always predict well. A mispredicted branch stalls the
  pipeline. That cost exists on every single string operation, not just
  the ones that actually need to grow. xstd::string doesn't have SSO at
  all. It always heap-allocates, no branch, no inline buffer, one code
  path. For the case SSO exists to serve, small strings that don't want
  an allocation, that's what fixed_string(N) is for: a fixed-capacity,
  stack or inline buffer with no heap involved, ever, and no per-op
  branch either, because there's only ever one storage location to begin
  with.

- std::vector already checks `is_trivially_copyable` internally and takes a
  memcpy-style fast path when that's true, so plain PODs aren't the
  problem. The gap is everything else: a type can have a real,
  non-trivial move constructor and destructor and still be completely
  safe to relocate with a raw memcpy, like a struct holding a
  unique_ptr-style owned resource, where the move constructor's only job
  is nulling out a pointer so the old object's destructor doesn't
  double-free it. That work is pointless during relocation specifically,
  because the old object is never going to be destructed at all.
  is_trivially_copyable can't see that distinction, so std::vector falls
  back to a real move+destroy call per element for anything that isn't
  fully trivial. xstd::vector has a trivially-relocatable trait: plain
  trivially-copyable types still get the fast path automatically, and
  anything else that's actually safe to relocate opts in by inheriting a
  marker, closing that gap instead of quietly paying for it. C++26 is
  bringing trivial relocation into the standard for real, but that means
  std::vector's behavior here doesn't change until an entire codebase is
  on a C++26 toolchain, which for most real projects is years away, if it
  happens at all. This has been usable now.

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