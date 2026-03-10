// ============================================================
// FILE: NkThread.h
// DESCRIPTION: Native OS thread wrapper (production-ready)
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 2.1.0
// ============================================================
// Wrapper C++ autour des threads OS natifs:
// - Windows: CreateThread / Win32 handles
// - Unix: pthread
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_NKTHREAD_H_INCLUDED
#define NKENTSEU_THREADING_NKTHREAD_H_INCLUDED

#include "NKCore/NkTraits.h"
#include "NKCore/NkTypes.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKThreading/NkThreadingExport.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <stdint.h>
    #include <pthread.h>
    #include <sched.h>
    #include <string.h>
    #if defined(NKENTSEU_PLATFORM_LINUX)
        #include <sys/syscall.h>
        #include <sys/types.h>
    #endif
    #include <unistd.h>
#endif

namespace nkentseu {
namespace threading {

    class NKTHREADING_API NkThread {
    public:
        using ThreadFunc = NkFunction<void>;
        using ThreadId = nk_uint64;

        NkThread() noexcept
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            : mHandle(nullptr)
            , mThreadId(0)
#else
            : mThread()
            , mJoinable(false)
#endif
        {
        }

        template<typename Func>
        explicit NkThread(Func&& func) noexcept
            : NkThread() {
            Start(ThreadFunc(traits::NkForward<Func>(func)));
        }

        ~NkThread() {
            if (Joinable()) {
                Detach();
            }
        }

        NkThread(NkThread&& other) noexcept
            : NkThread() {
            MoveFrom(other);
        }

        NkThread& operator=(NkThread&& other) noexcept {
            if (this != &other) {
                if (Joinable()) {
                    Detach();
                }
                MoveFrom(other);
            }
            return *this;
        }

        NkThread(const NkThread&) = delete;
        NkThread& operator=(const NkThread&) = delete;

        void Join() noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            if (mHandle != nullptr) {
                (void)WaitForSingleObject(mHandle, INFINITE);
                (void)CloseHandle(mHandle);
                mHandle = nullptr;
                mThreadId = 0;
            }
#else
            if (mJoinable) {
                (void)pthread_join(mThread, nullptr);
                mJoinable = false;
            }
#endif
        }

        void Detach() noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            if (mHandle != nullptr) {
                (void)CloseHandle(mHandle);
                mHandle = nullptr;
                mThreadId = 0;
            }
#else
            if (mJoinable) {
                (void)pthread_detach(mThread);
                mJoinable = false;
            }
#endif
        }

        [[nodiscard]] nk_bool Joinable() const noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            return mHandle != nullptr;
#else
            return mJoinable;
#endif
        }

        [[nodiscard]] ThreadId GetID() const noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            return static_cast<ThreadId>(mThreadId);
#else
            if (!mJoinable) {
                return 0;
            }
            ThreadId id = 0;
            const nk_size copySize =
                (sizeof(pthread_t) < sizeof(ThreadId)) ? sizeof(pthread_t) : sizeof(ThreadId);
            memcpy(&id, &mThread, static_cast<size_t>(copySize));
            return id;
#endif
        }

        [[nodiscard]] static ThreadId GetCurrentThreadId() noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            return static_cast<ThreadId>(::GetCurrentThreadId());
#elif defined(NKENTSEU_PLATFORM_LINUX)
            return static_cast<ThreadId>(::syscall(SYS_gettid));
#else
            ThreadId id = 0;
            pthread_t thread = pthread_self();
            const nk_size copySize =
                (sizeof(pthread_t) < sizeof(ThreadId)) ? sizeof(pthread_t) : sizeof(ThreadId);
            memcpy(&id, &thread, static_cast<size_t>(copySize));
            return id;
#endif
        }

        void SetName(const nk_char* name) noexcept {
            if (name == nullptr || name[0] == '\0') {
                return;
            }

#if defined(NKENTSEU_PLATFORM_WINDOWS)
            if (mHandle == nullptr) {
                return;
            }

            using SetThreadDescriptionFn = HRESULT (WINAPI*)(HANDLE, PCWSTR);
            HMODULE kernel32 = GetModuleHandleW(L"Kernel32.dll");
            if (kernel32 == nullptr) {
                return;
            }

            SetThreadDescriptionFn setThreadDescription =
                reinterpret_cast<SetThreadDescriptionFn>(
                    GetProcAddress(kernel32, "SetThreadDescription"));
            if (setThreadDescription == nullptr) {
                return;
            }

            wchar_t wideName[64] = {};
            const int charsWritten = MultiByteToWideChar(CP_UTF8, 0, name, -1, wideName, 64);
            if (charsWritten > 0) {
                (void)setThreadDescription(mHandle, wideName);
            }
#elif defined(NKENTSEU_PLATFORM_LINUX)
            if (!mJoinable) {
                return;
            }

            char shortName[16] = {};
            strncpy(shortName, name, sizeof(shortName) - 1);
            shortName[sizeof(shortName) - 1] = '\0';
            (void)pthread_setname_np(mThread, shortName);
#elif defined(NKENTSEU_PLATFORM_MACOS)
            if (mJoinable && pthread_equal(mThread, pthread_self())) {
                (void)pthread_setname_np(name);
            }
#else
            (void)name;
#endif
        }

        void SetAffinity(nk_uint32 cpuCore) noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            if (mHandle == nullptr) {
                return;
            }
            const nk_uint32 maxBits = static_cast<nk_uint32>(sizeof(DWORD_PTR) * 8U);
            if (cpuCore >= maxBits) {
                return;
            }
            const DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << cpuCore);
            (void)SetThreadAffinityMask(mHandle, mask);
