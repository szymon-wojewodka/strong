#ifndef STRONG_LIB
#define STRONG_LIB

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

template <typename S>
struct is_strong : std::false_type {};

template <typename T, typename Tag, typename... Skills>
struct is_strong<strong<T, Tag, Skills...>> : std::true_type {};

template <typename Self, typename List>
struct bases;

template <typename Self, typename... Skills>
struct bases<Self, list<Skills...>> : public Skills::template mixin<Self>... {};
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
class strong : public strong_detail::bases<strong<T, Tag, Skills...>,
                                           strong_detail::list<Skills...>> {
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

#endif  // STRONG_LIB
