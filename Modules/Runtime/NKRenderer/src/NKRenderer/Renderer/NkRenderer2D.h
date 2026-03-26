#pragma once
// =============================================================================
// NkRenderer2D.h
// Moteur de rendu 2D du NKRenderer.
//
// Fonctionnalités (comme SFML, mais avec le RHI NkIDevice) :
//   • Sprites (texture + couleur + transform)
//   • Formes libres : triangle, quadrilatère, cercle, ellipse, arc, polygone
//   • Rounded rectangles, lignes épaisses, flèches, croix
//   • Texte 2D (rasterisé via FreeType / atlas de glyphes)
//   • Texture atlas pour animation de sprites
//   • Batch rendering automatique (minimize les draw calls)
//   • Layers de profondeur (z-order)
//   • Camera 2D avec zoom / pan
//   • Clipping / scissor région
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Core/NkTexture.h"
#include "NKRenderer/Mesh/NkMesh.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Transform 2D
        // =============================================================================
        struct NkTransform2D {
            NkVec2f position  = {0,0};
            float   rotation  = 0.f;   // radians
            NkVec2f scale     = {1,1};
            NkVec2f origin    = {0,0}; // pivot (0,0=coin haut-gauche, 0.5,0.5=centre)

            NkMat4f ToMatrix() const;
            static NkTransform2D Identity() { return {}; }
        };

        // =============================================================================
        // Camera 2D
        // =============================================================================
        struct NkCamera2D {
            NkVec2f center   = {0,0};
            float   rotation = 0.f;
            float   zoom     = 1.f;

            NkMat4f GetViewProjection(float viewWidth, float viewHeight) const;
            void    Pan(const NkVec2f& delta)   { center = center + delta; }
            void    Zoom(float factor)          { zoom *= factor; zoom = NkClamp(zoom, 0.01f, 100.f); }
            void    Reset()                     { center = {0,0}; rotation = 0.f; zoom = 1.f; }
            NkVec2f WorldToScreen(const NkVec2f& p, float vw, float vh) const;
            NkVec2f ScreenToWorld(const NkVec2f& p, float vw, float vh) const;
        };

        // =============================================================================
        // Sprite — représentation d'un objet 2D texturé
        // =============================================================================
        struct NkSprite {
            NkTexture2D*    texture      = nullptr;
            NkAtlasRegion*  atlasRegion  = nullptr; // si sprite sheet
            NkVec2f         position     = {0,0};
            NkVec2f         size         = {1,1};   // taille world (pixels ou unités)
            float           rotation     = 0.f;
            NkVec2f         scale        = {1,1};
            NkVec2f         origin       = {0.5f,0.5f}; // pivot (0..1)
            NkColor4f       color        = {1,1,1,1};
            float           depth        = 0.f;     // z-order [0..1]
            bool            flipX        = false;
            bool            flipY        = false;
            NkVec4f         uvRect       = {0,0,1,1}; // sub-rect UV (x,y,w,h)
            NkBlendMode     blendMode    = NkBlendMode::Translucent;
            bool            visible      = true;
        };

        // =============================================================================
        // Animation de sprite (sprite sheet)
        // =============================================================================
        struct NkSpriteAnimation {
            NkString        name;
            NkVector<NkVec4f> frames;   // uvRect par frame
            float           fps         = 24.f;
            bool            loop        = true;
            bool            pingPong    = false;
        };

        class NkSpriteAnimator {
            public:
                void SetAnimation(const NkSpriteAnimation* anim);
                void Update(float dt);
                void Play()      { mPlaying = true; }
                void Pause()     { mPlaying = false; }
                void Stop()      { mPlaying = false; mTime = 0; mFrame = 0; }
                uint32     GetCurrentFrame() const { return mFrame; }
                NkVec4f    GetCurrentUV()    const;
                bool       IsFinished()      const { return !mAnim || (!mAnim->loop && mFrame >= (uint32)mAnim->frames.Size()-1); }

            private:
                const NkSpriteAnimation* mAnim    = nullptr;
                float                    mTime    = 0.f;
                uint32                   mFrame   = 0;
                bool                     mPlaying = false;
                bool                     mForward = true;
        };

        // =============================================================================
        // Paramètres de forme 2D
        // =============================================================================
        struct NkShapeStyle {
            NkColor4f fillColor   = {1,1,1,1};
            NkColor4f strokeColor = {0,0,0,1};
            float     strokeWidth = 1.f;
            bool      filled      = true;
            bool      stroked     = false;
            float     depth       = 0.f;
            NkBlendMode blendMode = NkBlendMode::Translucent;
        };

        // =============================================================================
        // Vertex 2D batch interne (position, uv, color)
        // =============================================================================
        struct NkBatchVert {
            float  x, y;      // position NDC ou world
            float  u, v;      // UV
            float  r, g, b, a;// couleur
        };

        // =============================================================================
        // Batch de rendu 2D
        // Un batch = une texture + un blendmode + des vertices
        // =============================================================================
        struct NkBatch2D {
            NkTextureHandle  textureHandle;
            NkSamplerHandle  samplerHandle;
            NkBlendMode      blendMode   = NkBlendMode::Translucent;
            uint32           firstVertex = 0;
            uint32           vertexCount = 0;
            uint32           firstIndex  = 0;
            uint32           indexCount  = 0;
            float            minDepth    = 0.f;
            float            maxDepth    = 1.f;
        };

        // =============================================================================
        // NkRenderer2D — moteur 2D principal
        // =============================================================================
        class NkRenderer2D {
            public:
                static constexpr uint32 MAX_VERTICES = 65536 * 4;
                static constexpr uint32 MAX_INDICES  = 65536 * 6;

                NkRenderer2D()  = default;
                ~NkRenderer2D() { Shutdown(); }
                NkRenderer2D(const NkRenderer2D&) = delete;
                NkRenderer2D& operator=(const NkRenderer2D&) = delete;

                // ── Cycle de vie ──────────────────────────────────────────────────────────
                bool Init(NkIDevice* device, NkRenderPassHandle renderPass,
                        uint32 width, uint32 height);
                void Shutdown();
                void Resize(uint32 width, uint32 height);

                // ── Frame ─────────────────────────────────────────────────────────────────
                void Begin(NkICommandBuffer* cmd,
                        const NkCamera2D& camera = NkCamera2D{});
                void End();   // Flush + submit toutes les batches

                // ── Camera ────────────────────────────────────────────────────────────────
                void SetCamera(const NkCamera2D& cam) { mCamera = cam; }
                const NkCamera2D& GetCamera() const   { return mCamera; }

                // ── Clipping ──────────────────────────────────────────────────────────────
                void PushClipRect(float x, float y, float w, float h);
                void PopClipRect();
                void ClearClipRect();

                // ── Sprites ───────────────────────────────────────────────────────────────
                void DrawSprite(const NkSprite& sprite);

                void DrawTexture(NkTexture2D* tex, float x, float y,
                                float w = 0, float h = 0, // 0,0 = taille naturelle
                                const NkColor4f& tint = NkColor4f::White(),
                                float rotation = 0.f, float depth = 0.f);

                void DrawSubTexture(NkTexture2D* tex,
                                    float x, float y, float w, float h,
                                    float srcX, float srcY, float srcW, float srcH,
                                    const NkColor4f& tint = NkColor4f::White(),
                                    float rotation = 0.f);

                // Atlas region
                void DrawAtlasRegion(NkTextureAtlas* atlas, const NkString& regionName,
                                    float x, float y, float w = 0, float h = 0,
                                    const NkColor4f& tint = NkColor4f::White(),
                                    float rotation = 0.f);

                // ── Formes géométriques ───────────────────────────────────────────────────

                // Ligne
                void DrawLine(float x0, float y0, float x1, float y1,
                            const NkColor4f& color = NkColor4f::White(),
                            float thickness = 1.f, float depth = 0.f);

                void DrawLine(NkVec2f a, NkVec2f b, const NkColor4f& color = NkColor4f::White(),
                            float thickness = 1.f);

                // Polyline
                void DrawPolyline(const NkVec2f* points, uint32 count,
                                const NkColor4f& color, float thickness = 1.f,
                                bool closed = false);

                // Flèche
                void DrawArrow(NkVec2f from, NkVec2f to,
                            const NkColor4f& color, float thickness = 1.f,
                            float headSize = 10.f);

                // Rectangle
                void DrawRect(float x, float y, float w, float h,
                            const NkShapeStyle& style = NkShapeStyle{},
                            float rotation = 0.f, NkVec2f origin = {0.5f,0.5f});
                void DrawRect(NkVec2f pos, NkVec2f size, const NkColor4f& fill,
                            float depth = 0.f);

                // Rounded rectangle
                void DrawRoundedRect(float x, float y, float w, float h, float radius,
                                    const NkShapeStyle& style = NkShapeStyle{},
                                    uint32 segments = 8);

                // Cercle
                void DrawCircle(float cx, float cy, float radius,
                                const NkShapeStyle& style = NkShapeStyle{},
                                uint32 segments = 32);
                void DrawCircle(NkVec2f center, float radius,
                                const NkColor4f& fill = NkColor4f::White(),
                                const NkColor4f& stroke = NkColor4f::Black(),
                                float strokeWidth = 1.f);

                // Disque rempli (alias DrawCircle avec fill only)
                void DrawDisc(float cx, float cy, float radius,
                            const NkColor4f& color = NkColor4f::White(),
                            uint32 segments = 32);

                // Arc
                void DrawArc(float cx, float cy, float radius,
                            float startAngleDeg, float endAngleDeg,
                            const NkColor4f& color, float thickness = 1.f,
                            uint32 segments = 32);

                // Ellipse
                void DrawEllipse(float cx, float cy, float rx, float ry,
                                const NkShapeStyle& style = NkShapeStyle{},
                                uint32 segments = 32);

                // Triangle
                void DrawTriangle(NkVec2f a, NkVec2f b, NkVec2f c,
                                const NkShapeStyle& style = NkShapeStyle{});

                void DrawTriangleFilled(NkVec2f a, NkVec2f b, NkVec2f c,
                                        const NkColor4f& colorA, const NkColor4f& colorB,
                                        const NkColor4f& colorC);

                // Polygone convexe
                void DrawPolygon(const NkVec2f* points, uint32 count,
                                const NkShapeStyle& style = NkShapeStyle{});

                // Polygone quelconque avec triangulation (ear-clipping)
                void DrawPolygonFilled(const NkVec2f* points, uint32 count,
                                        const NkColor4f& color = NkColor4f::White());

                // Quadrilatère libre
                void DrawQuad(NkVec2f a, NkVec2f b, NkVec2f c, NkVec2f d,
                            const NkColor4f& color = NkColor4f::White(),
                            NkTexture2D* tex = nullptr);

                // Bezier cubique
                void DrawBezier(NkVec2f p0, NkVec2f p1, NkVec2f p2, NkVec2f p3,
                                const NkColor4f& color = NkColor4f::White(),
                                float thickness = 1.f, uint32 segments = 32);

                // Pie (secteur de cercle rempli)
                void DrawPie(float cx, float cy, float radius,
                            float startAngleDeg, float endAngleDeg,
                            const NkColor4f& color = NkColor4f::White(),
                            uint32 segments = 32);

                // Gradient linéaire
                void DrawGradientRect(float x, float y, float w, float h,
                                    NkColor4f top, NkColor4f bottom,
                                    float depth = 0.f);

                void DrawGradientRectH(float x, float y, float w, float h,
                                        NkColor4f left, NkColor4f right,
                                        float depth = 0.f);

                // Point / pixel
                void DrawPoint(float x, float y, const NkColor4f& color = NkColor4f::White(),
                            float size = 4.f);

                // ── Statistiques ──────────────────────────────────────────────────────────
                const NkRendererStats& GetStats() const { return mStats; }

            private:
                // ── Device ────────────────────────────────────────────────────────────────
                NkIDevice*        mDevice      = nullptr;
                NkICommandBuffer* mCmd         = nullptr;
                uint32            mWidth       = 0;
                uint32            mHeight      = 0;
                bool              mInFrame     = false;

                // ── Camera ────────────────────────────────────────────────────────────────
                NkCamera2D        mCamera;

                // ── Batch CPU buffer ──────────────────────────────────────────────────────
                NkVector<NkBatchVert> mVertexBuffer;
                NkVector<uint32>      mIndexBuffer;
                NkVector<NkBatch2D>   mBatches;
                NkBatch2D*            mCurrentBatch = nullptr;

                // ── GPU buffers ───────────────────────────────────────────────────────────
                NkBufferHandle    mVBO;
                NkBufferHandle    mIBO;
                NkBufferHandle    mUBO;   // viewProj matrix

                // ── Pipelines ─────────────────────────────────────────────────────────────
                NkPipelineHandle  mSpritePipeline;        // alpha blend
                NkPipelineHandle  mShapeOpaquePipeline;   // opaque
                NkPipelineHandle  mShapeAlphaPipeline;    // alpha blend
                NkPipelineHandle  mShapeAdditivePipeline; // additive
                NkRenderPassHandle mRenderPass;

                // ── Textures built-in ─────────────────────────────────────────────────────
                NkTexture2D*      mWhiteTex  = nullptr;
                NkTexture2D*      mCircleTex = nullptr; // disque lisse dans texture 2D

                // ── Shaders ───────────────────────────────────────────────────────────────
                NkShaderHandle    mSpriteShader;
                NkShaderHandle    mShapeShader;

                // ── Descriptor ────────────────────────────────────────────────────────────
                NkDescSetHandle   mLayout;
                NkDescSetHandle   mDescSet;
                NkUnorderedMap<uint64, NkDescSetHandle> mTexDescSets; // cache par texture

                // ── Clip rect stack ───────────────────────────────────────────────────────
                struct ClipRect { float x,y,w,h; };
                NkVector<ClipRect> mClipStack;
                bool               mScissorDirty = false;

                // ── Stats ─────────────────────────────────────────────────────────────────
                NkRendererStats   mStats;

                // ── Helpers ───────────────────────────────────────────────────────────────
                bool InitShaders();
                bool InitPipelines();
                bool InitBuffers();
                bool InitDescriptors();
                bool InitBuiltinTextures();

                void FlushBatch();
                void StartNewBatch(NkTextureHandle tex, NkSamplerHandle samp, NkBlendMode blend, float depth);
                void EnsureBatch(NkTextureHandle tex, NkSamplerHandle samp, NkBlendMode blend, float depth);
                void EnsureWhiteBatch(NkBlendMode blend = NkBlendMode::Translucent, float depth = 0.f);

                void PushQuadVerts(NkVec2f p0, NkVec2f p1, NkVec2f p2, NkVec2f p3,
                                NkVec2f uv0, NkVec2f uv1, NkVec2f uv2, NkVec2f uv3,
                                NkColor4f c0, NkColor4f c1, NkColor4f c2, NkColor4f c3);
                void PushTriVerts(NkVec2f a, NkVec2f b, NkVec2f c,
                                NkVec2f ua, NkVec2f ub, NkVec2f uc,
                                NkColor4f ca, NkColor4f cb, NkColor4f cc);

                NkDescSetHandle GetOrCreateTexDescSet(NkTexture2D* tex);
                NkMat4f         GetViewProjection() const;
                void            ApplyClipScissor();

                // Triangulation ear-clipping pour polygones concaves
                static bool     TriangulatePolygon(const NkVec2f* pts, uint32 cnt,
                                                    NkVector<uint32>& indices);
        };

        // =============================================================================
        // NkTransform2D implementation
        // =============================================================================
        inline NkMat4f NkTransform2D::ToMatrix() const {
            float c = NkCos(rotation);
            float s = NkSin(rotation);
            // TRS avec pivot : T(pos) * R * S * T(-origin*size)
            NkMat4f mat = NkMat4f::Identity();
            mat.data[0] = c * scale.x;
            mat.data[1] = s * scale.x;
            mat.data[4] =-s * scale.y;
            mat.data[5] = c * scale.y;
            mat.data[12]= position.x - (c*origin.x*scale.x - s*origin.y*scale.y);
            mat.data[13]= position.y - (s*origin.x*scale.x + c*origin.y*scale.y);
            return mat;
        }

        inline NkMat4f NkCamera2D::GetViewProjection(float vw, float vh) const {
            float hw = (vw * 0.5f) / zoom;
            float hh = (vh * 0.5f) / zoom;
            NkMat4f proj = NkMat4f::Orthogonal(
                NkVec2f(-hw + center.x, -hh + center.y),
                NkVec2f( hw + center.x,  hh + center.y),
                -1.f, 1.f, true);
            NkMat4f rot = NkMat4f::RotationZ(NkAngle(rotation * 180.f / (float)NkPi));
            return proj * rot;
        }

    } // namespace renderer
} // namespace nkentseu
