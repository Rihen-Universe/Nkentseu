// =============================================================================
// NkSimulationRenderer.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkSimulationRenderer.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/VFX/NkVFXSystem.h"

namespace nkentseu {
namespace renderer {

    NkSimulationRenderer::~NkSimulationRenderer() { Shutdown(); }

    bool NkSimulationRenderer::Init(NkIDevice* device,
                                      NkRender3D* r3d,
                                      NkVFXSystem* vfx) {
        mDevice=device; mR3D=r3d; mVFX=vfx;
        return true;
    }

    void NkSimulationRenderer::Shutdown() {}

    // ── Personnage PV3DE ─────────────────────────────────────────────────────
    void NkSimulationRenderer::SubmitCharacter(NkMeshHandle    mesh,
                                                 NkMatInstHandle skinMat,
                                                 const NkMat4f&  transform,
                                                 const NkMat4f*  bones,
                                                 uint32          boneCount,
                                                 const NkBlendShapeState& bs,
                                                 const NkEmotionState&    em) {
        if (!mR3D) return;

        // 1. Mettre à jour les paramètres de peau selon état émotionnel
        UpdateSkinParams(skinMat, bs, em);

        // 2. Soumettre le mesh skinné
        NkDrawCallSkinned dc;
        dc.mesh     = mesh;
        dc.material = skinMat;
        dc.transform= transform;
        dc.boneMatrices.Resize(boneCount);
        for(uint32 i=0;i<boneCount;i++) dc.boneMatrices[i]=bones[i];
        dc.tint={1,1,1}; dc.alpha=1.f;
        mR3D->SubmitSkinned(dc);

        // 3. Effets biologiques via VFX
        if (mVFX) {
            // Larmes si sadness > 0.5 ou pain > 0.7
            if ((em.sadness > 0.5f || em.pain > 0.7f) && bs.tears > 0.1f) {
                // Spawn particles larmes (position yeux — approximée)
                NkMat4f eyeL = transform; eyeL[3][0] -= 0.03f; eyeL[3][1] += 0.08f;
                NkMat4f eyeR = transform; eyeR[3][0] += 0.03f; eyeR[3][1] += 0.08f;
                NkEmitterDesc ed;
                ed.position     = {eyeL[3][0],eyeL[3][1],eyeL[3][2]};
                ed.ratePerSec   = bs.tears * 3.f;
                ed.sizeStart    = 0.003f; ed.sizeEnd = 0.001f;
                ed.speedMin     = 0.02f;  ed.speedMax= 0.05f;
                ed.velocityDir  = {0,-1,0};
                ed.colorStart   = {0.7f,0.85f,1.f,0.8f};
                ed.colorEnd     = {0.7f,0.85f,1.f,0.f};
                ed.gravity      = {0,-2.f,0};
                ed.lifeMin      = 0.3f; ed.lifeMax = 0.6f;
                ed.maxParticles = 20;
                ed.loop         = true;
                // NOTE: dans un vrai usage l'émetteur serait persistant, pas créé chaque frame
            }

            // Rougeur (blush) → modifié dans UpdateSkinParams
            // Sueur → légère brillance (speculaire) gérée via material params
        }
    }

    // ── Foule ─────────────────────────────────────────────────────────────────
    void NkSimulationRenderer::SubmitCrowd(const AgentDesc* agents, uint32 count) {
        if (!mR3D || !agents) return;

        // Trier par matériau pour instancing
        // Pour l'instant : soumission directe (pas d'instancing par material)
        for (uint32 i=0;i<count;i++) {
            const auto& a = agents[i];
            if (!a.mesh.IsValid()) continue;
            NkDrawCall3D dc;
            dc.mesh      = a.mesh;
            dc.material  = a.material;
            dc.transform = a.transform;
            dc.tint      = {1,1,1};
            dc.alpha     = 1.f;
            dc.visible   = true;
            mR3D->Submit(dc);
        }
    }

    // ── Update paramètres peau ────────────────────────────────────────────────
    void NkSimulationRenderer::UpdateSkinParams(NkMatInstHandle skinMat,
                                                  const NkBlendShapeState& bs,
                                                  const NkEmotionState& em) {
        // Ces paramètres seraient transmis au matériau de peau SSS.
        // Les valeurs pilotent les uniforms dans le fragment shader skin.

        // Rougeur = blush + anger + joy
        float32 redness = bs.blush
                         + em.anger   * 0.4f
                         + em.joy     * 0.2f
                         + em.pain    * 0.3f;
        redness = (redness > 1.f) ? 1.f : redness;

        // Pâleur = fear + pallor
        float32 pallor = bs.pallor + em.fear * 0.5f + em.sadness * 0.2f;
        pallor = (pallor > 1.f) ? 1.f : pallor;

        // Brillance sueur = sweat + fatigue
        float32 sweat  = bs.sweat + em.fatigue * 0.4f;
        sweat  = (sweat  > 1.f) ? 1.f : sweat;

        // Passer à l'instance de matériau si valide
        // En vrai code, NkMatInstHandle → NkMaterialInstance* via le système
        // skinMat->SetFloat("uSkinRedness",    redness);
        // skinMat->SetFloat("uSkinPallor",     pallor);
        // skinMat->SetFloat("uSkinSweat",      sweat);
        // skinMat->SetFloat("uSSSIntensity",   0.5f + em.pain * 0.2f);
        (void)skinMat;  // TODO: lookup instance via NkMaterialSystem
    }

} // namespace renderer
} // namespace nkentseu
