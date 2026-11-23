#include "strong.h"

#ifdef NDEBUG
    #undef NDEBUG
#endif

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <numbers>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>

namespace {
    // Pomocnicze koncepty do sprawdzania, które operacje są dostępne.
    template <typename A, typename B>
    concept can_add = requires(A a, B b) { a + b; };

    template <typename A>
    concept can_compare = requires(A a) { a < a; };

    template <typename A>
    concept can_read = requires(A a) { a.value(); };

    template <typename A>
    concept can_hash = requires(A a) { std::hash<A>{}(a); };

    // Deklarujemy aliasy nazw umiejętności.
    template <typename Scalar>
    using arithmetic = skills<addable, scalable<Scalar>>;

    using comparable = skills<equality, ordered>;

    // Deklarujemy aliasy nazw kliku silnych typów.
    using seconds = strong<double, struct seconds_tag, arithmetic<double>, printable<"s">>;
    using speed = strong<double, struct speed_tag, arithmetic<double>, comparable, printable<"m/s">>;
    using meters = strong<double, struct meters_tag, arithmetic<double>, comparable, printable<"m">,
                          divisible_by<seconds, speed>, readable>;
    using kilograms = strong<double, struct kilograms_tag, arithmetic<double>, comparable>;
    using radians = strong<double, struct radians_tag, readable>;

    // Typy różniące się tylko znacznikiem są różnymi typami.
    static_assert(!std::is_same_v<meters, kilograms>);
    static_assert(!can_add<meters, kilograms>);
    static_assert(!can_add<meters, double>);
    static_assert(!std::is_convertible_v<double, meters>);

    // Obiekt silnego typu nie zajmuje więcej pamięci niż przechowywana wartość.
    static_assert(sizeof(meters) == sizeof(double));

    // Używamy wbudowanych umiejętności z własnymi implementacjami operacji.
    using degrees = strong<double, struct degrees_tag,
                           addable_with<[](double a, double b) { return std::fmod(a + b, 360.0); }>,
                           equality, printable<"°">,
                           convertible_to<radians, [](double d) { return d * std::numbers::pi / 180.0; }>>;

    // Bez umiejętności ordered i readable silny typ nie ma porównań ani metody value().
    static_assert(!can_compare<degrees> && !can_read<degrees>);

    // Tworzymy porządek leksykograficzny nierozróżniający wielkości liter.
    constexpr auto case_insensitive = [](const std::string& a, const std::string& b) {
        return std::lexicographical_compare_three_way(
            a.begin(), a.end(), b.begin(), b.end(),
            [](char x, char y) -> std::weak_ordering { return std::tolower(x) <=> std::tolower(y); });
    };

    constexpr auto case_insensitive_hash = [](const std::string& s) {
        std::string lower = s;
        std::ranges::transform(lower, lower.begin(), [](char c) { return std::tolower(c); });
        return std::hash<std::string>{}(lower);
    };

    // Umiejętność ordered_with dokłada równość zgodną z porządkiem, a hashable_with korzysta z tej równości.
    using word = strong<std::string, struct word_tag,
                        ordered_with<case_insensitive>, hashable_with<case_insensitive_hash>>;

    // Jawnie podana umiejętność ma pierwszeństwo przed zależnością.
    using exact_word = strong<std::string, struct exact_word_tag, ordered_with<case_insensitive>, equality>;

    // Umiejętność zdefiniowana przez użytkownika: łączenie ścieżek operatorem / i metoda depth().
    struct joinable {
        template <typename T>
        static constexpr bool accepts = std::is_same_v<T, std::string>;

        template <typename Self>
        struct mixin {
            friend Self operator/(const Self& a, const Self& b) {
                return Self(skill_access::get(a) + "/" + skill_access::get(b));
            }

            std::size_t depth() const {
                return std::ranges::count(skill_access::get(static_cast<const Self&>(*this)), '/');
            }
        };
    };

    using path = strong<std::string, struct path_tag, joinable, equality, printable<>>;

    // Umiejętność użytkownika z rodziny umiejętności wbudowanej: godziny są równe,
    // jeżeli wskazówka zegara pokazuje je w tym samym miejscu.
    struct same_on_dial {
        using family = ops::equality;

        template <typename Self>
        struct mixin {
            friend constexpr bool operator==(const Self& a, const Self& b) {
                return skill_access::get(a) % 12 == skill_access::get(b) % 12;
            }
        };
    };

    // Umiejętność hashable zależy od równości, a tę dostarcza same_on_dial.
    using hour = strong<int, struct hour_tag, same_on_dial,
                        hashable_with<[](int h) { return std::hash<int>{}(h % 12); }>>;

