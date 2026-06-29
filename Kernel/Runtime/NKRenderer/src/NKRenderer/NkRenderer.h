#pragma once
// =============================================================================
// NkRenderer.h  — NKRenderer v4.0
// Façade publique principale. 
// NKRenderer ne connaît PAS : NkScene, ECS, application.
// Seul lien avec la plateforme : NkSurfaceDesc (depuis NKWindow).
// =============================================================================
#include "Core/NkRendererTypes.h"
#include "Core/NkRendererConfig.h"
#include "NKRHI/Core/NkIDevice.h"

// Forward declarations — évite de tout inclure
namespace nkentseu {
    namespace renderer {
        class NkRenderGraph;
        class NkTextureLibrary;
        class NkShaderLibrary;
        class NkMeshSystem;
        class NkMaterialSystem;
        class NkRender2D;
        class NkRender3D;
        class NkTextRenderer;
        class NkPostProcessStack;
        class NkOverlayRenderer;
        class NkOffscreenTarget;
        class NkShadowSystem;
        class NkVirtualShadowMaps;
        class NkVFXSystem;
        class NkAnimationSystem;
        class NkSimulationRenderer;
        struct NkOffscreenDesc;
    }
}

namespace nkentseu {
    namespace renderer {

        // =========================================================================
        // NkRenderer — interface pure
        // =========================================================================
        class NkRenderer {
        public:
            virtual ~NkRenderer() = default;

            // ── Fabrique ─────────────────────────────────────────────────────────
            // La swapchain est entierement geree par NkIDevice (cf. NkDeviceFactory::Create
            // qui prend la surface). Le renderer recupere les dimensions via le device.
            static NkRenderer* Create(NkIDevice* device, const NkRendererConfig& cfg);
            static void        Destroy(NkRenderer*& renderer);

            // ── Cycle de vie ──────────────────────────────────────────────────────
            virtual bool Initialize() = 0;
            virtual void Shutdown()   = 0;
            virtual bool IsValid()    const = 0;

            // ── Frame ─────────────────────────────────────────────────────────────
            virtual bool BeginFrame() = 0;
            virtual void EndFrame()   = 0;
            virtual void Present()    = 0;

            // ── Resize (appeler depuis NkGraphicsContextResizeEvent) ──────────────
            virtual void OnResize(uint32 width, uint32 height) = 0;

            // ── Sous-systèmes ─────────────────────────────────────────────────────
            virtual NkRenderGraph*        GetRenderGraph()  = 0;
            virtual NkTextureLibrary*     GetTextures()     = 0;
            virtual NkShaderLibrary*      GetShaders()      = 0;
            virtual NkMeshSystem*         GetMeshSystem()   = 0;
            virtual NkMaterialSystem*     GetMaterials()    = 0;
            virtual NkRender2D*           GetRender2D()     = 0;
            virtual NkRender3D*           GetRender3D()     = 0;
            virtual class NkMaterialCollection* GetMaterialCollection() = 0;
            virtual NkTextRenderer*       GetTextRenderer() = 0;
            virtual NkPostProcessStack*   GetPostProcess()  = 0;
            virtual NkOverlayRenderer*    GetOverlay()      = 0;
            virtual NkVirtualShadowMaps*  GetShadow()       = 0;
            virtual NkVFXSystem*          GetVFX()          = 0;
            virtual NkAnimationSystem*    GetAnimation()    = 0;
            virtual NkSimulationRenderer* GetSimulation()   = 0;

            // ── Targets offscreen ─────────────────────────────────────────────────
            virtual NkOffscreenTarget* CreateOffscreen(const NkOffscreenDesc& desc) = 0;
            virtual void               DestroyOffscreen(NkOffscreenTarget*& t)      = 0;

            // Redirige la SORTIE FINALE du render graph (normalement la swapchain) vers
            // une texture externe (ex. offscreen d'un viewport editeur sur device PARTAGE).
            // handle null => swapchain (defaut). Reconstruit le graph. Permet de rendre le
            // PIPELINE COMPLET (ombres/eclairage/IBL/tonemap) dans un RT echantillonnable.
            virtual void SetFinalColorTarget(NkTextureHandle target) = 0;

            // ── Planar reflections (auto) ─────────────────────────────────────────
            // L'utilisateur enregistre un plan reflechissant ; le renderer fait
            // automatiquement la passe miroir avant la passe Geometry principale,
            // et met a jour le material cible avec le RT du reflet.
            // L'utilisateur n'a PLUS BESOIN de soumettre les drawcalls deux fois.
            virtual class NkPlanarReflectionSystem* GetPlanarReflection() = 0;

            // Phase H.6 : voxel AO system. L'app enregistre les occluders
            // (sol, gros meshes static) via GetVoxelAO()->RegisterOccluder()
            // puis appelle Build() une fois pour bake le voxel grid. Le PBR
            // shader sample automatiquement le grid pour atténuer l'IBL des
            // zones occluses (ex: objets sous le sol qui sont cachés du sky).
            virtual class NkVoxelAOSystem* GetVoxelAO() = 0;

            // ── Configuration dynamique ───────────────────────────────────────────
            virtual void SetVSync     (bool enabled)          = 0;
            virtual void SetPostConfig(const NkPostConfig& pp)= 0;
            virtual void SetWireframe (bool enabled)          = 0;

            // ── Sous-systemes runtime (enable/disable a chaud) ────────────────────
            // Active un (ou plusieurs) sous-systeme(s) : alloue et initialise s'il
            // n'existe pas encore. Reconstruit le render graph ensuite.
            // Renvoie true si au moins un sous-systeme a ete (re)cree avec succes,
            // false si tous etaient deja actifs ou si un init a echoue.
            virtual bool EnableSubsystem (NkSubsystemFlags flags) = 0;

            // Desactive un (ou plusieurs) sous-systeme(s) : shutdown et libere.
            // Reconstruit le render graph ensuite. Les dependances inverses sont
            // verifiees : desactiver RENDER2D ferme aussi TEXT/UI/OVERLAY si actifs.
            virtual void DisableSubsystem(NkSubsystemFlags flags) = 0;

            // Vrai si TOUS les flags fournis sont actuellement actifs.
            virtual bool IsSubsystemActive(NkSubsystemFlags flags) const = 0;

            // Etat global des sous-systemes (bitfield).
            virtual NkSubsystemFlags GetActiveSubsystems() const = 0;

            // ── Stats ─────────────────────────────────────────────────────────────
            virtual const NkRendererStats& GetStats()  const = 0;
            virtual void                   ResetStats()      = 0;

            // ── Accès bas niveau ──────────────────────────────────────────────────
            virtual NkIDevice*              GetDevice()     const = 0;
            virtual NkICommandBuffer*       GetCmd()        const = 0;
            virtual uint32                  GetFrameIndex() const = 0;
            virtual uint32                  GetWidth()      const = 0;
            virtual uint32                  GetHeight()     const = 0;
            virtual const NkRendererConfig& GetConfig()     const = 0;
        };

    } // namespace renderer
} // namespace nkentseu
