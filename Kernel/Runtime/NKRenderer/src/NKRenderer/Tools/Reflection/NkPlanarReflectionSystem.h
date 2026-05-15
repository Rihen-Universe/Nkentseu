#pragma once
// =============================================================================
// NkPlanarReflectionSystem.h  — NKRenderer Tools/Reflection
//
// Systeme automatique de reflexions planaires. L'utilisateur enregistre des
// plans reflechissants ; le renderer s'occupe AUTOMATIQUEMENT de :
//   1. Allouer et gerer les RT par plan
//   2. Avant la passe Geometry principale, re-rendre la scene avec une camera
//      miroir dans chaque RT
//   3. Choisir le bon cote du plan en fonction de la position camera
//      (camera +N -> reflete objets cote +N ; cote -N -> reflete cote -N)
//   4. Mettre a jour le material cible avec le RT et mirrorViewProj
//
// L'utilisateur (Demo4 par exemple) submit ses drawcalls une seule fois ;
// le systeme rejoue automatiquement la queue avec mirror_matrix pour le RT.
//
// Usage :
//   auto reflId = renderer->AddPlanarReflection({
//       .normal = {0,1,0}, .point = {0,0,0},
//       .rtWidth = 512, .rtHeight = 256, .hdr = true,
//       .targetMaterial = floorMatInst,
//   });
//   // ... tous les frames : juste submit normalement
//   r3d->BeginScene(ctx); r3d->Submit(sphere); ... r3d->EndScene();
//   // -> reflexion auto-gere par le renderer
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkRenderTarget.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
    namespace renderer {

        class NkRender3D;
        class NkMaterialSystem;
        class NkTextureLibrary;

        // Mode d'affichage des faces du sol miroir (style Blender) :
        //   FRONT_ONLY = face avant visible avec reflet, face arriere = discard
        //   BACK_ONLY  = face arriere visible (debug surtout), face avant = discard
        //   BOTH       = les deux faces visibles, chacune avec son propre RT
        //                (twoSided force auto -> active la 2eme passe miroir)
        enum class NkPlanarFaceMode : uint8 {
            FRONT_ONLY = 0,
            BACK_ONLY  = 1,
            BOTH       = 2
        };

        struct NkPlanarReflectionDesc {
            // Plane equation : N . (P - point) = 0. Normale orientee vers le
            // "cote actif" (cote dont on capture les objets pour le miroir).
            NkVec3f normal = {0.f, 1.f, 0.f};
            NkVec3f point  = {0.f, 0.f, 0.f};

            // Dimensions du RT (souvent demi-resolution suffit pour un effet
            // de reflet realiste).
            uint32 rtWidth  = 512;
            uint32 rtHeight = 256;
            bool   hdr      = true;
            NkString debugName;

            // Material cible : recevra le RT (binding "albedo") + le
            // mirrorViewProj (via UBO Camera). Si invalide, le systeme rend
            // dans le RT mais ne le bind nulle part — utile pour debug.
            NkMatInstHandle targetMaterial;

            // Cull mode pour le sol miroir lui-meme (lecture par le shader
            // via uniform faceMode dans le UBO materiau). Independant du
            // pipeline cullMode.
            NkPlanarFaceMode faceMode = NkPlanarFaceMode::FRONT_ONLY;

            // Si true : alloue un 2eme RT pour la face arriere (objets cote -N)
            // et fait une passe miroir supplementaire avec clip plane inverse.
            // Le shader ReflFloor sample le bon RT selon le cote vu. Force
            // automatiquement si faceMode == BOTH.
            bool twoSided = false;
        };

        struct NkPlanarReflectionHandle {
            uint32 idx = ~0u;
            bool IsValid() const { return idx != ~0u; }
        };

        class NkPlanarReflectionSystem {
            public:
                NkPlanarReflectionSystem() = default;
                ~NkPlanarReflectionSystem() { Shutdown(); }

                bool Init(NkIDevice* dev, NkTextureLibrary* texLib, NkMaterialSystem* matSys);
                void Shutdown();

                NkPlanarReflectionHandle Register(const NkPlanarReflectionDesc& desc);
                void Unregister(NkPlanarReflectionHandle handle);
                void Clear();

                // Appele par NkRendererImpl entre EndScene et le Flush principal :
                // pour chaque plane reg, calcule la cam miroir, fait flush la queue
                // de drawcalls dans le RT, met a jour le target material.
                // Si r3d->IsInScene() est false, no-op.
                void RenderReflections(NkICommandBuffer* cmd, NkRender3D* r3d);

                // Accessors
                uint32 Size() const { return (uint32)mPlanes.Size(); }

            private:
                struct Plane {
                    NkPlanarReflectionDesc desc;
                    NkRenderTarget         rtPos;    // cote +N (face avant)
                    NkRenderTarget         rtNeg;    // cote -N (face arriere, si twoSided)
                    bool                   active   = false;
                };

                NkIDevice*        mDevice  = nullptr;
                NkTextureLibrary* mTexLib  = nullptr;
                NkMaterialSystem* mMatSys  = nullptr;
                NkVector<Plane>   mPlanes;

                bool InitPlaneRT(Plane& p);
        };

    } // namespace renderer
} // namespace nkentseu
