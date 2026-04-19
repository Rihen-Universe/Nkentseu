#pragma once

#ifndef NKENTSEU_CORE_NKTRAITS_H_INCLUDED
#define NKENTSEU_CORE_NKTRAITS_H_INCLUDED

#include "NkTypes.h"
#include <type_traits>

namespace nkentseu {
    
    namespace traits {

        template <typename T, T V>
        struct NkIntegralConstant {
            static constexpr T value = V;
            using value_type = T;
            using type = NkIntegralConstant<T, V>;
            constexpr operator value_type() const noexcept { return value; }
            constexpr value_type operator()() const noexcept { return value; }
        };

        template <nk_bool V>
        using NkBoolConstant = NkIntegralConstant<nk_bool, V>;

        using NkTrueType = NkBoolConstant<true>;
        using NkFalseType = NkBoolConstant<false>;
        inline constexpr bool NkTrueType_v = NkTrueType::value;
        inline constexpr bool NkFalseType_v = NkFalseType::value;

        template <typename...>
        using NkVoidT = void;

        template <typename T>
        struct NkTypeIdentity {
            using type = T;
        };

        template <typename T>
        using NkTypeIdentity_t = typename NkTypeIdentity<T>::type;

        template <nk_bool B, typename T = void>
        struct NkEnableIf {};

        template <typename T>
        struct NkEnableIf<true, T> {
            using type = T;
        };

        template <nk_bool B, typename T = void>
        using NkEnableIf_t = typename NkEnableIf<B, T>::type;

        template <nk_bool B, typename T, typename F>
        struct NkConditional {
            using type = T;
        };

        template <typename T, typename F>
        struct NkConditional<false, T, F> {
            using type = F;
        };

        template <nk_bool B, typename T, typename F>
        using NkConditional_t = typename NkConditional<B, T, F>::type;

        template <typename T, typename U>
        struct NkIsSame : NkFalseType {};

        template <typename T>
        struct NkIsSame<T, T> : NkTrueType {};

        template <typename T, typename U>
        inline constexpr nk_bool NkIsSame_v = NkIsSame<T, U>::value;

        template <typename T>
        struct NkRemoveConst {
            using type = T;
        };

        template <typename T>
        struct NkRemoveConst<const T> {
            using type = T;
        };

        template <typename T>
        using NkRemoveConst_t = typename NkRemoveConst<T>::type;

        template <typename T>
        struct NkRemoveVolatile {
            using type = T;
        };

        template <typename T>
        struct NkRemoveVolatile<volatile T> {
            using type = T;
        };

        template <typename T>
        using NkRemoveVolatile_t = typename NkRemoveVolatile<T>::type;

        template <typename T>
        struct NkRemoveCV {
            using type = NkRemoveVolatile_t<NkRemoveConst_t<T>>;
        };

        template <typename T>
        using NkRemoveCV_t = typename NkRemoveCV<T>::type;

        template <typename T>
        struct NkRemoveReference {
            using type = T;
        };

        template <typename T>
        struct NkRemoveReference<T&> {
            using type = T;
        };

        template <typename T>
        struct NkRemoveReference<T&&> {
            using type = T;
        };

        template <typename T>
        using NkRemoveReference_t = typename NkRemoveReference<T>::type;

        template <typename T>
        struct NkAddLValueReference {
            using type = T&;
        };

        template <>
        struct NkAddLValueReference<void> {
            using type = void;
        };

        template <>
        struct NkAddLValueReference<const void> {
            using type = const void;
        };

        template <>
        struct NkAddLValueReference<volatile void> {
            using type = volatile void;
        };

        template <>
        struct NkAddLValueReference<const volatile void> {
            using type = const volatile void;
        };

        template <typename T>
        using NkAddLValueReference_t = typename NkAddLValueReference<T>::type;

        template <typename T>
        struct NkAddRValueReference {
            using type = T&&;
        };

        template <>
        struct NkAddRValueReference<void> {
            using type = void;
        };

        template <>
        struct NkAddRValueReference<const void> {
            using type = const void;
        };

