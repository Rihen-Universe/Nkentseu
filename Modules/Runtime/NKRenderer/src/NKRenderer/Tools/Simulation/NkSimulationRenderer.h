#pragma once
// =============================================================================
// NkSimulationRenderer.h  — NKRenderer v4.0  (Tools/Simulation/)
//
// Module de rendu pour simulations IA/émotionnelles.
// Stub pour intégration future :
//   • PV3DE (Patient Virtuel 3D Émotif) — modèles de peau SSS, yeux, larmes
//   • Simulation de foule
//   • Agents émotifs (blush, pâleur, transpiration)
//   • Déformations morphologiques (blend shapes)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
namespace renderer {

    class NkRender3D;
    class NkVFXSystem;

    // =========================================================================
    // Données émotionnelles (pour PV3DE)
    // =========================================================================
    struct NkEmotionState {
        float32 anger     = 0.f;  // 0..1
        float32 joy       = 0.f;
        float32 sadness   = 0.f;
        float32 fear      = 0.f;
        float32 disgust   = 0.f;
        float32 surprise  = 0.f;
        float32 pain      = 0.f;
        float32 fatigue   = 0.f;
    };

    // Blend shapes (morph targets) d'un personnage
    struct NkBlendShapeState {
        // Expressions basiques FACS-inspiré
        float32 browInnerUp    = 0.f;
        float32 browOuterUpL   = 0.f;
        float32 browOuterUpR   = 0.f;
        float32 browDownL      = 0.f;
        float32 browDownR      = 0.f;
        float32 eyeWideL       = 0.f;
        float32 eyeWideR       = 0.f;
        float32 eyeSquintL     = 0.f;
        float32 eyeSquintR     = 0.f;
        float32 eyeBlinkL      = 0.f;
        float32 eyeBlinkR      = 0.f;
        float32 jawOpen        = 0.f;
        float32 mouthSmileL    = 0.f;
        float32 mouthSmileR    = 0.f;
        float32 mouthFrownL    = 0.f;
        float32 mouthFrownR    = 0.f;
        float32 cheekPuffL     = 0.f;
        float32 cheekPuffR     = 0.f;
        float32 noseSneerL     = 0.f;
        float32 noseSneerR     = 0.f;
        float32 tongueOut      = 0.f;
        float32 blush          = 0.f;  // peau rougie
        float32 pallor         = 0.f;  // pâleur
        float32 sweat          = 0.f;  // sueur (wet shader param)
        float32 tears          = 0.f;  // larmes
    };

    // =========================================================================
    // NkSimulationRenderer
    // =========================================================================
    class NkSimulationRenderer {
    public:
        NkSimulationRenderer() = default;
        ~NkSimulationRenderer();

        bool Init(NkIDevice* device, NkRender3D* r3d, NkVFXSystem* vfx);
        void Shutdown();

        // ── Personnage PV3DE ─────────────────────────────────────────────────
        // Soumet un personnage avec état émotionnel + blend shapes.
        // Les blend shapes sont interpolés vers les valeurs cibles.
        void SubmitCharacter(NkMeshHandle    mesh,
                              NkMatInstHandle skinMaterial,
                              const NkMat4f& transform,
                              const NkMat4f* boneMatrices,
                              uint32          boneCount,
                              const NkBlendShapeState& blendShapes,
                              const NkEmotionState&    emotion);

        // ── Simulation de foule ──────────────────────────────────────────────
        struct AgentDesc {
            NkMeshHandle    mesh;
            NkMatInstHandle material;
            NkMat4f         transform;
            NkVec3f         velocity;
            float32         age;
            NkEmotionState  emotion;
        };
        void SubmitCrowd(const AgentDesc* agents, uint32 count);

        // ── Effets biologiques ───────────────────────────────────────────────
        void UpdateSkinParams(NkMatInstHandle skinMat,
                               const NkBlendShapeState& s,
                               const NkEmotionState& e);

    private:
        NkIDevice*  mDevice = nullptr;
        NkRender3D* mR3D    = nullptr;
        NkVFXSystem*mVFX    = nullptr;
    };

} // namespace renderer
} // namespace nkentseu
