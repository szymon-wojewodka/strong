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
} // ops

namespace strong_detail {
    template <typename... Ts>
    struct list {};

    template <typename S> struct is_strong : std::false_type {};

    template <typename T, typename Tag, typename... Skills>
    struct is_strong<strong<T, Tag, Skills...>> : std::true_type {};

    template <typename Self, typename List> struct bases;

    template <typename Self, typename... Skills>
    struct bases<Self, list<Skills...>> : public Skills::template mixin<Self>... {};
} // strong_detail

template <typename S>
constexpr bool is_strong_v = strong_detail::is_strong<std::remove_cvref_t<S>>::value;

#endif // STRONG_LIB 
