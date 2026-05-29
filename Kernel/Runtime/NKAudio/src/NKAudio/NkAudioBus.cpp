/**
 * @File    NkAudioBus.cpp
 * @Brief   Implementation NkAudioBus (Audio Bus hierarchique).
 *
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 */

#include "NKAudio/NkAudioBus.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace audio {

        // ── Constructor / Destructor ─────────────────────────────────────

        NkAudioBus::NkAudioBus(const char* name, NkAudioBus* parent) noexcept
            : mParent(parent), mActiveVoiceCount(0)
        {
            // Copy name (truncate si trop long)
            const char* src = name ? name : "Unnamed";
            usize n = ::strlen(src);
            if (n >= MAX_NAME_LEN) n = MAX_NAME_LEN - 1;
            ::memcpy(mName, src, n);
            mName[n] = '\0';

            // S'attacher au parent
            if (parent) parent->AttachChild(this);
        }

        NkAudioBus::~NkAudioBus() noexcept {
            // Les enfants ne sont PAS detruits ici (ownership = AudioEngine).
        }

        // ── Hierarchie ───────────────────────────────────────────────────

        NkAudioBus* NkAudioBus::GetChild(int32 idx) const noexcept {
            if (idx < 0 || idx >= mChildCount) return nullptr;
            return mChildren[idx];
        }

        bool NkAudioBus::AttachChild(NkAudioBus* child) noexcept {
            if (!child || mChildCount >= MAX_CHILDREN) return false;
            mChildren[mChildCount++] = child;
            return true;
        }

        NkAudioBus* NkAudioBus::FindDescendant(const char* name) noexcept {
            if (!name) return nullptr;
            for (int32 i = 0; i < mChildCount; ++i) {
                NkAudioBus* c = mChildren[i];
                if (!c) continue;
                if (::strcmp(c->mName, name) == 0) return c;
                NkAudioBus* sub = c->FindDescendant(name);
                if (sub) return sub;
            }
            return nullptr;
        }

        // ── Volume ───────────────────────────────────────────────────────

        void NkAudioBus::SetVolume(float32 volume) noexcept {
            if (volume < 0.0f) volume = 0.0f;
            mVolume = volume;
        }

        float32 NkAudioBus::GetEffectiveVolume() const noexcept {
            if (mMuted) return 0.0f;
            float32 v = mVolume;
            if (mParent) v *= mParent->GetEffectiveVolume();
            // Sidechain ducking : si le bus source a au moins une voix active,
            // attenuer ce bus par mSidechainAmt (typique : musique baisse de 70%
            // pendant un dialogue).
            if (mSidechainSrc && mSidechainSrc->HasActiveVoice()) {
                v *= (1.0f - mSidechainAmt);
            }
            return v;
        }

        // ── Tracking voix actives (sidechain) ────────────────────────────

        void NkAudioBus::IncrementActiveVoices() noexcept {
            ++mActiveVoiceCount;
        }

        void NkAudioBus::DecrementActiveVoices() noexcept {
            int32 cur = mActiveVoiceCount.Load();
            // Decrement seulement si > 0 (eviter underflow en cas de double cleanup).
            while (cur > 0) {
                if (mActiveVoiceCount.CompareExchangeWeak(cur, cur - 1)) break;
            }
        }

        int32 NkAudioBus::GetActiveVoiceCount() const noexcept {
            return mActiveVoiceCount.Load();
        }

        // ── Effets ───────────────────────────────────────────────────────

        bool NkAudioBus::AddEffect(IAudioEffect* effect) noexcept {
            if (!effect || mEffectCount >= MAX_EFFECTS) return false;
            mEffects[mEffectCount++] = effect;
            return true;
        }

        void NkAudioBus::RemoveEffect(IAudioEffect* effect) noexcept {
            for (int32 i = 0; i < mEffectCount; ++i) {
                if (mEffects[i] == effect) {
                    for (int32 j = i; j < mEffectCount - 1; ++j) {
                        mEffects[j] = mEffects[j + 1];
                    }
                    mEffects[--mEffectCount] = nullptr;
                    return;
                }
            }
        }

        void NkAudioBus::ClearEffects() noexcept {
            for (int32 i = 0; i < mEffectCount; ++i) mEffects[i] = nullptr;
            mEffectCount = 0;
        }

        // ── Sidechain ────────────────────────────────────────────────────

        void NkAudioBus::SetSidechainFromBus(NkAudioBus* sourceBus,
                                              float32 amount,
                                              float32 threshold) noexcept {
            mSidechainSrc = sourceBus;
            if (amount < 0.0f) amount = 0.0f;
            if (amount > 1.0f) amount = 1.0f;
            mSidechainAmt = amount;
            mSidechainThr = threshold;
        }

        void NkAudioBus::ClearSidechain() noexcept {
            mSidechainSrc = nullptr;
        }

        // ── Process (thread audio) ──────────────────────────────────────

        void NkAudioBus::ProcessChain(float32* outBuf, int32 frames, int32 channels) noexcept {
            if (!outBuf || frames <= 0 || channels <= 0) return;
            if (mMuted) {
                ::memset(outBuf, 0, usize(frames) * usize(channels) * sizeof(float32));
                return;
            }

            // Applique les effets DSP dans l'ordre (in-place)
            for (int32 i = 0; i < mEffectCount; ++i) {
                IAudioEffect* fx = mEffects[i];
                if (fx) fx->Process(outBuf, frames, channels);
            }

            // Applique le volume effectif (local * parent recursif)
            float32 v = GetEffectiveVolume();
            if (v < 0.999f || v > 1.001f) {
                usize total = usize(frames) * usize(channels);
                for (usize i = 0; i < total; ++i) outBuf[i] *= v;
            }
        }

    } // namespace audio
} // namespace nkentseu