        template <>
        struct NkAddRValueReference<volatile void> {
            using type = volatile void;
        };

        template <>
        struct NkAddRValueReference<const volatile void> {
            using type = const volatile void;
        };

        template <typename T>
        using NkAddRValueReference_t = typename NkAddRValueReference<T>::type;

        template <typename T>
        struct NkRemovePointer {
            using type = T;
        };

        template <typename T>
        struct NkRemovePointer<T*> {
            using type = T;
        };

        template <typename T>
        struct NkRemovePointer<T* const> {
            using type = T;
        };

        template <typename T>
        struct NkRemovePointer<T* volatile> {
            using type = T;
        };

        template <typename T>
        struct NkRemovePointer<T* const volatile> {
            using type = T;
        };

        template <typename T>
        using NkRemovePointer_t = typename NkRemovePointer<T>::type;

        template <typename T>
        struct NkRemoveExtent {
            using type = T;
        };

        template <typename T>
        struct NkRemoveExtent<T[]> {
            using type = T;
        };

        template <typename T, nk_size N>
        struct NkRemoveExtent<T[N]> {
            using type = T;
        };

        template <typename T>
        using NkRemoveExtent_t = typename NkRemoveExtent<T>::type;

        template <typename T>
        struct NkIsVoid : NkIsSame<void, NkRemoveCV_t<T>> {};

        template <typename T>
        inline constexpr nk_bool NkIsVoid_v = NkIsVoid<T>::value;

        template <typename T>
        struct NkIsNullPointer : NkIsSame<NkRemoveCV_t<T>, decltype(nullptr)> {};

        template <typename T>
        inline constexpr nk_bool NkIsNullPointer_v = NkIsNullPointer<T>::value;

        template <typename T>
        struct NkIsConst : NkFalseType {};

        template <typename T>
        struct NkIsConst<const T> : NkTrueType {};

        template <typename T>
        inline constexpr nk_bool NkIsConst_v = NkIsConst<T>::value;

        template <typename T>
        struct NkIsVolatile : NkFalseType {};

        template <typename T>
        struct NkIsVolatile<volatile T> : NkTrueType {};

        template <typename T>
        inline constexpr nk_bool NkIsVolatile_v = NkIsVolatile<T>::value;

        template <typename T>
        struct NkIsLValueReference : NkFalseType {};

        template <typename T>
        struct NkIsLValueReference<T&> : NkTrueType {};

        template <typename T>
        inline constexpr nk_bool NkIsLValueReference_v = NkIsLValueReference<T>::value;

        template <typename T>
        struct NkIsRValueReference : NkFalseType {};

        template <typename T>
        struct NkIsRValueReference<T&&> : NkTrueType {};

        template <typename T>
        inline constexpr nk_bool NkIsRValueReference_v = NkIsRValueReference<T>::value;

        template <typename T>
        struct NkIsReference
            : NkBoolConstant<NkIsLValueReference_v<T> || NkIsRValueReference_v<T>> {};

        template <typename T>
        inline constexpr nk_bool NkIsReference_v = NkIsReference<T>::value;

        template <typename T>
        struct NkIsPointer : NkFalseType {};

        template <typename T>
        struct NkIsPointer<T*> : NkTrueType {};

        template <typename T>
        struct NkIsPointer<T* const> : NkTrueType {};

        template <typename T>
        struct NkIsPointer<T* volatile> : NkTrueType {};

        template <typename T>
        struct NkIsPointer<T* const volatile> : NkTrueType {};

        template <typename T>
        inline constexpr nk_bool NkIsPointer_v = NkIsPointer<NkRemoveCV_t<T>>::value;

        template <typename T>
        struct NkIsArray : NkFalseType {};

        template <typename T>
        struct NkIsArray<T[]> : NkTrueType {};

        template <typename T, nk_size N>
        struct NkIsArray<T[N]> : NkTrueType {};

        template <typename T>
        inline constexpr nk_bool NkIsArray_v = NkIsArray<T>::value;

        template <typename T>
        struct NkIsIntegralBase : NkFalseType {};

