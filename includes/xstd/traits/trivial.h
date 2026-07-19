#pragma once


#ifndef __XSTD_TRIVIAL_H_GUARD
#define __XSTD_TRIVIAL_H_GUARD

#include <type_traits>


struct TriviallyRelocatable {}; // empty marker tag — inherit for your own types

template <typename T>
struct IsTriviallyRelocatable : std::bool_constant<std::is_base_of_v<TriviallyRelocatable, T> || std::is_trivially_copyable_v<T> > {};

template <typename T>
inline constexpr bool is_trivially_relocatable_v = IsTriviallyRelocatable<T>::value;

#endif
