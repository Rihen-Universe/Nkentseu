#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "PV3DE/Face/NkFaceController.h"

namespace nkentseu {
    namespace pv3de {

        // =====================================================================
        // NkViseme — 15 visèmes standard Preston Blair
        // =====================================================================
        enum class NkViseme : nk_uint8 {
            Silence = 0, // bouche fermée
            AA,          // "a", "ah"
            AE,          // "at", "bat"
            AO,          // "ought", "ou"
            AX,          // "about", schwa
            EE,          // "eat", "bee"
            EH,          // "egg", "bed"
            IH,          // "it", "bit"
            OH,          // "oak"
            OO,          // "ooze", "blue"
            UH,          // "up", "but"
            WW,          // "we", "owe"
            FF,          // "for", "phone"
            TH,          // "the", "thin"
            NN,          // "nun"
            COUNT
        };

        // =====================================================================
        // NkVisemeFrame — visème horodaté
        // =====================================================================
        struct NkVisemeFrame {
            nk_float32 timeMs;      // temps en ms depuis début de la phrase
            NkViseme   viseme;
            nk_float32 intensity;   // 0–1
        };

        // =====================================================================
        // NkSpeechRequest — phrase à prononcer
        // =====================================================================
        struct NkSpeechRequest {
            NkString text;
            nk_float32 pitchMult   = 1.f;   // multiplicateur de hauteur
            nk_float32 rateMult    = 1.f;   // multiplicateur de vitesse
            nk_float32 breathAmt   = 0.f;   // niveau de souffle 0–1
            nk_float32 tremorAmt   = 0.f;   // tremblement vocal 0–1
        };

        // =====================================================================
        // NkVisemeMapper
        // Convertit un texte en séquence de visèmes horodatée.
        // Phase 5 : implémentation basique basée sur des règles phonétiques
        //           français/anglais. Phase 6+ : intégration TTS externe.
        // =====================================================================
        class NkVisemeMapper {
        public:
            // Génère une séquence de visèmes depuis un texte.
            // durationMs : durée totale estimée de la phrase.
            static NkVector<NkVisemeFrame> Map(const NkString& text,
                                               nk_float32 durationMs);

        private:
            struct PhonemeRule {
                const char* grapheme;   // séquence de lettres
                NkViseme    viseme;
            };
            static const PhonemeRule kRules[];
            static NkViseme CharToViseme(char c);
        };

        // =====================================================================
        // NkLipSyncController
        // Applique une séquence de visèmes aux AU bouche du NkFaceController.
        // AU concernées : AU25 (lips part), AU26 (jaw drop), AU27 (mouth stretch)
        //                + AU18 (lip pucker pour OO/WW)
        //                + AU20 (lip stretch pour EE)
        // =====================================================================
        class NkLipSyncController {
        public:
            NkLipSyncController() = default;

            // Démarre la lecture d'une séquence de visèmes.
            void Play(const NkVector<NkVisemeFrame>& frames, nk_float32 startTimeMs = 0.f);

            // Arrête la lecture.
            void Stop();

            bool IsPlaying() const { return mPlaying; }

            // Appelé chaque frame — avance le curseur et met à jour les AUs.
            // faceCtrl : contrôleur facial sur lequel appliquer les AU.
            void Update(nk_float32 dt, NkFaceController& faceCtrl);

        private:
            void ApplyViseme(NkViseme v, nk_float32 intensity,
                             NkFaceController& fc);

            NkVector<NkVisemeFrame> mFrames;
            nk_float32 mCurrentMs = 0.f;
            nk_uint32  mFrameIdx  = 0;
            bool       mPlaying   = false;

            // Lissage entre visèmes (blend)
            NkViseme   mCurrentViseme  = NkViseme::Silence;
            NkViseme   mPreviousViseme = NkViseme::Silence;
            nk_float32 mBlendT        = 1.f;
        };

        // =====================================================================
        // NkSpeechEngine
        // Orchestrateur : reçoit des requêtes texte, génère les visèmes,
        // pilote NkLipSyncController, modifie les paramètres vocaux.
        // =====================================================================
        class NkSpeechEngine {
        public:
            NkSpeechEngine() = default;
            ~NkSpeechEngine() = default;

            void Init();

            // Soumettre une phrase à prononcer.
            // Si une phrase est en cours, elle est interrompue.
            void Speak(const NkSpeechRequest& request, NkFaceController& faceCtrl);

            // Interrompre immédiatement.
            void Interrupt(NkFaceController& faceCtrl);

            bool IsSpeaking() const { return mLipSync.IsPlaying(); }

            // Réponses prédéfinies indexées par état clinique
            // (utilisées par PatientLayer pour répondre aux questions).
            static NkString GetResponse(const NkString& question,
                                        nk_float32 painLevel,
                                        nk_float32 breathDifficulty);

            // Mise à jour frame
            void Update(nk_float32 dt, NkFaceController& faceCtrl);

        private:
            NkLipSyncController mLipSync;

            // Paramètres vocaux courants (transmis au moteur TTS externe)
            nk_float32 mCurrentPitch  = 1.f;
            nk_float32 mCurrentRate   = 1.f;
            nk_float32 mCurrentBreath = 0.f;
        };

    } // namespace pv3de
} // namespace nkentseu
