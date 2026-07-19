#pragma once

// ============================================================================
// xstd_string - Dynamic Length Strings. Replacement for std::string
// ----------------------------------------------------------------------------
// File:        xstd_string.h
// Author:      Jason Penick
// Website:     630Studios.com
// Created:     2026
//
// Copyright (c) 2026 Jason Penick. All rights reserved.
//
// This software is provided under the terms outlined in LICENSE.txt
// See README.md for full documentation and usage examples
// ============================================================================
//
// NOTES / CAVEATS - read before you use this somewhere clever
//
// - No SSO. Every xstd::string, even "a", heap-allocates. This is deliberate.
//   If you want small/known-bound strings without paying for an allocation,
//   use fixed_string<N> instead. Don't ask for SSO here, that's what
//   fixed_string is for.
//
// - No exceptions. at() and operator[] assert in debug (index out of range,
//   or indexing an empty/unassigned string) and do nothing in release -
//   trusts the caller completely. This is a "catch it in dev, don't crash
//   the build in someone's hands" tradeoff, not an oversight.
//
// - operator[](size()) is allowed and returns the null terminator, same as
//   real std::string - but ONLY once the string has real backing storage.
//   A default-constructed, never-assigned string has _Data == nullptr, and
//   indexing it (even at 0) is asserted against, not silently safe. If you
//   need "always safe to index even before first use," that's not what this
//   does - std::string only gets away with it because of SSO, which we
//   explicitly don't have.
// 
// - operator[](size()) returns a MUTABLE reference to the null terminator slot.
//   Writing anything other than '\0' there silently breaks the null-termination
//   invariant every other method relies on (c_str(), data(), any C interop).
//   This is legal per the API and not checked — same hole std::string has, and
//   we're not engineering around it. Don't write to that slot. You've been told.
//
// - Trivially relocatable. Tagged with TriviallyRelocatable so xstd::vector
//   growth memcpy's these instead of doing the move+destroy dance per
//   element. This is safe specifically because relocation guarantees the
//   old copy's destructor never runs - if you're ever unsure whether some
//   other type is safe to tag this way, that's the guarantee you need to
//   check for, not just "does it have a pointer in it."
//
// - Never shrinks. reserve()/assign() only grow capacity, matches
//   std::vector/std::string behavior. If you want the memory back, you
//   don't get shrink_to_fit right now - not written.
//
// - Always null-terminated. c_str()/data() are free, O(1), safe to hand to
//   any C API. This costs one extra byte and one extra write per mutation,
//   which we decided was worth it given how much this thing needs to
//   interop with C-ish stuff eventually.
//
// - ABI note: this is meant to cross the DLL/mod boundary without dragging
//   std::string's implementation-specific layout with it. That only holds
//   if everything touching it was built with the same compiler/toolchain -
//   see the C-API shim layer (not written yet) for the actual answer to
//   "how do I hand this to a mod I don't control the compiler for."
//
// - Not thread-safe. No locking, no atomics, don't share one instance
//   across threads without your own synchronization.
// ============================================================================

#ifndef __XSTD_STRING_H_GUARD
#define __XSTD_STRING_H_GUARD

#include <iosfwd>
#include <cstring>       // memcpy — bulk byte copy for growth/assign, cheaper than a per-char loop
#include <cassert>       // assert — debug-only bounds/misuse checks, compiled out in release
#include <utility>       // std::move / std::move_if_noexcept — move semantics in ctors, reserve, append
#include <string_view>   // std::string_view — cheap, non-owning pointer+length param type; not a container, no ABI risk
#include "traits/trivial.h"

namespace xstd
{
    /// <summary>
    /// A dynamically-sized, heap-backed string. Replacement for std::string with no STL
    /// dependency, no exceptions, and no SSO (use fixed_string&lt;N&gt; for small/bounded strings).
    /// Always null-terminated after any Assign/Append operation. Tagged TriviallyRelocatable —
    /// safe for xstd::vector to memcpy-relocate rather than move+destroy per element.
    /// </summary>
	class string: public TriviallyRelocatable
	{

        public:
            /// <summary>
            /// Mutable random-access iterator, just a raw char pointer into the internal buffer.
            /// Invalidated by any operation that reallocates (append/assign/reserve past capacity).
            /// </summary>
            using iterator = char*;

            /// <summary>
            /// Const random-access iterator, just a raw const char pointer into the internal buffer.
            /// Invalidated by any operation that reallocates (append/assign/reserve past capacity).
            /// </summary>
            using const_iterator = const char*;

