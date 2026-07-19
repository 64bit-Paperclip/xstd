#pragma once

// ============================================================================
// xstd_string_hash - FNV-1a hashing for xstd::string
// ----------------------------------------------------------------------------
// File:        xstd_string_hash.h
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
// - Separate file from xstd_string.h on purpose. Hashing is opt-in, not core
//   to what a string is - don't force every consumer of xstd::string to pull
//   in <cstdint> and the std::hash specialization machinery if they never
//   need to put one in a hash-based container.
//
// - FNV-1a, not something fancier. Simple, fast, well-tested, easy to make
//   constexpr later if we ever want compile-time hashing of literals. 
//
// - Computed in a fixed 64-bit width (uint64_t), not size_t. size_t is
//   32-bit on some platforms, and FNV-1a's published constants are tuned
//   for a specific bit width. We only narrow to size_t at the very end, in
//   the std::hash specialization, because that's what the container API
//   demands - not because the algorithm itself cares about size_t.
//
// - Every byte is cast through uint8_t before hashing. char is signed on
//   most platforms - XOR-ing a raw signed char in directly would sign-extend
//   high-bit characters unpredictably. Don't remove this cast.
//
// - fnv1a_hash() takes a raw (const char*, size_t) pair, not an xstd::string
//   directly, on purpose - it's reusable against any byte buffer
//   (fixed_string<N>, a raw xvector<char>, whatever), not locked to one type.
// ============================================================================

#ifndef __XSTD_STRING_HASH_H_GUARD
#define __XSTD_STRING_HASH_H_GUARD

#include <cstdint>       // uint64_t / uint8_t — fixed-width integers for a portable, deterministic hash
#include "string.h" // xstd::string — the type we're providing a hash for

namespace xstd
{
    /// <summary>
    /// Computes an FNV-1a hash over a raw byte buffer. Not tied to xstd::string —s
    /// works against any (pointer, length) pair, so it's reusable for fixed_string&lt;N&gt;
    /// or any other byte-buffer-shaped type.
    /// </summary>
    /// <param name="data">Pointer to the first byte to hash. May be null only if length is 0.</param>
    /// <param name="length">Number of bytes to hash.</param>
    /// <returns>A 64-bit FNV-1a hash of the given byte range.</returns>
    inline uint64_t fnv1a_hash(const char* data, size_t length)
    {
        constexpr uint64_t offset_basis = 14695981039346656037ull;
        constexpr uint64_t prime = 1099511628211ull;

        uint64_t hash = offset_basis;

        for (size_t i = 0; i < length; ++i)
        {
            hash ^= static_cast<uint8_t>(data[i]);
            hash *= prime; // multiplication wraps on overflow — expected and required
        }

        return hash;
    }

    /// <summary>
    /// Computes an FNV-1a hash of an xstd::string's contents.
    /// </summary>
    /// <param name="s">The string to hash.</param>
    /// <returns>A 64-bit FNV-1a hash of the string's contents.</returns>
    inline uint64_t hash_of(const string& s)
    {
        return fnv1a_hash(s.c_str(), s.size());
    }
}

namespace std
{
    /// <summary>
    /// Specialization so xstd::string works as a key in std::unordered_map, std::unordered_set,
    /// and anything else that relies on std::hash.
    /// </summary>
    template <>
    struct hash<xstd::string>
    {
        /// <summary>
        /// Computes the hash of an xstd::string, narrowed from the internal 64-bit FNV-1a result down to size_t as required by the std::hash contract.
        /// </summary>
        /// <param name="s">The string to hash.</param>
        /// <returns>The hash value, as a size_t.</returns>
        size_t operator()(const xstd::string& s) const noexcept
        {
            return static_cast<size_t>(xstd::fnv1a_hash(s.c_str(), s.size()));
        }
    };
}

#endif