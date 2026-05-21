#pragma once
// =============================================================================
// NkRender2D.h  — NKRenderer v4.0  (Tools/Render2D/)
// Sprites, shapes, 9-slice, clip stack, batching automatique.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        class NkShaderLibrary;   // forward — pour wirer les shaders compiles

        class NkRender2D {
            public:
                NkRender2D() = default;
                ~NkRender2D();

                // shaderLib peut etre nullptr : dans ce cas les pipelines sont crees
                // sans programme et le rendu ne produira rien (mode degrade).
                bool Init(NkIDevice* device, NkTextureLibrary* texLib,
                          NkShaderLibrary* shaderLib = nullptr,
                          uint32 maxVerts = 65536);
                void Shutdown();

                // Notification de redimensionnement (propage par NkRendererImpl).
                // Render2D ne possede pas de RT propre — on cache juste la taille
                // courante pour les futurs Begin() sans arguments.
                void OnResize(uint32 w, uint32 h) { mW = w; mH = h; }

                // ── Frame ──────────────────────────────────────────────────────────────
                void Begin(NkICommandBuffer* cmd, uint32 w, uint32 h,
                            float32 cX=0, float32 cY=0, float32 zoom=1.f, float32 rotDeg=0.f);
                void End();
                void FlushPending(NkICommandBuffer* cmd);

                // ── Lighting 2D (Phase E) ──────────────────────────────────────────────
                // Active/desactive le lit mode pour les draws SUIVANTS. Un drawcall
                // emis avec mLitMode=true verra son fragment colore par les lumieres
                // courantes (cf SetLights2D + ambient). Mettre a false pour les UI
                // overlay et autres elements non-eclaires.
                void SetLit(bool lit) { mLitMode = lit; }
                bool IsLit() const    { return mLitMode; }

                // E.7b : layer mask pour les SHAPES suivantes. Une light n'affecte
                // une shape que si (light.layerMask & shape.layerMask) != 0.
                // Default 0xFF (toutes les layers, light visible). Permet de
                // separer foreground/background ou world/HUD.
                void SetLayerMask(uint32 mask) { mLayerMask = mask & 0xFFu; }
                uint32 GetLayerMask() const    { return mLayerMask; }

                // Upload jusqu'a kMaxLights2D point lights + ambient. Doit etre
                // appele entre Begin() et End(), avant tout draw lit. Apres c'est
                // l'UBO bindé a chaque flush qui fournit les valeurs au shader.
                static constexpr uint32 kMaxLights2D    = 16;
                static constexpr uint32 kMaxCookies2D   = 8;
                static constexpr uint32 kMaxOccluders2D = 32;
                static constexpr uint32 kMaxAABBs2D     = 32;
                void SetLights2D(const NkLight2DDesc* lights, uint32 count,
                                  NkVec3f ambient = {0.1f, 0.1f, 0.1f});

                // Bind une texture comme cookie au slot [0..7]. Le `cookieIdx`
                // dans NkLight2DDesc reference ce slot. Doit etre appele avant
                // SetLights2D pour que le binding soit visible.
                void SetLightCookie(uint32 slot, NkTexHandle tex);

                // E.7c : bind une normal map au binding 12. Quand le user dessine
                // avec mNormalMode=true (cf SetNormalMap), le bit 8 du flags est
                // mis et le shader sample tnNormal pour faire un fake N.L 3D.
                void SetNormalMap(NkTexHandle tex);
                void SetNormalMode(bool e) { mNormalMode = e; }

                // E.5 : enregistre les casters d'ombre 2D (cercles uniquement
                // dans cette version, jusqu'a 32 max). Pour chaque fragment LIT,
                // le shader test si la ligne fragment->light est bloquee par un
                // de ces cercles -> ombre. Doit etre appele entre Begin/End.
                void SetShadowCasters2D(const NkShadowCaster2D* casters, uint32 count);

                // E.7 : enregistre des AABB casters d'ombre (32 max). Walls,
                // plateformes, coffres typiques de platformer. Le shader fait
                // un ray-AABB slab test en plus du ray-circle.
                void SetShadowCastersAABB2D(const NkShadowCasterAABB2D* aabbs, uint32 count);

                // E.7d : transform iso optionnel applique aux positions avant le
                // ray test. Permet d'avoir des ombres correctes en jeux iso 2:1
                // ou avec sol incline. La matrice est appliquee a fragPos +
                // lightPos + occluder.pos avant l'intersection. Default identity
                // (1,0,0,1) = pas de transform.
                //   m00 m01     ex pour iso 2:1 (Y compresse a 50%) :
                //   m10 m11        SetIsoTransform(1, 0, 0, 2);   // undo Y compression
                void SetIsoTransform(float32 m00, float32 m01, float32 m10, float32 m11);
                void SetIsoTransformIdentity() { SetIsoTransform(1.f, 0.f, 0.f, 1.f); }

                // ── Phase E Materials 2D : Glow sprite ─────────────────────────────────
                // SetGlowParams() configure la couleur + intensite + power du
                // halo radial (rim) ajoute en additive sur les DrawSpriteGlow
                // suivants. Appliques jusqu'au prochain SetGlowParams.
                //   color     : couleur RGB du halo (multiplie par intensity)
                //   intensity : 0..4+ (combien le halo "brille")
                //   power     : 0.5..8 (concentration au bord ; haut = bord fin)
                void SetGlowParams(NkVec3f color, float32 intensity = 1.f,
                                    float32 power = 3.f);
                // Dessine un sprite avec le pipeline Glow2D (1 quad, pas batche
                // — donc plus couteux que DrawSprite ; reserver aux objets clefs
                // comme power-ups / projectiles / UI accent). Le tint multiplie
                // la texture comme un DrawSprite standard.
                void DrawSpriteGlow(NkRectF dst, NkTexHandle tex,
                                     NkVec4f tint = {1,1,1,1},
                                     NkRectF uv  = {0,0,1,1});

                // ── Sprites ────────────────────────────────────────────────────────────
                void DrawSprite(NkRectF dst, NkTexHandle tex, NkVec4f tint={1,1,1,1}, NkRectF uv={0,0,1,1});
                void DrawSpriteRotated(NkRectF dst, NkTexHandle tex, float32 angleDeg, NkVec2f pivot={0.5f,0.5f}, NkVec4f tint={1,1,1,1}, NkRectF uv={0,0,1,1});
                void DrawNineSlice(NkRectF dst, NkTexHandle tex, float32 left, float32 top, float32 right, float32 bottom, NkVec4f tint={1,1,1,1});

                // ── Formes ────────────────────────────────────────────────────────────
                void FillRect      (NkRectF r,   NkVec4f color);
                void FillRectGradH (NkRectF r,   NkVec4f left, NkVec4f right);
                void FillRectGradV (NkRectF r,   NkVec4f top,  NkVec4f bottom);
                void FillRoundRect (NkRectF r,   NkVec4f color, float32 radius);
                void FillCircle    (NkVec2f c,   float32 radius, NkVec4f color, uint32 segs=32);
                void FillTriangle  (NkVec2f a, NkVec2f b, NkVec2f c, NkVec4f color);
                void DrawRect      (NkRectF r,   NkVec4f color, float32 thick=1.f);
                void DrawRoundRect (NkRectF r,   NkVec4f color, float32 radius, float32 thick=1.f);
                void DrawCircle    (NkVec2f c,   float32 radius, NkVec4f color, float32 thick=1.f, uint32 segs=32);
                void DrawLine      (NkVec2f a, NkVec2f b, NkVec4f color, float32 thick=1.f);
                void DrawArc       (NkVec2f c, float32 r, float32 a0, float32 a1, NkVec4f color, float32 thick=1.f);
                void DrawPolyline  (const NkVec2f* pts, uint32 n, NkVec4f color, float32 thick=1.f, bool closed=false);
                void DrawBezier    (NkVec2f p0, NkVec2f p1, NkVec2f p2, NkVec2f p3,
                                    NkVec4f color, float32 thick=1.f, uint32 segs=32);

                // ── Image générale ────────────────────────────────────────────────────
                void DrawImage(NkTexHandle tex, NkRectF dst, NkVec4f tint={1,1,1,1});
                void DrawImage(NkTexHandle tex, NkRectF dst, NkRectF src, NkVec4f tint={1,1,1,1});

                // ── Clip regions ──────────────────────────────────────────────────────
                void PushClip(NkRectF rect);
                void PopClip();

                // ── Blend / Layer ─────────────────────────────────────────────────────
                void SetBlendMode(NkBlendMode mode);
                void SetLayer(uint8 layer);

                // ── Stats ─────────────────────────────────────────────────────────────
                uint32 GetBatchCount()  const { return mBatchCount; }
                uint32 GetVertexCount() const { return mVertCount; }

            private:
                struct Vert2D {
                    NkVec2f pos; NkVec2f uv; uint32 color; uint32 texIdx;
                };
                struct Batch {
                    NkTexHandle tex; NkBlendMode blend; uint8 layer;
                    uint32 vStart; uint32 vCount;
                };

                NkIDevice*        mDevice   = nullptr;
                NkTextureLibrary* mTexLib   = nullptr;
                NkICommandBuffer* mCmd      = nullptr;
                NkVector<Vert2D>  mVerts;
                NkVector<Batch>   mBatches;
                NkVector<NkRectF> mClipStack;
                NkBufferHandle    mVBO, mIBO;
                NkBufferHandle    mUBOLights2D;       // Phase E : lights + ambient
                NkBufferHandle    mUBOShadows2D;      // Phase E.5 : circle shadow casters
                NkBufferHandle    mUBOShadowsAABB2D;  // Phase E.7a : AABB shadow casters
                NkPipelineHandle  mPipeAlpha, mPipeAdd, mPipeOpaque;
                // Phase E Materials 2D — pipeline Glow + buffers dedies pour 1 quad.
                NkPipelineHandle  mPipeGlow;
                NkBufferHandle    mVBOGlow;           // 4 vertices Vert2D
                NkBufferHandle    mIBOGlow;           // 6 indices uint16
                NkVec4f           mGlowColor   = {1.f, 0.5f, 0.1f, 1.f};
                NkVec4f           mGlowParams  = {3.f, 1.f, 0.f, 0.f}; // x=power
                NkDescSetHandle   mTexLayout, mTexSet;
                NkSamplerHandle   mLinearSampler;
                bool              mLitMode = false;
                bool              mNormalMode = false;
                uint32            mLayerMask = 0xFFu;     // layers actives par defaut
                float32           mIso[4]    = {1.f, 0.f, 0.f, 1.f}; // E.7d cache CPU
                NkBlendMode       mBlend   = NkBlendMode::NK_ALPHA;
                uint8             mLayer   = 0;
                bool              mInFrame = false;
                uint32            mW=0, mH=0;
                NkMat4f           mOrtho;
                uint32            mBatchCount=0, mVertCount=0;

                uint32 PackColor(NkVec4f c) {
                    uint8 r=(uint8)(c.x*255); uint8 g=(uint8)(c.y*255);
                    uint8 b=(uint8)(c.z*255); uint8 a=(uint8)(c.w*255);
                    return ((uint32)a<<24)|((uint32)b<<16)|((uint32)g<<8)|r;
                }
                void Flush();
                void PushQuad(NkVec2f tl,NkVec2f tr,NkVec2f br,NkVec2f bl,
                            NkVec2f uvTL,NkVec2f uvTR,NkVec2f uvBR,NkVec2f uvBL,
                            NkVec4f color, NkTexHandle tex);
        };

    } // namespace renderer
} // namespace nkentseu