		protected:
            /// <summary>
            /// Number of characters currently stored, excluding the null terminator.
            /// </summary>
			size_t _Length		= 0;

            /// <summary>
            /// Allocated capacity in characters, excluding the null terminator (buffer is _Reserved + 1 bytes).
            /// </summary>
			size_t _Reserved	= 0;

            /// <summary>
            /// Pointer to the heap-allocated character buffer. Null on a default-constructed,
            /// never-assigned string. Always null-terminated once allocated.
            /// </summary>
			char* _Data = nullptr;

            


		private:
            /// <summary>
            /// Raw allocation with no construction — allocates capacity + 1 bytes to always
            /// leave room for the null terminator.
            /// </summary>
            /// <param name="capacity">The number of characters to allocate room for, excluding the terminator.</param>
            /// <returns>A raw, uninitialized buffer of capacity + 1 bytes.</returns>
			static char* _alloc_raw(size_t capacity)
			{
				// +1 for null terminator, always
				return static_cast<char*>(::operator new(capacity + 1));
			}


		public:
			
            /// <summary>
            /// Default constructor. Produces an empty string with no allocation (_Data stays null until the first real Assign/Append/Reserve).
            /// </summary>
			string() = default;

            /// <summary>
            /// Destructor. Frees the backing buffer, if any was ever allocated.
            /// </summary>
			~string()
			{
				::operator delete(_Data);
			}

            /// <summary>
            /// Constructs from a null-terminated C string. A null pointer is treated as empty.
            /// </summary>
            /// <param name="str">The source C string. May be null.</param>
			string(const char* str)
			{
				assign(str);
			}

            /// <summary>
            /// Constructs from a std::string_view.
            /// </summary>
            /// <param name="sv">The source string view.</param>
			string(std::string_view sv)
			{
				assign(sv);
			}


            /// <summary>
            /// Copy constructor. Deep-copies the source string's contents into a fresh allocation.
            /// </summary>
            string(const string& other)
            {
                assign(std::string_view(other._Data, other._Length));
            }

            /// <summary>
            /// Copy assignment operator. Deep-copies the source string's contents, reusing existing
            /// capacity when it's sufficient rather than always reallocating.
            /// </summary>
            string& operator=(const string& other)
            {
                if (this == &other) return *this;
                assign(std::string_view(other._Data, other._Length));
                return *this;
            }

            /// <summary>
            /// Move constructor. Takes ownership of the source's buffer directly; the source is left empty.
            /// </summary>
            string(string&& other) noexcept
                : _Length(other._Length), _Reserved(other._Reserved), _Data(other._Data)
            {
                other._Data = nullptr;
                other._Length = other._Reserved = 0;
            }

            /// <summary>
            /// Move assignment operator. Frees this string's existing buffer, then takes ownership of
            /// the source's buffer directly; the source is left empty.
            /// </summary>
            string& operator=(string&& other) noexcept
            {
                if (this == &other)
                    return *this;

                ::operator delete(_Data);

                _Data           = other._Data;
                _Length         = other._Length;
                _Reserved       = other._Reserved;
                other._Data     = nullptr;
                other._Length   = other._Reserved = 0;

                return *this;
            }

            /// <summary>
            /// Appends a string_view's contents to the end of this string. Equivalent to Append(sv).
            /// </summary>
            /// <param name="sv">The string view to append.</param>
            /// <returns>Reference to this instance.</returns>
            string& operator+=(std::string_view sv)   { append(sv);     return *this; }

            /// <summary>
            /// Appends a null-terminated C string to the end of this string. Equivalent to Append(str).
            /// </summary>
            /// <param name="str">The C string to append. May be null (no-op).</param>
            /// <returns>Reference to this instance.</returns>
            string& operator+=(const char* str)       { append(str);    return *this; }

            /// <summary>
            /// Appends a single character to the end of this string. Equivalent to Append(c).
            /// </summary>
            /// <param name="c">The character to append.</param>
            /// <returns>Reference to this instance.</returns>
            string& operator+=(char c)                { append(c);      return *this; }

            /// <summary>
            /// Appends another xstd::string's contents to the end of this string.
            /// </summary>
            /// <param name="other">The string to append.</param>
            /// <returns>Reference to this instance.</returns>
            string& operator+=(const string& other)   { append(std::string_view(other._Data, other._Length)); return *this; }

