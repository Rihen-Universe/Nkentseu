#pragma once
// =============================================================================
// Nkentseu/Facial/NkFacialRig.h
// =============================================================================
// Système de rig facial complet basé sur le FACS (Facial Action Coding System).
//
// FACS est le standard anatomique développé par Ekman & Friesen (1978) qui
// décompose tout mouvement facial en Action Units (AU) indépendantes.
// C'est la base de MetaHuman (Unreal), Face Capture (Apple), Ziva Dynamics.
//
// ARCHITECTURE :
//   NkFACS         — enum des 44 Action Units + visèmes (phonèmes)
//   NkFacialRig    — composant ECS pilotant les AU → blend shapes
//   NkExpression   — combinaison nommée d'AU (joie, tristesse, surprise...)
//   NkFacialSystem — système ECS qui applique le rig au mesh chaque frame
//   NkLipSync      — synchronisation lèvres → audio (phonème → visème)
//   NkProcFace     — génération procédurale de visages paramétriques
//
// PIPELINE :
//   Sources (IA, audio, input, animation) → AU weights
//   AU weights → NkFacialRig::ApplyToMesh() → NkBlendShape weights
//   NkBlendShape weights → NkRender3D skinning → GPU
//
// PHYSIQUE PROCÉDURALE :
//   NkWrinkleMap — génère des maps de rides selon la courbure du mesh
//   NkFleshDeform — déformation de la chair sous-cutanée (muscles, graisse)
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKECS/World/NkWorld.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "Nkentseu/ECS/Components/Animation/NkAnimation.h"
#include "Nkentseu/ECS/Components/Rendering/NkRenderComponents.h"

namespace nkentseu {
    using namespace math;
    using namespace ecs;

    // =========================================================================
    // NkFACS — 44 Action Units + Visèmes (phonèmes faciaux)
    // =========================================================================
    /**
     * @enum NkFACS
     * @brief Action Units du Facial Action Coding System.
     *
     * Les AU sont des contractions musculaires atomiques. Toute expression
     * humaine peut être décomposée en combinaisons d'AU.
     *
     * Exemples d'expressions :
     *   Joie       : AU06 + AU12 (joues levées + coin des lèvres)
     *   Tristesse  : AU01 + AU04 + AU15 + AU17 (sourcils + lèvres)
     *   Peur       : AU01 + AU02 + AU04 + AU05 + AU07 + AU20 + AU26
     *   Dégoût     : AU09 + AU15 + AU16 (nez plissé + lèvre inférieure)
     *   Surprise   : AU01 + AU02 + AU05 + AU26 (sourcils + yeux + bouche)
     *   Colère     : AU04 + AU05 + AU07 + AU23 + AU24 + AU25
     */
    enum class NkFACS : uint8 {
        // ── Haut du visage (sourcils, front) ──────────────────────────────
        AU01_InnerBrowRaise  = 0,   ///< Partie interne sourcil levée (tristesse, peur)
        AU02_OuterBrowRaise,        ///< Partie externe sourcil levée (surprise)
        AU04_BrowLowerer,           ///< Sourcils abaissés/froncés (colère, concentration)

        // ── Yeux et paupières ─────────────────────────────────────────────
        AU05_UpperLidRaiser,        ///< Paupière supérieure levée (surprise, peur)
        AU06_CheekRaiser,           ///< Joues levées (joie Duchenne, sourire sincère)
        AU07_LidTightener,          ///< Paupière tendue (colère, dégoût)
        AU43_EyeClose,              ///< Fermeture des yeux (doux)
        AU45_Blink,                 ///< Clignement (rapide)
        AU46_WinkL,                 ///< Clin d'œil gauche
        AU46_WinkR,                 ///< Clin d'œil droit

        // ── Nez ───────────────────────────────────────────────────────────
        AU09_NoseWrinkler,          ///< Nez plissé (dégoût)
        AU38_NostrilDilator,        ///< Narines dilatées (respiration forte, colère)
        AU39_NostrilCompressor,     ///< Narines compressées

