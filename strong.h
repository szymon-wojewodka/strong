#ifndef STRONG_LIB
#define STRONG_LIB

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <ostream>
#include <type_traits>
#include <utility>

template <typename T, typename Tag, typename... Skills>
class strong;

template <typename... Skills>
struct skills {};

namespace ops {
struct access {};
struct addition {};
struct scaling {};
struct equality {};
struct ordering {};
struct printing {};
struct hashing {};
}  // namespace ops

namespace strong_detail {
template <typename... Ts>
struct list {};

template <typename... Lists>
struct concat;

template <>
struct concat<> {
  using type = list<>;
};

template <typename... Ts>
struct concat<list<Ts...>> {
  using type = list<Ts...>;
};

template <typename... As, typename... Bs, typename... Rest>
struct concat<list<As...>, list<Bs...>, Rest...>
    : concat<list<As..., Bs...>, Rest...> {};

template <typename... Lists>
using concat_t = typename concat<Lists...>::type;

template <typename Skill>
struct flatten {
  using type = list<Skill>;
};

template <typename... Skills>
struct flatten<skills<Skills...>> {
  using type = concat_t<typename flatten<Skills>::type...>;
};

template <typename Skill>
using flatten_t = typename flatten<Skill>::type;

template <typename Skill>
struct needs_of {
  using type = list<>;
};

template <typename Skill>
  requires requires { typename Skill::needs; }
struct needs_of<Skill> {
  using type = flatten_t<typename Skill::needs>;
};

template <typename Skill>
using needs_of_t = typename needs_of<Skill>::type;

template <typename Skill, typename Family>
concept in_family = requires { typename Skill::family; } &&
                    std::same_as<typename Skill::family, Family>;

template <typename A, typename B>
concept same_family =
    requires { typename B::family; } && in_family<A, typename B::family>;

template <typename A, typename B>
concept clashes = std::same_as<A, B> || same_family<A, B>;

template <typename Done, typename Queue>
struct closure;

template <typename... Done>
struct closure<list<Done...>, list<>> {
  using type = list<Done...>;
};

template <typename... Done, typename Next, typename... Rest>
struct closure<list<Done...>, list<Next, Rest...>>
    : std::conditional_t<(clashes<Done, Next> || ...),
                         closure<list<Done...>, list<Rest...>>,
                         closure<list<Done..., Next>,
                                 concat_t<list<Rest...>, needs_of_t<Next>>>> {};

template <typename Explicit>
struct full_list;

template <typename... Skills>
struct full_list<list<Skills...>>
    : closure<list<Skills...>, concat_t<needs_of_t<Skills>...>> {};

template <typename... Skills>
using full_list_t = typename full_list<flatten_t<skills<Skills...>>>::type;

template <typename Family, typename List>
struct list_has_family;

template <typename Family, typename... Skills>
struct list_has_family<Family, list<Skills...>>
    : std::bool_constant<(in_family<Skills, Family> || ...)> {};

template <typename S>
struct is_strong : std::false_type {};

template <typename T, typename Tag, typename... Skills>
struct is_strong<strong<T, Tag, Skills...>> : std::true_type {};

template <typename S, typename Family>
struct has_family : std::false_type {};

template <typename T, typename Tag, typename... Skills, typename Family>
struct has_family<strong<T, Tag, Skills...>, Family>
    : list_has_family<Family, full_list_t<Skills...>> {};

template <typename Self, typename List>
struct bases;

template <typename Self, typename... Skills>
struct bases<Self, list<Skills...>> : public Skills::template mixin<Self>... {};

template <std::size_t N>
struct fixed_string {
  char data[N];

  constexpr fixed_string(const char (&str)[N]) { std::copy_n(str, N, data); }
};

struct std_hash {
  template <typename T>
  constexpr std::size_t operator()(const T& value) const {
    return std::hash<T>{}(value);
  }
};

template <auto Cmp>
struct equal_by_order {
  constexpr bool operator()(const auto& a, const auto& b) const {
    return Cmp(a, b) == 0;
  }
};
}  // namespace strong_detail

template <typename S>
constexpr bool is_strong_v =
    strong_detail::is_strong<std::remove_cvref_t<S>>::value;

template <typename S, typename Family>
constexpr bool has_family_v = strong_detail::has_family<S, Family>::value;

class skill_access {
 public:
  template <typename S>
    requires is_strong_v<S>
  static constexpr decltype(auto) get(S&& s) {
    return (std::forward<S>(s).value_);
  }
};

