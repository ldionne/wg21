- D0315R2
    + Give lambdas linkage in some purposes

// Scratchpad for string literals:

  template <char cs[3]> struct bar { }; // this actually decays and declares a ptr
  template <std::size_t N, char cs[N]> struct bar { }; // bar<"abcd"> is a problem
  template <char cs[auto]> struct bar { };
  template <constexpr char cs[auto]> struct bar { }; // constexpr somehow disables decay?
  template <auto Array[std::size_t N]> struct bar { };
  template <auto (&Array)[std::size_t N]> struct bar { };
  template <[with std::size_t N, typename T] T array[N]> struct bar { };
  template <std::initializer_list<char> T> struct bar { };

  template <char const* s> struct bar { };
  bar<"foobar">;

  template <typename T, std::size_t N>
    template <T cs[N]>
    struct bar { };

  template <auto Array> struct bar { };
  bar<std::array{1, 2, 3}>;
  bar<std::array{"foobar"}>;


  template <std::string_literal s>
  struct bar { };

  template <auto Array> {}; // deduces pointer
  template <auto Array[N]> {}; // what if N already exists?

  bar<"abcd"> x1;
  bar<{'a', 'b', 'c'}> x2;




// Supporting arbitrary UDTs in template arguments:

  struct Foo {
    int x;
    friend constexpr bool operator==(Foo const& a, Foo const& b) {
      return a.x == b.x;
    }
  };

  template <Foo foo>
  struct bar { };

  constexpr Foo a{1}, b{2};

  template <>
  struct bar<b> { };

  bar<a> x;

// Supporting constexpr function parameters:

  auto pow(feet ft, constexpr int arr) {
    static_assert(n == 0);
    if constexpr (n == 0) {
      return true;
    } else {
      return 0;
    }
  }

  auto f = &foo; // what is this address? We should probably prohibit that
  f(1);
  f(2);

  // We can't perfect-forward constexpr-ness through higher order functions,
  // which kills part of the use case.