        // ── Joues ─────────────────────────────────────────────────────────
        AU13_CheekPuffer,           ///< Joues gonflées (retenir souffle)
        AU14_DimplerL,              ///< Fossette gauche
        AU14_DimplerR,              ///< Fossette droite

        // ── Bouche et lèvres ──────────────────────────────────────────────
        AU10_UpperLipRaiser,        ///< Lèvre supérieure levée (dégoût)
        AU11_NasalabialDeepener,    ///< Sillon nasolabial creusé
        AU12_LipCornerPullerL,      ///< Coin de la bouche gauche tiré (sourire)
        AU12_LipCornerPullerR,      ///< Coin de la bouche droit tiré
        AU15_LipCornerDepressorL,   ///< Coin gauche abaissé (tristesse)
        AU15_LipCornerDepressorR,   ///< Coin droit abaissé
        AU16_LowerLipDepressor,     ///< Lèvre inférieure abaissée
        AU17_ChinRaiser,            ///< Menton relevé
        AU18_LipPuckerer,           ///< Lèvres en avant (bisou)
        AU20_LipStretcher,          ///< Lèvres étirées (peur)
        AU22_LipFunneler,           ///< Lèvres en entonnoir
        AU23_LipTightener,          ///< Lèvres serrées (colère refoulée)
        AU24_LipPressor,            ///< Lèvres pressées ensemble
        AU25_LipsParting,           ///< Lèvres écartées (ouverture de la bouche)
        AU26_JawDrop,               ///< Mâchoire tombante (surprise, bouche ouverte)
        AU27_MouthStretch,          ///< Bouche très ouverte (cri)
        AU28_LipSucker,             ///< Lèvre rentrée

        // ── Visèmes (phonèmes → formes de bouche pour lip sync) ───────────
        VIS_Rest,                   ///< Bouche au repos
        VIS_Aa,                     ///< /a/ — "father"
        VIS_Ee,                     ///< /e/ ou /i/ — "see"
        VIS_Oh,                     ///< /o/ — "over"
        VIS_Oo,                     ///< /u/ — "you"
        VIS_Mm,                     ///< /m/, /b/, /p/ — lèvres fermées
        VIS_Ff,                     ///< /f/, /v/ — lèvre inférieure sur dents
        VIS_Th,                     ///< /θ/ — "think" (langue entre les dents)
        VIS_Ss,                     ///< /s/, /z/ — dents serrées
        VIS_Rr,                     ///< /r/ — lèvres arrondies
        VIS_Ww,                     ///< /w/ — lèvres proéminentes
        VIS_Ch,                     ///< /ʃ/ — "she"

        // ── AU procédurales (calculées dynamiquement) ─────────────────────
        AU_PROC_EyeDirection,       ///< Direction du regard (calculée depuis eye tracking)
        AU_PROC_HeadRotation,       ///< Rotation de la tête (stretch)
        AU_PROC_BreathCycle,        ///< Cycle respiratoire (nez/bouche)
        AU_PROC_MicroSaccade,       ///< Micro-mouvements oculaires involontaires

        FACS_COUNT
    };

    static constexpr uint32 kFACSCount = static_cast<uint32>(NkFACS::FACS_COUNT);

    // =========================================================================
    // NkAUMapping — Association AU → blend shape
    // =========================================================================
    struct NkAUMapping {
        NkFACS  au            = NkFACS::AU12_LipCornerPullerL;
        uint32  blendShapeIdx = 0;     ///< Index dans NkSkeletalMesh.blendShapeNames
        float32 maxWeight     = 1.f;   ///< Poids max pour cette AU sur ce shape
        float32 offset        = 0.f;   ///< Décalage (pour shapes négatifs)
        float32 curve         = 1.f;   ///< Puissance de la courbe (1=linéaire, 2=quadratique)

        // Combinaison avec d'autres AU (ex: AU06 renforce AU12 dans le sourire)
        NkFACS  modifierAU    = NkFACS::FACS_COUNT;  ///< AU modulatrice (FACS_COUNT = aucune)
        float32 modifierGain  = 0.f;
    };

