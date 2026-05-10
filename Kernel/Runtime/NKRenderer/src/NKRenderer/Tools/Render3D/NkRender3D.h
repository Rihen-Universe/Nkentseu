#pragma once
// =============================================================================
// NkRender3D.h  — NKRenderer v4.0  (Tools/Render3D/)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkSceneContext.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        class NkShadowSystem;
        class NkEnvironmentSystem;
        class NkShaderLibrary;
        class NkResources;

        // (NkViewMode et NkSceneContext sont definis dans Core/NkRendererTypes.h)

        class NkRender3D {
            public:
                NkRender3D() = default;
                ~NkRender3D();

                bool Init(NkIDevice* device, NkMeshSystem* mesh, NkMaterialSystem* mat,
                        NkRenderGraph* graph,
                        NkShadowSystem* shadow,
                        NkEnvironmentSystem* env,
                        NkShaderLibrary* shaderLib,
                        NkResources* resources,
                        uint32 framesInFlight = 1);
                void Shutdown();

                // Notification de redimensionnement (propage par NkRendererImpl).
                // Les RT sont geres par le PostProcess/le RenderGraph ; ici on cache
                // juste la taille courante pour le viewport implicite.
                void OnResize(uint32 w, uint32 h) { mW = w; mH = h; }

                // ── Frame ────────────────────────────────────────────────────────────
                void BeginScene(const NkSceneContext& ctx);
                void Flush(NkICommandBuffer* cmd);

                // Render des opaques castShadow=true depuis la perspective de la
                // lumiere (lightVP = lightProj * lightView), dans le FBO shadow
                // currentement bindé. Appele par NkShadowSystem dans la passe
                // Shadows du RenderGraph. Reutilise mUBOObject pour le model.
                void RenderShadowPass(NkICommandBuffer* cmd, const NkMat4f& lightVP);

                // Acces au scene context courant (pour NkShadowSystem qui a besoin
                // de la light direction + camera frustum pour le fitting).
                const NkSceneContext& GetSceneContext() const noexcept { return mCtx; }

                // ── Submit ───────────────────────────────────────────────────────────
                void Submit         (const NkDrawCall3D& dc);
                void SubmitMany     (const NkDrawCall3D* dcs, uint32 count);
                void SubmitInstanced(const NkDrawCallInstanced& dc);
                void SubmitSkinned  (const NkDrawCallSkinned& dc);
                void SubmitSkinnedTinted(const NkDrawCallSkinned& dc,
                                        NkVec3f tint, float32 alpha=1.f);

                void SetWireframe(bool e) { mWireframe=e; }

                // Phase E.6 : bind une texture comme cookie 3D au slot [0..7].
                // Le `cookieIdx` dans NkLightDesc reference ce slot. Surtout
                // utile pour les SPOT lights qui projettent un motif
                // (faisceau de fenetre, lampe-torche pattern, gobo etc).
                static constexpr uint32 kMaxCookies3D     = 8;  // sampler2D, spot+directional
                static constexpr uint32 kMaxCookiesCube3D = 4;  // samplerCube, point lights

                // Phase F.B.1 : taille du pool d'ObjectUBO par frame. Chaque drawcall
                // (shadow + opaque + skinned) consume un slot. Si le total des draws
                // d'une frame > kMaxObjectsPerFrame, on overflow et on log un warning.
                // 256 couvre largement les scenes de test (5 spheres + plane + cube).
                static constexpr uint32 kMaxObjectsPerFrame = 256;
                void SetLightCookie3D(uint32 slot, NkTextureHandle tex);

                // E.6b : bind une cubemap comme cookie pour point lights
                // (slot [0..3]). Utiliser cookieIdx dans NkLightDesc pour
                // referencer ce slot. Sample base sur la direction light→frag.
                void SetLightCookieCube3D(uint32 slot, NkTextureHandle cubeTex);

                // ── Debug gizmos ─────────────────────────────────────────────────────
                void DrawDebugLine  (NkVec3f a, NkVec3f b,   NkVec4f color, float32 life=0.f);
                void DrawDebugSphere(NkVec3f c, float32 r,   NkVec4f color);
                void DrawDebugCircle(NkVec3f c, float32 r, NkVec3f normal,   NkVec4f color);
                void DrawDebugAABB  (const NkAABB& box,       NkVec4f color);
                void DrawDebugAxes  (const NkMat4f& t, float32 size=1.f);
                void DrawDebugGrid  (NkVec3f origin, float32 spacing, int32 lines, NkVec4f color);
                void DrawDebugArrow (NkVec3f from, NkVec3f to, NkVec4f color);

            private:
                struct SortedDC { NkDrawCall3D dc; float32 depth; };
                struct DebugLine { NkVec3f a,b; NkVec4f color; float32 life; };

                NkIDevice*           mDevice  = nullptr;
                NkMeshSystem*        mMesh    = nullptr;
                NkMaterialSystem*    mMat     = nullptr;
                NkRenderGraph*       mGraph   = nullptr;
                NkShadowSystem*      mShadow  = nullptr;
                NkEnvironmentSystem* mEnv     = nullptr;
                NkShaderLibrary*     mShaderLib = nullptr;
                NkResources*         mResources = nullptr;

                NkSceneContext    mCtx;
                bool              mInScene  = false;
                bool              mWireframe= false;
                uint32            mW = 0, mH = 0;  // taille courante (mise a jour par OnResize)

                NkVector<SortedDC>              mOpaque;
                NkVector<SortedDC>              mTransparent;
                NkVector<NkDrawCallInstanced>   mInstanced;
                NkVector<NkDrawCallSkinned>     mSkinned;
                NkVector<DebugLine>             mDebugLines;

                // Ring buffers per-frame UBOs (taille = NkRendererConfig::framesInFlight,
                // clampe a [1,3]). mFrameSlot tourne 0..N-1 a chaque BeginScene.
                // Camera + Lights : 1 UBO par frame (donnees globales, ecrites une fois
                // par frame dans UploadUBOs).
                NkVector<NkBufferHandle>   mUBOCameraRing;
                NkVector<NkBufferHandle>   mUBOLightsRing;

                // Phase F.B.1 : pool d'ObjectUBO (frame x drawIdx). Vulkan interdit
                // vkCmdUpdateBuffer dans un renderPass actif, donc on ne peut pas
                // re-uploader le meme UBO entre deux draws. Solution : 1 UBO + 1
                // descriptor set par drawcall, tous pre-alloues a Init. WriteBuffer
                // (memcpy via mapped pointer) est legal dans le renderPass.
                // mObjectDrawIdx compte les draws consommes pour la frame courante,
                // reset a 0 dans BeginScene. Shadow + opaque + skinned partagent le
                // meme compteur monotone (ordre : shadow d'abord, puis opaque, puis
                // skinned).
                NkVector<NkVector<NkBufferHandle>>  mUBOObjectPool;   // [frame][drawIdx]
                NkBufferHandle             mSSBOBones;
                NkTextureHandle            mDefaultCubeWhite;   // E.6b : fallback cube cookie
                uint32                     mFramesInFlight = 1;
                uint32                     mFrameSlot      = 0;
                uint32                     mObjectDrawIdx  = 0;

                // Descriptor sets: set 0 = per-frame (camera+lights+shadow+env+shadowMap),
                //                  set 1 = per-object (model+bones)
                NkDescSetHandle              mGlobalLayout;
                NkVector<NkDescSetHandle>    mGlobalSetRing;
                NkDescSetHandle              mObjectLayout;
                // Phase F.B.1 : pool de descriptor sets per-object (frame x drawIdx).
                // Chaque set est pre-bind a son UBO du pool a Init (1:1).
                NkVector<NkVector<NkDescSetHandle>> mObjectSetPool;

                // PBR pipeline + shader (charges depuis Resources/Shaders/PBR/GL/).
                // Le pipeline est cree paresseusement au 1er FlushOpaque, quand
                // le RP de la pass Geometry du RenderGraph est connu (Vulkan/DX12
                // exigent la compatibilite RP a la creation). mPBRPipelineRP
                // track quel RP a servi a creer le pipeline pour le recreer si
                // le RP change (ex : resize swapchain, toggle PostProcess).
                ::nkentseu::NkShaderHandle mPBRShader;     // RHI shader handle
                NkPipelineHandle           mPBRPipeline;   // pipeline graphics PBR
                NkRenderPassHandle         mPBRPipelineRP{};

                // Shadow pipeline + shader (depth-only, reutilise dans les passes
                // de shadow map du NkShadowSystem). Partage mObjectLayout avec PBR.
                ::nkentseu::NkShaderHandle mShadowShader;
                NkPipelineHandle           mShadowPipeline;

                void UploadUBOs(NkICommandBuffer* cmd);
                void FlushOpaque     (NkICommandBuffer* cmd);
                void FlushTransparent(NkICommandBuffer* cmd);
                void FlushInstanced  (NkICommandBuffer* cmd);
                void FlushSkinned    (NkICommandBuffer* cmd);
                void FlushDebug      (NkICommandBuffer* cmd);
                void SortDrawCalls();

                // Cree (ou recree) le pipeline PBR pour qu'il soit compatible
                // avec le RP courant de la pass Geometry. Lazy : appelee au
                // debut de Flush() seulement quand la pass Geometry a deja
                // execute au moins une fois (donc son fb est cache). Idempotent
                // si le RP n'a pas change. Retourne false si shader ou create
                // ont echoue.
                bool EnsurePBRPipeline(NkRenderPassHandle currentRP);
        };

    } // namespace renderer
} // namespace nkentseu
