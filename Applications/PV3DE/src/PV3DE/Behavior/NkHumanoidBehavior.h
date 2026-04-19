#pragma once
// =============================================================================
// Humanoid/Behavior/NkHumanoidBehavior.h
// =============================================================================
// Noyau de décision comportemental de l'humanoïde.
//
// LIBRE ARBITRE CONTRAINT — comment ça marche :
//
//   1. Un stimulus arrive (douleur soudaine, question du médecin, bruit)
//   2. Le BehaviorEngine calcule une RÉPONSE BRUTE basée sur l'état clinique
//      et les règles biologiques (douleur intense → cri)
//   3. La personnalité FILTRE cette réponse :
//      - Stoïque (C élevé) : atténue l'expression de 50%
//      - Anxieux (N élevé) : amplifie de 30% et ajoute de l'hypervigilance
//   4. Le rôle CONTRAINT le résultat final :
//      - Infirmière en service : ne peut pas paniquer, expression ≤ 0.3
//   5. La MÉMOIRE ajoute un contexte :
//      - Si le médecin a déjà été brusque → méfiance → regard fuyant
//      - Si douleur chronique → habituation → moins d'expression au fil du temps
//
// BOUCLE DE DÉCISION (par frame) :
//
//   ProcessStimuli()
//     ↓ stimulus pool
//   EvaluateIntentions()   ← que veut faire le personnage ?
//     ↓ intentions classées par priorité
//   FilterByPersonality()  ← comment sa personnalité module la réponse ?
//     ↓ intentions modulées
//   FilterByRole()         ← le rôle autorise-t-il cette réponse ?
//     ↓ intentions autorisées
//   EmitBehaviors()        ← sortie : émotions, AUs, postures, discours
// =============================================================================

