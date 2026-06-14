/**
 * @File    NkAudioBus.h
 * @Brief   Audio Bus hierarchique (Master -> SFX/Music/Voice/UI -> voix).
 *
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 *
 * @Description
 *  Architecture style FMOD/Wwise : les voix sont routees vers des buses
 *  hierarchiques permettant :
 *   - Volume independant par groupe (musique, SFX, voix, UI)
 *   - Effets DSP par bus (reverb sur SFX, EQ sur musique, etc.)
 *   - Sidechain/ducking (musique baissee pendant dialogue)
 *   - Mute/solo par bus
 *
 * @Hierarchie
 *  AudioEngine
 *    └── BusMaster              (root, volume final, master effects)
 *          ├── BusMusic         (musique de fond)
 *          ├── BusSFX           (effets sonores - default pour voices)
 *          ├── BusVoice         (dialogues / voix-off)
 *          └── BusUI            (interface)
 *
 * @MultiPlatform
 *  100% C++17 standard. Aucune dependance OS.
 */

#pragma once

#ifndef NK_NKAUDIO_SRC_NKAUDIO_NKAUDIOBUS_H_INCLUDED
#define NK_NKAUDIO_SRC_NKAUDIO_NKAUDIOBUS_H_INCLUDED

#include "NKAudio/NKAudio.h"
#include "NKMemory/NkAllocator.h"
#include "NKCore/NkAtomic.h"

namespace nkentseu {
    namespace audio {

        // Forward decl
        class NkAudioBus;

        /**
         * @brief Bus audio hierarchique.
         *
         * Un bus represente un point de routage : les voix sont assignees
         * a un bus, les buses peuvent etre groupes via parent/children.
         * Le volume effectif d'une voix = volume_voix * volume_bus *
         * volume_bus_parent * ... * volume_master.
         */
        class NKENTSEU_AUDIO_API NkAudioBus {
        public:
            static constexpr int32 MAX_EFFECTS  = 8;   ///< Effets par bus
            static constexpr int32 MAX_CHILDREN = 16;  ///< Sous-buses par parent
            static constexpr int32 MAX_NAME_LEN = 32;  ///< Longueur max nom

            NkAudioBus(const char* name, NkAudioBus* parent = nullptr) noexcept;
            ~NkAudioBus() noexcept;

            NkAudioBus(const NkAudioBus&)            = delete;
            NkAudioBus& operator=(const NkAudioBus&) = delete;

            // ── Identite & hierarchie ────────────────────────────────────

            const char* GetName() const noexcept   { return mName; }
            NkAudioBus* GetParent() const noexcept { return mParent; }
            int32       GetChildCount() const noexcept { return mChildCount; }
            NkAudioBus* GetChild(int32 idx) const noexcept;

            /// Cherche un bus descendant (recherche recursive par nom).
            NkAudioBus* FindDescendant(const char* name) noexcept;

            // ── Volume & etat ────────────────────────────────────────────

            void    SetVolume(float32 volume) noexcept;
            float32 GetVolume() const noexcept            { return mVolume; }

            /// Volume effectif = volume_local * volume_parent (recursif).
            float32 GetEffectiveVolume() const noexcept;

            void SetMute(bool mute) noexcept   { mMuted = mute; }
            bool IsMuted() const noexcept      { return mMuted; }

            void SetSolo(bool solo) noexcept   { mSoloed = solo; }
            bool IsSoloed() const noexcept     { return mSoloed; }

            // ── Effets DSP ───────────────────────────────────────────────

            /**
             * @brief Ajoute un effet a la chaine du bus.
             * @return true si succes, false si chaine pleine (MAX_EFFECTS).
             */
            bool AddEffect(IAudioEffect* effect) noexcept;
            void RemoveEffect(IAudioEffect* effect) noexcept;
            void ClearEffects() noexcept;
            int32 GetEffectCount() const noexcept { return mEffectCount; }

            // ── Sidechain (ducking) ──────────────────────────────────────

            /**
             * @brief Active le ducking : ce bus est attenue quand le bus source
             *        joue. Typique : musique ducks pendant dialogue.
             *
             * @param sourceBus  Bus source declencheur (e.g. "Voice")
             * @param amount     Force du ducking [0..1] (1 = silence quand actif)
             * @param threshold  Niveau RMS du source au-dessus duquel ducker
             */
            void SetSidechainFromBus(NkAudioBus* sourceBus,
                                      float32 amount    = 0.7f,
                                      float32 threshold = 0.05f) noexcept;
            void ClearSidechain() noexcept;
            NkAudioBus* GetSidechainSource() const noexcept { return mSidechainSrc; }

            // ── Internal (utilise par AudioEngine) ───────────────────────

            /**
             * @brief Process le bus : recoit les samples mixes de ses voix +
             *        sous-buses, applique les effets, ecrit dans outBuf.
             *        Appele depuis le thread audio.
             */
            void ProcessChain(float32* outBuf, int32 frames, int32 channels) noexcept;

            /// Ajout enfant (appele par GetOrCreateBus).
            bool AttachChild(NkAudioBus* child) noexcept;

            /// Compteur de voix actives sur ce bus (gere par AudioEngine).
            /// Utilise par le sidechain pour detecter quand le bus joue.
            void IncrementActiveVoices() noexcept;
            void DecrementActiveVoices() noexcept;
            int32 GetActiveVoiceCount() const noexcept;
            bool  HasActiveVoice() const noexcept { return GetActiveVoiceCount() > 0; }

        private:
            char         mName[MAX_NAME_LEN];
            NkAudioBus*  mParent       = nullptr;
            NkAudioBus*  mChildren[MAX_CHILDREN] = {};
            int32        mChildCount   = 0;

            float32      mVolume       = 1.0f;
            bool         mMuted        = false;
            bool         mSoloed       = false;

            IAudioEffect* mEffects[MAX_EFFECTS] = {};
            int32         mEffectCount = 0;

            NkAudioBus*  mSidechainSrc  = nullptr;
            float32      mSidechainAmt  = 0.7f;
            float32      mSidechainThr  = 0.05f;

            // Tracking voix actives pour sidechain (atomique pour thread audio).
            NkAtomic<int32> mActiveVoiceCount;
        };

    } // namespace audio
} // namespace nkentseu

#endif // NK_NKAUDIO_SRC_NKAUDIO_NKAUDIOBUS_H_INCLUDED
