#pragma once
// =============================================================================
// Nkentseu/Facial/NkSkinMaterial.h
// =============================================================================
// Matériaux de rendu pour les surfaces organiques (peau, yeux, dents).
// Extension du système de matériaux NKRenderer — ne modifie pas NKRenderer.
//
// COMPOSANTS :
//   NkSkinMaterial   — matériau peau avec SSS, pores, rides dynamiques
//   NkEyeRig         — rig d'œil complet (cornée, iris, pupille, cils)
//   NkTeethMaterial  — matériau dents avec humidité
//   NkSkinRegion     — zones de peau avec paramètres différents (mains ≠ visage)
//
// SUBSURFACE SCATTERING (SSS) :
//   Implémentation pre-integrated SSS (type Penner/Jimenez).
//   3 couches de diffusion : épiderme (rouge), derme (vert), graisse (bleu).
//   Compatible avec le pipeline deferred de NKRenderer via extensions.
//
// WRINKLE MAPS (NkWrinkleMap — défini dans NkFacialRig.h) :
//   3 textures R8 mises à jour chaque frame par compute shader.
//   Échantillonnées dans NkSkinMaterial pour moduler le normal map dynamique.
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/String/NkString.h"
#include "NKRenderer/src/Core/NkRendererTypes.h"
#include "NkFacialRig.h"

namespace nkentseu {
    using namespace math;
    using namespace renderer;

    // =========================================================================
    // NkSkinMaterial — matériau peau réaliste
    // =========================================================================
    /**
     * @struct NkSkinMaterial
     * @brief Matériau de rendu pour la peau humaine et animale.
     *
     * 🔹 Subsurface Scattering (SSS) :
     *   La lumière pénètre dans la peau, diffuse dans les couches internes
     *   et ressort avec une teinte rougeâtre. Essentiel pour éviter le
     *   "plastic look" des personnages 3D.
     *
     * 🔹 Couches de diffusion (pre-integrated SSS) :
     *   • Épiderme    (rouge) : diffusion faible, couleur rouge-brun
     *   • Derme       (vert)  : diffusion moyenne, couleur chair
     *   • Graisse     (bleu)  : diffusion forte, couleur jaune pâle
     *
     * 🔹 Textures attendues :
     *   albedoTex      — couleur de base (sRGB)
     *   normalTex      — normales tangentes (TBN)
     *   roughnessTex   — rugosité (R), humidité/specular (G)
     *   sssColorTex    — couleur SSS locale (zones de sous-diffusion)
     *   wrinkleMaskTex — masque de zones ridables (UV-space)
     */
    struct NkSkinMaterial {
        // ── Textures ──────────────────────────────────────────────────────
        NkTextureHandle albedoTex;           ///< Albédo base (RGB)
        NkTextureHandle normalTex;           ///< Normal map tangent-space
        NkTextureHandle roughnessTex;        ///< R=roughness, G=wetness, B=cavity
        NkTextureHandle sssColorTex;         ///< Couleur SSS locale (facultatif)
        NkTextureHandle wrinkleMaskTex;      ///< Masque ridable (1=peut rider)
        NkTextureHandle detailNormalTex;     ///< Normal map micro-détails (pores)
        NkTextureHandle translucencyTex;     ///< Translucidité locale (oreilles, doigts)

        // ── SSS (Subsurface Scattering) ───────────────────────────────────
        bool    sssEnabled        = true;
        float32 sssRadius         = 1.0f;    ///< Rayon de diffusion (mm)
        NkVec3f sssColor          = {0.8f, 0.3f, 0.2f};  ///< Teinte SSS (rouge-chair)
        float32 sssStrength       = 0.5f;    ///< Force globale [0..1]

        // Poids des 3 couches (doivent sommer à 1)
        float32 sssLayerEpidermis  = 0.3f;  ///< Couche épiderme (diffusion rouge)
        float32 sssLayerDermis     = 0.5f;  ///< Couche derme (diffusion chair)
        float32 sssLayerHypodermus = 0.2f;  ///< Couche graisse (diffusion bleue)

        // ── Surface ───────────────────────────────────────────────────────
        float32 roughness         = 0.5f;
        float32 specularIntensity = 0.04f;   ///< Fresnel F0 (peau ≈ 0.04)
        float32 wetness           = 0.0f;    ///< 0=sec, 1=mouillé (après pluie)
        float32 oiliness          = 0.0f;    ///< Peau grasse (visage brillant)

