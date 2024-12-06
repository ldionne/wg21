#include <memory>
#include <algorithm>
#include <ranges>
#include <utility>

#include <cstddef>
#include <cassert>
#include <cstring>

#include <chrono>

#include <vector>
#include <string>

#include <benchmark/benchmark.h>

template <typename T>
constexpr inline bool is_trivially_relocatable_v =
    std::is_trivially_move_constructible_v<T> &&
    std::is_trivially_destructible_v<T>;

template <typename T>
constexpr inline bool is_replaceable_v =
    std::is_trivially_move_assignable_v<T> &&
    std::is_trivially_move_constructible_v<T> &&
    std::is_trivially_destructible_v<T>;

class replaceable_string : public std::string
{
public:
    using std::string::string;
    replaceable_string(const std::string &s)
        : std::string(s)
    {}
    replaceable_string(std::string &&s)
        : std::string(std::move(s))
    {}
};

class trivially_relocatable_string : public replaceable_string
{
public:
    using replaceable_string::replaceable_string;
};

template <>
constexpr inline bool is_replaceable_v<replaceable_string> = true;

template <>
constexpr inline bool is_replaceable_v<trivially_relocatable_string> = true;

#if defined(_LIBCPP_VERSION)
template <>
constexpr inline bool is_trivially_relocatable_v<trivially_relocatable_string> = true;
#endif

static_assert( std::is_nothrow_move_constructible_v<std::string>);
static_assert(!is_replaceable_v<std::string>);
static_assert(!is_trivially_relocatable_v<std::string>);

static_assert( std::is_nothrow_move_constructible_v<replaceable_string>);
static_assert( is_replaceable_v<replaceable_string>);
static_assert(!is_trivially_relocatable_v<replaceable_string>);

static_assert( std::is_nothrow_move_constructible_v<trivially_relocatable_string>);
static_assert( is_replaceable_v<trivially_relocatable_string>);
static_assert(
#if !defined(_LIBCPP_VERSION)
not
#endif
is_trivially_relocatable_v<trivially_relocatable_string>);


template <typename T>
T *relocate_at(T *dest, T *source)
{
    if constexpr (is_trivially_relocatable_v<T>) {
        ::memcpy(dest, source, sizeof(T)); // trivially_relocate_at
    } else {
        ::new ((void*)dest) T(std::move(*source));
        std::destroy_at(source);
    }
    return dest;
}

template <typename I, typename O>
I uninitialized_relocate(I first, I last, O output)
{
    // trivially relocate in batch, without relying on the optimizer
    if constexpr (
        std::contiguous_iterator<I> &&
        std::contiguous_iterator<O> &&
        is_trivially_relocatable_v<std::iter_value_t<I>>)
    {
        const auto distance = last - first;
        ::memmove(std::to_address(output),
                  std::to_address(first),
                  sizeof(std::iter_value_t<I>) * distance);

        return first + distance;
    } else {
        O outputCopy = output;

        try {
            for (; first != last; (void)++output, (void)++first)
                relocate_at(std::addressof(*output), std::addressof(*first));
        } catch (...) {
            std::destroy(first, last);
            std::destroy(outputCopy, output);
        }

        return first;
    }
}

template <typename I, typename O>
I uninitialized_relocate_backward(I first, I last, O output)
{
    if constexpr (
        std::contiguous_iterator<I> &&
        std::contiguous_iterator<O> &&
        is_trivially_relocatable_v<std::iter_value_t<I>>)
    {
        const auto distance = last - first;
        ::memmove(std::to_address(output) - distance,
                  std::to_address(first),
                  sizeof(std::iter_value_t<I>) * distance);

        return first + distance;
    } else {
        O outputCopy = output;

        try {
            while (first != last) {
                --last;
                --output;
                relocate_at(std::addressof(*output), std::addressof(*last));
            }
        } catch (...) {
            ++last;
            std::destroy(first, last);
            ++output;
            std::destroy(output, outputCopy);
        }

        return first;
    }
}