            /// <summary>
            /// Indexed character access. Asserts in debug that the string has real backing storage
            /// and that the index is in range (index == size() is allowed, returns the null terminator).
            /// No check in release — trusts the caller, matches std::string's unchecked contract.
            /// </summary>
            /// <param name="index">The character index. May equal size() to read the null terminator.</param>
            /// <returns>Reference to the character at the given index.</returns>
            char& operator[](size_t index)
            {
                assert(_Data != nullptr && "xstd::string::operator[] — called on empty/unassigned string");
                assert(index <= _Length && "xstd::string::operator[] — index out of range");
                return _Data[index];
            }

            /// <summary>
            /// Indexed character access (const). Asserts in debug that the string has real backing storage
            /// and that the index is in range (index == size() is allowed, returns the null terminator).
            /// No check in release — trusts the caller, matches std::string's unchecked contract.
            /// </summary>
            /// <param name="index">The character index. May equal size() to read the null terminator.</param>
            /// <returns>Const reference to the character at the given index.</returns>
            const char& operator[](size_t index) const
            {
                assert(_Data != nullptr && "xstd::string::operator[] — called on empty/unassigned string");
                assert(index <= _Length && "xstd::string::operator[] — index out of range");
                return _Data[index];
            }

            /// <summary>
            /// Equality comparison against another xstd::string. Compares contents, not capacity.
            /// </summary>
            /// <param name="other">The string to compare against.</param>
            /// <returns>True if contents are identical.</returns>
            bool operator==(const string& other) const
            {
                return std::string_view(_Data, _Length) == std::string_view(other._Data, other._Length);
            }

            /// <summary>
            /// Equality comparison against a null-terminated C string. A null pointer is only equal
            /// to an empty string.
            /// </summary>
            /// <param name="other">The C string to compare against. May be null.</param>
            /// <returns>True if contents are identical.</returns>
            bool operator==(const char* other) const
            {
                if (!other) return _Length == 0;
                return std::string_view(_Data, _Length) == std::string_view(other);
            }

            /// <summary>
            /// Equality comparison against a std::string_view.
            /// </summary>
            /// <param name="other">The string view to compare against.</param>
            /// <returns>True if contents are identical.</returns>
            bool operator==(std::string_view other) const
            {
                return std::string_view(_Data, _Length) == other;
            }

            /// <summary>
            /// Inequality comparison against another xstd::string.
            /// </summary>
            /// <param name="other">The string to compare against.</param>
            /// <returns>True if contents differ.</returns>
            bool operator!=(const string& other) const { return !(*this == other); }

            /// <summary>
            /// Inequality comparison against a null-terminated C string.
            /// </summary>
            /// <param name="other">The C string to compare against. May be null.</param>
            /// <returns>True if contents differ.</returns>
            bool operator!=(const char* other) const { return !(*this == other); }

            /// <summary>
            /// Inequality comparison against a std::string_view.
            /// </summary>
            /// <param name="other">The string view to compare against.</param>
            /// <returns>True if contents differ.</returns>
            bool operator!=(std::string_view other) const { return !(*this == other); }

            /// <summary>
            /// Lexicographic less-than comparison against another xstd::string. Enables use as a key
            /// in ordered containers / with std::sort.
            /// </summary>
            /// <param name="other">The string to compare against.</param>
            /// <returns>True if this string sorts before other.</returns>
            bool operator<(const string& other) const
            {
                return std::string_view(_Data, _Length) < std::string_view(other._Data, other._Length);
            }

            /// <summary>
            /// Lexicographic less-than comparison against a std::string_view.
            /// </summary>
            /// <param name="other">The string view to compare against.</param>
            /// <returns>True if this string sorts before other.</returns>
            bool operator<(std::string_view other) const
            {
                return std::string_view(_Data, _Length) < other;
            }

            /// <summary>
            /// Appends a string_view's contents to the end of this string, growing capacity
            /// (doubling, or exact-fit if doubling isn't enough) if needed. No-op on an empty view.
            /// </summary>
            /// <param name="sv">The string view to append.</param>
            void append(std::string_view sv)
            {
                if (sv.empty())
                    return;

                size_t newLength = _Length + sv.size();

                if (newLength > _Reserved)
                {
                    // grow with headroom (double, or exact fit if that's not enough) — same doubling policy as xstd_vector
                    size_t newCap = (_Reserved == 0) ? newLength : _Reserved * 2;
                    
                    if (newCap < newLength)
                        newCap = newLength;

                    reserve(newCap);
                }

                std::memcpy(_Data + _Length, sv.data(), sv.size());

                _Length = newLength;
                _Data[_Length] = '\0';
            }

            /// <summary>
            /// Mutable iterator to the first character.
            /// </summary>
            iterator       begin()        { return _Data; }