        template <> struct NkIsIntegralBase<bool> : NkTrueType {};
        template <> struct NkIsIntegralBase<char> : NkTrueType {};
        template <> struct NkIsIntegralBase<signed char> : NkTrueType {};
        template <> struct NkIsIntegralBase<unsigned char> : NkTrueType {};
        template <> struct NkIsIntegralBase<wchar_t> : NkTrueType {};
        template <> struct NkIsIntegralBase<char16_t> : NkTrueType {};
        template <> struct NkIsIntegralBase<char32_t> : NkTrueType {};
        template <> struct NkIsIntegralBase<short> : NkTrueType {};
        template <> struct NkIsIntegralBase<unsigned short> : NkTrueType {};
        template <> struct NkIsIntegralBase<int> : NkTrueType {};
        template <> struct NkIsIntegralBase<unsigned int> : NkTrueType {};
        template <> struct NkIsIntegralBase<long> : NkTrueType {};
        template <> struct NkIsIntegralBase<unsigned long> : NkTrueType {};
        template <> struct NkIsIntegralBase<long long> : NkTrueType {};
        template <> struct NkIsIntegralBase<unsigned long long> : NkTrueType {};

        #if defined(__cpp_char8_t)
        template <> struct NkIsIntegralBase<char8_t> : NkTrueType {};
        #endif

        template <typename T>
        struct NkIsIntegral : NkIsIntegralBase<NkRemoveCV_t<T>> {};

        template <typename T>
        inline constexpr nk_bool NkIsIntegral_v = NkIsIntegral<T>::value;

        template <typename T>
        struct NkIsFloatingPointBase : NkFalseType {};

        template <> struct NkIsFloatingPointBase<float> : NkTrueType {};
        template <> struct NkIsFloatingPointBase<double> : NkTrueType {};
        template <> struct NkIsFloatingPointBase<long double> : NkTrueType {};

        template <typename T>
        struct NkIsFloatingPoint : NkIsFloatingPointBase<NkRemoveCV_t<T>> {};

        template <typename T>
        inline constexpr nk_bool NkIsFloatingPoint_v = NkIsFloatingPoint<T>::value;

        template <typename T>
        struct NkIsArithmetic
            : NkBoolConstant<NkIsIntegral_v<T> || NkIsFloatingPoint_v<T>> {};

        template <typename T>
        inline constexpr nk_bool NkIsArithmetic_v = NkIsArithmetic<T>::value;

        template <typename T, nk_bool = NkIsArithmetic_v<T>>
        struct NkIsSignedImpl : NkFalseType {};

        template <typename T>
        struct NkIsSignedImpl<T, true> : NkBoolConstant<(T(-1) < T(0))> {};

        template <typename T>
        struct NkIsSigned : NkIsSignedImpl<NkRemoveCV_t<T>> {};

        template <typename T>
        inline constexpr nk_bool NkIsSigned_v = NkIsSigned<T>::value;

        template <typename T>
        struct NkIsUnsigned
            : NkBoolConstant<NkIsArithmetic_v<T> && !NkIsSigned_v<T>> {};

        template <typename T>
        inline constexpr nk_bool NkIsUnsigned_v = NkIsUnsigned<T>::value;

        #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
        template <typename T>
        struct NkIsEnum : NkBoolConstant<__is_enum(T)> {};
        template <typename T>
        struct NkIsClass : NkBoolConstant<__is_class(T)> {};
        template <typename T>
        struct NkIsUnion : NkBoolConstant<__is_union(T)> {};
        template <typename T>
        struct NkIsFunction : NkBoolConstant<std::is_function_v<T>> {};
        #else
        template <typename T> struct NkIsEnum : NkFalseType {};
        template <typename T> struct NkIsClass : NkFalseType {};
        template <typename T> struct NkIsUnion : NkFalseType {};
        template <typename T> struct NkIsFunction : NkFalseType {};
        #endif

        template <typename T>
        inline constexpr nk_bool NkIsEnum_v = NkIsEnum<T>::value;

