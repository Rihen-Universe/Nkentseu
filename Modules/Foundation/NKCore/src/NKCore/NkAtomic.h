// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkAtomic.h
// DESCRIPTION: Opérations atomiques multi-plateforme
// AUTEUR: Rihen
// DATE: 2026-02-08
// VERSION: 1.2.0
// MODIFICATIONS: Correction des types nk_xxx
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_NKATOMIC_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_NKATOMIC_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKPlatform/NkCompilerDetect.h"
#include "NKCore/NkInline.h"
#include "NkExport.h"

// Headers atomiques platform-specific
#if defined(NKENTSEU_CPP11)
#include <atomic>
#elif defined(NKENTSEU_COMPILER_MSVC)
#include <intrin.h>
#include <windows.h>
#include <emmintrin.h> // Pour _mm_pause()
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
// GCC/Clang built-ins
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Namespace core.
 */
namespace core {

/**
 * @brief Ordre mémoire pour opérations atomiques
 */
enum class NkMemoryOrder : nk_uint8 {
	NK_RELAXED, ///< Pas de garantie d'ordre
	NK_CONSUME, ///< Acquire pour dépendances
	NK_ACQUIRE, ///< Synchronisation lecture
	NK_RELEASE, ///< Synchronisation écriture
	NK_ACQREL,	///< Acquire + Release
	NK_SEQCST	///< Ordre séquentiel (le plus fort)
};

// ========================================
// ATOMIC TEMPLATE (C++11+)
// ========================================

#if defined(NKENTSEU_CPP11)

template <typename T> class NkAtomic {
public:
	NkAtomic() NKENTSEU_NOEXCEPT : mValue() {
	}
	explicit NkAtomic(T value) NKENTSEU_NOEXCEPT : mValue(value) {
	}

	NkAtomic(const NkAtomic &) = delete;
	NkAtomic &operator=(const NkAtomic &) = delete;

	// Load
	NKENTSEU_FORCE_INLINE T Load(NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) const NKENTSEU_NOEXCEPT {
		return mValue.load(ToStdMemoryOrder(order));
	}

	// Store
	NKENTSEU_FORCE_INLINE void Store(T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
		mValue.store(value, ToStdMemoryOrder(order));
	}

	// Exchange
	NKENTSEU_FORCE_INLINE T Exchange(T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
		return mValue.exchange(value, ToStdMemoryOrder(order));
	}

	// Compare-Exchange
	NKENTSEU_FORCE_INLINE nk_bool
	CompareExchangeWeak(T &expected, T desired, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
		return mValue.compare_exchange_weak(expected, desired, ToStdMemoryOrder(order));
	}

	NKENTSEU_FORCE_INLINE nk_bool
	CompareExchangeStrong(T &expected, T desired, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
		return mValue.compare_exchange_strong(expected, desired, ToStdMemoryOrder(order));
	}

	// Fetch-Add (pour types arithmétiques)
	NKENTSEU_FORCE_INLINE T FetchAdd(T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
		return mValue.fetch_add(value, ToStdMemoryOrder(order));
	}

	// Fetch-Sub
	NKENTSEU_FORCE_INLINE T FetchSub(T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
		return mValue.fetch_sub(value, ToStdMemoryOrder(order));
	}

	// Operators
	NKENTSEU_FORCE_INLINE operator T() const NKENTSEU_NOEXCEPT {
		return Load();
	}

	NKENTSEU_FORCE_INLINE T operator=(T value) NKENTSEU_NOEXCEPT {
		Store(value);
		return value;
	}

	NKENTSEU_FORCE_INLINE T operator++() NKENTSEU_NOEXCEPT {
		return FetchAdd(1) + 1;
	}

	NKENTSEU_FORCE_INLINE T operator++(int) NKENTSEU_NOEXCEPT {
		return FetchAdd(1);
	}

	NKENTSEU_FORCE_INLINE T operator--() NKENTSEU_NOEXCEPT {
		return FetchSub(1) - 1;
	}

	NKENTSEU_FORCE_INLINE T operator--(int) NKENTSEU_NOEXCEPT {
		return FetchSub(1);
	}

private:
	std::atomic<T> mValue;

	static std::memory_order ToStdMemoryOrder(NkMemoryOrder order) {
		switch (order) {
			case NkMemoryOrder::NK_RELAXED:
				return std::memory_order_relaxed;
			case NkMemoryOrder::NK_CONSUME:
				return std::memory_order_consume;
			case NkMemoryOrder::NK_ACQUIRE:
				return std::memory_order_acquire;
			case NkMemoryOrder::NK_RELEASE:
				return std::memory_order_release;
			case NkMemoryOrder::NK_ACQREL:
				return std::memory_order_acq_rel;
			case NkMemoryOrder::NK_SEQCST:
				return std::memory_order_seq_cst;
		}
		return std::memory_order_seq_cst;
	}
};

#else

// ========================================
// FALLBACK PRE-C++11 (intrinsics)
// ========================================

template <typename T> class NkAtomic {
public:
	NkAtomic() NKENTSEU_NOEXCEPT : mValue() {
	}
	explicit NkAtomic(T value) NKENTSEU_NOEXCEPT : mValue(value) {
	}

	NkAtomic(const NkAtomic &) = delete;
	NkAtomic &operator=(const NkAtomic &) = delete;

	// Load
	NKENTSEU_FORCE_INLINE T Load(NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) const NKENTSEU_NOEXCEPT {
#if defined(NKENTSEU_COMPILER_MSVC)
		MemoryBarrier();
		return mValue;
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
		__sync_synchronize();
		return mValue;
#else
		return mValue;
#endif
	}

	// Store
	NKENTSEU_FORCE_INLINE void Store(T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
#if defined(NKENTSEU_COMPILER_MSVC)
		mValue = value;
		MemoryBarrier();
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
		mValue = value;
		__sync_synchronize();
#else
		mValue = value;
#endif
	}

	// Exchange
	NKENTSEU_FORCE_INLINE T Exchange(T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
#if defined(NKENTSEU_COMPILER_MSVC)
		if (sizeof(T) == 4)
			return InterlockedExchange(reinterpret_cast<volatile long *>(const_cast<T *>(&mValue)),
									   static_cast<long>(value));
		if (sizeof(T) == 8)
			return InterlockedExchange64(reinterpret_cast<volatile long long *>(const_cast<T *>(&mValue)),
										 static_cast<long long>(value));
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
		return __sync_lock_test_and_set(&mValue, value);
#endif
		T old = mValue;
		mValue = value;
		return old;
	}

	NKENTSEU_FORCE_INLINE T FetchAdd(T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
#if defined(NKENTSEU_COMPILER_MSVC)
		if (sizeof(T) == 1) { // 8-bit
			char *ptr = reinterpret_cast<char *>(const_cast<T *>(&mValue));
			return static_cast<T>(_InterlockedExchangeAdd8(ptr, static_cast<char>(value)));
		} else if (sizeof(T) == 2) { // 16-bit
			short *ptr = reinterpret_cast<short *>(const_cast<T *>(&mValue));
			return static_cast<T>(_InterlockedExchangeAdd16(ptr, static_cast<short>(value)));
		} else if (sizeof(T) == 4) { // 32-bit
			long *ptr = reinterpret_cast<long *>(const_cast<T *>(&mValue));
			return static_cast<T>(_InterlockedExchangeAdd(ptr, static_cast<long>(value)));
		} else if (sizeof(T) == 8) { // 64-bit
			long long *ptr = reinterpret_cast<long long *>(const_cast<T *>(&mValue));
			return static_cast<T>(_InterlockedExchangeAdd64(ptr, static_cast<long long>(value)));
		}
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
		return __sync_fetch_and_add(&mValue, value);
#endif

		// Fallback générique non-atomique (pas thread-safe)
		T old = mValue;
		mValue = static_cast<T>(old + value);
		return old;
	}

	NKENTSEU_FORCE_INLINE T FetchSub(T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
		return FetchAdd(static_cast<T>(-value), order);
	}

	NKENTSEU_FORCE_INLINE nk_bool
	CompareExchangeWeak(T &expected, T desired, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
#if defined(NKENTSEU_COMPILER_MSVC)
		if (sizeof(T) == 1) { // 8-bit
			char *ptr = reinterpret_cast<char *>(const_cast<T *>(&mValue));
			char expectedChar = static_cast<char>(expected);
			char result = _InterlockedCompareExchange8(ptr, static_cast<char>(desired), expectedChar);
			nk_bool success = (result == expectedChar);
			if (!success)
				expected = static_cast<T>(result);
			return success;
		} else if (sizeof(T) == 2) { // 16-bit
			short *ptr = reinterpret_cast<short *>(const_cast<T *>(&mValue));
			short expectedShort = static_cast<short>(expected);
			short result = _InterlockedCompareExchange16(ptr, static_cast<short>(desired), expectedShort);
			nk_bool success = (result == expectedShort);
			if (!success)
				expected = static_cast<T>(result);
			return success;
		} else if (sizeof(T) == 4) { // 32-bit
			long *ptr = reinterpret_cast<long *>(const_cast<T *>(&mValue));
			long expectedLong = static_cast<long>(expected);
			long result = _InterlockedCompareExchange(ptr, static_cast<long>(desired), expectedLong);
			nk_bool success = (result == expectedLong);
			if (!success)
				expected = static_cast<T>(result);
			return success;
		} else if (sizeof(T) == 8) { // 64-bit
			long long *ptr = reinterpret_cast<long long *>(const_cast<T *>(&mValue));
			long long expectedLongLong = static_cast<long long>(expected);
			long long result = _InterlockedCompareExchange64(ptr, static_cast<long long>(desired), expectedLongLong);
			nk_bool success = (result == expectedLongLong);
			if (!success)
				expected = static_cast<T>(result);
			return success;
		}
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
		T expectedCopy = expected;
		T result = __sync_val_compare_and_swap(&mValue, expectedCopy, desired);
		nk_bool success = (result == expectedCopy);
		if (!success)
			expected = result;
		return success;
#endif

		// Fallback générique non-atomique
		T old = mValue;
		nk_bool fallbackSuccess = (old == expected);
		if (fallbackSuccess) {
			mValue = desired;
		} else {
			expected = old;
		}
		return fallbackSuccess;
	}

	NKENTSEU_FORCE_INLINE nk_bool
	CompareExchangeStrong(T &expected, T desired, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
		return CompareExchangeWeak(expected, desired, order);
	}

	NKENTSEU_FORCE_INLINE operator T() const NKENTSEU_NOEXCEPT {
		return Load();
	}

	NKENTSEU_FORCE_INLINE T operator=(T value) NKENTSEU_NOEXCEPT {
		Store(value);
		return value;
	}

	NKENTSEU_FORCE_INLINE T operator++() NKENTSEU_NOEXCEPT {
		return FetchAdd(1) + 1;
	}

	NKENTSEU_FORCE_INLINE T operator++(int) NKENTSEU_NOEXCEPT {
		return FetchAdd(1);
	}

	NKENTSEU_FORCE_INLINE T operator--() NKENTSEU_NOEXCEPT {
		return FetchSub(1) - 1;
	}

	NKENTSEU_FORCE_INLINE T operator--(int) NKENTSEU_NOEXCEPT {
		return FetchSub(1);
	}

	NKENTSEU_FORCE_INLINE T operator+=(T value) NKENTSEU_NOEXCEPT {
		return FetchAdd(value) + value;
	}

	NKENTSEU_FORCE_INLINE T operator-=(T value) NKENTSEU_NOEXCEPT {
		return FetchSub(value) - value;
	}

	NKENTSEU_FORCE_INLINE T operator&=(T value) NKENTSEU_NOEXCEPT {
		T old = Load();
		T desired;
		do {
			desired = static_cast<T>(old & value);
		} while (!CompareExchangeWeak(old, desired));
		return desired;
	}

	NKENTSEU_FORCE_INLINE T operator|=(T value) NKENTSEU_NOEXCEPT {
		T old = Load();
		T desired;
		do {
			desired = static_cast<T>(old | value);
		} while (!CompareExchangeWeak(old, desired));
		return desired;
	}

	NKENTSEU_FORCE_INLINE T operator^=(T value) NKENTSEU_NOEXCEPT {
		T old = Load();
		T desired;
		do {
			desired = static_cast<T>(old ^ value);
		} while (!CompareExchangeWeak(old, desired));
		return desired;
	}

private:
	volatile T mValue;
};

#endif

// ========================================
// TYPEDEFS COURANTS
// ========================================

using NkAtomicBool = NkAtomic<nk_bool>;
using NkAtomicInt8 = NkAtomic<nk_int8>;
using NkAtomicInt16 = NkAtomic<nk_int16>;
using NkAtomicInt32 = NkAtomic<nk_int32>;
using NkAtomicInt64 = NkAtomic<nk_int64>;
using NkAtomicUInt8 = NkAtomic<nk_uint8>;
using NkAtomicUInt16 = NkAtomic<nk_uint16>;
using NkAtomicUInt32 = NkAtomic<nk_uint32>;
using NkAtomicUInt64 = NkAtomic<nk_uint64>;
using NkAtomicSize = NkAtomic<nk_size>;
using NkAtomicPtr = NkAtomic<void *>;

// ========================================
// ATOMIC FLAG (spinlock)
// ========================================

class NkAtomicFlag {
public:
	NkAtomicFlag(nk_bool flag = false) NKENTSEU_NOEXCEPT : mFlag(flag) {
	}

