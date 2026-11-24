#ifndef STRONG_LIB
#define STRONG_LIB

#include <compare>
#include <concepts>
#include <functional>
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

template <typename S>
struct is_strong : std::false_type {};

template <typename T, typename Tag, typename... Skills>
struct is_strong<strong<T, Tag, Skills...>> : std::true_type {};

template <typename Self, typename List>
struct bases;

template <typename Self, typename... Skills>
struct bases<Self, list<Skills...>> : public Skills::template mixin<Self>... {};

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

class skill_access {
 public:
  template <typename S>
    requires is_strong_v<S>
  static constexpr decltype(auto) get(S&& s) {
    return (std::forward<S>(s).value_);
  }
};

template <typename T, typename Tag, typename... Skills>
class strong
    : public strong_detail::bases<strong<T, Tag, Skills...>,
                                  strong_detail::flatten_t<skills<Skills...>>> {
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

#endif  // STRONG_LIB