        template <typename T>
        inline constexpr nk_bool NkIsClass_v = NkIsClass<T>::value;

        template <typename T>
        inline constexpr nk_bool NkIsUnion_v = NkIsUnion<T>::value;

        template <typename T>
        inline constexpr nk_bool NkIsFunction_v = NkIsFunction<T>::value;

        template <typename T>
        struct NkAddPointer {
            using type = NkRemoveReference_t<T>*;
        };

        template <typename T>
        using NkAddPointer_t = typename NkAddPointer<T>::type;

        template <typename...>
        struct NkConjunction : NkTrueType {};

        template <typename B1>
        struct NkConjunction<B1> : B1 {};

        template <typename B1, typename... Bn>
        struct NkConjunction<B1, Bn...>
            : NkConditional_t<static_cast<nk_bool>(B1::value), NkConjunction<Bn...>, B1> {};

        template <typename...>
        struct NkDisjunction : NkFalseType {};

        template <typename B1>
        struct NkDisjunction<B1> : B1 {};

        template <typename B1, typename... Bn>
        struct NkDisjunction<B1, Bn...>
            : NkConditional_t<static_cast<nk_bool>(B1::value), B1, NkDisjunction<Bn...>> {};

        template <typename B>
        struct NkNegation : NkBoolConstant<!static_cast<nk_bool>(B::value)> {};

        template <typename T, typename... Types>
        struct NkIsAnyOf : NkDisjunction<NkIsSame<T, Types>...> {};

        template <typename T, typename... Types>
        inline constexpr nk_bool NkIsAnyOf_v = NkIsAnyOf<T, Types...>::value;

        template <typename T>
        struct NkDecay {
        private:
            using U = NkRemoveReference_t<T>;
        public:
            using type = NkConditional_t<
                NkIsArray_v<U>,
                NkAddPointer_t<NkRemoveExtent_t<U>>,
                NkConditional_t<
                    NkIsFunction_v<U>,
                    NkAddPointer_t<U>,
                    NkRemoveCV_t<U>>>;
        };

        template <typename T>
        using NkDecay_t = typename NkDecay<T>::type;

        // Legacy alias kept for old modules (NKContainers utilities).
        template <typename T>
        using NkDecayT = typename NkDecay<T>::type;

        template <typename T>
        NkAddRValueReference_t<T> NkDeclVal() noexcept;

        #define NkDeclType(...) decltype(__VA_ARGS__)

        template <typename T>
        constexpr T&& NkForward(NkRemoveReference_t<T>& value) noexcept {
            return static_cast<T&&>(value);
        }

        template <typename T>
        constexpr T&& NkForward(NkRemoveReference_t<T>&& value) noexcept {
            static_assert(!NkIsLValueReference_v<T>, "NkForward<T>(rvalue) with lvalue T");
            return static_cast<T&&>(value);
        }

        template <typename T>
        constexpr NkRemoveReference_t<T>&& NkMove(T&& value) noexcept {
            return static_cast<NkRemoveReference_t<T>&&>(value);
        }

        // Legacy macro from NkMacros.h collides with this function template name.
        #ifdef NkSwap
        #undef NkSwap
        #endif

        template <typename T>
        constexpr void NkSwap(T& a, T& b) noexcept(noexcept(T(NkMove(a))) &&
                                                noexcept(a = NkMove(b)) &&
                                                noexcept(b = NkMove(a))) {
            T tmp = NkMove(a);
            a = NkMove(b);
            b = NkMove(tmp);
        }

        template <typename T>
        struct NkIsCharacterType
            : NkIsAnyOf<NkRemoveCV_t<T>, nk_char, nk_wchar, nk_char8, nk_char16, nk_char32,
                        signed char, unsigned char> {};

        template <typename T>
        inline constexpr nk_bool NkIsCharacterType_v = NkIsCharacterType<T>::value;

        template <typename T>
        struct NkIsValidCharType : NkIsCharacterType<T> {};

        template <typename T>
        inline constexpr nk_bool NkIsValidCharType_v = NkIsValidCharType<T>::value;

