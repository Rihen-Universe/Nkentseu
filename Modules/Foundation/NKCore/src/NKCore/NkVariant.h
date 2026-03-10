#pragma once

#ifndef NKENTSEU_CORE_NKVARIANT_H_INCLUDED
#define NKENTSEU_CORE_NKVARIANT_H_INCLUDED

#include "NkInvoke.h"
#include "NKCore/Assert/NkAssert.h"

namespace nkentseu {
    

        namespace detail {

            template <typename T>
            struct NkAlwaysFalse : traits::NkFalseType {};

            template <typename... Ts>
            struct NkMaxSizeOf;

            template <typename T>
            struct NkMaxSizeOf<T> : traits::NkIntegralConstant<nk_size, sizeof(T)> {};

            template <typename T, typename U, typename... Rest>
            struct NkMaxSizeOf<T, U, Rest...>
                : traits::NkIntegralConstant<
                    nk_size,
                    (sizeof(T) > NkMaxSizeOf<U, Rest...>::value ? sizeof(T)
                                                                : NkMaxSizeOf<U, Rest...>::value)> {};

            template <typename... Ts>
            struct NkMaxAlignOf;

            template <typename T>
            struct NkMaxAlignOf<T> : traits::NkIntegralConstant<nk_size, alignof(T)> {};

            template <typename T, typename U, typename... Rest>
            struct NkMaxAlignOf<T, U, Rest...>
                : traits::NkIntegralConstant<
                    nk_size,
                    (alignof(T) > NkMaxAlignOf<U, Rest...>::value ? alignof(T)
                                                                    : NkMaxAlignOf<U, Rest...>::value)> {};

            template <typename T, typename... Ts>
            struct NkTypeIndex;

            template <typename T, typename... Rest>
            struct NkTypeIndex<T, T, Rest...> : traits::NkIntegralConstant<nk_size, 0u> {};

            template <typename T, typename U, typename... Rest>
            struct NkTypeIndex<T, U, Rest...>
                : traits::NkIntegralConstant<nk_size, 1u + NkTypeIndex<T, Rest...>::value> {};

            template <typename T>
            struct NkTypeIndex<T> {
                static_assert(NkAlwaysFalse<T>::value, "Type is not part of this NkVariant");
            };

            template <nk_size Index, typename... Ts>
            struct NkTypeAt;

            template <typename T, typename... Rest>
            struct NkTypeAt<0u, T, Rest...> {
                using type = T;
            };

            template <nk_size Index, typename T, typename... Rest>
            struct NkTypeAt<Index, T, Rest...> {
                using type = typename NkTypeAt<Index - 1u, Rest...>::type;
            };

            template <typename T, typename... Ts>
            inline constexpr nk_bool NkContainsType_v = traits::NkIsAnyOf<T, Ts...>::value;

        } // namespace detail

        template <typename... Ts>
        class NkVariant {
                static_assert(sizeof...(Ts) > 0u, "NkVariant requires at least one type");

            public:
                static constexpr nk_size kTypeCount = sizeof...(Ts);
                static constexpr nk_size kNpos = static_cast<nk_size>(-1);

                NkVariant() : mIndex(kNpos), mHasValue(false) {
                    using T0 = typename detail::NkTypeAt<0u, Ts...>::type;
        if constexpr (__is_constructible(T0)) {
            Emplace<T0>();
        }
                }

                ~NkVariant() {
                    Reset();
                }

                NkVariant(const NkVariant& other) : mIndex(kNpos), mHasValue(false) {
                    CopyFrom(other);
                }

                NkVariant(NkVariant&& other) noexcept : mIndex(kNpos), mHasValue(false) {
                    MoveFrom(traits::NkMove(other));
                }

                template <typename T,
                        typename D = traits::NkRemoveCV_t<traits::NkRemoveReference_t<T>>,
                        typename traits::NkEnableIf_t<detail::NkContainsType_v<D, Ts...>, int> = 0>
                NkVariant(T&& value) : mIndex(kNpos), mHasValue(false) {
                    Emplace<D>(traits::NkForward<T>(value));
                }

                NkVariant& operator=(const NkVariant& other) {
                    if (this == &other) {
                        return *this;
                    }
                    Reset();
                    CopyFrom(other);
                    return *this;
                }

                NkVariant& operator=(NkVariant&& other) noexcept {
                    if (this == &other) {
                        return *this;
                    }
                    Reset();
                    MoveFrom(traits::NkMove(other));
                    return *this;
                }

                template <typename T,
                        typename D = traits::NkRemoveCV_t<traits::NkRemoveReference_t<T>>,
                        typename traits::NkEnableIf_t<detail::NkContainsType_v<D, Ts...>, int> = 0>
                NkVariant& operator=(T&& value) {
                    Emplace<D>(traits::NkForward<T>(value));
                    return *this;
                }

                [[nodiscard]] nk_bool ValuelessByException() const noexcept {
                    return !mHasValue;
                }
                [[nodiscard]] nk_bool HasValue() const noexcept {
                    return mHasValue;
                }

                [[nodiscard]] nk_size Index() const noexcept {
                    return mHasValue ? mIndex : kNpos;
                }

                void Reset() noexcept {
                    if (!mHasValue) {
                        return;
                    }
                    DestroyByIndex();
                    mIndex = kNpos;
                    mHasValue = false;
                }

                template <typename T, typename... Args,
                        typename traits::NkEnableIf_t<detail::NkContainsType_v<T, Ts...>, int> = 0>
                T& Emplace(Args&&... args) {
                    Reset();
                    new (mStorage) T(traits::NkForward<Args>(args)...);
                    mIndex = detail::NkTypeIndex<T, Ts...>::value;
                    mHasValue = true;
                    return *reinterpret_cast<T*>(mStorage);
                }