	NKENTSEU_FORCE_INLINE nk_bool TestAndSet(NkMemoryOrder order = NkMemoryOrder::NK_ACQREL) NKENTSEU_NOEXCEPT {
		return mFlag.Exchange(true, order);
	}

	NKENTSEU_FORCE_INLINE void Clear(NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) NKENTSEU_NOEXCEPT {
		mFlag.Store(false, order);
	}

	NKENTSEU_FORCE_INLINE nk_bool IsSet() const NKENTSEU_NOEXCEPT {
		return mFlag.Load();
	}

private:
	NkAtomic<nk_bool> mFlag;
};

// ========================================
// MEMORY BARRIERS
// ========================================

NKENTSEU_FORCE_INLINE void NkAtomicThreadFence(NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) {
#if defined(NKENTSEU_CPP11)
	std::atomic_thread_fence(std::memory_order_seq_cst);
#elif defined(NKENTSEU_COMPILER_MSVC)
	MemoryBarrier();
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
	__sync_synchronize();
#endif
}

NKENTSEU_FORCE_INLINE void NkAtomicAcquireFence() {
#if defined(NKENTSEU_CPP11)
	std::atomic_thread_fence(std::memory_order_acquire);
#elif defined(NKENTSEU_COMPILER_MSVC)
	_ReadBarrier();
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
	__asm__ volatile("" ::: "memory");
#endif
}

NKENTSEU_FORCE_INLINE void NkAtomicReleaseFence() {
#if defined(NKENTSEU_CPP11)
	std::atomic_thread_fence(std::memory_order_release);
#elif defined(NKENTSEU_COMPILER_MSVC)
	_WriteBarrier();
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
	__asm__ volatile("" ::: "memory");
#endif
}

NKENTSEU_FORCE_INLINE void NkAtomicCompileBarrier() {
#if defined(NKENTSEU_COMPILER_MSVC)
	_ReadWriteBarrier();
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
	__asm__ volatile("" ::: "memory");
#endif
}

// ========================================
// FONCTIONS ATOMIQUES GLOBALES
// ========================================

/**
 * @brief Incrément atomique avec retour de l'ancienne valeur
 */
template <typename T>
NKENTSEU_FORCE_INLINE T NkAtomicIncrement(NkAtomic<T> &atomic, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) {
	return atomic.FetchAdd(static_cast<T>(1), order);
}

/**
 * @brief Décrément atomique avec retour de l'ancienne valeur
 */
template <typename T>
NKENTSEU_FORCE_INLINE T NkAtomicDecrement(NkAtomic<T> &atomic, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) {
	return atomic.FetchSub(static_cast<T>(1), order);
}

/**
 * @brief Ajoute atomiquement et retourne la nouvelle valeur
 */
template <typename T>
NKENTSEU_FORCE_INLINE T NkAtomicAdd(NkAtomic<T> &atomic, T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) {
	return atomic.FetchAdd(value, order) + value;
}

/**
 * @brief Soustrait atomiquement et retourne la nouvelle valeur
 */
template <typename T>
NKENTSEU_FORCE_INLINE T NkAtomicSubtract(NkAtomic<T> &atomic, T value, NkMemoryOrder order = NkMemoryOrder::NK_SEQCST) {
	return atomic.FetchSub(value, order) - value;
}

// ========================================
// SPINLOCK AVANCÉ
// ========================================

/**
 * @brief Spinlock avec backoff exponentiel
 */
class NkSpinLock {
public:
	NkSpinLock() : mFlag(false) {
	}