    // =========================================================================
    // NkExpression — Expression nommée (combinaison d'AU)
    // =========================================================================
    struct NkExpression {
        static constexpr uint32 kMaxName = 48u;
        char    name[kMaxName]  = {};
        float32 weights[kFACSCount] = {};  ///< Poids de chaque AU pour cette expression

        NkExpression() noexcept = default;
        explicit NkExpression(const char* n) noexcept {
            std::strncpy(name, n, kMaxName - 1);
        }

        // Expressions prédéfinies (built-in)
        static NkExpression Joy()     noexcept;
        static NkExpression Sadness() noexcept;
        static NkExpression Anger()   noexcept;
        static NkExpression Fear()    noexcept;
        static NkExpression Disgust() noexcept;
        static NkExpression Surprise()noexcept;
        static NkExpression Contempt()noexcept;
        static NkExpression Neutral() noexcept;
    };

    // =========================================================================
    // NkFacialRig — Composant ECS de rig facial
    // =========================================================================
    /**
     * @struct NkFacialRig
     * @brief Composant ECS qui pilote le rig facial d'un personnage via le FACS.
     *
     * 🔹 Pipeline d'application :
     *   NkFacialSystem::Execute() chaque frame :
     *   1. Lit les AU weights (depuis drivers, expressions blended, animations)
     *   2. Applique les AU → blend shape weights via NkAUMapping
     *   3. Écrit les blend shape weights dans NkSkeletalMesh
     *   4. NkRenderSystem applique les blend shapes au vertex buffer GPU
     *
     * 🔹 Sources de pilotage (non exclusives) :
     *   • Direct : SetAU(NkFACS::AU12, 0.8f)
     *   • Expression : BlendToExpression("Joy", 0.6f)
     *   • Animation : AU track dans NkAnimationClip
     *   • IA/ML : inference d'un modèle de visage
     *   • Audio : NkLipSync (phonème → visème → AU)
     *   • Physics : vitesse de la tête → AU_PROC_HeadRotation
     */
    struct NkFacialRig {
        // ── Poids des Action Units (source de vérité) ─────────────────────
        float32 auWeights[kFACSCount] = {};  ///< [0..1] pour chaque AU

        // ── Mappings AU → blend shapes ────────────────────────────────────
        static constexpr uint32 kMaxMappings = 256u;
        NkAUMapping mappings[kMaxMappings]   = {};
        uint32      mappingCount             = 0;

        // ── Expressions prédéfinies ────────────────────────────────────────
        static constexpr uint32 kMaxExpressions = 64u;
        NkExpression expressions[kMaxExpressions] = {};
        uint32       expressionCount              = 0;

        // ── Entité mesh cible ─────────────────────────────────────────────
        NkEntityId   meshEntity = NkEntityId::Invalid();
        uint32       meshComponentOffset = 0;  ///< Peut référencer NkSkinnedMeshComponent

        // ── Paramètres globaux ────────────────────────────────────────────
        float32 globalInfluence  = 1.f;   ///< Intensité globale [0..1]
        float32 symmetryBalance  = 0.f;   ///< [-1=gauche seul, 0=symétrique, 1=droit seul]

        // ── API ───────────────────────────────────────────────────────────

        /**
         * @brief Définit le poids d'une Action Unit.
         */
        void SetAU(NkFACS au, float32 weight) noexcept {
            const auto idx = static_cast<uint32>(au);
            if (idx < kFACSCount) auWeights[idx] = NkClamp(weight, 0.f, 1.f);
        }

        [[nodiscard]] float32 GetAU(NkFACS au) const noexcept {
            const auto idx = static_cast<uint32>(au);
            return (idx < kFACSCount) ? auWeights[idx] : 0.f;
        }

