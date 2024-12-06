#include <memory>
#include <algorithm>
#include <ranges>
#include <utility>
#include <cstddef>
#include <cassert>
#include <cstring>
#include <cmath>

#include <format>
#include <print>

#include <chrono>
#include <numeric>
#include <vector>

#include <string>
#include <string_view>

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

std::string test_string;

template <typename Container>
Container createVector()
{
    Container v;

    constexpr int N = 10'000;
    v.reserve(N);

    for (int i = 0; i < N; ++i) {
        v.push_back(test_string);
    }

    return v;
}

template <typename Container, typename F>
void bench_impl(std::string_view container, std::string_view label, F &&fun)
{
    constexpr int RUNS = 1'000;

    using clock = std::chrono::high_resolution_clock;
    using resolution = std::chrono::nanoseconds;

    std::vector<resolution> results;
    results.reserve(RUNS);

    const Container original = createVector<Container>();

    for (int i = 0; i < RUNS; ++i) {
        Container v = original;
        v.reserve(v.capacity() + 10); // so that insertions don't reallocate

        const auto begin = clock::now();
        fun(v);
        const auto end = clock::now();

        if (i >= 100)
            results.push_back(std::chrono::duration_cast<resolution>(end - begin));
    }

    auto mean = std::accumulate(results.begin(), results.end(), resolution()) / results.size();

    // ... can't do math on std::chrono objects, need to go down to the representation ...
    using R = resolution::rep;
    auto variance_fun = [mean = mean.count()](R acc, resolution val) -> R {
        return acc + ((val.count() - mean) * (val.count() - mean));
    };

    auto variance = std::accumulate(results.begin(), results.end(), R(), variance_fun) / (results.size() - 1); // unbiased
    auto stddev = std::sqrt((double)variance);

    std::println("\n{:=^40}", label);
    std::println("CONTAINER : {}", container);
    std::println("Iterations: {}", RUNS);
    std::println("Mean      : {}", mean);
    std::println("Stddev    : {:.2f}", stddev);
}

template <typename String, typename F>
void safetyCheck(std::string_view label, F &&fun)
{
    auto container1 = createVector<std::vector<String>>();
    fun(container1);

    auto container2 = createVector<my_vector<String>>();
    fun(container2);

    std::println(stderr, "Safety check for {}", label);

    if (!std::ranges::equal(container1, container2)) {
        std::println(stderr, "Container differ");
        std::abort();
    }
}

template <typename F>
void bench(std::string_view label, F &&fun)
{

#if 1
    // safetyCheck<std::string>(label, fun);
    safetyCheck<replaceable_string>(label, fun);
#endif

    // bench_impl<std::vector<std::string>>("std::vector<std::string>", label, fun);
    bench_impl<my_vector<std::string>>("my_vector<std::string>", label, fun);

    // bench_impl<std::vector<replaceable_string>>("std::vector<replaceable_string>", label, fun);
    bench_impl<my_vector<replaceable_string>>("my_vector<replaceable_string>", label, fun);

    bench_impl<my_vector<trivially_relocatable_string>>("my_vector<trivially_relocatable_string>", label, fun);
}

int main()
{
    struct {
        std::string sso;
        std::string test_string;
    } ssoRuns[] = {
        { "Short string", "x" },
        { "Long string", "0123456789012345678901234567890123456789" }
    };

    for (const auto &run : ssoRuns) {
        test_string = run.test_string;

        bench(run.sso + " erase", [](auto &v) {
            v.erase(v.begin() + 100, v.begin() + 200);
        });

        bench(run.sso + " emplace", [](auto &v) {
            v.emplace(v.begin() + 100, test_string);
        });

        bench(run.sso + " reallocation", [](auto &v) {
            v.reserve(v.capacity() + 10); // force a reallocation
        });
    }

}