	void Lock() {
		nk_size backoff = 1;
		while (true) {
			// Essayer d'acquérir le lock
			if (!mFlag.TestAndSet(NkMemoryOrder::NK_ACQUIRE)) {
				return; // Success
			}

			// Backoff exponentiel
			for (nk_size i = 0; i < backoff; ++i) {
#if defined(NKENTSEU_COMPILER_MSVC) && (defined(_M_IX86) || defined(_M_X64))
				_mm_pause();
#elif (defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)) && (defined(__i386__) || defined(__x86_64__))
				__builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
				__asm__ __volatile__("yield");
#else
				// Fallback no-op pour architectures sans instruction de pause dédiée.
#endif
			}

			// Double le backoff, avec un maximum
			if (backoff < 1024) {
				backoff <<= 1;
			}
		}
	}

	nk_bool TryLock() {
		return !mFlag.TestAndSet(NkMemoryOrder::NK_ACQUIRE);
	}

	void Unlock() {
		mFlag.Clear(NkMemoryOrder::NK_RELEASE);
	}

private:
	NkAtomicFlag mFlag;
};

/**
 * @brief Scope guard pour spinlock
 */
class NkScopedSpinLock {
public:
	explicit NkScopedSpinLock(NkSpinLock &lock) : mLock(lock) {
		mLock.Lock();
	}

	~NkScopedSpinLock() {
		mLock.Unlock();
	}

	// Pas de copie
	NkScopedSpinLock(const NkScopedSpinLock &) = delete;
	NkScopedSpinLock &operator=(const NkScopedSpinLock &) = delete;

private:
	NkSpinLock &mLock;
};

} // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_NKATOMIC_H_INCLUDED