template <typename I, typename S, typename O>
I uninitialized_relocate_n(I first, S size, O output)
{
    if constexpr (
        std::contiguous_iterator<I> &&
        std::contiguous_iterator<O> &&
        is_trivially_relocatable_v<std::iter_value_t<I>>)
    {
        ::memmove(std::to_address(output),
                  std::to_address(first),
                  sizeof(std::iter_value_t<I>) * size);

        return first + size;
    } else {
        O outputCopy = output;
        S count = 0;
        try {
            for (; size > 0; (void)++output, (void)++first, (void)--size, (void)++count)
                relocate_at(std::addressof(*output), std::addressof(*first));
        } catch (...) {
            std::destroy_n(first, size);
            std::destroy_n(outputCopy, count);
            throw;
        }

        return first;
    }
}



template <class T>
class my_vector
{
    T *m_begin = nullptr;
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;

public:
    my_vector() = default;
    explicit my_vector(std::size_t count, const T &element)
    {
        my_vector v;
        v.reserve(count);
        while (count--)
            v.push_back(element);
        swap(v);
    }

    ~my_vector()
    {
        std::destroy_n(m_begin, m_size);
        ::free(m_begin);
    }

    my_vector(my_vector &&other) noexcept
        : m_begin(std::exchange(other.m_begin, nullptr)),
          m_size(std::exchange(other.m_size, 0)),
          m_capacity(std::exchange(other.m_capacity, 0))
    {}

    my_vector(const my_vector &other)
    {
        my_vector v;

        v.m_begin = (T *)::malloc(sizeof(T) * other.m_capacity);
        v.m_capacity = other.m_capacity;
        std::uninitialized_copy_n(other.m_begin, other.m_size, v.m_begin);
        v.m_size = other.m_size;

        swap(v);
    }


    // TODO, return actual iterator objects...
    using iterator = T *;
    using const_iterator = const T *;

    iterator begin() { return m_begin; }
    const_iterator begin() const { return m_begin; }
    iterator end() { return m_begin + m_size; }
    const_iterator end() const { return m_begin + m_size; }


    void reserve(std::size_t newCapacity)
    {
        if (newCapacity < m_size)
            return;
        reallocate(newCapacity);
    }

    void push_back(const T &value)
    {
        maybeReallocate();
        ::new (m_begin + m_size) T(value);
        ++m_size;
    }

    iterator erase(iterator position)
    {
        return erase(position, position + 1);
    }

    iterator erase(iterator first, iterator last)
    {
        assert(first <= last);
        assert(last <= end());

        const auto distance = last - first;
        if (distance == 0)
            return first;

        // std::vector::erase is "overspecified" in that it can't throw an exception,
        // unless one is thrown by, T's move assignment operator. If that happens, what can the user do?
        // "Some elements" between `first` and `end()` have been assigned to;
        // some between `last` and `end()` have been moved-from, and there's no way to know which is which.
        // Sure, one could inspect them and figure out what happened, but in the general case
        // the only safe recovery is to wipe out the entire [first, end()) tail.
        // Which is exactly what the uninitialized_relocate algorithm would do anyway.
        // So here we could either check for is_nothrow_move_constructible_v, or
        // put a try/catch around uninitialized_relocate and truncate the size.

        if constexpr (is_replaceable_v<T> && std::is_nothrow_move_constructible_v<T>) {
            // Relocation-based. Make a "window"
            std::destroy(first, last);

            // Relocate to the left
//            try {
                uninitialized_relocate(last, end(), first);
//            } catch (...) {
//                m_size = first - begin();
//                throw;
//            }
        } else {
            // Assignment-based. Move-assign to the left
            auto newEnd = std::move(last, end(), first);

            // Destroy the tail of moved-from objects
            std::destroy(newEnd, end());
        }

        m_size -= distance;
        return first;
    }

    template <typename ...Args>
    iterator emplace(iterator position, Args && ... args)
    {
        const auto index = position - begin();

        maybeReallocate(); // stupid, but whatever

        position = begin() + index;

        if (position == end()) {
            std::construct_at(std::to_address(position), std::forward<Args>(args)...);
            ++m_size;
            return position;
        }

        T tmp(std::forward<Args>(args)...);

        if constexpr (is_replaceable_v<T>) {
            try {
                // Make a window
                uninitialized_relocate_backward(position, end(), end() + 1);
                try {
                    // Construct a T in the window
                    std::construct_at(std::to_address(position), std::move(tmp));
                    ++m_size;
                } catch (...) {
                    // If this fails, then destroy the relocated tail (after the gap)
                    std::destroy(position + 1, end() + 1);
                    throw;
                }
            } catch (...) {
                // All elements after `position` have been already destroyed, one way or another
                // So just update the size
                m_size = position - begin();
                throw;
            }
        } else {
            // Move-construct the last element one position over
            std::construct_at(std::to_address(end()), std::move(*(end() - 1)));
            ++m_size;

            // Move-assign the rest of the tail one position over
            std::move_backward(position, end() - 2, end() - 1);

            // Move-assign the new T
            *position = std::move(tmp);
        }

        return position;
    }

