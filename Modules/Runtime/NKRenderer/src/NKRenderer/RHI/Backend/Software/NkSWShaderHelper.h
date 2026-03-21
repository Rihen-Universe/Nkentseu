#pragma once
// =============================================================================
// NkSWShaderHelper.h
// Utilitaires pour créer des shaders CPU dans le backend Software.
//
// Interface claire entre NkSLCodeGen_CPP (qui génère des fonctions C++) et
// NkSoftwareDevice::CreateShader (qui les consomme).
//
// Usage :
//   NkSWShaderBuilder builder;
//   builder.SetVertexFn([](const void* vdata, uint32 idx, const void* ubo) {
//       const MyVertex* v = (const MyVertex*)vdata + idx;
//       const MyUBO*    u = (const MyUBO*)ubo;
//       NkSWVertex out;
//       NkVec4f wp = u->model * NkVec4f(v->pos, 1.f);
//       out.position = u->proj * u->view * wp;
//       out.color    = NkVec4f(v->color, 1.f);
//       return out;
//   });
//   builder.SetFragmentFn([](const NkSWVertex& v, const void* ubo, const void* tex) {
//       return v.color;
//   });
//   NkShaderDesc desc = builder.Build();
//   NkShaderHandle h = device->CreateShader(desc);
// =============================================================================
#include "NkSoftwareTypes.h"
#include "NKRenderer/RHI/Core/NkIDevice.h"

namespace nkentseu {
class NkSoftwareDevice;  // forward declaration

// =============================================================================
// NkSWShaderContext — contexte complet passé aux shaders (à caster depuis uniformData)
// Permet d'accéder aux pushConstants et à l'instanceIndex depuis un shader CPU
// =============================================================================
struct NkSWShaderContext {
    const void* uboData         = nullptr;   // pointeur UBO binding 0
    const uint8* pushConstants  = nullptr;   // push constants (256 bytes max)
    uint32       pushConstSize  = 0;
    uint32       instanceIndex  = 0;

    template<typename T>
    const T* UBO() const { return static_cast<const T*>(uboData); }

    template<typename T>
    const T* PushConst() const {
        return pushConstants ? reinterpret_cast<const T*>(pushConstants) : nullptr;
    }
};

// =============================================================================
// NkSWShaderBuilder — construit un NkShaderDesc avec des lambdas CPU
// =============================================================================
class NkSWShaderBuilder {
public:
    NkSWShaderBuilder& SetName(const char* name) { mName = name; return *this; }

    NkSWShaderBuilder& SetVertexFn(NkSWVertexFn fn) {
        mVertFn = traits::NkMove(fn); return *this;
    }
    NkSWShaderBuilder& SetFragmentFn(NkSWFragmentFn fn) {
        mFragFn = traits::NkMove(fn); return *this;
    }
    NkSWShaderBuilder& SetComputeFn(NkSWComputeFn fn) {
        mCompFn = traits::NkMove(fn); mIsCompute = true; return *this;
    }

    // ── Geometry shader CPU ───────────────────────────────────────────────────
    // Le GS reçoit une primitive (tableau de vertices) et peut en émettre
    // de nouvelles via ctx.emitVertex() + ctx.endPrimitive().
    // Exemple: extrusion de normales, instancing procédural, billboard, etc.
    NkSWShaderBuilder& SetGeometryFn(NkSWGeomFn fn,
                                      NkPrimitiveTopology outTopo =
                                          NkPrimitiveTopology::NK_TRIANGLE_LIST) {
        mGeomFn      = traits::NkMove(fn);
        mGeomOutTopo = outTopo;
        return *this;
    }

