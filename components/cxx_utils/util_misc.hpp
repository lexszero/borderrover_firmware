#pragma once

#include <bits/c++config.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <optional>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <vector>

template <typename... T>
constexpr auto make_array(T&&... values) ->
	std::array<
		typename std::decay<
			typename std::common_type<T...>::type>::type,
			sizeof...(T)>
		{
			return std::array<
				typename std::decay<
					typename std::common_type<T...>::type>::type,
					sizeof...(T)>{std::forward<T>(values)...};
		}

template <typename E>
constexpr auto to_underlying(E e) noexcept
{
	return static_cast<std::underlying_type_t<E>>(e);
}

template <size_t N, typename... Types>
using lookup_table = std::array<std::tuple<Types...>, N>;

namespace detail {

template <typename T>
struct compare
{
	static inline bool is_eq(const T& x, const T& y)
	{
		return x == y;
	}
};

template <>
struct compare<const char*>
{
	static inline bool is_eq(const char* x, const char* y)
	{
		return std::strcmp(x, y) == 0;
	}
};

}

/** Perform lookup in an array of tuples.
 * 
 * @param table		   Lookup table of type `std::array<std::tuple<Types...>, N`
 * @param key			 Key to look up. Must be of one of the tuple element types.
 * @param default_value   Default value in case `key` is not found
 */
template <size_t N, typename Tin, typename Tout, typename... Types>
const Tout lookup(const lookup_table<N, Types...>& table, const Tin key, const Tout default_value)
{
	for (auto item : table)
	{
		if (detail::compare<Tin>::is_eq(std::get<Tin>(item), key))
		{
			return std::get<Tout>(item);
		}
	}
	return default_value;
}

/** Perform lookup in an array of tuples.
 * 
 * @param table		   Lookup table of type `std::array<std::tuple<Types...>, N`
 * @param key			 Key to look up. Must be of one of the tuple element types.
 */
template <size_t N, typename Tin, typename Tout, typename... Types>
const Tout lookup(const lookup_table<N, Types...>& table, const Tin key)
{
	for (auto item : table)
	{
		if (detail::compare<Tin>::is_eq(std::get<Tin>(item), key))
		{
			return std::get<Tout>(item);
		}
	}
	throw std::invalid_argument("Unknown name " + to_string(key));
}

/**
 * Perform lookup in an array of tuples.
 *
 * @param table		   Lookup table of type `std::array<std::tuple<Types...>, N`
 * @param key			 Key to look up. Must be of one of the tuple element types.
 * @param result		  Mapped item if key was found, otherwise value is not changed.
 * @return				True if key was found
 */
template <size_t N, typename Tin, typename Tout, typename... Types>
bool checked_lookup(const lookup_table<N, Types...>& table, const Tin key, Tout& result)
{
	for (const auto& item : table)
	{
		if (detail::compare<Tin>::is_eq(std::get<Tin>(item), key))
		{
			result = std::get<Tout>(item);
			return true;
		}
	}
	return false;
}

/** Hides underlying type to enforce stricter typechecking */
template <typename Tag, typename ValueType>
struct newtype
{
	using type = ValueType;
	newtype() = default;
	newtype(const ValueType& val) : value(val) {};
	ValueType value;
};

/**
 * Pretty-prints an iterable container
 * ItemType must have operator<< implemented
 */
template <typename Iterator>
std::ostream& serialize_iterable(std::ostream& os, Iterator begin, Iterator end)
{
	os << "[";
	if (begin != end) {
		os << static_cast<typename Iterator::value_type>(*begin);
		for (Iterator it = std::next(begin); it != end; ++it)
		{
			os << ", " << static_cast<typename Iterator::value_type>(*it);
		}
	}
	os << "]";
	return os;
}

/** Pretty print std::array<> */
template <typename ItemType, size_t N>
std::ostream& operator<<(std::ostream& os, const std::array<ItemType, N>& val)
{
	return serialize_iterable(os, val.begin(), val.end());
}

/** Pretty print std::vector<> */
template <typename ItemType>
std::ostream& operator<<(std::ostream& os, const std::vector<ItemType>& val)
{
	return serialize_iterable(os, val.begin(), val.end());
}

/** Pretty print std::optional<> */
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::optional<T>& val)
{
	if (val)
		return os << *val;
	else
		return os << "<empty>";
}

/** Pretty print time */
template <class Clock, typename ...Args>
std::ostream& operator<<(std::ostream& os, const std::chrono::time_point<Clock, Args...> t)
{
	auto time = Clock::to_time_t(t);
	return os << std::put_time(std::localtime(&time), "%T");
}

/** Pretty print exceptions */
std::ostream& operator<<(std::ostream& os, const std::exception& exc);

/**
 * Implement generic to_string() for types that don't have it by exploiting
 * existing ostream::operator<< implementations
 */
template <typename T>
std::string to_string(const T& value)
{
	std::ostringstream os;
	os << value;
	return os.str();
}

std::string formatBytes(size_t bytes);