                template <typename T>
                [[nodiscard]] nk_bool HoldsAlternative() const noexcept {
                    if (!mHasValue) {
                        return false;
                    }
                    return mIndex == detail::NkTypeIndex<T, Ts...>::value;
                }

                template <typename T>
                T& Get() noexcept {
                    return *reinterpret_cast<T*>(mStorage);
                }

                template <typename T>
                const T& Get() const noexcept {
                    return *reinterpret_cast<const T*>(mStorage);
                }

                template <typename T>
                T& GetChecked() {
                    NK_ASSERT(HoldsAlternative<T>());
                    return Get<T>();
                }

                template <typename T>
                const T& GetChecked() const {
                    NK_ASSERT(HoldsAlternative<T>());
                    return Get<T>();
                }

                template <typename T>
                T* GetIf() noexcept {
                    return HoldsAlternative<T>() ? reinterpret_cast<T*>(mStorage) : nullptr;
                }

                template <typename T>
                const T* GetIf() const noexcept {
                    return HoldsAlternative<T>() ? reinterpret_cast<const T*>(mStorage) : nullptr;
                }

                template <typename Visitor>
                void Visit(Visitor&& visitor) {
                    if (!mHasValue) {
                        return;
                    }
                    VisitImpl<0u>(visitor);
                }

                template <typename Visitor>
                void Visit(Visitor&& visitor) const {
                    if (!mHasValue) {
                        return;
                    }
                    VisitImplConst<0u>(visitor);
                }

                void Swap(NkVariant& other) {
                    if (this == &other) {
                        return;
                    }
                    NkVariant tmp(traits::NkMove(other));
                    other = traits::NkMove(*this);
                    *this = traits::NkMove(tmp);
                }

            private:
                template <nk_size I>
                using TypeAt = typename detail::NkTypeAt<I, Ts...>::type;

                template <nk_size I = 0u>
                void DestroyByIndex() noexcept {
                    if constexpr (I < kTypeCount) {
                        if (mIndex == I) {
                            reinterpret_cast<TypeAt<I>*>(mStorage)->~TypeAt<I>();
                            return;
                        }
                        DestroyByIndex<I + 1u>();
                    }
                }

                template <nk_size I = 0u>
                void CopyFrom(const NkVariant& other) {
                    if (!other.mHasValue) {
                        return;
                    }
                    if constexpr (I < kTypeCount) {
                        if (other.mIndex == I) {
                            using T = TypeAt<I>;
                            new (mStorage) T(*reinterpret_cast<const T*>(other.mStorage));
                            mIndex = I;
                            mHasValue = true;
                            return;
                        }
                        CopyFrom<I + 1u>(other);
                    }
                }

                template <nk_size I = 0u>
                void MoveFrom(NkVariant&& other) {
                    if (!other.mHasValue) {
                        return;
                    }
                    if constexpr (I < kTypeCount) {
                        if (other.mIndex == I) {
                            using T = TypeAt<I>;
                            new (mStorage) T(traits::NkMove(*reinterpret_cast<T*>(other.mStorage)));
                            mIndex = I;
                            mHasValue = true;
                            return;
                        }
                        MoveFrom<I + 1u>(traits::NkMove(other));
                    }
                }

                template <nk_size I = 0u, typename Visitor>
                void VisitImpl(Visitor& visitor) {
                    if constexpr (I < kTypeCount) {
                        if (mIndex == I) {
                            NkInvoke(visitor, *reinterpret_cast<TypeAt<I>*>(mStorage));
                            return;
                        }
                        VisitImpl<I + 1u>(visitor);
                    }
                }

                template <nk_size I = 0u, typename Visitor>
                void VisitImplConst(Visitor& visitor) const {
                    if constexpr (I < kTypeCount) {
                        if (mIndex == I) {
                            NkInvoke(visitor, *reinterpret_cast<const TypeAt<I>*>(mStorage));
                            return;
                        }
                        VisitImplConst<I + 1u>(visitor);
                    }
                }

                alignas(detail::NkMaxAlignOf<Ts...>::value) nk_uint8 mStorage[detail::NkMaxSizeOf<Ts...>::value];
                nk_size mIndex;
                nk_bool mHasValue;
        };

        template <typename T, typename... Ts>
        [[nodiscard]] nk_bool NkHoldsAlternative(const NkVariant<Ts...>& variant) noexcept {
            return variant.template HoldsAlternative<T>();
        }

        template <typename T, typename... Ts>
        T* NkGetIf(NkVariant<Ts...>* variant) noexcept {
            if (!variant) {
                return nullptr;
            }
            return variant->template GetIf<T>();
        }

        template <typename T, typename... Ts>
        const T* NkGetIf(const NkVariant<Ts...>* variant) noexcept {
            if (!variant) {
                return nullptr;
            }
            return variant->template GetIf<T>();
        }

        template <typename Visitor, typename... Ts>
        void NkVisit(Visitor&& visitor, NkVariant<Ts...>& variant) {
            variant.Visit(traits::NkForward<Visitor>(visitor));
        }

        template <typename Visitor, typename... Ts>
        void NkVisit(Visitor&& visitor, const NkVariant<Ts...>& variant) {
            variant.Visit(traits::NkForward<Visitor>(visitor));
        }

    
} // namespace nkentseu

#endif // NKENTSEU_CORE_NKVARIANT_H_INCLUDED