    // ── Tessellation CPU ──────────────────────────────────────────────────────
    // Le domain shader reçoit le patch + coords bary + uniforms.
    // patchSize = 3 (triangle) ou 4 (quad).
    // tessOuter/Inner = facteurs de subdivision (1.0 = aucune subdivision).
    NkSWShaderBuilder& SetTessFn(NkSWTessFn fn, uint32 patchSize=3,
                                   float tessOuter=4.f, float tessInner=4.f) {
        mTessFn      = traits::NkMove(fn);
        mPatchSize   = patchSize;
        mTessOuter   = tessOuter;
        mTessInner   = tessInner;
        return *this;
    }

    NkShaderDesc Build() {
        NkShaderDesc desc;
        desc.debugName = mName.CStr();

        if (mVertFn) {
            NkShaderStageDesc vs;
            vs.stage      = NkShaderStage::NK_VERTEX;
            vs.cpuVertFn  = &mVertFn;
            vs.entryPoint = "main";
            desc.AddStage(vs);
        }

        if (mFragFn && !mIsCompute) {
            NkShaderStageDesc fs;
            fs.stage      = NkShaderStage::NK_FRAGMENT;
            fs.cpuFragFn  = &mFragFn;
            fs.entryPoint = "main";
            desc.AddStage(fs);
        }

        if (mCompFn && mIsCompute) {
            NkShaderStageDesc cs;
            cs.stage      = NkShaderStage::NK_COMPUTE;
            cs.cpuCompFn  = &mCompFn;
            cs.entryPoint = "main";
            desc.AddStage(cs);
        }

        // Note : geomFn et tessFn ne passent pas par NkShaderStageDesc.
        // Utiliser ApplyTo(device, shaderHandle) après CreateShader() pour les configurer.
        return desc;
    }

