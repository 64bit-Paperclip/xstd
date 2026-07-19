#pragma once

// ============================================================================
// ordered_vector - Sorted, invariant-preserving vector. Duplicates allowed.
// ----------------------------------------------------------------------------
// File:        ordered_vector.h
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
// NOTES / CAVEATS - read before you use
//
// - DataType must support operator< and operator==. Ordering and equality are both
//   required — one for keeping sort order, one for contains()/erase() to
//   confirm an exact match at the position binary search finds.
//
// - Duplicates are allowed. insert() always inserts, even if an equal
//   element already exists. If you want unique-only (set) semantics, that's
//   a different type.
//
// - No push_back(), no positional insert(iterator, value). Those don't
//   exist on this type at all, on purpose. There is no way to insert
//   anything except through insert(), which always respects sort order.
//   The invariant can't be violated through this type's own public API.
//
// - Insert/erase are O(N) (element shift), same real cost as any sorted
//   contiguous array. Lookup (contains) is O(log N) via binary search.
//   This is the right tool for read-heavy, moderate-to-large, possibly
//   sparse data, not for something inserted/erased constantly at scale.
//
// - Trivially relocatable types (see traits/trivial.h) get their shift-on-
//   insert/erase done via memmove instead of a per-element move+destroy
//   loop. memmove, not memcpy, source and destination ranges overlap
//   since this is a shift within the same buffer, and memcpy is undefined
//   behavior on overlapping ranges.
//
// - No exceptions. Same policy as the rest of xstd.
//
// - Not thread-safe.
// ============================================================================

#ifndef __XSTD_ORDERED_VECTOR_H_GUARD
#define __XSTD_ORDERED_VECTOR_H_GUARD

#include <cassert>    // assert — debug-only misuse checks
#include <cstring>    // memmove — overlap-safe bulk shift for trivially-relocatable types
#include <utility>    // std::move_if_noexcept — used in the non-trivial shift-on-insert/erase path
#include "vector_base.h"

namespace xstd
{
    /// <summary>
    /// A vector that maintains its elements in sorted order at all times. Duplicates are
    /// allowed. T must support operator&lt; (for ordering) and operator== (for exact-match
    /// checks). Provides O(log N) lookup via binary search and O(N) insert/erase, matching
    /// the real cost of a sorted contiguous array. No push_back or positional insert exist
    /// on this type — the only way to add an element is insert(), which always respects
    /// sort order, so the invariant cannot be violated through this type's own API.
    /// </summary>
    template <typename DataType>
    class ordered_vector : public vector_base<DataType>
    {
        using base = vector_base<DataType>;

    public:
        using iterator = typename base::iterator;
        using const_iterator = typename base::const_iterator;

    private:
        /// <summary>
        /// Finds the first position where value could be inserted without breaking sort
        /// order (the first element not less than value).
        /// </summary>
        /// <param name="value">The value to locate a position for.</param>
        /// <returns>Index of the first element &gt;= value, or size() if none.</returns>
        size_t _lower_bound(const DataType& value) const
        {
            size_t lo = 0, hi = this->_Count;
            while (lo < hi)
            {
                size_t mid = lo + (hi - lo) / 2;
                if (this->_Data[mid] < value) lo = mid + 1;
                else hi = mid;
            }
            return lo;
        }

    public:
        /// <summary>
        /// Default constructor. Produces an empty ordered_vector with no allocation.
        /// </summary>
        ordered_vector() = default;

        /// <summary>
        /// Inserts a copy of value at the position required to keep the vector sorted.
        /// Duplicates are allowed — an equal element does not block insertion. Trivially
        /// relocatable types shift via memmove; everything else shifts via a move+destroy loop.
        /// </summary>
        /// <param name="value">The value to insert.</param>
        /// <returns>Iterator to the newly inserted element.</returns>
        iterator insert(const DataType& value)
        {
            size_t idx = _lower_bound(value);
            this->_grow_if_needed();

            if constexpr (is_trivially_relocatable_v<DataType>)
            {
                if (idx < this->_Count)
                {
                    std::memmove(this->_Data + idx + 1, this->_Data + idx,
                        (this->_Count - idx) * sizeof(DataType));
                }
                this->_Data[idx] = value;
            }
            else
            {
                if (idx == this->_Count)
                {
                    ::new (this->_Data + this->_Count) DataType(value);
                }
                else
                {
                    ::new (this->_Data + this->_Count) DataType(std::move_if_noexcept(this->_Data[this->_Count - 1]));
                    for (iterator p = this->end() - 1; p != this->begin() + idx; --p)
                        *p = std::move_if_noexcept(*(p - 1));

                    this->_Data[idx] = value;
                }
            }

            ++this->_Count;
            return this->begin() + idx;
        }