    std::size_t capacity() const { return m_capacity; }
    std::size_t size() const { return m_size; }

    T &operator[](std::size_t i) { return m_begin[i]; }
    const T &operator[](std::size_t i) const { return m_begin[i]; }

    void swap(my_vector &other) noexcept
    {
        using std::swap;
        swap(m_begin, other.m_begin);
        swap(m_size, other.m_size);
        swap(m_capacity, other.m_capacity);
    }

private:
    void maybeReallocate()
    {
        if (m_size == m_capacity)
            reallocate(m_capacity + 32);
    }

    void reallocate(std::size_t newCapacity)
    {
        assert(newCapacity >= m_size);
        if (newCapacity == m_capacity)
            return;

        my_vector newVector;
        newVector.m_begin = (T *)::malloc(sizeof(T) * newCapacity);
        newVector.m_capacity = newCapacity;

        if constexpr (std::is_nothrow_move_constructible_v<T>) { // nothrow relocatable?
            uninitialized_relocate_n(m_begin, m_size, newVector.m_begin);
        } else {
            // "move if noexcept" - like?
            if constexpr (std::is_copy_constructible_v<T>)
                std::uninitialized_copy_n(m_begin, m_size, newVector.m_begin);
            else
                std::uninitialized_move_n(m_begin, m_size, newVector.m_begin);

            std::destroy_n(m_begin, m_size);
        }

        newVector.m_size = m_size;
        m_size = 0;
        swap(newVector);
    }
};

template <template <typename...> typename Container, typename T>
static void eraseBench(benchmark::State &state, const T &fillValue)
{
    constexpr size_t N = 1'000;
    const auto original = Container<T>(N, fillValue);

    using clock = std::chrono::high_resolution_clock;

    for (auto _ : state) {
        auto v = original;
        const auto begin = clock::now();
        v.erase(v.begin() + 100, v.begin() + 101);
        const auto end = clock::now();

        const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(end - begin);
        state.SetIterationTime(elapsed.count());
    }
}

template <template <typename...> typename Container, typename T>
static void emplaceBench(benchmark::State &state, const T &fillValue)
{
    constexpr size_t N = 1'000;
    const auto original = Container<T>(N, fillValue);

    using clock = std::chrono::high_resolution_clock;

    for (auto _ : state) {
        auto v = original;
        v.reserve(v.capacity() + 10);
        const auto begin = clock::now();
        v.emplace(v.begin() + 100, fillValue);
        const auto end = clock::now();

        const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(end - begin);
        state.SetIterationTime(elapsed.count());
    }
}

template <template <typename...> typename Container, typename T>
static void reallocationBench(benchmark::State &state, const T &fillValue)
{
    constexpr size_t N = 1'000;
    const auto original = Container<T>(N, fillValue);

    using clock = std::chrono::high_resolution_clock;

    for (auto _ : state) {
        auto v = original;
        const auto begin = clock::now();
        v.reserve(v.capacity() + 10); // force reallocation
        const auto end = clock::now();

        const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(end - begin);
        state.SetIterationTime(elapsed.count());
    }
}

const std::string SHORT_STRING = "x";
const std::string LONG_STRING = "0123456789012345678901234567890123456789";