        /**
         * @brief Blend vers une expression nommée.
         * @param name   Nom de l'expression (voir NkExpression).
         * @param weight Poids du blend [0..1].
         * @note Les AU existantes sont additionnées (pas remplacées).
         *       Pour une expression exclusive, appeler ResetAUs() avant.
         */
        void BlendToExpression(const char* name, float32 weight) noexcept;

        /**
         * @brief Blend entre deux expressions avec un facteur t [0..1].
         */
        void BlendExpressions(const char* fromExpr,
                               const char* toExpr,
                               float32 t) noexcept;

        /**
         * @brief Remet à zéro toutes les AU.
         */
        void ResetAUs() noexcept {
            for (uint32 i = 0; i < kFACSCount; ++i) auWeights[i] = 0.f;
        }

        /**
         * @brief Applique les AU aux blend shapes du mesh cible.
         * @note Appelé automatiquement par NkFacialSystem. Peut être appelé
         *       manuellement pour un update ponctuel.
         */
        void ApplyToMesh(NkSkinnedMeshComponent& mesh) const noexcept;

        /**
         * @brief Retarget depuis un autre rig (correspondance par noms d'AU).
         * Permet de transférer les AU d'un rig source vers ce rig même si
         * les mappings de blend shapes sont différents.
         */
        void RetargetFrom(const NkFacialRig& source) noexcept {
            for (uint32 i = 0; i < kFACSCount; ++i)
                auWeights[i] = source.auWeights[i];
        }

        // ── Enregistrement d'une expression ──────────────────────────────
        /**
         * @brief Enregistre une expression nommée avec les AU courantes.
         */
        bool SaveExpression(const char* name) noexcept;

        /**
         * @brief Enregistre une expression prédéfinie.
         */
        bool AddExpression(const NkExpression& expr) noexcept {
            if (expressionCount >= kMaxExpressions) return false;
            expressions[expressionCount++] = expr;
            return true;
        }

        [[nodiscard]] const NkExpression* FindExpression(const char* name) const noexcept;

        // ── Mappings ──────────────────────────────────────────────────────
        bool AddMapping(const NkAUMapping& mapping) noexcept {
            if (mappingCount >= kMaxMappings) return false;
            mappings[mappingCount++] = mapping;
            return true;
        }

        /**
         * @brief Configure automatiquement les mappings depuis les noms
         *        des blend shapes du mesh (convention de nommage FACS).
         *
         * Convention : les blend shapes nommés "AU01", "AU06_L", "VIS_Ee"...
         * sont automatiquement mappés aux AU correspondantes.
         */
        void AutoMapFromBlendShapeNames(const NkSkinnedMeshComponent& mesh) noexcept;
    };
    NK_COMPONENT(NkFacialRig)

    // =========================================================================
    // NkMicroExpressionDriver — micro-expressions procédurales automatiques
    // =========================================================================
    /**
     * @struct NkMicroExpressionDriver
     * @brief Génère des micro-expressions involontaires procédurales.
     *
     * Les micro-expressions durent 1/25 à 1/5 de seconde et trahissent
     * les émotions réelles même quand le personnage les contrôle.
     * Utilisé pour les PNJ, les cutscenes, les personnages IA.
     */
    struct NkMicroExpressionDriver {
        // ── Configuration ─────────────────────────────────────────────────
        float32 frequency       = 0.1f;   ///< Fréquence d'apparition (par seconde)
        float32 intensity       = 0.3f;   ///< Intensité max des micro-expressions
        float32 minDuration     = 0.04f;  ///< Durée min (1/25 sec)
        float32 maxDuration     = 0.2f;   ///< Durée max (1/5 sec)
        bool    onlyNegative    = false;  ///< Uniquement émotions négatives (trahison)

        // ── Émotion sous-jacente (pilote les micro-expressions) ───────────
        float32 baseJoy         = 0.f;   ///< Joie sous-jacente
        float32 baseFear        = 0.f;   ///< Peur sous-jacente
        float32 baseAnger       = 0.f;   ///< Colère sous-jacente
        float32 baseDisgust     = 0.f;   ///< Dégoût sous-jacent
        float32 baseSadness     = 0.f;   ///< Tristesse sous-jacente

