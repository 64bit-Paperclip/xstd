#pragma once
// ============================================================================
// vector_base - Shared mechanics for vector-family containers
// ----------------------------------------------------------------------------
// File:        vector_base.h
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
// - Not meant to be used directly. This exists purely to share allocation,
//   storage, iteration, and indexed-access logic between vector<T> (unordered,
//   arbitrary-position mutation) and ordered_vector<T> (sorted, invariant-
//   preserving). Neither derived type exposes this base's constructors/
//   assignment as anything other than what gets inherited.
//
// - No exceptions. at() and operator[] assert in debug and do nothing in
//   release. Same tradeoff as everywhere else in xstd.
//
// - Trivially relocatable types (see traits/trivial.h) get memcpy'd during
//   growth instead of moved+destroyed element by element.
//
// - Never shrinks. reserve() only grows capacity.
//
// - Destructor is non-virtual. Safe as long as nothing ever deletes through
//   a vector_base<T>* pointer to a derived object that owns resources
//   beyond what this base already manages. Neither vector<T> nor
//   ordered_vector<T> currently does — if that ever changes, this needs
//   revisiting.
//
// - Not thread-safe.
// ============================================================================



#ifndef __XVECTOR_BASE_H_GUARD
#define __XVECTOR_BASE_H_GUARD

#include <cassert>          // assert — debug-only bounds/misuse checks, compiled out in release
#include <cstring>          // memcpy — bulk relocation for trivially-relocatable types during growth
#include <utility>          // std::move / std::move_if_noexcept — move semantics in ctors, reserve, insert, erase
#include <type_traits>      // std::is_base_of_v / std::is_trivially_copyable_v — backs the relocation trait
#include "traits/trivial.h"

namespace xstd
{
    template <typename DataType>
    class vector_base
    {
        public:
            /// <summary>
            /// Mutable random-access iterator, just a raw pointer into the internal buffer.
            /// Invalidated by any operation that reallocates (push_back/insert/reserve past capacity).
            /// </summary>
            using iterator = DataType*;

            /// <summary>
            /// Const random-access iterator, just a raw const pointer into the internal buffer.
            /// Invalidated by any operation that reallocates (push_back/insert/reserve past capacity).
            /// </summary>
            using const_iterator = const DataType*;

        protected:
            /// <summary>
            /// Pointer to the heap-allocated element buffer. Null on a default-constructed,
            /// never-reserved vector.
            /// </summary>
            DataType* _Data = nullptr;

            /// <summary>
            /// Number of live, constructed elements currently stored.
            /// </summary>
            size_t    _Count = 0;

            /// <summary>
            /// Allocated capacity, in elements. May exceed _Count.
            /// </summary>
            size_t    _ReservedCount = 0;

        protected:
            /// <summary>
            /// Raw allocation with no construction — mirrors operator new[] without invoking
            /// any element constructors.
            /// </summary>
            /// <param name="count">Number of elements' worth of raw storage to allocate.</param>
            /// <returns>A raw, uninitialized buffer sized for count elements, or null if count is 0.</returns>
            static DataType* _alloc_raw(size_t count)
            {
                if (count == 0) return nullptr;
                return static_cast<DataType*>(::operator new(count * sizeof(DataType)));
            }

            /// <summary>
            /// Grows capacity (doubling) if the vector is at capacity. No-op otherwise.
            /// </summary>
            void _grow_if_needed()
            {
                if (_Count < _ReservedCount) return;
                size_t newCap = (_ReservedCount == 0) ? 1 : _ReservedCount * 2;
                reserve(newCap);
            }

        public:
            /// <summary>
            /// Default constructor. Produces an empty vector with no allocation.
            /// </summary>
            vector_base() = default;

            /// <summary>
            /// Destructor. Destroys all live elements, then frees the backing buffer.
            /// </summary>
            ~vector_base()
            {
                clear();                              // destroy live elements
                ::operator delete(_Data);              // raw dealloc, no ctor/dtor calls
            }

