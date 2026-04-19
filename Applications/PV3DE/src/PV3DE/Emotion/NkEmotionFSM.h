#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "PV3DE/Core/NkClinicalState.h"

namespace nkentseu {
    namespace pv3de {

        // =====================================================================
        // NkEmotionTransition — règle de transition entre deux états
        // =====================================================================
        struct NkEmotionTransition {
            EmotionState from;
            EmotionState to;
            nk_float32   blendTime;    // secondes
            nk_float32   minIntensity; // seuil déclencheur (0–1)
        };

        // =====================================================================
        // NkEmotionOutput — sortie de la FSM consommée par NkFaceController
        //                   et NkBodyController
        // =====================================================================
        struct NkEmotionOutput {
            EmotionState state     = EmotionState::Neutral;
            nk_float32   intensity = 0.f;   // 0–1
            nk_float32   blendT    = 0.f;   // 0=état précédent, 1=état courant

            // Paramètres vocaux
            nk_float32 voicePitch      = 1.f;   // multiplicateur
            nk_float32 voiceBreath     = 0.f;   // souffle 0–1
            nk_float32 voiceTremor     = 0.f;   // tremblement 0–1
            nk_float32 speechRate      = 1.f;   // vitesse relative

            // Paramètres corporels
            nk_float32 breathRate      = 16.f;  // resp/min
            nk_float32 breathAmplitude = 0.3f;  // amplitude cage thoracique
            nk_float32 tremorAmplitude = 0.f;   // tremblement membres 0–1
        };

        // =====================================================================
        // NkEmotionFSM
        // Machine à états émotionnels pilotée par NkClinicalState.
        // Gère les transitions interpolées et calcule NkEmotionOutput.
        // =====================================================================
        class NkEmotionFSM {
        public:
            NkEmotionFSM();
            ~NkEmotionFSM() = default;

            void Init();

            // Mise à jour depuis l'état clinique courant
            void Update(const NkClinicalState& clinical, nk_float32 dt);

            // Forcer un état directement (tests, cas cliniques scriptés)
            void ForceState(EmotionState state, nk_float32 intensity = 1.f);

            const NkEmotionOutput& GetOutput() const { return mOutput; }
            EmotionState           GetCurrentState() const { return mCurrentState; }

        private:
            void ComputeOutput();
            void ApplyBodyParams(EmotionState state, nk_float32 intensity);
            void ApplyVoiceParams(EmotionState state, nk_float32 intensity);

            EmotionState mCurrentState  = EmotionState::Neutral;
            EmotionState mPreviousState = EmotionState::Neutral;
            nk_float32   mIntensity     = 0.f;
            nk_float32   mBlendTimer    = 0.f;
            nk_float32   mBlendDuration = 0.5f; // transition par défaut 500ms
            bool         mTransitioning = false;

            NkEmotionOutput mOutput;

            // Tableau des transitions enregistrées
            NkVector<NkEmotionTransition> mTransitions;
        };

    } // namespace pv3de
} // namespace nkentseu
