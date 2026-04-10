#pragma once
// =============================================================================
// NkCommandPool.h
// Pool de command buffers — évite les new/delete à chaque frame.
//
// Principe : Acquire() retourne un CB libre (ou en crée un nouveau),
// Release() le remet dans le pool libre, Reset() détruit tout.
//
// Utilisation typique (par frame) :
//   auto* cmd = mPool.Acquire();
//   cmd->Begin();
//   // ... record ...
//   cmd->End();
//   device->Submit(&cmd, 1, fence);
//   // Après WaitFence :
//   mPool.Release(cmd);
//
// Thread-safety : Acquire/Release sont protégés par mutex.
// =============================================================================
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKThreading/NkMutex.h"

namespace nkentseu {

    class NkCommandPool {
        public:
            // ── Construction ──────────────────────────────────────────────────────────
            NkCommandPool() = default;

            explicit NkCommandPool(NkIDevice* device, NkCommandBufferType type = NkCommandBufferType::NK_GRAPHICS) : mDevice(device), mType(type) {}

            ~NkCommandPool() { Reset(); }

            // Non-copiable, déplaçable
            NkCommandPool(const NkCommandPool&) = delete;
            NkCommandPool& operator=(const NkCommandPool&) = delete;

            NkCommandPool(NkCommandPool&& o) noexcept
                : mDevice(o.mDevice), mType(o.mType)
                , mFree(traits::NkMove(o.mFree)), mInFlight(traits::NkMove(o.mInFlight)) {
                o.mDevice = nullptr;
            }

            // ── Init tardive (si construit par défaut) ─────────────────────────────
            void Init(NkIDevice* device,
                    NkCommandBufferType type = NkCommandBufferType::NK_GRAPHICS) {
                mDevice = device;
                mType   = type;
            }

            // ── Acquire — obtenir un CB prêt à l'emploi (Reset() appelé dessus) ───
            NkICommandBuffer* Acquire() {
                threading::NkScopedLock lock(mMutex);
                NkICommandBuffer* cb = nullptr;
                if (!mFree.IsEmpty()) {
                    cb = mFree.Back();
                    mFree.PopBack();
                    cb->Reset();
                } else {
                    cb = mDevice->CreateCommandBuffer(mType);
                }
                mInFlight.PushBack(cb);
                return cb;
            }

            // ── Release — remettre un CB terminé dans le pool libre ───────────────
            void Release(NkICommandBuffer* cb) {
                threading::NkScopedLock lock(mMutex);
                for (usize i = 0; i < mInFlight.Size(); i++) {
                    if (mInFlight[i] == cb) {
                        mInFlight.Erase(mInFlight.Begin() + i);
                        mFree.PushBack(cb);
                        break;
                    }
                }
            }

            // ── ReleaseAll — libérer tous les CBs en vol (après WaitIdle) ─────────
            void ReleaseAll() {
                threading::NkScopedLock lock(mMutex);
                for (usize i = 0; i < mInFlight.Size(); i++) mFree.PushBack(mInFlight[i]);
                mInFlight.Clear();
            }

            // ── Reset — détruire tous les CBs (à l'arrêt du device) ──────────────
            void Reset() {
                if (!mDevice) return;
                threading::NkScopedLock lock(mMutex);
                for (usize i = 0; i < mInFlight.Size(); i++) mDevice->DestroyCommandBuffer(mInFlight[i]);
                for (usize i = 0; i < mFree.Size(); i++)     mDevice->DestroyCommandBuffer(mFree[i]);
                mInFlight.Clear();
                mFree.Clear();
            }

            // ── Stats ─────────────────────────────────────────────────────────────
            uint32 FreeCount()     const { return (uint32)mFree.Size(); }
            uint32 InFlightCount() const { return (uint32)mInFlight.Size(); }
            uint32 TotalCount()    const { return FreeCount() + InFlightCount(); }

        private:
            NkIDevice*                      mDevice = nullptr;
            NkCommandBufferType             mType   = NkCommandBufferType::NK_GRAPHICS;
            mutable threading::NkMutex      mMutex;
            NkVector<NkICommandBuffer*>     mFree;
            NkVector<NkICommandBuffer*>     mInFlight;
    };

} // namespace nkentseu
