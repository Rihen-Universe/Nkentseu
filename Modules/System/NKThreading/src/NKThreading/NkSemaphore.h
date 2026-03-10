// ============================================================
// FILE: NkSemaphore.h
// DESCRIPTION: Counting semaphore synchronization primitive
// AUTHOR: Rihen
// DATE: 2026-03-06
// VERSION: 2.0.0
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_NKSEMAPHORE_H_INCLUDED
#define NKENTSEU_THREADING_NKSEMAPHORE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKThreading/NkConditionVariable.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkThreadingExport.h"

namespace nkentseu {
namespace threading {

    class NKTHREADING_API NkSemaphore {
    public:
        explicit NkSemaphore(nk_uint32 initialCount = 0u,
                             nk_uint32 maxCount = 0xFFFFFFFFu) noexcept;
        ~NkSemaphore() = default;

        NkSemaphore(const NkSemaphore&) = delete;
        NkSemaphore& operator=(const NkSemaphore&) = delete;

        void Acquire() noexcept;
        [[nodiscard]] nk_bool TryAcquire() noexcept;
        [[nodiscard]] nk_bool TryAcquireFor(nk_uint32 milliseconds) noexcept;

        [[nodiscard]] nk_bool Release(nk_uint32 count = 1u) noexcept;
        [[nodiscard]] nk_uint32 GetCount() const noexcept;
        [[nodiscard]] nk_uint32 GetMaxCount() const noexcept;

    private:
        nk_uint32 mCount;
        nk_uint32 mMaxCount;
        mutable NkMutex mMutex;
        NkConditionVariable mCondVar;
    };

} // namespace threading
} // namespace nkentseu

#endif // NKENTSEU_THREADING_NKSEMAPHORE_H_INCLUDED
