#pragma once
// =============================================================================
// NkOpenGLRenderer2D.h — OpenGL 2D renderer backend
// Extends NkBatchRenderer2D; implements SubmitBatches + UploadProjection.
// Requires OpenGL 3.3 core or GLES 3.0.
// =============================================================================
#include "NKCanvas/Renderer/Batch/NkBatchRenderer2D.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"

namespace nkentseu {
    namespace renderer {

        class NkOpenGLRenderer2D final : public NkBatchRenderer2D {
            public:
                NkOpenGLRenderer2D()  = default;
                ~NkOpenGLRenderer2D() override { if (IsValid()) Shutdown(); }

                // NkIRenderer2D
                bool Initialize(NkIGraphicsContext* ctx) override;
                void Shutdown()                          override;
                bool IsValid()                   const   override { return mIsValid; }
                void Clear(const NkColor2D& col)         override;

                // GPU texture ops (called by NkTexture)
                static uint32 CreateGLTexture(uint32 w, uint32 h, const uint8* rgba);
                static void   UpdateGLTexture(uint32 id, uint32 x, uint32 y, uint32 w, uint32 h, const uint8* rgba);
                static void   DeleteGLTexture(uint32 id);
                static void   SetGLTextureFilter(uint32 id, NkTextureFilter filter);
                static void   SetGLTextureWrap  (uint32 id, NkTextureWrap   wrap);

                // GPU shader ops (called by NkShader via dispatch). GLSL vert+frag.
                struct NkShaderSources_OpaqueFwd; // pour eviter d'inclure NkShaderBackend.h ici
                static uint32 CreateGLShader (const struct NkShaderSources& sources);
                static void   DestroyGLShader(uint32 id);
                static void   UseGLShader    (uint32 id);
                static void   SetGLShaderFloat  (uint32 id, const char* name, float v);
                static void   SetGLShaderVec2   (uint32 id, const char* name, float x, float y);
                static void   SetGLShaderVec3   (uint32 id, const char* name, float x, float y, float z);
                static void   SetGLShaderVec4   (uint32 id, const char* name, float x, float y, float z, float w);
                static void   SetGLShaderMat4   (uint32 id, const char* name, const float* mat16);
                static void   SetGLShaderTexture(uint32 id, const char* name, uint32 texGPUId, uint32 slot);

                // GPU render-texture ops (FBO + color attachment 2D RGBA8).
                static uint32 CreateGLRenderTexture(uint32 w, uint32 h);
                static void   DestroyGLRenderTexture(uint32 handle);
                static void   BindGLRenderTexture(uint32 handle);
                static void   UnbindGLRenderTexture();
                static uint32 GetGLRenderTextureColorId(uint32 handle);

            protected:
                void BeginBackend()  override;
                void EndBackend()    override;
                void SubmitBatches(const NkBatchGroup* groups, uint32 groupCount,
                                const NkVertex2D* verts, uint32 vCount,
                                const uint32*     idx,   uint32 iCount) override;
                void UploadProjection(const float32 proj[16]) override;
                void ApplyScissor(bool enabled, const NkRect2i& rect) override;

            private:
                bool CompileShader();
                void SetupVAO();
                void ApplyBlendMode(NkBlendMode mode);
                void BindTexture(const NkTexture* tex);

                NkIGraphicsContext* mCtx      = nullptr;
                bool                mIsValid  = false;

                // GL objects (stored as uint32 to avoid GL headers in this header)
                uint32 mProgram   = 0;
                uint32 mVAO       = 0;
                uint32 mVBO       = 0;
                uint32 mEBO       = 0;
                int32  mUniProj   = -1;   // uniform location: u_Projection
                int32  mUniTex    = -1;   // uniform location: u_Texture

                uint32 mLastBoundTexId = 0;
                NkBlendMode mLastBlend = NkBlendMode::NK_NONE;

                // 1x1 white texture used for untextured geometry
                uint32 mWhiteTexId = 0;
        };

    } // namespace renderer
} // namespace nkentseu