        /// <summary>
        /// Inserts value (moved) at the position required to keep the vector sorted.
        /// Duplicates are allowed. Trivially relocatable types shift via memmove; everything
        /// else shifts via a move+destroy loop.
        /// </summary>
        /// <param name="value">The value to insert.</param>
        /// <returns>Iterator to the newly inserted element.</returns>
        iterator insert(DataType&& value)
        {
            size_t idx = _lower_bound(value);
            this->_grow_if_needed();

            if constexpr (is_trivially_relocatable_v<DataType>)
            {
                if (idx < this->_Count)
                {
                    std::memmove(this->_Data + idx + 1, this->_Data + idx,
                        (this->_Count - idx) * sizeof(DataType));
                }
                this->_Data[idx] = std::move(value);
            }
            else
            {
                if (idx == this->_Count)
                {
                    ::new (this->_Data + this->_Count) DataType(std::move(value));
                }
                else
                {
                    ::new (this->_Data + this->_Count) DataType(std::move_if_noexcept(this->_Data[this->_Count - 1]));
                    for (iterator p = this->end() - 1; p != this->begin() + idx; --p)
                        *p = std::move_if_noexcept(*(p - 1));

                    this->_Data[idx] = std::move(value);
                }
            }

            ++this->_Count;
            return this->begin() + idx;
        }

        /// <summary>
        /// Returns true if an element equal to value exists.
        /// </summary>
        /// <param name="value">The value to search for.</param>
        /// <returns>True if found.</returns>
        bool contains(const DataType& value) const
        {
            size_t idx = _lower_bound(value);
            return idx < this->_Count&& this->_Data[idx] == value;
        }

        /// <summary>
        /// Finds the first element equal to value.
        /// </summary>
        /// <param name="value">The value to search for.</param>
        /// <returns>Pointer to the element, or nullptr if not found.</returns>
        DataType* find(const DataType& value)
        {
            size_t idx = _lower_bound(value);
            return (idx < this->_Count&& this->_Data[idx] == value) ? &this->_Data[idx] : nullptr;
        }

        /// <summary>
        /// Finds the first element equal to value (const).
        /// </summary>
        /// <param name="value">The value to search for.</param>
        /// <returns>Pointer to the element, or nullptr if not found.</returns>
        const DataType* find(const DataType& value) const
        {
            size_t idx = _lower_bound(value);
            return (idx < this->_Count&& this->_Data[idx] == value) ? &this->_Data[idx] : nullptr;
        }

        /// <summary>
        /// Erases the first element equal to value, shifting subsequent elements down.
        /// Trivially relocatable types shift via memmove; everything else shifts via a
        /// move+destroy loop.
        /// </summary>
        /// <param name="value">The value to erase.</param>
        /// <returns>True if an element was found and erased.</returns>
        bool erase(const DataType& value)
        {
            size_t idx = _lower_bound(value);
            if (idx >= this->_Count || !(this->_Data[idx] == value))
                return false;

            if constexpr (is_trivially_relocatable_v<DataType>)
            {
                std::memmove(this->_Data + idx, this->_Data + idx + 1,
                    (this->_Count - idx - 1) * sizeof(DataType));
            }
            else
            {
                for (size_t i = idx; i + 1 < this->_Count; ++i)
                    this->_Data[i] = std::move_if_noexcept(this->_Data[i + 1]);
                this->_Data[this->_Count - 1].~DataType();
            }

            --this->_Count;
            return true;
        }
    };
}

#endif