namespace strong_detail {
template <typename X>
constexpr const auto& value_of(const X& x) {
  if constexpr (is_strong_v<X>) {
    return skill_access::get(x);
  } else {
    return x;
  }
}
}  // namespace strong_detail

template <typename T, typename Tag, typename... Skills>
class strong
    : public strong_detail::bases<strong<T, Tag, Skills...>,
                                  strong_detail::full_list_t<Skills...>> {
 public:
  using value_type = T;
  using tag_type = Tag;

  constexpr strong()
    requires std::default_initializable<T>
      : value_() {}

  template <typename... Args>
    requires(sizeof...(Args) > 0) && std::constructible_from<T, Args...> &&
            (!is_strong_v<Args> && ...)
  constexpr explicit strong(Args&&... args)
      : value_(std::forward<Args>(args)...) {}

 private:
  friend skill_access;

  T value_;
};

struct readable {
  using family = ops::access;

  template <typename Self>
  struct mixin {
    constexpr const auto& value() const {
      return skill_access::get(static_cast<const Self&>(*this));
    }
  };
};

template <auto Eq>
struct equality_with {
  using family = ops::equality;

  template <typename Self>
  struct mixin {
    friend constexpr bool operator==(const Self& a, const Self& b) {
      return Eq(skill_access::get(a), skill_access::get(b));
    }
  };
};

using equality = equality_with<std::equal_to<>{}>;

template <auto Op>
struct addable_with {
  using family = ops::addition;

  template <typename Self>
  struct mixin {
    friend constexpr Self operator+(const Self& a, const Self& b) {
      return Self(Op(skill_access::get(a), skill_access::get(b)));
    }

    constexpr Self& operator+=(const Self& b) {
      Self& self = static_cast<Self&>(*this);
      self = self + b;
      return self;
    }
  };
};

using addable = addable_with<std::plus<>{}>;

template <typename Scalar>
struct scalable {
  using family = ops::scaling;

  template <typename Self>
  struct mixin {
    friend constexpr Self operator*(const Self& a, const Scalar& b) {
      return Self(skill_access::get(a) * b);
    }

    friend constexpr Self operator*(const Scalar& b, const Self& a) {
      return a * b;
    }

    friend constexpr Self operator/(const Self& a, const Scalar& b) {
      return Self(skill_access::get(a) / b);
    }

    constexpr Self& operator*=(const Scalar& b) {
      Self& self = static_cast<Self&>(*this);
      self = self * b;
      return self;
    }

    constexpr Self& operator/=(const Scalar& b) {
      Self& self = static_cast<Self&>(*this);
      self = self / b;
      return self;
    }
  };
};

template <auto Cmp>
struct ordered_with {
  using family = ops::ordering;

  using needs = equality_with<strong_detail::equal_by_order<Cmp>{}>;

  template <typename Self>
  struct mixin {
    friend constexpr auto operator<=>(const Self& a, const Self& b) {
      return Cmp(skill_access::get(a), skill_access::get(b));
    }
  };
};

using ordered = ordered_with<std::compare_three_way{}>;

template <strong_detail::fixed_string Suffix = "">
struct printable {
  using family = ops::printing;

  template <typename Self>
  struct mixin {
    friend std::ostream& operator<<(std::ostream& os, const Self& a) {
      return os << skill_access::get(a) << Suffix.data;
    }
  };
};

template <typename Target, auto Conv = std::identity{}>
struct convertible_to {
  template <typename Self>
  struct mixin {
    constexpr explicit operator Target() const {
      return Target(Conv(skill_access::get(static_cast<const Self&>(*this))));
    }
  };
};

template <typename Other, typename Result, auto Op = std::divides<>{}>
struct divisible_by {
  template <typename Self>
  struct mixin {
    friend constexpr Result operator/(const Self& a, const Other& o) {
      return Result(Op(skill_access::get(a), strong_detail::value_of(o)));
    }
  };
};

template <auto H>
struct hashable_with {
  using family = ops::hashing;
  using needs = equality;

  template <typename Self>
  struct mixin {
   private:
    friend struct std::hash<Self>;

    static constexpr std::size_t hash(const Self& s) {
      return H(skill_access::get(s));
    }
  };
};

using hashable = hashable_with<strong_detail::std_hash{}>;

template <typename T, typename Tag, typename... Skills>
  requires has_family_v<strong<T, Tag, Skills...>, ops::hashing>
struct std::hash<strong<T, Tag, Skills...>> {
  constexpr std::size_t operator()(const strong<T, Tag, Skills...>& s) const {
    return strong<T, Tag, Skills...>::hash(s);
  }
};

#endif  // STRONG_LIB
