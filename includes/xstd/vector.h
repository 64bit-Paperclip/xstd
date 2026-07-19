#pragma once

// ============================================================================
// vector - Dynamic array. Replacement for std::vector
// ----------------------------------------------------------------------------
// File:        vector.h
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
// - No exceptions. at() and operator[] assert in debug (index out of range,
//   or on an empty/unassigned vector) and do nothing in release - trusts
//   the caller completely. Same tradeoff as everywhere else in xstd: catch
//   it in dev, don't crash the build in someone's hands.
//
// - operator[] has no "index == size()" carve-out like xstd::string does.
//   There's no null-terminator equivalent here - every index at or past
//   size() is out of range, full stop, asserted against.
//
// - Growth doubles capacity, same policy as std::vector. If you don't want
//   reallocation churn, reserve() up front.
//
// - Trivially relocatable types (see traits/trivial.h) get memcpy'd during
//   growth instead of moved+destroyed element by element. Your own types
//   opt in by inheriting TriviallyRelocatable; plain PODs get it for free
//   via is_trivially_copyable.
//
// - Never shrinks. reserve() only grows capacity. No shrink_to_fit - not
//   written.
//
// - Not thread-safe. No locking, no atomics, don't share one instance
//   across threads without your own synchronization.
// ============================================================================


#ifndef __XVECTOR_H_GUARD
#define __XVECTOR_H_GUARD

#include <cassert>       // assert — debug-only bounds/misuse checks, compiled out in release
#include <cstring>       // memcpy — bulk relocation for trivially-relocatable types during growth
#include <utility>       // std::move / std::move_if_noexcept — move semantics in ctors, reserve, insert, erase
#include "traits/trivial.h"
#include "vector_base.h"

namespace xstd
{

    /// <summary>
    /// A dynamically-sized array. Replacement for std::vector with no STL dependency
    /// and no exceptions. Uses raw allocation + placement-new so capacity can exceed
    /// the number of live elements, same as std::vector. Growth memcpy's elements
    /// tagged trivially-relocatable rather than move+destroy per element.
    /// </summary>
    template <typename DataType>
    class vector: public vector_base<DataType>
    {
        using base = vector_base<DataType>;

        public:
            /// <summary>
            /// Mutable random-access iterator, just a raw pointer into the internal buffer.
            /// Invalidated by any operation that reallocates (push_back/insert/reserve past capacity).
            /// </summary>
            using iterator = typename base::iterator;

            /// <summary>
            /// Const random-access iterator, just a raw const pointer into the internal buffer.
            /// Invalidated by any operation that reallocates (push_back/insert/reserve past capacity).
            /// </summary>
            using const_iterator = typename base::const_iterator;

      

        public:
            /// <summary>
            /// Default constructor. Produces an empty vector with no allocation.
            /// </summary>
            vector() = default;

            /// <summary>
            /// Erases the element at pos, shifting subsequent elements down by one.
            /// </summary>
            /// <param name="pos">Iterator to the element to erase.</param>
            /// <returns>Iterator to the element that now occupies pos's former position.</returns>
            iterator erase(iterator pos)
            {
                return erase(pos, pos + 1);
            }

            /// <summary>
            /// Erases the range [first, last), shifting subsequent elements down to fill the gap.
            /// </summary>
            /// <param name="first">Iterator to the first element to erase.</param>
            /// <param name="last">Iterator one past the last element to erase.</param>
            /// <returns>Iterator to the element that now occupies first's former position.</returns>
            iterator erase(iterator first, iterator last)
            {
                assert(first >= this->begin() && last <= this->end() && first <= last && "xstd::vector::erase — invalid range");

                size_t eraseCount = static_cast<size_t>(last - first);
                if (eraseCount == 0) return first;

                iterator writePos = first;
                iterator readPos = last;
                for (; readPos != this->end(); ++writePos, ++readPos)
                    *writePos = std::move_if_noexcept(*readPos);

                for (iterator p = writePos; p != this->end(); ++p)
                    p->~DataType();

                this->_Count -= eraseCount;
                return first;
            }

            /// <summary>
            /// Inserts a copy of value before pos, shifting subsequent elements up by one.
            /// May invalidate existing iterators if growth occurs.
            /// </summary>
            /// <param name="pos">Iterator indicating where to insert.</param>
            /// <param name="value">The value to copy in.</param>
            /// <returns>Iterator to the newly inserted element.</returns>
            iterator insert(iterator pos, const DataType& value)
            {
                size_t index = static_cast<size_t>(pos - this->begin());
                this->_grow_if_needed();
                pos = this->begin() + index;

                if (pos == this->end())
                {
                    ::new (this->_Data + this->_Count) DataType(value);
                }
                else
                {
                    ::new (this->_Data + this->_Count) DataType(std::move_if_noexcept(this->_Data[this->_Count - 1]));
                    for (iterator p = this->end() - 1; p != pos; --p)
                        *p = std::move_if_noexcept(*(p - 1));

                    *pos = value;
                }

                ++this->_Count;
                return pos;
            }

            /// <summary>
            /// Inserts value (moved) before pos, shifting subsequent elements up by one.
            /// May invalidate existing iterators if growth occurs.
            /// </summary>
            /// <param name="pos">Iterator indicating where to insert.</param>
            /// <param name="value">The value to move in.</param>
            /// <returns>Iterator to the newly inserted element.</returns>
            iterator insert(iterator pos, DataType&& value)
            {
                size_t index = static_cast<size_t>(pos - this->begin());
                this->_grow_if_needed();
                pos = this->begin() + index;

                if (pos == this->end())
                {
                    ::new (this->_Data + this->_Count) DataType(std::move(value));
                }
                else
                {
                    ::new (this->_Data + this->_Count) DataType(std::move_if_noexcept(this->_Data[this->_Count - 1]));
                    for (iterator p = this->end() - 1; p != pos; --p)
                        *p = std::move_if_noexcept(*(p - 1));

                    *pos = std::move(value);
                }

                ++this->_Count;
                return pos;
            }

            /// <summary>
            /// Appends a copy of value to the end, growing capacity if needed.
            /// </summary>
            /// <param name="value">The value to copy in.</param>
            void push_back(const DataType& value)
            {
                this->_grow_if_needed();
                ::new (this->_Data + this->_Count) DataType(value);
                ++this->_Count;
            }

            /// <summary>
            /// Appends value (moved) to the end, growing capacity if needed.
            /// </summary>
            /// <param name="value">The value to move in.</param>
            void push_back(DataType&& value)
            {
                this->_grow_if_needed();
                ::new (this->_Data + this->_Count) DataType(std::move(value));
                ++this->_Count;
            }

            /// <summary>
            /// Constructs a new element in place at the end, forwarding args directly to DataType's constructor. Growing capacity if needed.
            /// </summary>
            /// <param name="args">Constructor arguments forwarded to DataType.</param>
            /// <returns>Reference to the newly constructed element.</returns>
            template <typename... Args>
            DataType& emplace_back(Args&&... args)
            {
                this->_grow_if_needed();
                ::new (this->_Data + this->_Count) DataType(std::forward<Args>(args)...);
                return this->_Data[this->_Count++];
            }

            /// <summary>
            /// Destroys and removes the last element. UB if the vector is empty (matches std::vector — caller's responsibility to check).
            /// </summary>
            void pop_back()
            {
                --this->_Count;
                this->_Data[this->_Count].~DataType();
            }

    };

}



#endif