        template <typename T>
        struct NkIsFundamental
            : NkBoolConstant<NkIsArithmetic_v<T> || NkIsVoid_v<T> || NkIsNullPointer_v<T>> {};

        template <typename T>
        inline constexpr nk_bool NkIsFundamental_v = NkIsFundamental<T>::value;

        template <typename T>
        struct NkIsScalar
            : NkBoolConstant<NkIsArithmetic_v<T> || NkIsEnum_v<T> || NkIsPointer_v<T> ||
                            NkIsNullPointer_v<T>> {};

        template <typename T>
        inline constexpr nk_bool NkIsScalar_v = NkIsScalar<T>::value;

        template <typename T>
        struct NkIsObject
            : NkBoolConstant<!NkIsReference_v<T> && !NkIsVoid_v<T> && !NkIsFunction_v<T>> {};

        template <typename T>
        inline constexpr nk_bool NkIsObject_v = NkIsObject<T>::value;

        #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
        template <typename Base, typename Derived>
        struct NkIsBaseOf : NkBoolConstant<__is_base_of(Base, Derived)> {};
        #else
        template <typename Base, typename Derived>
        struct NkIsBaseOf : NkFalseType {};
        #endif

        template <typename Base, typename Derived>
        inline constexpr nk_bool NkIsBaseOf_v = NkIsBaseOf<Base, Derived>::value;

        template <typename T>
        struct NkIsCompound : NkBoolConstant<!NkIsFundamental_v<T>> {};

        template <typename T>
        inline constexpr nk_bool NkIsCompound_v = NkIsCompound<T>::value;

        template <typename T>
        struct NkNounType {
            using type = NkRemoveCV_t<NkRemoveReference_t<T>>;
        };

        template <typename T>
        using NkNounType_t = typename NkNounType<T>::type;

        template <typename T>
        struct NkIsPlatformSupported : NkTrueType {};

        #if !NKENTSEU_INT128_AVAILABLE
        template <>
        struct NkIsPlatformSupported<nk_int128> : NkFalseType {};
        template <>
        struct NkIsPlatformSupported<nk_uint128> : NkFalseType {};
        #endif

        template <typename T>
        inline constexpr nk_bool NkIsPlatformSupported_v = NkIsPlatformSupported<T>::value;

        
        template<typename T>
        struct NkIsTriviallyCopyable : NkBoolConstant<
            NkIsArithmetic_v<T> || NkIsPointer_v<T>
        > {};

        template<typename T>
        inline constexpr bool NkIsTriviallyCopyable_v = NkIsTriviallyCopyable<T>::value;

        
        template<typename T>
        struct NkIsCopyConstructible : NkBoolConstant<
            NkIsArithmetic_v<T> || NkIsPointer_v<T>
        > {};

        template<typename T>
        inline constexpr bool NkIsCopyConstructible_v = NkIsCopyConstructible<T>::value;

        template<typename T>
        struct NkIsMoveConstructible : NkIsCopyConstructible<T> {};

        template<typename T>
        inline constexpr bool NkIsMoveConstructible_v = NkIsMoveConstructible<T>::value;

        template<typename T>
        struct NkIsTriviallyMoveConstructible : NkIsMoveConstructible<T> {};

        template<typename T>
        inline constexpr bool NkIsTriviallyMoveConstructible_v = NkIsTriviallyMoveConstructible<T>::value;

        template <typename T>
        inline constexpr bool NkIsTriviallyDestructible = __is_trivially_destructible(T);

        template <typename T>
        struct NkIsTriviallyConstructible : NkBoolConstant<__is_trivially_destructible(T)> {
            // determine whether remove_all_extents_t<_Ty> is a reference type or can trivially be explicitly destroyed
        };

        template <typename T>
        inline constexpr bool NkIsTriviallyConstructible_v = __is_trivially_constructible(T);
    } // namespace traits

    namespace traits_alias {
        using namespace traits;
    }

    
} // namespace nkentseu

#endif // NKENTSEU_CORE_NKTRAITS_H_INCLUDED