        // ── État interne (géré par NkFacialSystem) ────────────────────────
        float32 nextTriggerTime = 0.f;
        float32 currentTimer    = 0.f;
        float32 currentDuration = 0.f;
        NkFACS  currentAUs[4]  = {};
        float32 currentWeights[4] = {};
        uint32  currentAUCount  = 0;
        bool    active          = false;

        bool enabled = true;

        /**
         * @brief Mis à jour par NkFacialSystem chaque frame.
         * @return true si une micro-expression a été déclenchée.
         */
        bool Update(float32 dt, NkFacialRig& rig) noexcept;
    };
    NK_COMPONENT(NkMicroExpressionDriver)

    // =========================================================================
    // NkLipSync — Synchronisation lèvres ↔ audio
    // =========================================================================
    /**
     * @struct NkLipSync
     * @brief Mappe les phonèmes audio vers des visèmes faciaux.
     *
     * Supporte deux modes :
     *   • Prébaked : liste de phonèmes timestampés (depuis analyse audio)
     *   • Temps réel : analyse fréquentielle live → phonème → visème
     */
    struct NkLipSync {
        struct PhonemeKey {
            float32 time;       ///< Timestamp en secondes
            NkFACS  viseme;     ///< Visème correspondant
            float32 weight;     ///< Intensité [0..1]
        };

        static constexpr uint32 kMaxKeys = 4096u;
        PhonemeKey keys[kMaxKeys] = {};
        uint32     keyCount       = 0;
        float32    currentTime    = 0.f;
        bool       playing        = false;
        bool       loop           = false;

        /**
         * @brief Charge les phonèmes depuis un fichier .phoneme (JSON).
         * Compatible avec outputs de odin-voice, rhubarb lip sync.
         */
        bool LoadFromFile(const char* path) noexcept;

        /**
         * @brief Mise à jour chaque frame.
         * @param dt Delta-time.
         * @param rig Le NkFacialRig à animer.
         */
        void Update(float32 dt, NkFacialRig& rig) noexcept;

        void Play()    noexcept { playing = true; }
        void Stop()    noexcept { playing = false; currentTime = 0.f; }
        void Pause()   noexcept { playing = false; }
        void Resume()  noexcept { playing = true; }
    };
    NK_COMPONENT(NkLipSync)

    // =========================================================================
    // NkWrinkleMap — Maps de rides dynamiques procédurales
    // =========================================================================
    /**
     * @struct NkWrinkleMap
     * @brief Génère dynamiquement des maps de rides selon la déformation du mesh.
     *
     * 3 types de maps (textures GPU R8 mises à jour via compute shader) :
     *   • Compression  : peau compressée (rides de plissement)
     *   • Étirement    : peau étirée (rides de tension)
     *   • Plis         : courbure forte (rides profondes)
     *
     * Ces maps sont échantillonnées dans le shader de peau (NkSkinMaterial)
     * pour ajouter/accentuer le normal map de rides dynamiquement.
     */
    struct NkWrinkleMap {
        nk_uint64 compressionMapHandle = 0;  ///< Texture GPU R8 (rides de compression)
        nk_uint64 stretchMapHandle     = 0;  ///< Texture GPU R8 (rides d'étirement)
        nk_uint64 foldMapHandle        = 0;  ///< Texture GPU R8 (plis profonds)

        nk_uint32 resolution           = 256;  ///< Résolution des maps UV
        float32   compressionThreshold = 0.02f;
        float32   stretchThreshold     = 0.05f;
        float32   intensity            = 1.f;
        bool      enabled              = true;
        bool      initialized          = false;