            /// <summary>
                        /// Copy constructor. Deep-copies the source vector's elements into a fresh allocation.
                        /// </summary>
            vector_base(const vector_base& other)
            {
                reserve(other._Count);
                for (size_t i = 0; i < other._Count; ++i)
                    ::new (_Data + i) DataType(other._Data[i]);

                _Count = other._Count;
            }

            /// <summary>
            /// Copy assignment operator. Deep-copies the source vector's elements, reallocating
            /// only if existing capacity is insufficient.
            /// </summary>
            vector_base& operator=(const vector_base& other)
            {
                if (this == &other)
                    return *this;

                clear();

                if (_ReservedCount < other._Count)
                {
                    ::operator delete(_Data);
                    _Data = _alloc_raw(other._Count);
                    _ReservedCount = other._Count;
                }

                for (size_t i = 0; i < other._Count; ++i)
                    ::new (_Data + i) DataType(other._Data[i]);

                _Count = other._Count;

                return *this;
            }

            /// <summary>
            /// Move constructor. Takes ownership of the source's buffer directly; the source is left empty.
            /// </summary>
            vector_base(vector_base&& other) noexcept
                : _Data(other._Data), _Count(other._Count), _ReservedCount(other._ReservedCount)
            {
                other._Data = nullptr;
                other._Count = other._ReservedCount = 0;
            }

            /// <summary>
            /// Move assignment operator. Frees this vector's existing buffer, then takes ownership
            /// of the source's buffer directly; the source is left empty.
            /// </summary>
            vector_base& operator=(vector_base&& other) noexcept
            {
                if (this == &other)
                    return *this;

                clear();
                ::operator delete(_Data);

                _Data = other._Data;
                _Count = other._Count;
                _ReservedCount = other._ReservedCount;
                other._Data = nullptr;
                other._Count = other._ReservedCount = 0;

                return *this;
            }

            /// <summary>
            /// Indexed element access. Asserts in debug that the vector has real backing storage
            /// and that the index is strictly less than size(). No check in release — trusts the
            /// caller, matches std::vector's unchecked contract.
            /// </summary>
            /// <param name="index">The element index. Must be less than size().</param>
            /// <returns>Reference to the element at the given index.</returns>
            DataType& operator[](size_t index)
            {
                assert(_Data != nullptr && "xstd::vector_base::operator[] — called on empty/unassigned vector");
                assert(index < _Count && "xstd::vector_base::operator[] — index out of range");
                return _Data[index];
            }

            /// <summary>
            /// Indexed element access (const). Asserts in debug that the vector has real backing storage
            /// and that the index is strictly less than size(). No check in release.
            /// </summary>
            /// <param name="index">The element index. Must be less than size().</param>
            /// <returns>Const reference to the element at the given index.</returns>
            const DataType& operator[](size_t index) const
            {
                assert(_Data != nullptr && "xstd::vector_base::operator[] — called on empty/unassigned vector");
                assert(index < _Count && "xstd::vector_base::operator[] — index out of range");
                return _Data[index];
            }

            /// <summary>
            /// Checked element access. Asserts in debug that the index is strictly less than size().
            /// No check in release — trusts the caller.
            /// </summary>
            /// <param name="index">The element index. Must be less than size().</param>
            /// <returns>Reference to the element at the given index.</returns>
            DataType& at(size_t index)
            {
                assert(index < _Count && "xstd::vector_base::at — index out of range");
                return _Data[index];
            }

            /// <summary>
            /// Checked element access (const). Asserts in debug that the index is strictly less than
            /// size(). No check in release — trusts the caller.
            /// </summary>
            /// <param name="index">The element index. Must be less than size().</param>
            /// <returns>Const reference to the element at the given index.</returns>
            const DataType& at(size_t index) const
            {
                assert(index < _Count && "xstd::vector_base::at — index out of range");
                return _Data[index];
            }