    // Après CreateShader(Build()), appeler ApplyTo() pour configurer geomFn et tessFn
    // directement sur le NkSWShader interne (contourne NkShaderStageDesc).
    void ApplyTo(NkSoftwareDevice* device, NkShaderHandle handle) {
        NkSWShader* sh = device->GetShader(handle.id);
        if (!sh) return;
        if (mGeomFn) {
            sh->geomFn           = mGeomFn;
            sh->geomOutputTopology = mGeomOutTopo;
        }
        if (mTessFn) {
            sh->tessFn    = mTessFn;
            sh->patchSize = mPatchSize;
            sh->tessOuter = mTessOuter;
            sh->tessInner = mTessInner;
        }
    }

private:
    NkString            mName      = "sw_shader";
    NkSWVertexFn        mVertFn;
    NkSWFragmentFn      mFragFn;
    NkSWComputeFn       mCompFn;
    NkSWGeomFn          mGeomFn;
    NkSWTessFn          mTessFn;
    NkPrimitiveTopology mGeomOutTopo = NkPrimitiveTopology::NK_TRIANGLE_LIST;
    uint32              mPatchSize  = 3;
    float               mTessOuter  = 4.f;
    float               mTessInner  = 4.f;
    bool                mIsCompute  = false;
};

// =============================================================================
// Shaders CPU prêts à l'emploi
// =============================================================================

// Passthrough : position XYZ → clip space, couleur directe
inline NkShaderDesc NkSWShader_Passthrough() {
    NkSWShaderBuilder b;
    b.SetName("SW_Passthrough");
    b.SetVertexFn([](const void* vd, uint32 idx, const void*) -> NkSWVertex {
        const float* f = (const float*)vd + idx * 7; // xyz + rgba
        NkSWVertex v;
        v.position = { f[0], f[1], f[2], 1.f };
        v.color    = { f[3], f[4], f[5], f[6] };
        return v;
    });
    b.SetFragmentFn([](const NkSWVertex& v, const void*, const void*) -> NkVec4f {
        return v.color;
    });
    return b.Build();
}

// Phong simple (Nx3 position + Nx3 normal + Nx3 color + matrice mvp dans UBO)
// Layout UBO attendu : { mat4 mvp; vec4 lightDir; vec4 lightColor; }
struct NkSWPhongUBO {
    float mvp[16];
    float lightDir[4];
    float lightColor[4];
};

inline NkShaderDesc NkSWShader_Phong() {
    NkSWShaderBuilder b;
    b.SetName("SW_Phong");
    b.SetVertexFn([](const void* vd, uint32 idx, const void* ubo) -> NkSWVertex {
        const float* f = (const float*)vd + idx * 9; // xyz + nxyz + rgb
        NkSWVertex v;
        v.normal = { f[3], f[4], f[5] };
        v.color  = { f[6], f[7], f[8], 1.f };

        // Appliquer la matrice MVP
        if (ubo) {
            const NkSWPhongUBO* u = (const NkSWPhongUBO*)ubo;
            // Multiplie [x,y,z,1] par la matrice column-major
            float x=f[0], y=f[1], z=f[2], w=1.f;
            const float* m = u->mvp;
            v.position.x = m[0]*x + m[4]*y + m[ 8]*z + m[12]*w;
            v.position.y = m[1]*x + m[5]*y + m[ 9]*z + m[13]*w;
            v.position.z = m[2]*x + m[6]*y + m[10]*z + m[14]*w;
            v.position.w = m[3]*x + m[7]*y + m[11]*z + m[15]*w;
        } else {
            v.position = { f[0], f[1], f[2], 1.f };
        }
        return v;
    });
    b.SetFragmentFn([](const NkSWVertex& v, const void* ubo, const void*) -> NkVec4f {
        NkVec3f N = { v.normal.x, v.normal.y, v.normal.z };
        float len = NkSqrt(N.x*N.x + N.y*N.y + N.z*N.z);
        if (len > 0.001f) { N.x/=len; N.y/=len; N.z/=len; }

        float diff = 0.5f;
        if (ubo) {
            const NkSWPhongUBO* u = (const NkSWPhongUBO*)ubo;
            NkVec3f L = { -u->lightDir[0], -u->lightDir[1], -u->lightDir[2] };
            float ll = NkSqrt(L.x*L.x + L.y*L.y + L.z*L.z);
            if (ll > 0.001f) { L.x/=ll; L.y/=ll; L.z/=ll; }
            diff = NkMax(0.f, N.x*L.x + N.y*L.y + N.z*L.z);
        }

        float ambient = 0.15f;
        float lit     = ambient + (1.f - ambient) * diff;
        return { v.color.x*lit, v.color.y*lit, v.color.z*lit, v.color.w };
    });
    return b.Build();
}

// Shader texture simple (xyz + uv dans vertex, sampler2D en binding 1)
inline NkShaderDesc NkSWShader_Textured() {
    NkSWShaderBuilder b;
    b.SetName("SW_Textured");
    b.SetVertexFn([](const void* vd, uint32 idx, const void* ubo) -> NkSWVertex {
        const float* f = (const float*)vd + idx * 5; // xyz + uv
        NkSWVertex v;
        v.uv = { f[3], f[4] };

        if (ubo) {
            const float* m = (const float*)ubo;
            float x=f[0], y=f[1], z=f[2], w=1.f;
            v.position.x = m[0]*x + m[4]*y + m[ 8]*z + m[12]*w;
            v.position.y = m[1]*x + m[5]*y + m[ 9]*z + m[13]*w;
            v.position.z = m[2]*x + m[6]*y + m[10]*z + m[14]*w;
            v.position.w = m[3]*x + m[7]*y + m[11]*z + m[15]*w;
        } else {
            v.position = { f[0], f[1], f[2], 1.f };
        }
        return v;
    });
    b.SetFragmentFn([](const NkSWVertex& v, const void*, const void* texPtr) -> NkVec4f {
        if (!texPtr) return { v.uv.x, v.uv.y, 0.f, 1.f }; // debug UV
        const NkSWTexture* tex = (const NkSWTexture*)texPtr;
        return tex->Sample(v.uv.x, v.uv.y);
    });
    return b.Build();
}

} // namespace nkentseu