#pragma once
// =============================================================================
// GLRenderer2D.h — Renderer 2D OpenGL portable (GL 3.3 Core / GLES 3.0)
//
// Primitives :
//   - DrawQuad(x, y, w, h, color)              — rectangle plein color
//   - DrawQuadOutline(x, y, w, h, color, thick)— rectangle contour
//   - DrawLine(x1, y1, x2, y2, color, thick)   — ligne
//   - DrawCircle(cx, cy, r, color, segs)       — disque plein
//   - DrawCircleOutline(cx, cy, r, color, thick, segs)
//   - DrawTexturedQuad(x, y, w, h, tex, uv0, uv1, color, modulate)
//                                              — quad textured (glyphes, sprites)
//
// Batching : tous les draws s'accumulent dans un vertex buffer dynamique,
// Flush automatique au changement de shader/texture ou à End().
//
// Coordonnees ecran : (0,0) top-left, +X droite, +Y bas.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"
#include "NKMath/NkVec.h"
#include "NKMath/NkColor.h"

namespace nkentseu { namespace songoo {

    // Vertex : pos(2) + uv(2) + color(4) + textureFlag(1) = 9 floats
    struct GLVertex {
        float x, y;
        float u, v;
        float r, g, b, a;
        float useTex;  // 0 = color only, 1 = sample texture (alpha mask), modulated by color
    };

    class GLRenderer2D {
    public:
        GLRenderer2D()  = default;
        ~GLRenderer2D() = default;

        bool Init();
        void Shutdown();

        // Frame
        void Begin(int viewportW, int viewportH);
        void End();
        void Flush();   // force submit du batch courant

        // State
        void SetClip(int x, int y, int w, int h);   // scissor (en coordonnees ecran top-left)
        void ClearClip();
        void Clear(float r, float g, float b, float a = 1.0f);

        // Primitives 2D
        void DrawQuad(float x, float y, float w, float h, math::NkColor c);
        void DrawQuadOutline(float x, float y, float w, float h, math::NkColor c, float thickness = 1.0f);
        void DrawLine(float x1, float y1, float x2, float y2, math::NkColor c, float thickness = 1.0f);
        void DrawCircle(float cx, float cy, float r, math::NkColor c, int segments = 48);
        void DrawCircleOutline(float cx, float cy, float r, math::NkColor c, float thickness = 1.0f, int segments = 48);
        void DrawTriangle(float x0, float y0, float x1, float y1, float x2, float y2, math::NkColor c);

        // Texturé : la texture doit être bindée AVANT (via BindTexture).
        // Le shader multiplie c.rgb par color.rgb et c.a par texture.r (atlas alpha-coverage).
        void BindTexture(uint32 textureId);
        void DrawTexturedQuad(float x, float y, float w, float h,
                              float u0, float v0, float u1, float v1,
                              math::NkColor color);
        // Variante RGBA : la texture est echantillonnee en RGBA et modulee
        // (multiplication par color.rgba). Pour les logos et sprites couleur.
        void DrawTexturedQuadRGBA(float x, float y, float w, float h,
                                  float u0, float v0, float u1, float v1,
                                  math::NkColor color);

        // Info
        int ViewportW() const noexcept { return mViewportW; }
        int ViewportH() const noexcept { return mViewportH; }

    private:
        // OpenGL handles
        uint32 mProgram     = 0;
        uint32 mVAO         = 0;
        uint32 mVBO         = 0;
        uint32 mEBO         = 0;
        int    mLocProj     = -1;
        int    mLocTex      = -1;
        uint32 mWhiteTex    = 0;       // 1x1 blanc pour les draws non-texturés
        uint32 mCurrentTex  = 0;

        // Batching
        static constexpr int kMaxQuads = 4096;
        GLVertex* mVertices = nullptr;
        uint16*   mIndices  = nullptr;
        int       mQuadCount = 0;

        // Viewport / projection
        int   mViewportW = 0;
        int   mViewportH = 0;
        float mProj[16];

        // Scissor
        bool  mScissorEnabled = false;

        // Helpers
        uint32 CompileShader(const char* vsSrc, const char* fsSrc);
        void   FlushBatch();
        void   PushQuad(float x, float y, float w, float h,
                        float u0, float v0, float u1, float v1,
                        math::NkColor c, float useTex);
        void   EnsureRoom(int quadsNeeded);
    };

}} // namespace nkentseu::pong
