#pragma once
// =============================================================================
// NkRenderer.h
// En-tête d'inclusion unique du système NKRenderer.
//
// NKRenderer est la couche haut-niveau au-dessus de NKRHI.
// Il ne réimplémente RIEN de ce que NKRHI a déjà fourni.
// Il expose des éléments haut-niveau pour simplifier la création d'éléments
// graphiques et fournit un système de moteur de rendu 2D/3D AAA temps-réel
// performant, destiné aux usages professionnels (film, archiviz, simulation,
// VR/AR/XR, IA) avec une philosophie proche de UE/UPBGE mais propre.
//
// Architecture en couches :
//
//   ┌────────────────────────────────────────────────────────────────────────┐
//   │  NkRenderer  (façade unique, point d'entrée)                          │
//   │  ├── NkResourceManager  — textures, meshes, shaders, matériaux        │
//   │  ├── NkRenderGraph      — ordonnancement des passes, barrières auto   │
//   │  ├── NkMaterialSystem   — PBR, Toon, Unlit, Archviz, Custom...        │
//   │  ├── NkRender2D         — sprites, shapes, texte, tilemap             │
//   │  ├── NkRender3D         — meshes, lumières, ombres CSM, instancing    │
//   │  ├── NkPostProcessStack — SSAO, Bloom, DOF, FXAA, Tonemap...          │
//   │  ├── NkOverlayRenderer  — debug, gizmos, stats HUD                   │
//   │  └── NkScene            — entités, hiérarchie, culling               │
//   └────────────────────────────────────────────────────────────────────────┘
//
// Dépendance : NKRHI uniquement (NkIDevice + NkICommandBuffer)
//
// Convention d'indentation stricte :
//   class NkFoo {
//      public:
//           ...
//      private:
//           ...
//   };
//
//   namespace nkentseu {
//       namespace renderer {
//           ...
//       }
//   }
// =============================================================================

#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Core/NkResourceManager.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Scene/NkScene.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/PostProcess/NkPostProcessStack.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // NkRenderer — façade principale du système de rendu
        // =============================================================================
        class NkRenderer {
           public:
                NkRenderer()  = default;
                ~NkRenderer() { Shutdown(); }

                NkRenderer(const NkRenderer&)            = delete;
                NkRenderer& operator=(const NkRenderer&) = delete;

                // ── Cycle de vie ──────────────────────────────────────────────────────────
                bool Init    (NkIDevice* device, const NkRendererConfig& cfg = {});
                void Shutdown();
                void Resize  (uint32 width, uint32 height);
                bool IsValid () const { return mIsValid; }

                // ── Accès aux sous-systèmes ────────────────────────────────────────────────
                NkResourceManager&   Resources()    { return mResources; }
                NkMaterialSystem&    Materials()    { return mMaterials; }
                NkRender2D&          Renderer2D()   { return mRender2D; }
                NkRender3D&          Renderer3D()   { return mRender3D; }
                NkPostProcessStack&  PostProcess()  { return mPostProcess; }
                NkOverlayRenderer&   Overlay()      { return mOverlay; }
                NkRenderGraph&       RenderGraph()  { return mRenderGraph; }

                // ── Rendu de frame ────────────────────────────────────────────────────────

                // Rendre une scène complète (3D + 2D overlay + post-process)
                void RenderFrame(NkICommandBuffer* cmd, NkScene& scene, float dt = 0.f);

                // Rendu 3D standalone (sans scène, pour des usages spécifiques)
                void Render3D(NkICommandBuffer* cmd, const NkRenderScene& scene);

                // Rendu 2D avec callback — batch automatique
                // Exemple : renderer.Render2D(cmd, w, h, [&](NkRender2D& r) {
                //     r.DrawRect({10,10,100,50}, NkColorF::Red());
                // });
                template<typename Fn>
                void Render2D(NkICommandBuffer* cmd, uint32 w, uint32 h, Fn&& fn) {
                    if (!mIsValid || !cmd) return;
                    mRender2D.Begin(cmd, w, h);
                    fn(mRender2D);
                    mRender2D.End();
                }

                // ── Post-processing ───────────────────────────────────────────────────────
                void SetPostProcessEnabled(bool enabled) { mPostProcessEnabled = enabled; }
                bool IsPostProcessEnabled() const { return mPostProcessEnabled; }

                // ── Environnement / Skybox ────────────────────────────────────────────────
                void SetEnvironmentHDRI (const char* hdriPath);
                void SetEnvironmentColor(const NkColorF& skyTop,
                                         const NkColorF& skyBottom);
                void SetAmbientIntensity(float v);

                // ── Statistiques ──────────────────────────────────────────────────────────
                const NkRendererStats& GetStats()     const { return mStats; }
                void                   ResetStats()         { mStats = {}; }
                void                   DisplayStats  (NkICommandBuffer* cmd,
                                                     float x = 10.f, float y = 10.f);

                // ── Accès au device sous-jacent ───────────────────────────────────────────
                NkIDevice* GetDevice() const { return mDevice; }

           private:
                NkIDevice*           mDevice            = nullptr;
                NkRendererConfig     mConfig;
                bool                 mIsValid            = false;
                bool                 mPostProcessEnabled = true;

                NkResourceManager    mResources;
                NkMaterialSystem     mMaterials;
                NkRender2D           mRender2D;
                NkRender3D           mRender3D;
                NkPostProcessStack   mPostProcess;
                NkOverlayRenderer    mOverlay;
                NkRenderGraph        mRenderGraph;

                NkRendererStats      mStats;
        };

    } // namespace renderer
} // namespace nkentseu
