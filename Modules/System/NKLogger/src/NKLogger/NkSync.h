// -----------------------------------------------------------------------------
// FICHIER: NKLogger/NkSync.h
// DESCRIPTION: Primitives de synchronisation internes à NKLogger (sans STL sync)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_LOGGER_NKSYNC_H_INCLUDED
#define NKENTSEU_LOGGER_NKSYNC_H_INCLUDED

#include "NKCore/NkAtomic.h"
#include "NKCore/NkTypes.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKThreading/NkMutex.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <errno.h>
    #include <pthread.h>
    #include <sched.h>
    #include <stdint.h>
    #include <string.h>
    #include <unistd.h>
    #if defined(NKENTSEU_PLATFORM_LINUX)
        #include <sys/syscall.h>
        #include <sys/types.h>
    #endif
    #include <time.h>
#endif

namespace nkentseu {
namespace loggersync {

//     class threading::NkMutex {
//     public:
//         threading::NkMutex() noexcept
//             : mInitialized(true) {
// #if defined(NKENTSEU_PLATFORM_WINDOWS)
//             InitializeSRWLock(&mMutex);
// #else
//             mInitialized = (pthread_mutex_init(&mMutex, nullptr) == 0);
// #endif
//         }

//         ~threading::NkMutex() {
// #if !defined(NKENTSEU_PLATFORM_WINDOWS)
//             if (mInitialized) {
//                 (void)pthread_mutex_destroy(&mMutex);
//             }
// #endif
//         }

//         threading::NkMutex(const threading::NkMutex&) = delete;
//         threading::NkMutex& operator=(const threading::NkMutex&) = delete;

//         void Lock() noexcept {
//             if (!mInitialized) {
//                 return;
//             }
// #if defined(NKENTSEU_PLATFORM_WINDOWS)
//             AcquireSRWLockExclusive(&mMutex);
// #else
//             (void)pthread_mutex_lock(&mMutex);
// #endif
//         }

//         void Unlock() noexcept {
//             if (!mInitialized) {
//                 return;
//             }
// #if defined(NKENTSEU_PLATFORM_WINDOWS)
//             ReleaseSRWLockExclusive(&mMutex);
// #else
//             (void)pthread_mutex_unlock(&mMutex);
// #endif
//         }

//     private:
// #if defined(NKENTSEU_PLATFORM_WINDOWS)
//         SRWLOCK mMutex;
// #else
//         pthread_mutex_t mMutex;
// #endif
//         nk_bool mInitialized;

//         friend class NkConditionVariable;
//     };

    class NkScopedLock {
    public:
        explicit NkScopedLock(threading::NkMutex& mutex) noexcept
            : mMutex(&mutex) {
            mMutex->Lock();
        }

        ~NkScopedLock() {
            if (mMutex != nullptr) {
                mMutex->Unlock();
            }
        }

        NkScopedLock(const NkScopedLock&) = delete;
        NkScopedLock& operator=(const NkScopedLock&) = delete;

        NkScopedLock(NkScopedLock&& other) noexcept
            : mMutex(other.mMutex) {
            other.mMutex = nullptr;
        }

        NkScopedLock& operator=(NkScopedLock&& other) noexcept {
            if (this != &other) {
                if (mMutex != nullptr) {
                    mMutex->Unlock();
                }
                mMutex = other.mMutex;
                other.mMutex = nullptr;
            }
            return *this;
        }

        void Unlock() noexcept {
            if (mMutex != nullptr) {
                mMutex->Unlock();
                mMutex = nullptr;
            }
        }

        [[nodiscard]] threading::NkMutex* GetMutex() noexcept {
            return mMutex;
        }

    private:
        threading::NkMutex* mMutex;
    };

    class NkConditionVariable {
    public:
        NkConditionVariable() noexcept
            : mInitialized(true) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            InitializeConditionVariable(&mCondVar);
#else
            mInitialized = (pthread_cond_init(&mCondVar, nullptr) == 0);
#endif
        }

        ~NkConditionVariable() {
#if !defined(NKENTSEU_PLATFORM_WINDOWS)
            if (mInitialized) {
                (void)pthread_cond_destroy(&mCondVar);
            }
#endif
        }

        NkConditionVariable(const NkConditionVariable&) = delete;
        NkConditionVariable& operator=(const NkConditionVariable&) = delete;

        void Wait(NkScopedLock& lock) noexcept {
            threading::NkMutex* mutex = lock.GetMutex();
            if (mutex == nullptr || !mInitialized) {
                return;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            (void)SleepConditionVariableSRW(&mCondVar, &mutex->Get(), INFINITE, 0);
#else
            (void)pthread_cond_wait(&mCondVar, &mutex->Get());
#endif
        }

        [[nodiscard]] nk_bool WaitFor(NkScopedLock& lock, nk_uint32 milliseconds) noexcept {
            threading::NkMutex* mutex = lock.GetMutex();
            if (mutex == nullptr || !mInitialized) {
                return false;
            }

#if defined(NKENTSEU_PLATFORM_WINDOWS)
            const BOOL woke = SleepConditionVariableSRW(&mCondVar, &mutex->Get(), milliseconds, 0);
            if (woke) {
                return true;
            }
            return GetLastError() != ERROR_TIMEOUT;
#else
            timespec deadline{};
            if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
                return false;
            }

            deadline.tv_sec += static_cast<time_t>(milliseconds / 1000U);
            deadline.tv_nsec += static_cast<long>(milliseconds % 1000U) * 1000000L;
            if (deadline.tv_nsec >= 1000000000L) {
                deadline.tv_sec += 1;
                deadline.tv_nsec -= 1000000000L;
            }

            int result = 0;
            do {
                result = pthread_cond_timedwait(&mCondVar, &mutex->Get(), &deadline);
            } while (result == EINTR);

            return result == 0;
#endif
        }