            /// <summary>
            /// Mutable iterator one past the last character.
            /// </summary>
            iterator       end()          { return _Data + _Length; }

            /// <summary>
            /// Const iterator to the first character.
            /// </summary>
            const_iterator begin() const  { return _Data; }

            /// <summary>
            /// Const iterator one past the last character.
            /// </summary>
            const_iterator end() const    { return _Data + _Length; }

            /// <summary>
            /// Const iterator to the first character. Always const, regardless of the object's own constness.
            /// </summary>
            const_iterator cbegin() const { return _Data; }

            /// <summary>
            /// Const iterator one past the last character. Always const, regardless of the object's own constness.
            /// </summary>
            const_iterator cend() const   { return _Data + _Length; }

            /// <summary>
            /// Appends a null-terminated C string to the end of this string. A null pointer is a no-op.
            /// </summary>
            /// <param name="str">The C string to append. May be null.</param>
            void append(const char* str)
            {
                if (str)
                    append(std::string_view(str));
            }

            /// <summary>
            /// Appends a single character to the end of this string.
            /// </summary>
            /// <param name="c">The character to append.</param>
            void append(char c)
            {
                append(std::string_view(&c, 1));
            }

            /// <summary>
            /// Core assignment implementation. Replaces the string's contents with the given view,
            /// reallocating only if the new content exceeds current capacity or no buffer exists yet.
            /// Result is always null-terminated.
            /// </summary>
            /// <param name="sv">The source string view.</param>
            void assign(std::string_view sv)
            {
                if (sv.size() > _Reserved || _Data == nullptr)
                {
                    ::operator delete(_Data);

                    _Data       = _alloc_raw(sv.size());
                    _Reserved   = sv.size();
                }

                if (sv.size() > 0)
                    std::memcpy(_Data, sv.data(), sv.size());

                _Length         = sv.size();
                _Data[_Length]  = '\0';
            }

            /// <summary>
            /// Assigns from a null-terminated C string. A null pointer assigns an empty string.
            /// </summary>
            /// <param name="str">The source C string. May be null.</param>
            void assign(const char* str)
            {
                assign(str ? std::string_view(str) : std::string_view());
            }


            
            /// <summary>
            /// Checked character access. Asserts in debug that the index is strictly less than size()
            /// (unlike operator[], does not permit index == size()). No check in release.
            /// </summary>
            /// <param name="index">The character index. Must be less than size().</param>
            /// <returns>Reference to the character at the given index.</returns>
            char& at(size_t index)
            {
                assert(index < _Length && "xstd::string::at — index out of range");
                return _Data[index];
            }

            /// <summary>
            /// Checked character access (const). Asserts in debug that the index is strictly less than
            /// size() (unlike operator[], does not permit index == size()). No check in release.
            /// </summary>
            /// <param name="index">The character index. Must be less than size().</param>
            /// <returns>Const reference to the character at the given index.</returns>
            const char& at(size_t index) const
            {
                assert(index < _Length && "xstd::string::at — index out of range");
                return _Data[index];
            }

            /// <summary>
            /// Empties the string. Capacity is left untouched — no deallocation occurs.
            /// </summary>
            void clear()
            {
                _Length = 0;
                if (_Data)
                    _Data[0] = '\0';

                // capacity intentionally untouched — matches xvector::clear() / std::string::clear()
            }

            /// <summary>
            /// Returns a null-terminated pointer to the internal buffer. Safe to call on an empty or
            /// never-assigned string (returns a pointer to a static empty string in that case).
            /// </summary>
            const char* c_str() const   { return _Data ? _Data : ""; }

            /// <summary>
            /// Returns a null-terminated pointer to the internal buffer. Identical to c_str().
            /// </summary>
            const char* data() const    { return _Data ? _Data : ""; }

            /// <summary>
            /// The number of characters currently stored, excluding the null terminator.
            /// </summary>
            size_t length() const       { return _Length; }

            /// <summary>
            /// The number of characters currently stored, excluding the null terminator. Identical to length().
            /// </summary>
            size_t size() const         { return _Length; }

            /// <summary>
            /// The current allocated capacity in characters, excluding the null terminator.
            /// </summary>
            size_t capacity() const     { return _Reserved; }

            /// <summary>
            /// Returns true if the string contains no characters.
            /// </summary>
            bool   empty() const        { return _Length == 0; }