        // ── Micro-détails ─────────────────────────────────────────────────
        float32 poreScale         = 2.0f;    ///< Échelle UV des pores
        float32 poreStrength      = 0.3f;    ///< Force des pores [0..1]
        float32 detailNormalScale = 1.0f;    ///< Échelle UV micro-normal

        // ── Translucidité ─────────────────────────────────────────────────
        bool    translucencyEnabled = true;
        float32 translucencyStrength = 0.3f; ///< Forte sur oreilles/doigts
        NkVec3f translucencyColor  = {0.9f, 0.4f, 0.3f};  ///< Couleur rétro-lumière

        // ── Rides dynamiques ──────────────────────────────────────────────
        NkEntityId wrinkleMapEntity = NkEntityId::Invalid(); ///< Entité avec NkWrinkleMap
        float32    wrinkleStrength  = 1.0f;

        // ── Veines / Imperfections ────────────────────────────────────────
        float32 veinStrength       = 0.0f;    ///< Veines visibles
        NkVec3f veinColor          = {0.4f, 0.2f, 0.5f};
        float32 freckleStrength    = 0.0f;    ///< Taches de rousseur
    };
    NK_COMPONENT(NkSkinMaterial)

    // =========================================================================
    // NkSkinRegion — zones de peau avec paramètres différents
    // =========================================================================
    /**
     * @struct NkSkinRegion
     * @brief Zones de peau avec paramètres physiques différents.
     *
     * Exemple : le visage a des paramètres SSS différents des mains.
     * Implémenté via une texture de masques multi-canaux (R=visage, G=mains...)
     */
    struct NkSkinRegion {
        static constexpr uint32 kMaxRegions = 8u;

        struct Region {
            NkString name;
            float32  sssRadius    = 1.f;
            float32  sssStrength  = 0.5f;
            float32  roughness    = 0.5f;
            float32  wetness      = 0.f;
            NkVec3f  sssColor     = {0.8f, 0.3f, 0.2f};
        };

        NkTextureHandle regionMaskTex;       ///< Texture multi-canal (un canal par zone)
        Region          regions[kMaxRegions];
        uint32          regionCount = 0;

        bool AddRegion(const char* name, float32 sssR = 1.f,
                       float32 sssStr = 0.5f) noexcept {
            if (regionCount >= kMaxRegions) return false;
            regions[regionCount].name = name;
            regions[regionCount].sssRadius   = sssR;
            regions[regionCount].sssStrength = sssStr;
            ++regionCount;
            return true;
        }
    };
    NK_COMPONENT(NkSkinRegion)

    // =========================================================================
    // NkEyeRig — rig d'œil complet
    // =========================================================================
    /**
     * @struct NkEyeRig
     * @brief Composant ECS pour le rig et le rendu d'un œil réaliste.
     *
     * 🔹 Anatomie simulée :
     *   • Sclère (blanc de l'œil) avec veines et teinte
     *   • Iris avec texture procédurale ou réelle
     *   • Pupille (dilation procédurale selon luminosité ou émotion)
     *   • Cornée avec reflet humide (wet shader)
     *   • Limbus (bord iris/sclère)
     *   • Cils riggés (simplement, 4-5 segments)
     *
     * 🔹 Animation procédurale automatique :
     *   • Clignement (sinusoïde + événement random)
     *   • Micro-saccades (mouvements involontaires du regard)
     *   • Dilation de la pupille (luminosité ambiante + émotion)
     *   • Humidité cornéenne (larmes légères)
     */
    struct NkEyeRig {
        // ── Identification ────────────────────────────────────────────────
        enum class Side : uint8 { Left, Right };
        Side side = Side::Left;

        // ── Entités des composants de l'œil ──────────────────────────────
        NkEntityId scleraEntity   = NkEntityId::Invalid();  ///< Sclère (mesh)
        NkEntityId irisEntity     = NkEntityId::Invalid();  ///< Iris (mesh)
        NkEntityId corneaEntity   = NkEntityId::Invalid();  ///< Cornée (mesh transparent)
        NkEntityId pupilEntity    = NkEntityId::Invalid();  ///< Pupille (scale de l'iris)
        uint32     eyeBoneIndex   = 0;                      ///< Os de l'œil dans NkSkeleton