#elif defined(NKENTSEU_PLATFORM_LINUX)
            if (!mJoinable || cpuCore >= static_cast<nk_uint32>(CPU_SETSIZE)) {
                return;
            }
            cpu_set_t set;
            CPU_ZERO(&set);
            CPU_SET(static_cast<int>(cpuCore), &set);
            (void)pthread_setaffinity_np(mThread, sizeof(set), &set);
#else
            (void)cpuCore;
#endif
        }

        void SetPriority(nk_int32 priority) noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            if (mHandle == nullptr) {
                return;
            }

            int nativePriority = THREAD_PRIORITY_NORMAL;
            if (priority <= -2) {
                nativePriority = THREAD_PRIORITY_LOWEST;
            } else if (priority == -1) {
                nativePriority = THREAD_PRIORITY_BELOW_NORMAL;
            } else if (priority == 1) {
                nativePriority = THREAD_PRIORITY_ABOVE_NORMAL;
            } else if (priority >= 2) {
                nativePriority = THREAD_PRIORITY_HIGHEST;
            }
            (void)SetThreadPriority(mHandle, nativePriority);
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_MACOS)
            if (!mJoinable) {
                return;
            }

            int policy = 0;
            sched_param param{};
            if (pthread_getschedparam(mThread, &policy, &param) != 0) {
                return;
            }

            const int minPriority = sched_get_priority_min(policy);
            const int maxPriority = sched_get_priority_max(policy);
            if (minPriority < 0 || maxPriority < 0) {
                return;
            }

            nk_int32 clamped = priority;
            if (clamped < -2) {
                clamped = -2;
            } else if (clamped > 2) {
                clamped = 2;
            }

            const int span = maxPriority - minPriority;
            const int mapped = minPriority + ((clamped + 2) * span) / 4;
            param.sched_priority = mapped;
            (void)pthread_setschedparam(mThread, policy, &param);
#else
            (void)priority;
#endif
        }

        [[nodiscard]] nk_uint32 GetCurrentCore() const noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            return static_cast<nk_uint32>(GetCurrentProcessorNumber());
#elif defined(NKENTSEU_PLATFORM_LINUX)
            const int cpu = sched_getcpu();
            return (cpu >= 0) ? static_cast<nk_uint32>(cpu) : 0;
#else
            return 0;
#endif
        }

    private:
        struct ThreadStartData {
            explicit ThreadStartData(ThreadFunc&& function) noexcept
                : Function(traits::NkMove(function)) {
            }

            ThreadFunc Function;
        };

        void Start(ThreadFunc&& function) noexcept {
            if (Joinable()) {
                Detach();
            }

            ThreadStartData* startData = new ThreadStartData(traits::NkMove(function));

#if defined(NKENTSEU_PLATFORM_WINDOWS)
            mHandle = CreateThread(
                nullptr,
                0,
                &NkThread::ThreadEntry,
                startData,
                0,
                &mThreadId);

            if (mHandle == nullptr) {
                delete startData;
                mThreadId = 0;
            }
#else
            const int result = pthread_create(&mThread, nullptr, &NkThread::ThreadEntry, startData);
            if (result != 0) {
                delete startData;
                mJoinable = false;
            } else {
                mJoinable = true;
            }
#endif
        }

        void MoveFrom(NkThread& other) noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            mHandle = other.mHandle;
            mThreadId = other.mThreadId;

            other.mHandle = nullptr;
            other.mThreadId = 0;
#else
            mThread = other.mThread;
            mJoinable = other.mJoinable;

            other.mJoinable = false;
#endif
        }

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        static DWORD WINAPI ThreadEntry(LPVOID userData) {
            ThreadStartData* startData = static_cast<ThreadStartData*>(userData);
            if (startData == nullptr) {
                return 0;
            }

            ThreadFunc function = traits::NkMove(startData->Function);
            delete startData;

            if (function) {
                function();
            }
            return 0;
        }
#else
        static void* ThreadEntry(void* userData) {
            ThreadStartData* startData = static_cast<ThreadStartData*>(userData);
            if (startData == nullptr) {
                return nullptr;
            }

            ThreadFunc function = traits::NkMove(startData->Function);
            delete startData;

            if (function) {
                function();
            }
            return nullptr;
        }
#endif

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        HANDLE mHandle;
        DWORD mThreadId;
#else
        pthread_t mThread;
        nk_bool mJoinable;
#endif
    };

} // namespace threading
} // namespace nkentseu

#endif // NKENTSEU_THREADING_NKTHREAD_H_INCLUDED