            /// <summary>
            /// The actual heap footprint of the backing buffer in bytes, including the null terminator.
            /// Returns 0 if no buffer has ever been allocated.
            /// </summary>
            /// <returns>Total allocated bytes, or 0 if unallocated.</returns>
            size_t allocated_bytes() const
            {
                return _Data ? (_Reserved + 1) : 0;
            }

            /// <summary>
            /// Grows capacity to at least newCap characters, preserving existing content. Never shrinks —
            /// a request smaller than or equal to current capacity is a no-op.
            /// </summary>
            /// <param name="newCap">The minimum capacity, in characters, to reserve.</param>
            void reserve(size_t newCap)
            {
                if (newCap <= _Reserved) return; // never shrinks

                char* newData = _alloc_raw(newCap);

                if (_Length > 0)
                    std::memcpy(newData, _Data, _Length);

                newData[_Length] = '\0';

                ::operator delete(_Data);

                _Data       = newData;
                _Reserved    = newCap;
            }


            /// <summary>
            /// Concatenates two xstd::strings, returning a new string. Allocates exactly once.
            /// </summary>
            /// <param name="lhs">The left-hand string.</param>
            /// <param name="rhs">The right-hand string.</param>
            /// <returns>A new string containing the concatenated result.</returns>
            friend string operator+(const string& lhs, const string& rhs)
            {
                string result;
                result.reserve(lhs._Length + rhs._Length);
                result.append(std::string_view(lhs._Data, lhs._Length));
                result.append(std::string_view(rhs._Data, rhs._Length));
                return result;
            }

            /// <summary>
            /// Concatenates an xstd::string with a null-terminated C string, returning a new string.
            /// </summary>
            /// <param name="lhs">The left-hand string.</param>
            /// <param name="rhs">The right-hand C string. May be null.</param>
            /// <returns>A new string containing the concatenated result.</returns>
            friend string operator+(const string& lhs, const char* rhs)
            {
                string result;
                result.reserve(lhs._Length + (rhs ? std::strlen(rhs) : 0));
                result.append(std::string_view(lhs._Data, lhs._Length));
                result.append(rhs);
                return result;
            }

            /// <summary>
            /// Concatenates a null-terminated C string with an xstd::string, returning a new string.
            /// </summary>
            /// <param name="lhs">The left-hand C string. May be null.</param>
            /// <param name="rhs">The right-hand string.</param>
            /// <returns>A new string containing the concatenated result.</returns>
            friend string operator+(const char* lhs, const string& rhs)
            {
                string result;
                result.reserve((lhs ? std::strlen(lhs) : 0) + rhs._Length);
                result.append(lhs);
                result.append(std::string_view(rhs._Data, rhs._Length));
                return result;
            }

            /// <summary>
            /// Concatenates an xstd::string with a std::string_view, returning a new string.
            /// </summary>
            /// <param name="lhs">The left-hand string.</param>
            /// <param name="rhs">The right-hand string view.</param>
            /// <returns>A new string containing the concatenated result.</returns>
            friend string operator+(const string& lhs, std::string_view rhs)
            {
                string result;
                result.reserve(lhs._Length + rhs.size());
                result.append(std::string_view(lhs._Data, lhs._Length));
                result.append(rhs);
                return result;
            }

            /// <summary>
            /// Concatenates a std::string_view with an xstd::string, returning a new string.
            /// </summary>
            /// <param name="lhs">The left-hand string view.</param>
            /// <param name="rhs">The right-hand string.</param>
            /// <returns>A new string containing the concatenated result.</returns>
            friend string operator+(std::string_view lhs, const string& rhs)
            {
                string result;
                result.reserve(lhs.size() + rhs._Length);
                result.append(lhs);
                result.append(std::string_view(rhs._Data, rhs._Length));
                return result;
            }

            /// <summary>
            /// Concatenates an xstd::string with a single character, returning a new string.
            /// </summary>
            /// <param name="lhs">The left-hand string.</param>
            /// <param name="rhs">The character to append.</param>
            /// <returns>A new string containing the concatenated result.</returns>
            friend string operator+(const string& lhs, char rhs)
            {
                string result;
                result.reserve(lhs._Length + 1);
                result.append(std::string_view(lhs._Data, lhs._Length));
                result.append(rhs);
                return result;
            }

            /// <summary>
            /// Stream output operator. Writes the string contents to the output stream.
            /// </summary>
            /// <param name="os">The output stream.</param>
            /// <param name="s">The string to write.</param>
            /// <returns>Reference to the output stream.</returns>
            friend std::ostream& operator<<(std::ostream& os, const string& s) { return os << s.c_str(); }
    };

}

#endif

