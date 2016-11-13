- D0315R2
    + Give lambdas linkage in some purposes
    + In [temp.over.link], reword to something like this:
        To make sure this does not happen, we propose amending \textbf{[temp.over.link]
        14.5.6.1/5} as follows:
        \begin{quote}
          Two expressions involving template parameters are considered equivalent
          if two function definitions containing the expressions would satisfy the
          one-definition rule (3.2), except that the tokens used to name the template
          parameters may differ as long as a token used to name a template parameter
          in one expression is replaced by another token that names the same template
          parameter in the other expression. \added{Two lambda-expressions appearing
          in full expressions involving template parameters are never considered
          equivalent. [ \textit{Note:} The intent is to avoid lambda-expressions
          appearing in the signature of a function template with external linkage.
          \textit{-- end note} ]}
        \end{quote}


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
