//////////////////////////////////////////////////////////////////////////////
// scratch pad

  for constexpr (auto x = init; pred(x); f(x)) {
    loop-body(x);
  }

  1. init must be a constant expression
  2. pred(x) must be constexpr
  3. f(x) must be constexpr (how do we change the type of x? can we at all?)





//////////////////////////////////////////////////////////////////////////////
// WITH NON-TYPE TEMPLATE PARAMETERS (ATTEMPT 1)
struct break_ { };

template <auto sequence, std::size_t i, std::size_t ...j, typename F>
void for_each_impl(std::index_sequence<i, j...>, F f) {
  auto result = f.template operator()<sequence[i]>();
  if constexpr (std::is_same<decltype(result), break_>{}) {
    return;
  } else {
    for_each_impl<sequence>(std::index_sequence<j...>{}, f);
  }
}

template <auto sequence, typename F>
constexpr void for_each(F f) {
  constexpr std::size_t size = sequence.size();
  using Indices = std::make_index_sequence<size>;
  for_each_impl<sequence>(Indices{}, f);
}






struct Foo { int i; long j; float k; };

int main() {
  Foo foo;
  constexpr std::vector<reflect::DataMember> members = reflexpr(foo).get_data_members();

  for_each<members>([&foo]<auto member>() {
    std::cout << foo.*unreflexpr(member) << std::endl;
    if constexpr (member.name() == "j") {
      return break_{};
    }
  });
}



























//////////////////////////////////////////////////////////////////////////////
// WITH NON-TYPE TEMPLATE PARAMETERS (ATTEMPT 2)
#define CONSTANT(...) \
  union { static constexpr auto value() { return __VA_ARGS__; } }

#define CONSTANT_VALUE(...) \
  [] { using R = CONSTANT(__VA_ARGS__); return R{}; }()





struct break_ { };

template <typename Sequence, std::size_t i, std::size_t ...j, typename F>
void for_each_impl(std::index_sequence<i, j...>, F f) {
  auto result = f.template operator()<CONSTANT(Sequence::value()[i])>();
  if constexpr (std::is_same<decltype(result), break_>{}) {
    return;
  } else {
    for_each_impl<Sequence>(std::index_sequence<j...>{}, f);
  }
}

template <typename Sequence, typename F>
constexpr void for_each(F f) {
  constexpr std::size_t size = Sequence::value().size();
  using Indices = std::make_index_sequence<size>;
  for_each_impl<Sequence>(Indices{}, f);
}











struct Foo { int i; long j; float k; };

int main() {
  Foo foo;
  constexpr std::vector<reflect::DataMember> members = reflexpr(foo).get_data_members();

  for_each<CONSTANT(members)>([&foo]<typename Member>() {
    std::cout << foo.*unreflexpr(Member::value()) << std::endl;
    if constexpr (Member::value().name() == "j") {
      return break_{};
    }
  });
}















//////////////////////////////////////////////////////////////////////////////
// WITH CONSTEXPR-FOR

struct Foo { int i; long j; float k; };

int main() {
  Foo foo;
  constexpr std::vector<reflect::DataMember> members = reflexpr(foo).get_data_members();

  for constexpr (reflect::DataMember member : members) {
    std::cout << foo.*unreflexpr(member) << std::endl;
    if constexpr (member.name() == "j") {
      break;
    }
  }
}









for_each<CONSTANT(members)>([&foo]<typename Member>() {
  std::cout << foo.*unreflexpr(Member::value()) << std::endl;
  if constexpr (Member::value().name() == "j") {
    return break_{};
  }
});

                    VS

for constexpr (reflect::DataMember member : members) {
  std::cout << foo.*unreflexpr(member) << std::endl;
  if constexpr (member.name() == "j") {
    break constexpr;
    ...
  } else {
    static_assert(member.name() != "j");
  }
}





////// Richard's scratch pad

constexpr reflect::stmts print(auto &foo, auto &members) {
  reflect::stmts results;
  for (auto m : members) {
    results += [{
      std::cout << foo.*unreflexpr(m) << std::endl;
    }];
    if (member.name() == "j")
      break;
  }
  return results;
}

vector<DataMember> members = ...;
<- print(foo, members);


















//////////////////////////////

int main() {
  Foo foo;
  constexpr std::vector<reflect::DataMember> members = reflexpr(foo).get_data_members();
  {
    do_something_with(members);
  }...
}


template <typename ...Args>
void f(Args ...args) {
  { use(args); }...;

  for ...(auto& arg : args) {
    use(arg);
  }
}

for ...(constexpr auto member : members) {
  use(member);
}