    // Użytkownika tworzy umiejętność z własną rodziną, zależnościami i operatorami ++.
    struct counting {};

    struct countable {
        using family = counting;
        using needs = skills<readable, equality>;

        template <typename T>
        static constexpr bool accepts = std::integral<T>;

        template <typename Self>
        struct mixin {
            constexpr Self& operator++() {
                Self& self = static_cast<Self&>(*this);
                ++skill_access::get(self);
                return self;
            }

            constexpr Self operator++(int) {
                Self old = static_cast<const Self&>(*this);
                ++*this;
                return old;
            }
        };
    };

    // Umiejętności readable i equality zostaną dołożone jako zależności.
    using counter = strong<int, struct counter_tag, countable>;
    static_assert(has_family_v<counter, ops::access> && has_family_v<counter, ops::equality>);

    // Wszystkie operacje poza wypisywaniem można wykonywać w czasie kompilowania.
    constexpr meters total = meters{1.0} + meters{2.0} * 2.0 + meters{0.5} / 2.0;
    static_assert(total == meters{5.25});
    static_assert(total > meters{5.0});
}

int main() {
    meters a{3.0}, b{4.5};
    meters c = a + b;
    assert(c == meters{7.5});
    assert(c.value() == 7.5);
    assert(2.0 * a == meters{6.0});

    bool e = a < b;
    assert(e);

    meters f = 2.0 * a;
    assert(f == meters{6.0});

    // Nie kompiluje się, próba dodawania wartości różnych typów.
    // [[maybe_unused]] auto u = a + seconds{1.0};

    // Nie kompiluje się, typ seconds nie umie porównywać.
    // [[maybe_unused]] auto w = seconds{1.0} < seconds{2.0};

    // Nie kompiluje się, ponieważ konstruktor jest jawny.
    // [[maybe_unused]] meters m = 3.0;

    // Iloraz odległości i czasu jest prędkością.
    speed v = meters{100.0} / seconds{8.0};
    assert(v == speed{12.5});

    degrees d{350.0};
    d += degrees{20.0};
    assert(d == degrees{10.0});

    radians r = static_cast<radians>(degrees{90.0});
    assert(std::abs(r.value() - std::numbers::pi / 2) < 1e-9);
    static_assert(!std::is_convertible_v<degrees, radians>);

    // Umiejętność ordered dokłada umiejętność equality.
    using length = strong<double, struct length_tag, ordered>;
    static_assert(has_family_v<length, ops::equality>);
    assert(length{1.0} == length{1.0});

    assert(word{"Ala"} == word{"ALA"});
    assert(!(exact_word{"Ala"} == exact_word{"ALA"}));
    assert((exact_word{"Ala"} <=> exact_word{"ALA"}) == std::weak_ordering::equivalent);

    std::unordered_set<word> words{word{"Ala"}, word{"ALA"}, word{"Ola"}};
    assert(words.size() == 2);
    static_assert(can_hash<word> && !can_hash<meters>);

    path p = path{"usr"} / path{"local"} / path{"bin"};
    assert(p == path{"usr/local/bin"});
    assert(p.depth() == 2);

    assert(hour{1} == hour{13} && hour{1} != hour{2});
    std::unordered_set<hour> hours{hour{1}, hour{13}, hour{25}, hour{2}};
    assert(hours.size() == 2);

    counter n;
    ++n;
    assert((n++).value() == 1);
    assert(n == counter{2});

    std::ostringstream os;
    os << c << ' ' << v << ' ' << d << ' ' << p;
    assert(os.str() == "7.5m 12.5m/s 10° usr/local/bin");

    // Nie kompiluje się, degrees nie ma umiejętności ordered.
    // d < d;

    // Nie kompiluje się, degrees nie ma umiejętności readable.
    // d.value();

    // Nie kompiluje się, meters nie ma umiejętności hashable.
    // std::hash<meters>{}(a);

    // Nie kompiluje się, konwersja jest tylko jawna.
    // [[maybe_unused]] radians x = degrees{90.0};

    // Nie kompiluje się, typ z const.
    // strong<const int, struct t1> x;

    // Nie kompiluje się, typ z volatile.
    // strong<volatile int, struct t1> x;

    // Nie kompiluje się, dwie umiejętności dodawania.
    // strong<int, struct t2, addable, addable_with<std::minus<>{}>> y;

    // Nie kompiluje się, wartości std::string nie da się mnożyć przez liczbę.
    // strong<std::string, struct t3, scalable<double>> z;

    // Nie kompiluje się, dwie umiejętności z rodziny ops::equality.
    // strong<int, struct t4, equality, same_on_dial> h;

    // Nie kompiluje się, countable obsługuje tylko typy całkowite.
    // strong<double, struct t5, countable> k;
}