// Erase
BENCHMARK_TEMPLATE2_CAPTURE(eraseBench, my_vector, std::string, MyVectorStdStringEraseShort, SHORT_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(eraseBench, my_vector, replaceable_string, MyVectorRepStringEraseShort, SHORT_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(eraseBench, my_vector, trivially_relocatable_string, MyVectorTCStringEraseShort, SHORT_STRING)->UseManualTime();

BENCHMARK_TEMPLATE2_CAPTURE(eraseBench, my_vector, std::string, MyVectorStdStringEraseLong, LONG_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(eraseBench, my_vector, replaceable_string, MyVectorRepStringEraseLong, LONG_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(eraseBench, my_vector, trivially_relocatable_string, MyVectorTCStringEraseLong, LONG_STRING)->UseManualTime();

BENCHMARK_TEMPLATE2_CAPTURE(eraseBench, std::vector, std::string, StdVectorStdStringEraseShort, SHORT_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(eraseBench, std::vector, replaceable_string, StdVectorRepStringEraseShort, SHORT_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(eraseBench, std::vector, trivially_relocatable_string, StdVectorTCStringEraseShort, SHORT_STRING)->UseManualTime();

BENCHMARK_TEMPLATE2_CAPTURE(eraseBench, std::vector, std::string, StdVectorStdStringEraseLong, LONG_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(eraseBench, std::vector, replaceable_string, StdVectorRepStringEraseLong, LONG_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(eraseBench, std::vector, trivially_relocatable_string, StdVectorTCStringEraseLong, LONG_STRING)->UseManualTime();

// Emplace
BENCHMARK_TEMPLATE2_CAPTURE(emplaceBench, my_vector, std::string, MyVectorStdStringEmplaceShort, SHORT_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(emplaceBench, my_vector, replaceable_string, MyVectorRepStringEmplaceShort, SHORT_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(emplaceBench, my_vector, trivially_relocatable_string, MyVectorTCStringEmplaceShort, SHORT_STRING)->UseManualTime();

BENCHMARK_TEMPLATE2_CAPTURE(emplaceBench, my_vector, std::string, MyVectorStdStringEmplaceLong, LONG_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(emplaceBench, my_vector, replaceable_string, MyVectorRepStringEmplaceLong, LONG_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(emplaceBench, my_vector, trivially_relocatable_string, MyVectorTCStringEmplaceLong, LONG_STRING)->UseManualTime();

BENCHMARK_TEMPLATE2_CAPTURE(emplaceBench, std::vector, std::string, StdVectorStdStringEmplaceShort, SHORT_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(emplaceBench, std::vector, replaceable_string, StdVectorRepStringEmplaceShort, SHORT_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(emplaceBench, std::vector, trivially_relocatable_string, StdVectorTCStringEmplaceShort, SHORT_STRING)->UseManualTime();

BENCHMARK_TEMPLATE2_CAPTURE(emplaceBench, std::vector, std::string, StdVectorStdStringEmplaceLong, LONG_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(emplaceBench, std::vector, replaceable_string, StdVectorRepStringEmplaceLong, LONG_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(emplaceBench, std::vector, trivially_relocatable_string, StdVectorTCStringEmplaceLong, LONG_STRING)->UseManualTime();

// Reallocate
BENCHMARK_TEMPLATE2_CAPTURE(reallocationBench, my_vector, std::string, MyVectorStdStringReallocateShort, SHORT_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(reallocationBench, my_vector, replaceable_string, MyVectorRepStringReallocateShort, SHORT_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(reallocationBench, my_vector, trivially_relocatable_string, MyVectorTCStringReallocateShort, SHORT_STRING)->UseManualTime();

BENCHMARK_TEMPLATE2_CAPTURE(reallocationBench, my_vector, std::string, MyVectorStdStringReallocateLong, LONG_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(reallocationBench, my_vector, replaceable_string, MyVectorRepStringReallocateLong, LONG_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(reallocationBench, my_vector, trivially_relocatable_string, MyVectorTCStringReallocateLong, LONG_STRING)->UseManualTime();

BENCHMARK_TEMPLATE2_CAPTURE(reallocationBench, std::vector, std::string, StdVectorStdStringReallocateShort, SHORT_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(reallocationBench, std::vector, replaceable_string, StdVectorRepStringReallocateShort, SHORT_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(reallocationBench, std::vector, trivially_relocatable_string, StdVectorTCStringReallocateShort, SHORT_STRING)->UseManualTime();

BENCHMARK_TEMPLATE2_CAPTURE(reallocationBench, std::vector, std::string, StdVectorStdStringReallocateLong, LONG_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(reallocationBench, std::vector, replaceable_string, StdVectorRepStringReallocateLong, LONG_STRING)->UseManualTime();
BENCHMARK_TEMPLATE2_CAPTURE(reallocationBench, std::vector, trivially_relocatable_string, StdVectorTCStringReallocateLong, LONG_STRING)->UseManualTime();


BENCHMARK_MAIN();