        // ── Paramètres visuels ────────────────────────────────────────────
        NkVec3f  irisColor        = {0.25f, 0.5f, 0.9f};   ///< Couleur de l'iris
        NkVec3f  scleraColor      = {0.95f, 0.93f, 0.88f}; ///< Teinte du blanc
        float32  irisRadius       = 0.12f;    ///< Rayon iris [0..0.5 UV normalisé]
        float32  limbusWidth      = 0.02f;    ///< Largeur du limbus

        // ── Cornée ────────────────────────────────────────────────────────
        float32  corneaRefraction = 1.336f;   ///< Indice de réfraction (eau≈1.336)
        float32  corneaWetness    = 0.8f;     ///< Humidité [0..1]
        float32  tearMeniscus     = 0.0f;     ///< Ménisque lacrymal (yeux larmoyants)

        // ── Pupille ───────────────────────────────────────────────────────
        float32  pupilDilation    = 0.4f;     ///< Diamètre pupille [0=fermé, 1=plein]
        float32  pupilSpeed       = 2.f;      ///< Vitesse de dilation (unités/s)
        bool     autoDilation     = true;     ///< Dilation auto selon lumière ambiante

        // ── Animation procédurale ─────────────────────────────────────────
        bool     autoBlinkEnabled   = true;
        float32  blinkRate          = 0.25f;  ///< Clignements par seconde
        float32  blinkDuration      = 0.15f;  ///< Durée d'un clignement (s)
        bool     autoSaccadeEnabled = true;
        float32  saccadeFrequency   = 3.f;    ///< Micro-saccades par seconde
        float32  saccadeAmplitude   = 0.002f; ///< Amplitude (radians)

        // ── Direction du regard ───────────────────────────────────────────
        NkVec3f  lookTarget       = {0, 0, -1};  ///< Point regardé (espace monde)
        bool     useLookTarget    = false;
        float32  lookBlendSpeed   = 10.f;    ///< Vitesse de suivi du regard

        // ── État interne ──────────────────────────────────────────────────
        float32  blinkTimer       = 0.f;
        float32  blinkPhase       = 0.f;     ///< [0=ouvert, 1=fermé]
        float32  saccadeTimer     = 0.f;
        NkVec2f  saccadeOffset    = {};
        float32  currentPupil     = 0.4f;
        NkVec3f  currentLookDir   = {0, 0, -1};

        bool     enabled          = true;
    };
    NK_COMPONENT(NkEyeRig)

    // =========================================================================
    // NkEyeSystem — anime les yeux procéduralement
    // =========================================================================
    class NkEyeSystem final : public NkSystem {
    public:
        [[nodiscard]] NkSystemDesc Describe() const override {
            return NkSystemDesc{}
                .Writes<NkEyeRig>()
                .Reads<NkTransform>()
                .InGroup(NkSystemGroup::PostUpdate)
                .WithPriority(780.f)
                .Named("NkEyeSystem");
        }

        void Execute(NkWorld& world, float32 dt) noexcept override;

    private:
        void UpdateBlink  (NkEyeRig& eye, float32 dt) noexcept;
        void UpdateSaccade(NkEyeRig& eye, float32 dt) noexcept;
        void UpdatePupil  (NkEyeRig& eye, float32 dt,
                           float32 ambientLuminance) noexcept;
        void UpdateLook   (NkEyeRig& eye, NkWorld& world,
                           const NkTransform& tf, float32 dt) noexcept;

        float32 mAmbientLuminance = 0.5f;
    };

    // =========================================================================
    // NkTeethMaterial — matériau dents avec humidité et visèmes
    // =========================================================================
    struct NkTeethMaterial {
        NkTextureHandle albedoTex;
        NkTextureHandle normalTex;
        NkVec3f  baseColor     = {0.92f, 0.90f, 0.85f};   ///< Ivoire
        float32  roughness     = 0.3f;
        float32  wetness       = 0.6f;    ///< Salive sur l'émail
        float32  translucency  = 0.15f;   ///< Légère translucidité des dents
        NkVec3f  subsurfaceColor = {0.8f, 0.75f, 0.7f};

        // Phonèmes → forme des lèvres/mâchoire (visème mapping simplifié)
        float32  jawOpenAmount   = 0.f;   ///< [0=fermé, 1=grand ouvert]
        float32  teethVisibility = 0.f;   ///< [0=cachés, 1=visibles]
        bool     showSaliva      = true;
    };
    NK_COMPONENT(NkTeethMaterial)

} // namespace nkentseu