#include "NkPersonality.h"
#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
    namespace humanoid {

        // =====================================================================
        // NkStimulus — un stimulus entrant
        // =====================================================================
        enum class NkStimulusType : nk_uint8 {
            Pain = 0,           // douleur physique
            Touch,              // contact physique (exam médical)
            SuddenNoise,        // bruit soudain → réflexe sursaut
            Question,           // question posée par l'interlocuteur
            ProlongedWait,      // attente prolongée → impatience/anxiété
            PositiveReinforce,  // "vous allez bien vous en sortir"
            NegativeNews,       // "c'est grave"
            DoctorApproach,     // approche de quelqu'un
            ProcedureStart,     // début d'un soin (piqûre, auscultation)
            EnvironmentChange,  // changement dans l'environnement
        };

        struct NkStimulus {
            NkStimulusType type;
            nk_float32     intensity = 0.f;  // 0–1
            nk_float32     duration  = 0.f;  // secondes (0 = instantané)
            NkString       content;          // texte de la question, etc.
            bool           isHandled = false;
        };

        // =====================================================================
        // NkBehaviorIntent — ce que le personnage veut faire
        // =====================================================================
        enum class NkIntentType : nk_uint8 {
            ExpressEmotion = 0, // montrer une émotion (face + corps)
            Speak,              // parler / répondre
            MoveBody,           // changer de posture
            ReachForPain,       // main vers zone douloureuse
            AvoidEyeContact,    // regard fuyant
            SeekReassurance,    // chercher du réconfort (regarder le médecin)
            Wince,              // grimace de douleur réflexe (très court)
            Microexpression,    // expression fugace involontaire
            Cry,                // pleurer
            Guard,              // se protéger avec le bras / bras croisés
        };

        struct NkBehaviorIntent {
            NkIntentType type;
            nk_float32   priority  = 0.f; // 0–1 (1 = urgent/involontaire)
            nk_float32   intensity = 0.f; // 0–1
            nk_float32   duration  = 0.f; // secondes
            NkString     data;            // texte si Speak, etc.
            bool         isInvoluntary = false; // réflexe = non filtrable par personnalité
        };

        // =====================================================================
        // NkBehaviorOutput — sortie du système de comportement
        // =====================================================================
        struct NkBehaviorOutput {
            // Expression faciale cible
            struct FaceTarget {
                nk_uint8   auId      = 0;
                nk_float32 intensity = 0.f;
                nk_float32 duration  = 0.f; // 0 = persistant
                bool       isFlash   = false; // micro-expression (<300ms)
            };
            NkVector<FaceTarget> faceTargets;

            // Corps
            nk_uint8   targetPose    = 0;    // NkBodyPose
            nk_float32 poseBlendTime = 0.5f;
            bool       reachForPain  = false;
            NkVec3f    painSite;             // position 3D du site douloureux

            // Regard
            nk_float32 gazeYaw   = 0.f;
            nk_float32 gazePitch = 0.f;
            bool       avoidEyeContact = false;

            // Parole
            NkString   speechText;
            bool       shouldSpeak = false;

            // Pleurs
            bool       crying = false;
            nk_float32 cryIntensity = 0.f;

            // Asymétrie faciale (appliquée par NkFaceController)
            nk_float32 asymmetry = 0.f; // [−1, 1] : gauche vs droite dominant
        };

        // =====================================================================
        // NkHumanoidBehavior — noyau de décision
        // =====================================================================
        class NkHumanoidBehavior {
        public:
            NkHumanoidBehavior() = default;

            void Init(const NkPersonality& personality) noexcept;

            // ── Stimuli ───────────────────────────────────────────────────────
            // Ajouter un stimulus (depuis PatientLayer, SceneScript, etc.)
            void AddStimulus(const NkStimulus& s) noexcept;

            // Raccourcis cliniques
            void OnPain(nk_float32 intensity) noexcept;
            void OnQuestion(const char* text)  noexcept;
            void OnTouch(nk_float32 intensity, bool isMedical = true) noexcept;
            void OnNegativeNews(nk_float32 severity) noexcept;
            void OnReassurance() noexcept;

            // ── Update ────────────────────────────────────────────────────────
            // dt : temps depuis la dernière frame
            // clinicalState : état clinique courant (depuis DiagnosticEngine)
            void Update(nk_float32 dt,
                        nk_float32 painLevel,
                        nk_float32 anxietyLevel,
                        nk_float32 fatigueLevel) noexcept;

            // ── Sortie ────────────────────────────────────────────────────────
            const NkBehaviorOutput& GetOutput() const noexcept { return mOutput; }

            // ── Accès mémoire ─────────────────────────────────────────────────
            // Fait le personnage se souvenir de cet événement
            void RememberEvent(const char* key, nk_float32 valence) noexcept;
            nk_float32 RecallEvent(const char* key) const noexcept;

        private:
            // ── Étapes du pipeline de décision ───────────────────────────────
            void ProcessStimuli() noexcept;
            void EvaluateIntentions(nk_float32 pain,
                                    nk_float32 anxiety,
                                    nk_float32 fatigue) noexcept;
            void FilterByPersonality() noexcept;
            void FilterByRole()        noexcept;
            void EmitBehaviors()       noexcept;

            // ── Générateurs d'intentions ──────────────────────────────────────
            void GeneratePainResponse    (nk_float32 pain)    noexcept;
            void GenerateAnxietyResponse (nk_float32 anxiety) noexcept;
            void GenerateFatigueResponse (nk_float32 fatigue) noexcept;
            void GenerateMicroexpressions(nk_float32 dt)      noexcept;
            void GenerateIdleBehaviors   (nk_float32 dt)      noexcept;

            // ── État interne ──────────────────────────────────────────────────
            NkPersonality                mPersonality;
            NkVector<NkStimulus>         mStimuli;       // file de stimuli
            NkVector<NkBehaviorIntent>   mIntentions;    // intentions générées
            NkBehaviorOutput             mOutput;

            // Timers comportementaux
            nk_float32 mMicroExprTimer   = 0.f;
            nk_float32 mIdleTimer        = 0.f;
            nk_float32 mGazeSaccadeTimer = 0.f;
            nk_float32 mCryTimer         = 0.f;

            // État émotionnel résiduel (pas la FSM, la mémoire courte)
            nk_float32 mResidualPain     = 0.f; // douleur perçue (lissée)
            nk_float32 mResidualAnxiety  = 0.f;

            // Mémoire épisodique simple : key → valence émotionnelle [-1, 1]
            struct MemoryEntry {
                char       key[64] = {};
                nk_float32 valence = 0.f;
                nk_float32 decay   = 0.f; // vitesse d'oubli
            };
            static constexpr nk_uint32 kMaxMemories = 32;
            MemoryEntry mMemories[kMaxMemories] = {};
            nk_uint32   mMemoryCount = 0;
        };

    } // namespace humanoid
} // namespace nkentseu