        /**
         * @brief Met à jour les wrinkle maps selon le mesh déformé courant.
         * Exécuté en compute shader — coût < 0.3ms sur GPU moderne.
         * @param cmd     Command buffer pour les dispatches compute.
         * @param restPosBuffer  SSBO des positions au repos (bind pose).
         * @param curPosBuffer   SSBO des positions courantes (après skinning).
         * @param uvBuffer       SSBO des coordonnées UV.
         * @param vertexCount    Nombre de vertices.
         */
        void Update(NkICommandBuffer* cmd,
                    nk_uint64 restPosBuffer,
                    nk_uint64 curPosBuffer,
                    nk_uint64 uvBuffer,
                    nk_uint32 vertexCount) noexcept;
    };
    NK_COMPONENT(NkWrinkleMap)

    // =========================================================================
    // NkFacialSystem — Système ECS d'application du rig facial
    // =========================================================================
    class NkFacialSystem final : public NkSystem {
        public:
            [[nodiscard]] NkSystemDesc Describe() const override {
                return NkSystemDesc{}
                    .Reads<NkTransform>()
                    .Writes<NkFacialRig>()
                    .Writes<NkMicroExpressionDriver>()
                    .Writes<NkLipSync>()
                    .Writes<NkSkinnedMeshComponent>()
                    .InGroup(NkSystemGroup::PostUpdate)
                    .WithPriority(800.f)  // Après contraintes, avant rendu
                    .Named("NkFacialSystem");
            }

            void Execute(NkWorld& world, float32 dt) noexcept override;

        private:
            void ProcessFacialRig(NkWorld& world,
                                   NkEntityId id,
                                   NkFacialRig& rig,
                                   float32 dt) noexcept;
    };

    // =========================================================================
    // NkProcFace — Génération procédurale de visages
    // =========================================================================
    /**
     * @struct NkProcFaceParams
     * @brief Paramètres pour la génération procédurale d'un visage.
     *
     * Chaque paramètre [0..1] est une interpolation entre deux extrêmes.
     * Le générateur produit un NkEditableMesh + NkFacialRig configuré.
     */
    struct NkProcFaceParams {
        // ── Morphologie générale ──────────────────────────────────────────
        float32 age          = 0.3f;   ///< 0=enfant, 0.3=adulte, 1=vieux
        float32 weight       = 0.5f;   ///< 0=maigre, 0.5=normal, 1=gros
        float32 gender       = 0.5f;   ///< 0=masculin, 0.5=neutre, 1=féminin
        float32 ethnicity[4] = {1,0,0,0}; ///< Blend entre archétypes [EUR/AFR/ASI/OTH]

        // ── Tête ─────────────────────────────────────────────────────────
        float32 headWidth    = 0.5f;
        float32 headHeight   = 0.5f;
        float32 headDepth    = 0.5f;
        float32 jawWidth     = 0.5f;
        float32 jawSharpness = 0.5f;
        float32 cheekBones   = 0.5f;

        // ── Front ─────────────────────────────────────────────────────────
        float32 foreheadHeight = 0.5f;
        float32 foreheadSlope  = 0.5f;

        // ── Yeux ──────────────────────────────────────────────────────────
        float32 eyeSize       = 0.5f;
        float32 eyeSeparation = 0.5f;
        float32 eyeSlant      = 0.5f;  // 0=négatif, 0.5=neutre, 1=positif
        float32 eyeDepth      = 0.5f;  // 0=saillant, 1=profond

        // ── Nez ───────────────────────────────────────────────────────────
        float32 noseWidth     = 0.5f;
        float32 noseLength    = 0.5f;
        float32 noseTip       = 0.5f;  // 0=retroussé, 1=crochu
        float32 noseBridge    = 0.5f;

        // ── Bouche ────────────────────────────────────────────────────────
        float32 mouthWidth    = 0.5f;
        float32 lipFullness   = 0.5f;
        float32 cupidsBow     = 0.5f;
        float32 mouthHeight   = 0.5f;  // position verticale

        // ── Oreilles ──────────────────────────────────────────────────────
        float32 earSize       = 0.5f;
        float32 earProtrusion = 0.3f;

        // ── Seed aléatoire (pour variations procédurales reproductibles) ──
        uint32  seed = 0;
    };

} // namespace nkentseu
