#pragma once
// =============================================================================
// Humanoid/Core/NkPersonality.h
// =============================================================================
// Définit la PERSONNALITÉ et le RÔLE d'un humanoïde.
//
// PRINCIPE DU LIBRE ARBITRE CONTRAINT :
//   Un humanoïde a des traits fixes (Big Five) + un rôle qui définit
//   ses limites de comportement. Dans ces limites, il décide librement
//   comment réagir à chaque stimulus.
//
//   Exemple :
//     Rôle = Patient médical
//       → Limites : répond aux questions médicales, exprime la douleur,
//                   peut refuser de se déshabiller (pudeur = trait pudique)
//       → Libre arbitre : peut mentir sur l'intensité de sa douleur si trait
//                         stoïque, peut pleurer ou pas selon trait émotivité
//
//     Rôle = Infirmière
//       → Limites : professionnelle, calme, ne peut pas paniquer en service
//       → Libre arbitre : peut avoir une opinion personnelle sur le médecin,
//                         peut être fatiguée (fin de garde = contextuel)
//
// MODÈLE Big Five (OCEAN) :
//   O = Ouverture à l'expérience  [0..1]
//   C = Conscienciosité            [0..1]
//   E = Extraversion               [0..1]
//   A = Agréabilité                [0..1]
//   N = Neuroticisme               [0..1]
//
// Ces traits influencent :
//   - La vitesse de réponse émotionnelle (N élevé → réactivité rapide)
//   - L'amplitude des expressions faciales (E élevé → plus expressif)
//   - La tendance à parler (E → verbeux, I → bref)
//   - La résistance à la douleur (C → stoïque)
//   - La confiance envers le médecin (A → coopératif)
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace humanoid {

        // =====================================================================
        // NkRoleType — rôles disponibles
        // =====================================================================
        enum class NkRoleType : nk_uint8 {
            Patient = 0,       // patient dans un contexte médical
            Nurse,             // infirmière / infirmier
            Doctor,            // médecin
            FamilyMember,      // proche du patient
            NPC_Background,    // personnage d'ambiance (couloir, salle d'attente)
            Custom,            // rôle défini par script .nkrole
        };

        // =====================================================================
        // NkRoleConstraints — limites comportementales du rôle
        // =====================================================================
        struct NkRoleConstraints {
            // Émotions autorisées à être exprimées publiquement
            bool canShowPain        = true;
            bool canShowFear        = true;
            bool canShowAnger       = true;
            bool canCry             = true;
            bool canPanic           = true;
            bool canRefuseToAnswer  = true;

            // Limites de l'expression faciale (facteur multiplicatif)
            nk_float32 expressionScale = 1.0f;  // 0.3 = très retenu, 1.5 = très expressif

            // Cohérence du discours
            bool mustAnswerQuestions  = false;  // infirmière en service = oui
            bool canLieAboutPain      = true;   // patient stoïque = oui
            bool canInitiateConversation = true;

            // Comportements physiques autorisés
            bool canReachForPainSite  = true;   // main vers zone douloureuse
            bool canGuardWithArm      = true;    // se protéger avec le bras
            bool canRock              = true;    // se balancer (détresse)

            NkString roleDescription; // texte libre décrivant le rôle
        };

        // =====================================================================
        // NkPersonalityTraits — Big Five + traits supplémentaires
        // =====================================================================
        struct NkPersonalityTraits {
            // Big Five OCEAN [0..1]
            nk_float32 openness         = 0.5f;  // curiosité, créativité
            nk_float32 conscientiousness= 0.5f;  // discipline, stoïcisme
            nk_float32 extraversion     = 0.5f;  // sociabilité, expressivité
            nk_float32 agreeableness    = 0.5f;  // coopération, confiance
            nk_float32 neuroticism      = 0.5f;  // réactivité émotionnelle

            // Traits supplémentaires médicalement pertinents
            nk_float32 painTolerance    = 0.5f;  // résistance à la douleur
            nk_float32 anxietyBaseline  = 0.1f;  // anxiété de fond (trait vs état)
            nk_float32 trustInAuthority = 0.6f;  // confiance envers médecins
            nk_float32 privacy          = 0.5f;  // pudeur / intimité

            // ── Dérivés calculés ──────────────────────────────────────────────
            // Amplitude maximale des expressions faciales
            nk_float32 ExpressionAmplitude() const noexcept {
                return 0.4f + extraversion * 0.4f + (1.f - conscientiousness) * 0.2f;
            }

            // Vitesse de transition émotionnelle (personnes névrotiques = rapides)
            nk_float32 EmotionalReactivity() const noexcept {
                return 0.3f + neuroticism * 0.7f;
            }

            // Probabilité de pleurer (douleur sévère)
            nk_float32 CryProbability() const noexcept {
                return neuroticism * 0.5f + (1.f - conscientiousness) * 0.3f + 0.1f;
            }

            // Tendance à minimiser la douleur
            nk_float32 PainUnderstatement() const noexcept {
                return conscientiousness * 0.4f + (1.f - neuroticism) * 0.3f;
            }

            // Verbosité des réponses
            nk_float32 Verbosity() const noexcept {
                return 0.2f + extraversion * 0.6f + agreeableness * 0.2f;
            }

            // Asymétrie faciale naturelle (gauche/droite dominance) [-1..1]
            nk_float32 facialAsymmetry = 0.f; // défini à la création du personnage
        };

        // =====================================================================
        // NkLifeContext — contexte de vie du personnage
        // =====================================================================
        struct NkLifeContext {
            NkString name;
            nk_uint32 ageYears   = 40;
            NkString  gender;           // M/F/O
            NkString  profession;
            NkString  familySituation;  // "marié, 2 enfants"

            // Antécédents médicaux pertinents pour PV3DE
            bool hasChronicPain     = false;
            bool hasPainPhobia      = false; // hyperalgésie, catastrophisme
            bool hasMedicalAnxiety  = false;
            bool isOnPainkillers    = false;  // réduit l'expression de la douleur

            // Contexte situationnel courant
            NkString currentSituation; // "aux urgences depuis 2h, seul"
            nk_float32 baselineFatigue = 0.f; // fatigue de départ (0=reposé)
            nk_float32 baselineAnxiety = 0.f; // anxiété situationnelle de départ

            // Relation avec l'interlocuteur courant
            nk_float32 trustInCurrentDoctor = 0.5f; // 0=méfiant, 1=confiant
        };

        // =====================================================================
        // NkPersonality — package complet du personnage
        // =====================================================================
        struct NkPersonality {
            NkRoleType           role;
            NkRoleConstraints    constraints;
            NkPersonalityTraits  traits;
            NkLifeContext        life;

            // ── Presets ───────────────────────────────────────────────────────

            // Patient stoïque (homme âgé, peu expressif)
            static NkPersonality StoicPatient() noexcept {
                NkPersonality p;
                p.role = NkRoleType::Patient;
                p.traits.conscientiousness = 0.85f;
                p.traits.neuroticism       = 0.20f;
                p.traits.extraversion      = 0.25f;
                p.traits.painTolerance     = 0.80f;
                p.constraints.expressionScale = 0.45f;
                p.constraints.canLieAboutPain = true;
                p.life.hasChronicPain = true;
                p.life.ageYears = 65;
                return p;
            }

            // Patient anxieux (jeune adulte, hyperexpressif)
            static NkPersonality AnxiousPatient() noexcept {
                NkPersonality p;
                p.role = NkRoleType::Patient;
                p.traits.neuroticism       = 0.85f;
                p.traits.extraversion      = 0.70f;
                p.traits.painTolerance     = 0.20f;
                p.traits.anxietyBaseline   = 0.40f;
                p.constraints.expressionScale = 1.4f;
                p.life.hasMedicalAnxiety = true;
                p.life.ageYears = 28;
                return p;
            }

            // Patient coopératif standard
            static NkPersonality CooperativePatient() noexcept {
                NkPersonality p;
                p.role = NkRoleType::Patient;
                p.traits.agreeableness     = 0.75f;
                p.traits.trustInAuthority  = 0.80f;
                p.constraints.mustAnswerQuestions = true;
                return p;
            }
        };

        // =====================================================================
        // Sérialisation / désérialisation depuis .nkpersonality JSON
        // =====================================================================
        bool SavePersonality(const char* path, const NkPersonality& p) noexcept;
        bool LoadPersonality(const char* path, NkPersonality& out) noexcept;

    } // namespace humanoid
} // namespace nkentseu