        void NotifyOne() noexcept {
            if (!mInitialized) {
                return;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            WakeConditionVariable(&mCondVar);
#else
            (void)pthread_cond_signal(&mCondVar);
#endif
        }

        void NotifyAll() noexcept {
            if (!mInitialized) {
                return;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            WakeAllConditionVariable(&mCondVar);
#else
            (void)pthread_cond_broadcast(&mCondVar);
#endif
        }

    private:
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        CONDITION_VARIABLE mCondVar;
#else
        pthread_cond_t mCondVar;
#endif
        nk_bool mInitialized;
    };

    using NkAtomicBool = NkAtomicBool;

    inline nk_uint64 GetCurrentThreadId() noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        return static_cast<nk_uint64>(::GetCurrentThreadId());
#elif defined(NKENTSEU_PLATFORM_LINUX)
        return static_cast<nk_uint64>(::syscall(SYS_gettid));
#else
        nk_uint64 id = 0;
        pthread_t thread = pthread_self();
        const nk_size copySize = (sizeof(thread) < sizeof(id)) ? sizeof(thread) : sizeof(id);
        memcpy(&id, &thread, static_cast<size_t>(copySize));
        return id;
#endif
    }

    class NkThread {
    public:
        using ThreadRoutine = void (*)(void*);

        NkThread() noexcept
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            : mHandle(nullptr)
#else
            : mThread()
#endif
            , mJoinable(false) {
        }

        ~NkThread() {
            if (mJoinable) {
                Detach();
            }
        }

        NkThread(const NkThread&) = delete;
        NkThread& operator=(const NkThread&) = delete;

        NkThread(NkThread&& other) noexcept
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            : mHandle(other.mHandle)
#else
            : mThread(other.mThread)
#endif
            , mJoinable(other.mJoinable) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            other.mHandle = nullptr;
#endif
            other.mJoinable = false;
        }

        NkThread& operator=(NkThread&& other) noexcept {
            if (this != &other) {
                if (mJoinable) {
                    Detach();
                }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
                mHandle = other.mHandle;
                other.mHandle = nullptr;
#else
                mThread = other.mThread;
#endif
                mJoinable = other.mJoinable;
                other.mJoinable = false;
            }
            return *this;
        }

        nk_bool Start(ThreadRoutine routine, void* userData) noexcept {
            if (routine == nullptr || mJoinable) {
                return false;
            }

            StartData* data = new StartData();
            data->Routine = routine;
            data->UserData = userData;

#if defined(NKENTSEU_PLATFORM_WINDOWS)
            mHandle = CreateThread(nullptr, 0, &ThreadEntry, data, 0, nullptr);
            if (mHandle == nullptr) {
                delete data;
                return false;
            }
            mJoinable = true;
            return true;
#else
            if (pthread_create(&mThread, nullptr, &ThreadEntry, data) != 0) {
                delete data;
                return false;
            }
            mJoinable = true;
            return true;
#endif
        }

        [[nodiscard]] nk_bool Joinable() const noexcept {
            return mJoinable;
        }

        void Join() noexcept {
            if (!mJoinable) {
                return;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            (void)WaitForSingleObject(mHandle, INFINITE);
            (void)CloseHandle(mHandle);
            mHandle = nullptr;
#else
            (void)pthread_join(mThread, nullptr);
#endif
            mJoinable = false;
        }

        void Detach() noexcept {
            if (!mJoinable) {
                return;
            }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            (void)CloseHandle(mHandle);
            mHandle = nullptr;
#else
            (void)pthread_detach(mThread);
#endif
            mJoinable = false;
        }

    private:
        struct StartData {
            ThreadRoutine Routine;
            void* UserData;
        };

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        static DWORD WINAPI ThreadEntry(LPVOID data) {
            StartData* startData = static_cast<StartData*>(data);
            if (startData != nullptr && startData->Routine != nullptr) {
                startData->Routine(startData->UserData);
            }
            delete startData;
            return 0;
        }
#else
        static void* ThreadEntry(void* data) {
            StartData* startData = static_cast<StartData*>(data);
            if (startData != nullptr && startData->Routine != nullptr) {
                startData->Routine(startData->UserData);
            }
            delete startData;
            return nullptr;
        }
#endif

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        HANDLE mHandle;
#else
        pthread_t mThread;
#endif
        nk_bool mJoinable;
    };

} // namespace loggersync
} // namespace nkentseu

#endif // NKENTSEU_LOGGER_NKSYNC_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION - NkSync
// =============================================================================
//
//   threading::NkMutex m;
//   loggersync::NkLockGuard guard(m);
//   // section critique
//
// =============================================================================
