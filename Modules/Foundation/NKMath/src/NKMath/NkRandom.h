//
// NkRandom.h
// Singleton de génération de nombres pseudo-aléatoires.
// Utilise rand() comme backend (suffisant pour le jeu).
// Copyright (c) 2024 Rihen. All rights reserved.
//

#pragma once
#ifndef __NKENTSEU_RANDOM_H__
#define __NKENTSEU_RANDOM_H__

#include "NKMath/NkLegacySystem.h"
#include "NkRange.h"
#include "NkVec.h"
#include "NkMat.h"
#include "NkQuat.h"
#include "NkColor.h"
#include <cstdlib>
#include <ctime>

namespace nkentseu {

    namespace math {

        // =====================================================================
        // NkRandom
        //
        // Singleton d'accès global.
        // Usage: NkRandom::Instance().NextFloat32()
        // Ou via la référence globale NkRand.
        //
        // Note: rand() est seedé une seule fois à la construction.
        // Pour un usage multi-thread, chaque thread devrait avoir son propre
        // générateur (utiliser un générateur par thread-local si besoin).
        // =====================================================================

        class NkRandom {
            public:

                // ── Singleton ─────────────────────────────────────────────────────

                static NkRandom& Instance() noexcept {
                    static NkRandom inst;
                    return inst;
                }

                NkRandom(const NkRandom&)            = delete;
                NkRandom& operator=(const NkRandom&) = delete;

                // ── Entiers non-signés ────────────────────────────────────────────

                uint8 NextUInt8() noexcept {
                    return static_cast<uint8>(rand()) % 256;
                }

                uint8 NextUInt8(uint8 max) noexcept {
                    return (max != 0) ? NextUInt8() % max : 0;
                }

                uint8 NextUInt8(uint8 min, uint8 max) noexcept {
                    return NextUInt8(NkRangeUInt(min, max));
                }

                uint8 NextUInt8(const NkRangeUInt& range) noexcept {
                    uint32 len = range.Len();
                    return (len != 0) ? (NextUInt8() % len) + range.GetMin() : range.GetMin();
                }

                uint32 NextUInt32() noexcept {
                    return static_cast<uint32>(rand());
                }

                uint32 NextUInt32(uint32 max) noexcept {
                    return (max != 0u) ? NextUInt32() % max : 0u;
                }

                uint32 NextUInt32(uint32 min, uint32 max) noexcept {
                    return NextUInt32(NkRangeUInt(min, max));
                }

                uint32 NextUInt32(const NkRangeUInt& range) noexcept {
                    uint32 len = range.Len();
                    return (len != 0u) ? (NextUInt32() % len) + range.GetMin() : range.GetMin();
                }

                // ── Entiers signés ────────────────────────────────────────────────

                int32 NextInt32() noexcept {
                    // Mappe [0, RAND_MAX] → [-RAND_MAX/2, RAND_MAX/2]
                    return static_cast<int32>(NextUInt32() % static_cast<uint32>(RAND_MAX + 1u))
                        - RAND_MAX / 2;
                }

                int32 NextInt32(int32 limit) noexcept {
                    int32 v = NextInt32();
                    return (limit != 0) ? v % limit : 0;
                }

                int32 NextInt32(int32 min, int32 max) noexcept {
                    return NextInt32(NkRangeInt(min, max));
                }

                int32 NextInt32(const NkRangeInt& range) noexcept {
                    int32 len = range.Len();
                    return (len != 0)
                        ? (static_cast<int32>(NextUInt32()) % len) + range.GetMin()
                        : range.GetMin();
                }

                // ── Flottants [0, 1) ──────────────────────────────────────────────

                float32 NextFloat32() noexcept {
                    return static_cast<float32>(rand()) / static_cast<float32>(RAND_MAX);
                }

                float32 NextFloat32(float32 limit) noexcept {
                    return NextFloat32() * limit;
                }

                float32 NextFloat32(float32 min, float32 max) noexcept {
                    return NextFloat32(NkRangeFloat(min, max));
                }

                float32 NextFloat32(const NkRangeFloat& range) noexcept {
                    return NextFloat32() * range.Len() + range.GetMin();
                }

                // ── Limites ───────────────────────────────────────────────────────

                uint32 MaxUInt() const noexcept { return static_cast<uint32>(RAND_MAX); }
                int32  MaxInt()  const noexcept { return  RAND_MAX / 2 + 1; }
                int32  MinInt()  const noexcept { return -RAND_MAX / 2; }

                template<typename T>
                T NextInRange(const NkRangeT<T>& range) noexcept {
                    return range.GetMin() + static_cast<T>(NextFloat32() * range.Len());
                }

                template<typename T>
                NkVec2T<T> NextVec2() noexcept {
                    return { (T)NextFloat32(), (T)NextFloat32() };
                }

                template<typename T>
                NkVec3T<T> NextVec3() noexcept {
                    return { (T)NextFloat32(), (T)NextFloat32(), (T)NextFloat32() };
                }

                template<typename T>
                NkVec4T<T> NextVec4() noexcept {
                    return { (T)NextFloat32(), (T)NextFloat32(), (T)NextFloat32(), (T)NextFloat32() };
                }

                template<typename T>
                NkMat2T<T> NextMat2() noexcept {
                    return { NextVec2<T>(), NextVec2<T>() };
                }

                template<typename T>
                NkMat3T<T> NextMat3() noexcept {
                    return { NextVec3<T>(), NextVec3<T>(), NextVec3<T>() };
                }

                template<typename T>
                NkMat4T<T> NextMat4() noexcept {
                    return { NextVec4<T>(), NextVec4<T>(), NextVec4<T>(), NextVec4<T>() };
                }

                NkColor NextColor() noexcept {
                    return { NextUInt8(), NextUInt8(), NextUInt8(), 255 };
                }

                NkColor NextColorA() noexcept {
                    return { NextUInt8(), NextUInt8(), NextUInt8(), NextUInt8() };
                }

                NkHSV NextHSV() noexcept {
                    return { NextFloat32(), NextFloat32(), NextFloat32() };
                }

            private:

                NkRandom() noexcept {
                    srand(static_cast<uint32>(time(nullptr)));
                }

        }; // class NkRandom


        // =====================================================================
        // Référence globale de commodité
        //
        // Usage : NkRand.NextFloat32()
        // Note  : Ce n'est PAS une macro — le nom de classe NkRandom reste utilisable.
        // =====================================================================

        static inline NkRandom& NkRand = NkRandom::Instance();

    } // namespace math

} // namespace nkentseu

#endif // __NKENTSEU_RANDOM_H__
