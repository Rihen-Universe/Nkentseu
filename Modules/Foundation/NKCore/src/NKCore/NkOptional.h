#pragma once

#ifndef NKENTSEU_CORE_NKOPTIONAL_H_INCLUDED
#define NKENTSEU_CORE_NKOPTIONAL_H_INCLUDED

#include "NkTraits.h"

namespace nkentseu {
    namespace core {

        struct NkNullOpt_t {
            explicit constexpr NkNullOpt_t(int) noexcept {}
        };

        inline constexpr NkNullOpt_t NkNullOpt{0};

        template <typename T>
        class NkOptional {
            public:
                constexpr NkOptional() noexcept : mHasValue(false) {}
                constexpr NkOptional(NkNullOpt_t) noexcept : mHasValue(false) {}

                NkOptional(const T& value) : mHasValue(false) {
                    Emplace(value);
                }

                NkOptional(T&& value) : mHasValue(false) {
                    Emplace(traits::NkMove(value));
                }

                NkOptional(const NkOptional& other) : mHasValue(false) {
                    if (other.mHasValue) {
                        Emplace(*other.Data());
                    }
                }

                NkOptional(NkOptional&& other) noexcept : mHasValue(false) {
                    if (other.mHasValue) {
                        Emplace(traits::NkMove(*other.Data()));
                        other.Reset();
                    }
                }

                ~NkOptional() {
                    Reset();
                }

                NkOptional& operator=(NkNullOpt_t) noexcept {
                    Reset();
                    return *this;
                }

                NkOptional& operator=(const NkOptional& other) {
                    if (this == &other) {
                        return *this;
                    }
                    if (!other.mHasValue) {
                        Reset();
                        return *this;
                    }
                    Emplace(*other.Data());
                    return *this;
                }

                NkOptional& operator=(NkOptional&& other) noexcept {
                    if (this == &other) {
                        return *this;
                    }
                    if (!other.mHasValue) {
                        Reset();
                        return *this;
                    }
                    Emplace(traits::NkMove(*other.Data()));
                    other.Reset();
                    return *this;
                }

                NkOptional& operator=(const T& value) {
                    Emplace(value);
                    return *this;
                }

                NkOptional& operator=(T&& value) {
                    Emplace(traits::NkMove(value));
                    return *this;
                }

                template <typename... Args>
                T& Emplace(Args&&... args) {
                    Reset();
                    new (mStorage) T(traits::NkForward<Args>(args)...);
                    mHasValue = true;
                    return *Data();
                }

                void Reset() noexcept {
                    if (mHasValue) {
                        Data()->~T();
                        mHasValue = false;
                    }
                }

                [[nodiscard]] nk_bool HasValue() const noexcept {
                    return mHasValue;
                }

                explicit constexpr operator nk_bool() const noexcept {
                    return mHasValue;
                }

                T* operator->() noexcept {
                    return Data();
                }

                const T* operator->() const noexcept {
                    return Data();
                }

                T& operator*() noexcept {
                    return *Data();
                }

                const T& operator*() const noexcept {
                    return *Data();
                }

                T& Value() noexcept {
                    return *Data();
                }

                const T& Value() const noexcept {
                    return *Data();
                }

                T* GetIf() noexcept {
                    return mHasValue ? Data() : nullptr;
                }

                const T* GetIf() const noexcept {
                    return mHasValue ? Data() : nullptr;
                }

                T ValueOr(T fallback) const {
                    if (mHasValue) {
                        return *Data();
                    }
                    return fallback;
                }

            private:
                T* Data() noexcept {
                    return reinterpret_cast<T*>(mStorage);
                }

                const T* Data() const noexcept {
                    return reinterpret_cast<const T*>(mStorage);
                }

                alignas(T) nk_uint8 mStorage[sizeof(T)];
                nk_bool mHasValue;
        };

    } // namespace core
} // namespace nkentseu

#endif // NKENTSEU_CORE_NKOPTIONAL_H_INCLUDED

