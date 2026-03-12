#pragma once

#ifndef NKENTSEU_THREADING_NKTHREADLOCAL_H_INCLUDED
#define NKENTSEU_THREADING_NKTHREADLOCAL_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {
namespace threading {

/**
 * Thread-local value wrapper with optional explicit initial value.
 */
template<typename T>
class NkThreadLocal {
public:
    NkThreadLocal() = default;

    explicit NkThreadLocal(const T& initialValue)
        : mInitialValue(initialValue)
        , mHasInitialValue(true) {}

    explicit NkThreadLocal(T&& initialValue)
        : mInitialValue(traits::NkMove(initialValue))
        , mHasInitialValue(true) {}

    T& Get() const noexcept {
        if (mHasInitialValue) {
            thread_local T value = mInitialValue;
            return value;
        }
        thread_local T value{};
        return value;
    }

    void Set(const T& value) noexcept {
        Get() = value;
    }

    void Set(T&& value) noexcept {
        Get() = traits::NkMove(value);
    }

    T* operator->() noexcept {
        return &Get();
    }

    const T* operator->() const noexcept {
        return &Get();
    }

    T& operator*() noexcept {
        return Get();
    }

    const T& operator*() const noexcept {
        return Get();
    }

private:
    T mInitialValue{};
    nk_bool mHasInitialValue = false;
};

} // namespace threading

namespace entseu {

// Legacy compatibility alias.
template<typename T>
using NkThreadLocal = ::nkentseu::threading::NkThreadLocal<T>;

} // namespace entseu
} // namespace nkentseu

#endif // NKENTSEU_THREADING_NKTHREADLOCAL_H_INCLUDED
