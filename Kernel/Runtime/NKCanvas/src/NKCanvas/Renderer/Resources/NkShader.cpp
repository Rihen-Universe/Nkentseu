// =============================================================================
// NkShader.cpp — Implementation user-facing (delegue au dispatch backend).
// =============================================================================

#include "NkShader.h"
#include "NkShaderBackend.h"
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"
#include "NKCanvas/Renderer/Core/NkTransform.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"

namespace nkentseu {
    namespace renderer {

        // Active backend dispatch table (set par chaque backend renderer en fin
        // d'Initialize via NkShaderSetBackend). Vide par defaut : tant qu'aucun
        // renderer n'a initialise, NkShader::Compile retournera false et toutes
        // les operations sont des no-ops surs.
        static NkShaderBackend gShaderBackend{};

        void NkShaderSetBackend(const NkShaderBackend& backend) {
            gShaderBackend = backend;
        }

        // ── Stub : Create renvoie 0, le reste est no-op. Le log nominatif n'est
        //          emis qu'une fois par backend pour eviter de polluer la console.
        namespace {
            // Helper de log unique. fprintf utilise pour eviter une dep sur NKLogger
            // (le logger global n'est pas force initialise au moment du Initialize
            // backend ; et NKLog macros prennent un logger objet, pas une categorie).
        }
        static uint32 sUnsupportedShaderCreate(const NkShaderSources&) {
            return 0; // signal a NkShader::Compile que c'est non supporte
        }

        void NkShaderInstallUnsupportedBackend(const char* backendName) {
            (void)backendName; // reserve pour un log futur via NKLogger global
            NkShaderBackend stub{};
            stub.Create = &sUnsupportedShaderCreate;
            // Tous les autres callbacks restent nullptr → NkShader::SetXxx no-op
            // safe (verifies par if(gShaderBackend.SetXxx) dans NkShader.cpp).
            NkShaderSetBackend(stub);
        }

        // ── Move semantics ─────────────────────────────────────────────────────

        NkShader::NkShader(NkShader&& o) noexcept
            : mGLSLVert(o.mGLSLVert), mGLSLFrag(o.mGLSLFrag)
            , mHLSLVert(o.mHLSLVert), mHLSLFrag(o.mHLSLFrag)
            , mMSLVert(o.mMSLVert),   mMSLFrag(o.mMSLFrag)
            , mSPIRVVert(o.mSPIRVVert), mSPIRVVertWords(o.mSPIRVVertWords)
            , mSPIRVFrag(o.mSPIRVFrag), mSPIRVFragWords(o.mSPIRVFragWords)
            , mGPUId(o.mGPUId)
        {
            o.mGPUId = 0;
        }

        NkShader& NkShader::operator=(NkShader&& o) noexcept {
            if (this != &o) {
                Destroy();
                mGLSLVert = o.mGLSLVert; mGLSLFrag = o.mGLSLFrag;
                mHLSLVert = o.mHLSLVert; mHLSLFrag = o.mHLSLFrag;
                mMSLVert  = o.mMSLVert;  mMSLFrag  = o.mMSLFrag;
                mSPIRVVert = o.mSPIRVVert; mSPIRVVertWords = o.mSPIRVVertWords;
                mSPIRVFrag = o.mSPIRVFrag; mSPIRVFragWords = o.mSPIRVFragWords;
                mGPUId = o.mGPUId;
                o.mGPUId = 0;
            }
            return *this;
        }

        // ── Sources ────────────────────────────────────────────────────────────

        void NkShader::SetSourceGLSL(const char* v, const char* f) { mGLSLVert = v; mGLSLFrag = f; }
        void NkShader::SetSourceHLSL(const char* v, const char* f) { mHLSLVert = v; mHLSLFrag = f; }
        void NkShader::SetSourceMSL (const char* v, const char* f) { mMSLVert  = v; mMSLFrag  = f; }
        void NkShader::SetSourceSPIRV(const uint32* vc, usize vw, const uint32* fc, usize fw) {
            mSPIRVVert = vc; mSPIRVVertWords = vw;
            mSPIRVFrag = fc; mSPIRVFragWords = fw;
        }

        // ── Compile / Destroy ──────────────────────────────────────────────────

        bool NkShader::Compile(NkIRenderer2D& /*renderer*/) {
            if (!gShaderBackend.Create) return false; // backend pas encore initialise
            Destroy();                                 // libere precedent eventuel
            NkShaderSources src{};
            src.glslVert  = mGLSLVert;  src.glslFrag  = mGLSLFrag;
            src.hlslVert  = mHLSLVert;  src.hlslFrag  = mHLSLFrag;
            src.mslVert   = mMSLVert;   src.mslFrag   = mMSLFrag;
            src.spirvVert = mSPIRVVert; src.spirvVertWords = mSPIRVVertWords;
            src.spirvFrag = mSPIRVFrag; src.spirvFragWords = mSPIRVFragWords;
            mGPUId = gShaderBackend.Create(src);
            return mGPUId != 0;
        }

        void NkShader::Destroy() {
            if (mGPUId != 0 && gShaderBackend.Destroy) {
                gShaderBackend.Destroy(mGPUId);
            }
            mGPUId = 0;
        }

        // ── Uniformes (no-op si backend null ou shader pas compile) ────────────

        void NkShader::SetFloat(const char* name, float32 v) {
            if (mGPUId && gShaderBackend.SetFloat) gShaderBackend.SetFloat(mGPUId, name, v);
        }
        void NkShader::SetVec2(const char* name, NkVec2f v) {
            if (mGPUId && gShaderBackend.SetVec2) gShaderBackend.SetVec2(mGPUId, name, v.x, v.y);
        }
        void NkShader::SetVec3(const char* name, float32 x, float32 y, float32 z) {
            if (mGPUId && gShaderBackend.SetVec3) gShaderBackend.SetVec3(mGPUId, name, x, y, z);
        }
        void NkShader::SetVec4(const char* name, float32 x, float32 y, float32 z, float32 w) {
            if (mGPUId && gShaderBackend.SetVec4) gShaderBackend.SetVec4(mGPUId, name, x, y, z, w);
        }
        void NkShader::SetColor(const char* name, const NkColor2D& c) {
            const float32 inv = 1.f / 255.f;
            SetVec4(name, c.r * inv, c.g * inv, c.b * inv, c.a * inv);
        }
        void NkShader::SetMat4(const char* name, const NkTransform& t) {
            SetMat4(name, t.GetMatrix());
        }
        void NkShader::SetMat4(const char* name, const float32* mat16) {
            if (mGPUId && gShaderBackend.SetMat4) gShaderBackend.SetMat4(mGPUId, name, mat16);
        }
        void NkShader::SetTexture(const char* name, const NkTexture* tex, uint32 slot) {
            if (!mGPUId || !gShaderBackend.SetTexture) return;
            const uint32 texId = tex ? tex->GetGPUId() : 0;
            gShaderBackend.SetTexture(mGPUId, name, texId, slot);
        }

    } // namespace renderer
} // namespace nkentseu