            /// <summary>
            /// Reference to the last element. Asserts in debug that the vector is non-empty.
            /// </summary>
            DataType& back()
            {
                assert(_Count > 0 && "xstd::vector_base::back — called on empty vector");
                return _Data[_Count - 1];
            }

            /// <summary>
            /// Const reference to the last element. Asserts in debug that the vector is non-empty.
            /// </summary>
            const DataType& back() const
            {
                assert(_Count > 0 && "xstd::vector_base::back — called on empty vector");
                return _Data[_Count - 1];
            }

            /// <summary>
            /// Reference to the first element. Asserts in debug that the vector is non-empty.
            /// </summary>
            DataType& front()
            {
                assert(_Count > 0 && "xstd::vector_base::front — called on empty vector");
                return _Data[0];
            }

            /// <summary>
            /// Const reference to the first element. Asserts in debug that the vector is non-empty.
            /// </summary>
            const DataType& front() const
            {
                assert(_Count > 0 && "xstd::vector_base::front — called on empty vector");
                return _Data[0];
            }

            /// <summary>
            /// Raw pointer to the internal buffer. May be null if the vector has never allocated.
            /// </summary>
            DataType* data() { return _Data; }

            /// <summary>
            /// Const raw pointer to the internal buffer. May be null if the vector has never allocated.
            /// </summary>
            const DataType* data() const { return _Data; }

            /// <summary>
            /// The number of live elements currently stored.
            /// </summary>
            size_t size() const { return _Count; }

            /// <summary>
            /// The current allocated capacity, in elements.
            /// </summary>
            size_t capacity() const { return _ReservedCount; }

            /// <summary>
            /// Returns true if the vector contains no elements.
            /// </summary>
            bool empty() const { return _Count == 0; }

            /// <summary>
            /// Mutable iterator to the first element.
            /// </summary>
            iterator begin() { return _Data; }

            /// <summary>
            /// Mutable iterator one past the last element.
            /// </summary>
            iterator end() { return _Data + _Count; }

            /// <summary>
            /// Const iterator to the first element.
            /// </summary>
            const_iterator begin() const { return _Data; }

            /// <summary>
            /// Const iterator one past the last element.
            /// </summary>
            const_iterator end() const { return _Data + _Count; }

            /// <summary>
            /// Const iterator to the first element. Always const, regardless of the object's own constness.
            /// </summary>
            const_iterator cbegin() const { return _Data; }

            /// <summary>
            /// Const iterator one past the last element. Always const, regardless of the object's own constness.
            /// </summary>
            const_iterator cend() const { return _Data + _Count; }


            /// <summary>
            /// Destroys all live elements. Capacity is left untouched — no deallocation occurs.
            /// </summary>
            void clear()
            {
                for (size_t i = 0; i < _Count; ++i)
                    _Data[i].~DataType();
                _Count = 0;
                // capacity intentionally untouched — matches std::vector::clear()
            }

            /// <summary>
            /// Grows capacity to at least newCap elements, preserving existing elements. Never
            /// shrinks — a request smaller than or equal to current capacity is a no-op. Elements
            /// tagged trivially-relocatable are memcpy'd; everything else is moved (or copied, if
            /// the move constructor isn't noexcept) and the old copy explicitly destroyed.
            /// </summary>
            /// <param name="newCap">The minimum capacity, in elements, to reserve.</param>
            void reserve(size_t newCap)
            {
                if (newCap <= _ReservedCount) return; // never shrinks, matches std::vector

                DataType* newData = _alloc_raw(newCap);

                if constexpr (is_trivially_relocatable_v<DataType>)
                {
                    std::memcpy(newData, _Data, _Count * sizeof(DataType));
                    // no destructor calls needed — old memory is treated as dead, never touched again
                }
                else
                {
                    for (size_t i = 0; i < _Count; ++i)
                    {
                        ::new (newData + i) DataType(std::move_if_noexcept(_Data[i]));
                        _Data[i].~DataType();
                    }
                }

                ::operator delete(_Data);
                _Data = newData;
                _ReservedCount = newCap;
            }
    };


}

#endif