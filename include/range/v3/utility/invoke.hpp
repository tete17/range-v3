/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2013-present
//  Copyright Casey Carter 2016
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//
#ifndef RANGES_V3_UTILITY_INVOKE_HPP
#define RANGES_V3_UTILITY_INVOKE_HPP

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <meta/meta.hpp>
#include <range/v3/detail/config.hpp>
#include <range/v3/utility/concepts.hpp>
#include <range/v3/utility/static_const.hpp>

RANGES_DIAGNOSTIC_PUSH
RANGES_DIAGNOSTIC_IGNORE_PRAGMAS
RANGES_DIAGNOSTIC_IGNORE_CXX17_COMPAT

// constexpr invoke leads to premature function template instantiation
// on clang-3.x and gcc-4.x
#ifndef RANGES_CONSTEXPR_INVOKE
#if !defined(__GNUC__) || \
    ((defined(__clang__) && __clang__ >= 4) || \
     (!defined(__clang__) && __GNUC__ >= 5))
#define RANGES_CONSTEXPR_INVOKE 1
#else
#define RANGES_CONSTEXPR_INVOKE 0
#endif
#endif

namespace ranges
{
    inline namespace v3
    {
        /// \addtogroup group-utility
        /// @{
        template<typename T>
        struct is_reference_wrapper
          : meta::if_<
                std::is_same<uncvref_t<T>, T>,
                std::false_type,
                is_reference_wrapper<uncvref_t<T>>>
        {};

        template<typename T>
        struct is_reference_wrapper<reference_wrapper<T>>
          : std::true_type
        {};

        template<typename T>
        struct is_reference_wrapper<std::reference_wrapper<T>>
          : std::true_type
        {};

        template<typename T>
        using is_reference_wrapper_t = meta::_t<is_reference_wrapper<T>>;

        struct invoke_fn
        {
        private:
            template<typename MemberFunctionPtr, typename First, typename... Rest>
            static constexpr auto invoke_member_fn(
                std::true_type, detail::any, MemberFunctionPtr fn, First &&first, Rest &&... rest)
            RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
            (
                (static_cast<First &&>(first).*fn)(static_cast<Rest &&>(rest)...)
            )
            template<typename MemberFunctionPtr, typename First, typename... Rest>
            static constexpr auto invoke_member_fn(
                std::false_type, std::true_type, MemberFunctionPtr fn, First &&first, Rest &&... rest)
            RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
            (
                (static_cast<First &&>(first).get().*fn)(static_cast<Rest &&>(rest)...)
            )
            template<typename MemberFunctionPtr, typename First, typename... Rest>
            static constexpr auto invoke_member_fn(
                std::false_type, std::false_type, MemberFunctionPtr fn, First &&first, Rest &&... rest)
            RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
            (
                ((*static_cast<First &&>(first)).*fn)(static_cast<Rest &&>(rest)...)
            )

            template<typename MemberDataPtr, typename First>
            static constexpr auto invoke_member_data(
                std::true_type, detail::any, MemberDataPtr ptr, First &&first)
            RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
            (
                static_cast<First &&>(first).*ptr
            )
            template<typename MemberDataPtr, typename First>
            static constexpr auto invoke_member_data(
                std::false_type, std::true_type, MemberDataPtr ptr, First &&first)
            RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
            (
                static_cast<First &&>(first).get().*ptr
            )
            template<typename MemberDataPtr, typename First>
            static constexpr auto invoke_member_data(
                std::false_type, std::false_type, MemberDataPtr ptr, First &&first)
            RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
            (
                (*static_cast<First &&>(first)).*ptr
            )
        public:
            template<typename F, typename Obj, typename First, typename... Rest,
                meta::if_c<detail::is_function<F>::value, int> = 0>
            constexpr auto operator()(F Obj::*ptr, First &&first, Rest &&... rest) const
            RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
            (
                invoke_fn::invoke_member_fn(
                    std::is_base_of<Obj, detail::decay_t<First>>{},
                    is_reference_wrapper_t<detail::decay_t<First>>{},
                    ptr, static_cast<First &&>(first), static_cast<Rest &&>(rest)...)
            )
            template<typename Data, typename Obj, typename First,
                meta::if_c<!detail::is_function<Data>::value, int> = 0>
            constexpr auto operator()(Data Obj::*ptr, First &&first) const
            RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
            (
                invoke_fn::invoke_member_data(
                    std::is_base_of<Obj, detail::decay_t<First>>{},
                    is_reference_wrapper_t<detail::decay_t<First>>{},
                    ptr, static_cast<First &&>(first))
            )
            template<typename F, typename... Args,
                meta::if_c<!std::is_member_pointer<uncvref_t<F>>::value, int> = 0>
            CONCEPT_PP_IIF(RANGES_CONSTEXPR_INVOKE)(CONCEPT_PP_EXPAND, CONCEPT_PP_EAT)(constexpr)
            auto operator()(F &&fn, Args &&... args) const
            RANGES_DECLTYPE_AUTO_RETURN_NOEXCEPT
            (
                static_cast<F &&>(fn)(static_cast<Args &&>(args)...)
            )
        };
        RANGES_INLINE_VARIABLE(invoke_fn, invoke)

        /// \cond
        namespace detail
        {
            template<typename T>
            struct reference_wrapper_
            {
                T *t_ = nullptr;
                constexpr reference_wrapper_() = default;
                constexpr reference_wrapper_(T &t) noexcept
                  : t_(std::addressof(t))
                {}
                constexpr reference_wrapper_(T &&) = delete;
                constexpr T &get() const noexcept
                {
                    return *t_;
                }
            };
            template<typename T>
            struct reference_wrapper_<T &> : reference_wrapper_<T>
            {
                using reference_wrapper_<T>::reference_wrapper_;
            };
            template<typename T>
            struct reference_wrapper_<T &&>
            {
                T *t_ = nullptr;
                constexpr reference_wrapper_() = default;
                constexpr reference_wrapper_(T &&t) noexcept
                  : t_(std::addressof(t))
                {}
                constexpr T &&get() const noexcept
                {
                    return static_cast<T &&>(*t_);
                }
            };
        }
        /// \endcond

        // Can be used to store rvalue references in addition to lvalue references.
        // Also, see: https://cplusplus.github.io/LWG/lwg-active.html#2993
        template<typename T>
        struct reference_wrapper : private detail::reference_wrapper_<T>
        {
        private:
            using base_ = detail::reference_wrapper_<T>;
        public:
            using type = meta::_t<std::remove_reference<T>>;
            using reference = meta::if_<std::is_reference<T>, T, T &>;

            constexpr reference_wrapper() = default;
            CONCEPT_template(typename U)(
                requires Constructible<base_, U>() &&
                    !Same<uncvref_t<U>, reference_wrapper>())
            (constexpr) reference_wrapper(U &&u)
                noexcept(std::is_nothrow_constructible<base_, U>::value)
              : detail::reference_wrapper_<T>{static_cast<U &&>(u)}
            {}
            constexpr reference get() const noexcept
            {
                return this->base_::get();
            }
            constexpr operator reference() const noexcept
            {
                return get();
            }
            CONCEPT_requires(!True(std::is_rvalue_reference<T>()))()
            operator std::reference_wrapper<type> () const noexcept
            {
                return {get()};
            }
            template<typename ...Args>
            constexpr auto operator()(Args &&...args) const
            RANGES_DECLTYPE_NOEXCEPT(
                invoke(std::declval<reference>(), std::declval<Args>()...))
            {
                return invoke(get(), static_cast<Args &&>(args)...);
            }
        };

        template<typename Sig, typename Enable /*= void*/>
        struct result_of {};
        template<typename Fun, typename...Args>
        struct result_of<Fun(Args...), meta::void_<
            decltype(invoke(std::declval<Fun>(), std::declval<Args>()...))>>
        {
            using type = decltype(invoke(std::declval<Fun>(), std::declval<Args>()...));
        };
    } // namespace v3
} // namespace ranges

RANGES_DIAGNOSTIC_POP

#endif // RANGES_V3_UTILITY_INVOKE_HPP
