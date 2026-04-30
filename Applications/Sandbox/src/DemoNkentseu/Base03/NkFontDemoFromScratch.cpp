// -----------------------------------------------------------------------------
// FICHIER: NkFontDemo.cpp
// DESCRIPTION: Démo NKFont V2.1 — texte 3D extrudé à partir des contours vectoriels
//              Utilise GetGlyphOutline pour générer un maillage 3D complet.
//
// Touches :
//   +/- : augmente/diminue la taille du texte (scale GPU)
//   S   : bascule mode SDF / mode normal (rebuild de l'atlas)
//   ESC : quitte
// -----------------------------------------------------------------------------

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKFont/NkFont.h"
#include "NKFont/Core/NkFontDetect.h"
#include <math.h>
#include <string.h>

namespace nkentseu { struct NkEntryState; }
using namespace nkentseu;
using namespace nkentseu::math;

// ============================================================
// SHADERS
// ============================================================

static constexpr const char* kVert2D = R"GLSL(
#version 460 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
out vec2 vUV; out vec4 vColor;
void main(){gl_Position=vec4(aPos,0,1);vUV=aUV;vColor=aColor;}
)GLSL";

static constexpr const char* kFrag2D_Normal = R"GLSL(
#version 460 core
in vec2 vUV; in vec4 vColor; out vec4 fragColor;
layout(binding=0) uniform sampler2D uAtlas;
void main(){
    float a=texture(uAtlas,vUV).a;
    if(a<0.01)discard;
    fragColor=vec4(vColor.rgb,vColor.a*a);
}
)GLSL";

static constexpr const char* kFrag2D_SDF = R"GLSL(
#version 460 core
in vec2 vUV; in vec4 vColor; out vec4 fragColor;
layout(binding=0) uniform sampler2D uAtlas;
layout(location=1) uniform float uSmoothing;
void main(){
    float dist=texture(uAtlas,vUV).r;
    float a=smoothstep(0.5-uSmoothing, 0.5+uSmoothing, dist);
    if(a<0.01) discard;
    fragColor=vec4(vColor.rgb, vColor.a*a);
}
)GLSL";

static constexpr const char* kVert3D_NoTex = R"GLSL(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aColor;
out vec4 vColor;
layout(std140, binding = 0) uniform MVPBlock {
    mat4 uMVP;
};
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
}
)GLSL";

static constexpr const char* kFrag3D_NoTex = R"GLSL(
#version 460 core
in vec4 vColor;
out vec4 fragColor;
void main() {
    fragColor = vColor;
}
)GLSL";

static constexpr const char* kVert3D = R"GLSL(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
out vec2 vUV; out vec4 vColor;
layout(std140,binding=0) uniform CamUBO{mat4 mvp;}uCam;
void main(){gl_Position=uCam.mvp*vec4(aPos,1);vUV=aUV;vColor=aColor;}
)GLSL";

static constexpr const char* kFrag3D = R"GLSL(
#version 460 core
in vec2 vUV; in vec4 vColor; out vec4 fragColor;
layout(binding=0) uniform sampler2D uAtlas;
void main(){float a=texture(uAtlas,vUV).a;if(a<0.01)discard;fragColor=vec4(vColor.rgb,vColor.a*a);}
)GLSL";

struct FontVtx2D { float x, y, u, v, r, g, b, a; };
struct FontVtx3D { float x, y, z, u, v, r, g, b, a; };
struct FontVtx3D_NoTex { float x, y, z; float nx, ny, nz; float r, g, b, a; };
struct alignas(16) CamUBO { float mvp[16]; };

using NkText3DVertexCallback = void(*)(const FontVtx3D_NoTex* vertices, uint32_t count, void* userData);

// ============================================================
// NkFontRenderer
// ============================================================

class NkFontRenderer {
public:
    static constexpr nkft_uint32 MAX_VERTS_2D = 16384u;
    static constexpr nkft_uint32 MAX_VERTS_3D = 6144u;
    static constexpr nkft_uint32 MAX_VERTS_3D_NO_TEX = 65536u;

    NkFontAtlas   atlas;
    NkFont*       fontRef  = nullptr;
    NkFont*       font2D   = nullptr;
    NkFont*       font3D   = nullptr;
    NkFontProfile profile;
    bool          isSDF    = false;

    bool Init(NkIDevice* device, NkRenderPassHandle rp, const char* fontPath, bool sdf = false) {
        mDevice = device;
        if (!device) return false;
        mFontPath = fontPath;
        return Rebuild(rp, sdf);
    }

    bool Rebuild(NkRenderPassHandle rp, bool sdf) {
        if (!mDevice) return false;
        DestroyGPU();
        atlas.Clear();
        isSDF = sdf;

        profile = NkFontDetector::AnalyzeFile(mFontPath.CStr());
        nkft_float32 sz2D  = NkFontDetector::SnapSize(profile, 18.f);
        nkft_float32 sz3D  = NkFontDetector::SnapSize(profile, 56.f);
        nkft_float32 szRef = NkFontDetector::SnapSize(profile, 64.f);

        NkFontConfig c2D, c3D, cRef;
        NkFontDetector::ApplyOptimalConfig(profile, sz2D, &c2D);
        NkFontDetector::ApplyOptimalConfig(profile, sz3D, &c3D);
        NkFontDetector::ApplyOptimalConfig(profile, szRef, &cRef);

        c2D.glyphRanges = c3D.glyphRanges = cRef.glyphRanges = NkFontAtlas::GetGlyphRangesDefault();

        atlas.texGlyphPadding = sdf ? 4 : 2;
        atlas.sdfMode   = sdf;
        atlas.sdfSpread = 6;

        font2D  = atlas.AddFontFromFile(mFontPath.CStr(), sz2D,  &c2D);
        font3D  = atlas.AddFontFromFile(mFontPath.CStr(), sz3D,  &c3D);
        fontRef = atlas.AddFontFromFile(mFontPath.CStr(), szRef, &cRef);

        if (!font2D || !font3D || !fontRef) {
            logger.Info("[NkFontRenderer] AddFontFromFile FAIL\n");
            return false;
        }
        if (!atlas.Build()) {
            logger.Info("[NkFontRenderer] Build FAIL\n");
            return false;
        }

        // Upload GPU pour le 2D
        {
            nkft_uint8* px = nullptr;
            nkft_int32 W = 0, H = 0;
            atlas.GetTexDataAsRGBA32(&px, &W, &H);
            if (!px || !W || !H) return false;
            NkTextureDesc td = NkTextureDesc::Tex2D((nkft_uint32)W, (nkft_uint32)H, NkGPUFormat::NK_RGBA8_UNORM);
            td.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
            td.mipLevels = 1u;
            td.debugName = "NkFontAtlasV2";
            mAtlasTex = mDevice->CreateTexture(td);
            if (!mAtlasTex.IsValid()) return false;
            mDevice->WriteTexture(mAtlasTex, px, (nkft_uint32)W * 4u);
            atlas.ClearTexData();
            logger.Info("[NkFontRenderer] Atlas GPU:{0}x{1} mode={2}\n", W, H, sdf ? "SDF" : "Normal");
        }

        // Sampler 2D
        {
            NkSamplerDesc sd{};
            bool nearest = !sdf && NkFontDetector::RecommendsNearestFilter(profile);
            sd.magFilter = NkFilter::NK_LINEAR;
            sd.minFilter = NkFilter::NK_LINEAR;
            sd.mipFilter = NkMipFilter::NK_NONE;
            sd.addressU  = NkAddressMode::NK_CLAMP_TO_EDGE;
            sd.addressV  = NkAddressMode::NK_CLAMP_TO_EDGE;
            mSampler = mDevice->CreateSampler(sd);
            if (!mSampler.IsValid()) return false;
        }

        if (!BuildShaders(sdf)) return false;
        if (!BuildLayouts()) return false;
        if (!BuildPipelines(rp)) return false;
        if (!BuildBuffers()) return false;
        if (!BuildDescriptors()) return false;
        if (!BuildPipeline3DNoTex(rp)) return false;

        mEnabled = true;
        logger.Info("[NkFontRenderer] Rebuild OK mode={0}\n", sdf ? "SDF" : "Normal");
        return true;
    }

    void Shutdown() {
        DestroyGPU();
        mDevice = nullptr;
    }

    void BeginFrame() {
        mVerts2D.Clear();
        mVerts3D.Clear();
        mVerts3DNoTex.Clear();
    }

    void DrawText2D(NkFont* fnt, const char* text,
                    float px, float py, float sw, float sh,
                    float r = 1, float g = 1, float b = 1, float a = 1,
                    float scale = 1.f) {
        if (!mEnabled || !fnt || !text) return;

        auto NX = [sw](float x) { return x / sw * 2.f - 1.f; };
        auto NY = [sh](float y) { return 1.f - y / sh * 2.f; };

        const char* p = text;
        float curX = px;

        while (*p) {
            NkFontCodepoint cp = NkFont::DecodeUTF8(&p, nullptr);
            if (cp == '\n') {
                curX = px;
                py += fnt->lineAdvance * scale;
                continue;
            }
            const NkFontGlyph* gl = fnt->FindGlyph(cp);
            if (!gl) continue;

            if (gl->visible && (mVerts2D.Size() + 6u) <= MAX_VERTS_2D) {
                float x0 = curX + gl->x0 * scale;
                float y0 = py   + gl->y0 * scale;
                float x1 = curX + gl->x1 * scale;
                float y1 = py   + gl->y1 * scale;
                float nx0 = NX(x0), ny0 = NY(y0);
                float nx1 = NX(x1), ny1 = NY(y1);

                mVerts2D.PushBack({nx0, ny0, gl->u0, gl->v0, r, g, b, a});
                mVerts2D.PushBack({nx1, ny0, gl->u1, gl->v0, r, g, b, a});
                mVerts2D.PushBack({nx0, ny1, gl->u0, gl->v1, r, g, b, a});
                mVerts2D.PushBack({nx1, ny0, gl->u1, gl->v0, r, g, b, a});
                mVerts2D.PushBack({nx1, ny1, gl->u1, gl->v1, r, g, b, a});
                mVerts2D.PushBack({nx0, ny1, gl->u0, gl->v1, r, g, b, a});
            }
            curX += gl->advanceX * scale;
        }
    }

    // ============================================================
    // HELPERS POUR L'EXTRUSION ROBUSTE
    // ============================================================

    // Calcule l'aire signée d'un contour 2D
    // > 0 = CCW (contour extérieur), < 0 = CW (trou)
    // static float ComputeContourArea(const NkVector<NkVec2f>& contour) {
    //     float area = 0.f;
    //     size_t n = contour.Size();
    //     for (size_t i = 0; i < n; ++i) {
    //         const NkVec2f& p0 = contour[i];
    //         const NkVec2f& p1 = contour[(i + 1) % n];
    //         area += p0.x * p1.y - p0.y * p1.x;
    //     }
    //     return area * 0.5f;
    // }

    // Calcule la normale sortante pour une arête latérale d'extrusion
    // isOuter = true pour contour extérieur (CCW), false pour trou (CW)
    static NkVec3f ComputeOutwardSideNormal(const NkVec2f& edgeStart, 
                                            const NkVec2f& edgeEnd, 
                                            bool isOuter) {
        // Direction de l'arête en 2D
        NkVec2f edge = NkVec2f(edgeEnd.x - edgeStart.x, edgeEnd.y - edgeStart.y).Normalized();
        
        // Normale 2D sortante : rotation de 90°
        // Pour CCW (extérieur) : normale = (edge.y, -edge.x) → pointe vers l'extérieur
        // Pour CW (trou) : normale = (-edge.y, edge.x) → pointe aussi vers l'extérieur du trou
        NkVec2f normal2D = isOuter 
            ? NkVec2f( edge.y, -edge.x) 
            : NkVec2f(-edge.y,  edge.x);
        
        return NkVec3f(normal2D.x, normal2D.y, 0.f).Normalized();
    }

    // ============================================================
    // HELPERS — VERSION SIMPLIFIÉE ET ROBUSTE
    // ============================================================

    // Triangulation ear-clipping pour polygones concaves (face avant uniquement)
    // static bool IsEar(const NkVector<NkVec2f>& poly, size_t i0, size_t i1, size_t i2) {
    //     const NkVec2f& A = poly[i0];
    //     const NkVec2f& B = poly[i1];
    //     const NkVec2f& C = poly[i2];
        
    //     // Angle convexe en B ? (cross > 0 pour CCW)
    //     float cross = (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
    //     if (cross <= 0.0001f) return false;
        
    //     // Aucun sommet à l'intérieur du triangle ABC ?
    //     for (size_t k = 0; k < poly.Size(); ++k) {
    //         if (k == i0 || k == i1 || k == i2) continue;
    //         const NkVec2f& P = poly[k];
    //         float w0 = (A.x - P.x) * (B.y - P.y) - (A.y - P.y) * (B.x - P.x);
    //         float w1 = (B.x - P.x) * (C.y - P.y) - (B.y - P.y) * (C.x - P.x);
    //         float w2 = (C.x - P.x) * (A.y - P.y) - (C.y - P.y) * (A.x - P.x);
    //         bool allPos = (w0 > 0.0001f && w1 > 0.0001f && w2 > 0.0001f);
    //         bool allNeg = (w0 < -0.0001f && w1 < -0.0001f && w2 < -0.0001f);
    //         if (allPos || allNeg) return false;
    //     }
    //     return true;
    // }

    // ============================================================
// HELPERS POUR LA GESTION DES TROUS
// ============================================================

// Calcule l'aire signée d'un contour 2D
// > 0 = CCW (contour extérieur), < 0 = CW (trou)
static float ComputeContourArea(const NkVector<NkVec2f>& contour) {
    float area = 0.f;
    size_t n = contour.Size();
    for (size_t i = 0; i < n; ++i) {
        const NkVec2f& p0 = contour[i];
        const NkVec2f& p1 = contour[(i + 1) % n];
        area += p0.x * p1.y - p0.y * p1.x;
    }
    return area * 0.5f;
}

// ============================================================
// HELPERS — TRIANGULATION SIMPLE
// ============================================================

static bool IsEar(const NkVector<NkVec2f>& poly, size_t i0, size_t i1, size_t i2) {
    const NkVec2f& A = poly[i0];
    const NkVec2f& B = poly[i1];
    const NkVec2f& C = poly[i2];
    
    float cross = (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
    if (cross <= 0.0001f) return false;
    
    for (size_t k = 0; k < poly.Size(); ++k) {
        if (k == i0 || k == i1 || k == i2) continue;
        const NkVec2f& P = poly[k];
        float w0 = (A.x - P.x) * (B.y - P.y) - (A.y - P.y) * (B.x - P.x);
        float w1 = (B.x - P.x) * (C.y - P.y) - (B.y - P.y) * (C.x - P.x);
        float w2 = (C.x - P.x) * (A.y - P.y) - (C.y - P.y) * (A.x - P.x);
        bool allPos = (w0 > 0.0001f && w1 > 0.0001f && w2 > 0.0001f);
        bool allNeg = (w0 < -0.0001f && w1 < -0.0001f && w2 < -0.0001f);
        if (allPos || allNeg) return false;
    }
    return true;
}

static void TriangulateContour(const NkVector<NkVec2f>& contour,
                                NkVector<uint32_t>& outIndices) {
    if (contour.Size() < 3) return;
    
    NkVector<size_t> indices;
    for (size_t i = 0; i < contour.Size(); ++i) indices.PushBack(i);
    
    while (indices.Size() > 3) {
        bool found = false;
        for (size_t i = 0; i < indices.Size(); ++i) {
            size_t i0 = indices[(i + indices.Size() - 1) % indices.Size()];
            size_t i1 = indices[i];
            size_t i2 = indices[(i + 1) % indices.Size()];
            
            if (IsEar(contour, i0, i1, i2)) {
                outIndices.PushBack((uint32_t)i0);
                outIndices.PushBack((uint32_t)i1);
                outIndices.PushBack((uint32_t)i2);
                indices.Erase(indices.Begin() + i);
                found = true;
                break;
            }
        }
        if (!found) break;
    }
    
    if (indices.Size() == 3) {
        outIndices.PushBack((uint32_t)indices[0]);
        outIndices.PushBack((uint32_t)indices[1]);
        outIndices.PushBack((uint32_t)indices[2]);
    }
}

// ============================================================
// GenerateText3DMesh — VERSION MINIMALISTE ET ROBUSTE
// ============================================================

void GenerateText3DMesh(NkFont* fnt, const char* text,
                        const NkMat4f& worldMatrix,
                        float r, float g, float b, float a,
                        float extrusionDepth,
                        NkText3DVertexCallback callback, void* userData) const {
    if (!fnt || !text || !callback) return;

    const float scale = 0.02f;
    float tw = fnt->CalcTextSizeX(text);
    float curX = -tw * 0.5f * scale;
    float curY = -fnt->ascent * 0.5f * scale;

    auto WorldPos = [&](float lx, float ly, float lz) -> NkVec3f {
        const float* m = worldMatrix.data;
        return NkVec3f{
            m[0]*lx + m[4]*ly + m[8]*lz + m[12],
            m[1]*lx + m[5]*ly + m[9]*lz + m[13],
            m[2]*lx + m[6]*ly + m[10]*lz + m[14]
        };
    };

    const char* p = text;
    while (*p) {
        NkFontCodepoint cp = NkFont::DecodeUTF8(&p, nullptr);
        if (cp == '\n') {
            curX = -tw * 0.5f * scale;
            curY -= fnt->lineAdvance * scale;
            continue;
        }

        NkVector<NkFontOutlineVertex> outline;
        if (!fnt->GetGlyphOutlinePoints(cp, outline)) continue;
        if (outline.Size() < 3) continue;

        // Découpage en contours individuels
        NkVector<NkVector<NkVec2f>> contours;
        NkVector<NkVec2f> current;
        for (size_t i = 0; i < outline.Size(); ++i) {
            current.PushBack({outline[i].x, outline[i].y});
            if (outline[i].isEndOfContour) {
                if (current.Size() >= 3) contours.PushBack(current);
                current.Clear();
            }
        }
        if (current.Size() >= 3) contours.PushBack(current);

        // Normales fixes
        NkVec3f normFront = NkVec3f(0.f, 0.f, -1.f);
        NkVec3f normBack  = NkVec3f(0.f, 0.f,  1.f);

        // Traitement de chaque contour
        for (const auto& contour : contours) {
            // Points 3D avant/arrière
            NkVector<NkVec3f> frontPts, backPts;
            for (const auto& pt : contour) {
                float lx = curX + pt.x * scale;
                float ly = curY + pt.y * scale;
                frontPts.PushBack(WorldPos(lx, ly, 0.f));
                backPts.PushBack(WorldPos(lx, ly, extrusionDepth));
            }

            // Triangulation du contour
            NkVector<uint32_t> triIndices;
            TriangulateContour(contour, triIndices);

            // Faces avant (winding CCW → normale -Z)
            for (size_t t = 0; t + 2 < triIndices.Size(); t += 3) {
                uint32_t i0 = triIndices[t], i1 = triIndices[t+1], i2 = triIndices[t+2];
                FontVtx3D_NoTex tri[3] = {
                    {frontPts[i0].x, frontPts[i0].y, frontPts[i0].z, normFront.x, normFront.y, normFront.z, r,g,b,a},
                    {frontPts[i1].x, frontPts[i1].y, frontPts[i1].z, normFront.x, normFront.y, normFront.z, r,g,b,a},
                    {frontPts[i2].x, frontPts[i2].y, frontPts[i2].z, normFront.x, normFront.y, normFront.z, r,g,b,a}
                };
                callback(tri, 3, userData);
            }

            // Faces arrière (winding inversé → normale +Z)
            for (size_t t = 0; t + 2 < triIndices.Size(); t += 3) {
                uint32_t i0 = triIndices[t], i1 = triIndices[t+1], i2 = triIndices[t+2];
                FontVtx3D_NoTex tri[3] = {
                    {backPts[i0].x, backPts[i0].y, backPts[i0].z, normBack.x, normBack.y, normBack.z, r,g,b,a},
                    {backPts[i2].x, backPts[i2].y, backPts[i2].z, normBack.x, normBack.y, normBack.z, r,g,b,a},
                    {backPts[i1].x, backPts[i1].y, backPts[i1].z, normBack.x, normBack.y, normBack.z, r,g,b,a}
                };
                callback(tri, 3, userData);
            }

            // Faces latérales — normales toujours sortantes
            for (size_t i = 0; i < contour.Size(); ++i) {
                size_t j = (i + 1) % contour.Size();
                
                const NkVec3f& a0 = frontPts[i];
                const NkVec3f& a1 = frontPts[j];
                const NkVec3f& b0 = backPts[i];
                const NkVec3f& b1 = backPts[j];

                // Normale depuis géométrie 3D
                NkVec3f edgeFront = a1 - a0;
                NkVec3f edgeSide = b0 - a0;
                NkVec3f sideNorm = edgeFront.Cross(edgeSide).Normalized();
                
                // Vérification "vers l'extérieur"
                NkVec3f edgeCenter = (a0 + a1) * 0.5f;
                NkVec3f quadCenter = (a0 + a1 + b0 + b1) * 0.25f;
                NkVec3f toEdge = edgeCenter - quadCenter;
                if (sideNorm.Dot(toEdge) < 0.f) {
                    sideNorm = -sideNorm;
                }

                // Quad latéral : deux triangles
                FontVtx3D_NoTex quad[6] = {
                    {a0.x, a0.y, a0.z, sideNorm.x, sideNorm.y, sideNorm.z, r,g,b,a},
                    {b0.x, b0.y, b0.z, sideNorm.x, sideNorm.y, sideNorm.z, r,g,b,a},
                    {b1.x, b1.y, b1.z, sideNorm.x, sideNorm.y, sideNorm.z, r,g,b,a},
                    {a0.x, a0.y, a0.z, sideNorm.x, sideNorm.y, sideNorm.z, r,g,b,a},
                    {b1.x, b1.y, b1.z, sideNorm.x, sideNorm.y, sideNorm.z, r,g,b,a},
                    {a1.x, a1.y, a1.z, sideNorm.x, sideNorm.y, sideNorm.z, r,g,b,a}
                };
                callback(quad, 6, userData);
            }
        }

        curX += fnt->GetCharAdvance(cp) * scale;
    }
}

    void EndFrame(NkICommandBuffer* cmd, const NkMat4f& view, const NkMat4f& proj,
                  float sdfSmoothing = 0.1f) {
        if (!mEnabled || !cmd) return;
        RefreshDescriptors();

        nkft_uint32 n2 = mVerts2D.Size();
        if (n2 > 0) {
            mDevice->WriteBuffer(mVbo2D, mVerts2D.Begin(), n2 * sizeof(FontVtx2D));
            cmd->BindGraphicsPipeline(mPipe2D);
            cmd->BindDescriptorSet(mDs2D, 0u);
            cmd->BindVertexBuffer(0, mVbo2D);
            cmd->Draw(n2);
        }

        nkft_uint32 n3d = mVerts3DNoTex.Size();
        if (n3d > 0) {
            mDevice->WriteBuffer(mVbo3DNoTex, mVerts3DNoTex.Begin(), n3d * sizeof(FontVtx3D_NoTex));
            CamUBO cam;
            NkMat4f mvp = proj * view;
            for (int i = 0; i < 16; ++i) cam.mvp[i] = mvp.data[i];
            mDevice->WriteBuffer(mUbo3DNoTex, &cam, sizeof(cam));
            cmd->BindGraphicsPipeline(mPipe3DNoTex);
            cmd->BindDescriptorSet(mDs3DNoTex, 0u);
            cmd->BindVertexBuffer(0, mVbo3DNoTex);
            cmd->Draw(n3d);
        }

        nkft_uint32 n3 = mVerts3D.Size();
        if (n3 > 0) {
            mDevice->WriteBuffer(mVbo3D, mVerts3D.Begin(), n3 * sizeof(FontVtx3D));
            CamUBO cam;
            NkMat4f mvp = proj * view;
            for (int i = 0; i < 16; ++i) cam.mvp[i] = mvp.data[i];
            mDevice->WriteBuffer(mUbo3D, &cam, sizeof(cam));
            cmd->BindGraphicsPipeline(mPipe3D);
            cmd->BindDescriptorSet(mDs3D, 0u);
            cmd->BindVertexBuffer(0, mVbo3D);
            cmd->Draw(n3);
        }
    }

    NkVector<FontVtx3D_NoTex>& Get3DNoTexVertices() { return mVerts3DNoTex; }
    void Clear3DNoTexVertices() { mVerts3DNoTex.Clear(); }
    bool IsEnabled() const { return mEnabled; }

private:
    NkIDevice*   mDevice = nullptr;
    bool         mEnabled = false;
    NkString     mFontPath;

    NkTextureHandle  mAtlasTex;
    NkSamplerHandle  mSampler;
    NkShaderHandle   mShd2D, mShd3D, mShd3DNoTex;
    NkDescSetHandle  mDsl2D, mDsl3D, mDs2D, mDs3D;
    NkDescSetHandle  mDsl3DNoTex, mDs3DNoTex;
    NkPipelineHandle mPipe2D, mPipe3D, mPipe3DNoTex;
    NkBufferHandle   mVbo2D, mVbo3D, mUbo3D;
    NkBufferHandle   mVbo3DNoTex, mUbo3DNoTex;
    NkVector<FontVtx2D> mVerts2D;
    NkVector<FontVtx3D> mVerts3D;
    NkVector<FontVtx3D_NoTex> mVerts3DNoTex;

    void DestroyGPU() {
        if (!mDevice) {
            mEnabled = false;
            return;
        }
        auto d = mDevice;
        if (mDs2D.IsValid())    d->FreeDescriptorSet(mDs2D);
        if (mDs3D.IsValid())    d->FreeDescriptorSet(mDs3D);
        if (mDsl2D.IsValid())   d->DestroyDescriptorSetLayout(mDsl2D);
        if (mDsl3D.IsValid())   d->DestroyDescriptorSetLayout(mDsl3D);
        if (mPipe2D.IsValid())  d->DestroyPipeline(mPipe2D);
        if (mPipe3D.IsValid())  d->DestroyPipeline(mPipe3D);
        if (mShd2D.IsValid())   d->DestroyShader(mShd2D);
        if (mShd3D.IsValid())   d->DestroyShader(mShd3D);
        if (mVbo2D.IsValid())   d->DestroyBuffer(mVbo2D);
        if (mVbo3D.IsValid())   d->DestroyBuffer(mVbo3D);
        if (mUbo3D.IsValid())   d->DestroyBuffer(mUbo3D);
        if (mSampler.IsValid()) d->DestroySampler(mSampler);
        if (mAtlasTex.IsValid()) d->DestroyTexture(mAtlasTex);
        if (mDs3DNoTex.IsValid())   d->FreeDescriptorSet(mDs3DNoTex);
        if (mDsl3DNoTex.IsValid())  d->DestroyDescriptorSetLayout(mDsl3DNoTex);
        if (mPipe3DNoTex.IsValid()) d->DestroyPipeline(mPipe3DNoTex);
        if (mShd3DNoTex.IsValid())  d->DestroyShader(mShd3DNoTex);
        if (mVbo3DNoTex.IsValid())  d->DestroyBuffer(mVbo3DNoTex);
        if (mUbo3DNoTex.IsValid())  d->DestroyBuffer(mUbo3DNoTex);
        mEnabled = false;
    }

    bool BuildShaders(bool sdf) {
        {
            NkShaderDesc sd;
            sd.debugName = "FS2D";
            sd.AddGLSL(NkShaderStage::NK_VERTEX, kVert2D);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT, sdf ? kFrag2D_SDF : kFrag2D_Normal);
            mShd2D = mDevice->CreateShader(sd);
            if (!mShd2D.IsValid()) return false;
        }
        {
            NkShaderDesc sd;
            sd.debugName = "FS3D";
            sd.AddGLSL(NkShaderStage::NK_VERTEX, kVert3D);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kFrag3D);
            mShd3D = mDevice->CreateShader(sd);
            if (!mShd3D.IsValid()) return false;
        }
        {
            NkShaderDesc sd;
            sd.debugName = "FS3D_NoTex";
            sd.AddGLSL(NkShaderStage::NK_VERTEX, kVert3D_NoTex);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kFrag3D_NoTex);
            mShd3DNoTex = mDevice->CreateShader(sd);
            if (!mShd3DNoTex.IsValid()) return false;
        }
        return true;
    }

    bool BuildLayouts() {
        {
            NkDescriptorSetLayoutDesc l;
            l.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
            mDsl2D = mDevice->CreateDescriptorSetLayout(l);
            if (!mDsl2D.IsValid()) return false;
        }
        {
            NkDescriptorSetLayoutDesc l;
            l.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
            l.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_VERTEX);
            mDsl3D = mDevice->CreateDescriptorSetLayout(l);
            if (!mDsl3D.IsValid()) return false;
        }
        {
            NkDescriptorSetLayoutDesc l;
            l.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_VERTEX);
            mDsl3DNoTex = mDevice->CreateDescriptorSetLayout(l);
            if (!mDsl3DNoTex.IsValid()) return false;
        }
        return true;
    }

    bool BuildPipelines(NkRenderPassHandle rp) {
        NkVertexLayout vl2D;
        vl2D.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT, 0u, "POSITION", 0)
            .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT, 2 * sizeof(float), "TEXCOORD", 0)
            .AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT, 4 * sizeof(float), "COLOR", 0)
            .AddBinding(0, sizeof(FontVtx2D));
        {
            NkDepthStencilDesc ds = NkDepthStencilDesc::Default();
            ds.depthTestEnable  = false;
            ds.depthWriteEnable = false;
            NkRasterizerDesc rast = NkRasterizerDesc::Default();
            rast.cullMode = NkCullMode::NK_NONE;
            NkGraphicsPipelineDesc pd;
            pd.shader = mShd2D;
            pd.vertexLayout = vl2D;
            pd.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            pd.rasterizer = rast;
            pd.depthStencil = ds;
            pd.blend = NkBlendDesc::Alpha();
            pd.renderPass = rp;
            pd.debugName = "FP2D";
            pd.descriptorSetLayouts.PushBack(mDsl2D);
            mPipe2D = mDevice->CreateGraphicsPipeline(pd);
            if (!mPipe2D.IsValid()) return false;
        }

        NkVertexLayout vl3D;
        vl3D.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0u, "POSITION", 0)
            .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT, 3 * sizeof(float), "TEXCOORD", 0)
            .AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT, 5 * sizeof(float), "COLOR", 0)
            .AddBinding(0, sizeof(FontVtx3D));
        {
            NkDepthStencilDesc ds = NkDepthStencilDesc::Default();
            ds.depthTestEnable  = true;
            ds.depthWriteEnable = false;
            NkRasterizerDesc rast = NkRasterizerDesc::Default();
            rast.cullMode = NkCullMode::NK_NONE;
            NkGraphicsPipelineDesc pd;
            pd.shader = mShd3D;
            pd.vertexLayout = vl3D;
            pd.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            pd.rasterizer = rast;
            pd.depthStencil = ds;
            pd.blend = NkBlendDesc::Alpha();
            pd.renderPass = rp;
            pd.debugName = "FP3D";
            pd.descriptorSetLayouts.PushBack(mDsl3D);
            mPipe3D = mDevice->CreateGraphicsPipeline(pd);
            if (!mPipe3D.IsValid()) return false;
        }
        return true;
    }

    bool BuildPipeline3DNoTex(NkRenderPassHandle rp) {
        NkVertexLayout vl3DNoTex;
        vl3DNoTex.AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT, 0u, "POSITION", 0)
                  .AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT, 3 * sizeof(float), "NORMAL", 0)
                  .AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT, 6 * sizeof(float), "COLOR", 0)
                  .AddBinding(0, sizeof(FontVtx3D_NoTex));
        NkDepthStencilDesc ds = NkDepthStencilDesc::Default();
        ds.depthTestEnable  = true;
        ds.depthWriteEnable = true;
        NkRasterizerDesc rast = NkRasterizerDesc::Default();
        rast.cullMode = NkCullMode::NK_BACK;
        NkGraphicsPipelineDesc pd;
        pd.shader = mShd3DNoTex;
        pd.vertexLayout = vl3DNoTex;
        pd.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        pd.rasterizer = rast;
        pd.depthStencil = ds;
        pd.blend = NkBlendDesc::Opaque();
        pd.renderPass = rp;
        pd.debugName = "FP3D_NoTex";
        pd.descriptorSetLayouts.PushBack(mDsl3DNoTex);
        mPipe3DNoTex = mDevice->CreateGraphicsPipeline(pd);
        return mPipe3DNoTex.IsValid();
    }

    bool BuildBuffers() {
        {
            NkBufferDesc bd = NkBufferDesc::Vertex(MAX_VERTS_2D * sizeof(FontVtx2D), nullptr);
            bd.usage = NkResourceUsage::NK_UPLOAD;
            bd.debugName = "FVbo2D";
            mVbo2D = mDevice->CreateBuffer(bd);
            if (!mVbo2D.IsValid()) return false;
        }
        {
            NkBufferDesc bd = NkBufferDesc::Vertex(MAX_VERTS_3D * sizeof(FontVtx3D), nullptr);
            bd.usage = NkResourceUsage::NK_UPLOAD;
            bd.debugName = "FVbo3D";
            mVbo3D = mDevice->CreateBuffer(bd);
            if (!mVbo3D.IsValid()) return false;
        }
        mUbo3D = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(CamUBO)));
        if (!mUbo3D.IsValid()) return false;
        {
            NkBufferDesc bd = NkBufferDesc::Vertex(MAX_VERTS_3D_NO_TEX * sizeof(FontVtx3D_NoTex), nullptr);
            bd.usage = NkResourceUsage::NK_UPLOAD;
            bd.debugName = "FVbo3DNoTex";
            mVbo3DNoTex = mDevice->CreateBuffer(bd);
            if (!mVbo3DNoTex.IsValid()) return false;
        }
        mUbo3DNoTex = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(CamUBO)));
        if (!mUbo3DNoTex.IsValid()) return false;
        mVerts2D.Reserve(MAX_VERTS_2D);
        mVerts3D.Reserve(MAX_VERTS_3D);
        mVerts3DNoTex.Reserve(MAX_VERTS_3D_NO_TEX);
        return true;
    }

    bool BuildDescriptors() {
        mDs2D = mDevice->AllocateDescriptorSet(mDsl2D);
        if (!mDs2D.IsValid()) return false;
        mDs3D = mDevice->AllocateDescriptorSet(mDsl3D);
        if (!mDs3D.IsValid()) return false;
        mDs3DNoTex = mDevice->AllocateDescriptorSet(mDsl3DNoTex);
        if (!mDs3DNoTex.IsValid()) return false;
        RefreshDescriptors();
        if (mUbo3D.IsValid()) {
            NkDescriptorWrite w{};
            w.set = mDs3D;
            w.binding = 1u;
            w.type = NkDescriptorType::NK_UNIFORM_BUFFER;
            w.buffer = mUbo3D;
            w.bufferRange = sizeof(CamUBO);
            mDevice->UpdateDescriptorSets(&w, 1u);
        }
        if (mUbo3DNoTex.IsValid()) {
            NkDescriptorWrite w{};
            w.set = mDs3DNoTex;
            w.binding = 0u;
            w.type = NkDescriptorType::NK_UNIFORM_BUFFER;
            w.buffer = mUbo3DNoTex;
            w.bufferRange = sizeof(CamUBO);
            mDevice->UpdateDescriptorSets(&w, 1u);
        }
        return true;
    }

    void RefreshDescriptors() {
        if (!mAtlasTex.IsValid() || !mSampler.IsValid()) return;
        auto Bind = [&](NkDescSetHandle ds, nkft_uint32 slot) {
            NkDescriptorWrite w{};
            w.set = ds;
            w.binding = slot;
            w.type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            w.texture = mAtlasTex;
            w.sampler = mSampler;
            w.textureLayout = NkResourceState::NK_SHADER_READ;
            mDevice->UpdateDescriptorSets(&w, 1u);
        };
        Bind(mDs2D, 0u);
        Bind(mDs3D, 0u);
    }
};

// ============================================================
// nkmain
// ============================================================

int nkmain(const NkEntryState& state) {
    NkWindowConfig wc;
    wc.title = "NKFont V2.1 — 3D extrudé à partir des contours";
    wc.width = 1280;
    wc.height = 720;
    wc.centered = true;
    wc.resizable = true;

    NkWindow window;
    if (!window.Create(wc)) return 1;

    NkDeviceInitInfo di;
    di.api = NkGraphicsApi::NK_GFX_API_OPENGL;
    di.surface = window.GetSurfaceDesc();
    di.width = window.GetSize().width;
    di.height = window.GetSize().height;
    di.context.vulkan.appName = "NkFontDemo";
    di.context.vulkan.engineName = "Nkentseu";

    NkIDevice* device = NkDeviceFactory::Create(di);
    if (!device || !device->IsValid()) {
        window.Close();
        return 1;
    }

    nkft_uint32 W = device->GetSwapchainWidth();
    nkft_uint32 H = device->GetSwapchainHeight();

    NkICommandBuffer* cmd = device->CreateCommandBuffer();
    if (!cmd) return 1;

    NkFontRenderer* fr = new NkFontRenderer();
    if (!fr->Init(device, device->GetSwapchainRenderPass(), "Resources/Fonts/Roboto-Regular.ttf", false)) {
        delete fr;
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    bool running = true;
    bool pendingRebuild = false;
    bool nextSDF = false;
    auto& ev = NkEvents();

    float textScale = 1.f, targetSize = 18.f;
    if (fr->fontRef && fr->fontRef->fontSize > 0) {
        textScale = targetSize / fr->fontRef->fontSize;
    }

    ev.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    ev.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        auto k = e->GetKey();
        if (k == NkKey::NK_ESCAPE) running = false;
        if (k == NkKey::NK_NUMPAD_ADD || k == NkKey::NK_EQUALS) {
            targetSize = fminf(targetSize + 2.f, 128.f);
            if (fr->fontRef && fr->fontRef->fontSize > 0) textScale = targetSize / fr->fontRef->fontSize;
        }
        if (k == NkKey::NK_NUMPAD_SUB || k == NkKey::NK_MINUS) {
            targetSize = fmaxf(targetSize - 2.f, 6.f);
            if (fr->fontRef && fr->fontRef->fontSize > 0) textScale = targetSize / fr->fontRef->fontSize;
        }
        if (k == NkKey::NK_S) {
            pendingRebuild = true;
            nextSDF = !fr->isSDF;
        }
    });
    ev.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = (nkft_uint32)e->GetWidth();
        H = (nkft_uint32)e->GetHeight();
    });

    NkChrono timer;
    double tPrev = timer.Elapsed().seconds;
    double fpsAcc = 0;
    int fpsCnt = 0;
    float fpsDisp = 0, rot = 0, bob = 0;
    logger.Info("[FontDemo] +/- taille | S=toggle SDF\n");

    auto vertexCallback = [](const FontVtx3D_NoTex* vertices, uint32_t count, void* userData) {
        NkFontRenderer* renderer = static_cast<NkFontRenderer*>(userData);
        NkVector<FontVtx3D_NoTex>& out = renderer->Get3DNoTexVertices();
        for (uint32_t i = 0; i < count; ++i) out.PushBack(vertices[i]);
    };

    while (running) {
        double now = timer.Elapsed().seconds;
        float dt = (float)(now - tPrev);
        tPrev = now;
        fpsAcc += dt;
        ++fpsCnt;
        if (fpsAcc >= 0.5) {
            fpsDisp = (float)fpsCnt / (float)fpsAcc;
            fpsAcc = 0; fpsCnt = 0;
        }
        rot += 20 * dt;
        bob += 80 * dt;

        ev.PollEvents();
        if (!running) break;
        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight()) device->OnResize(W, H);
        if (pendingRebuild) {
            pendingRebuild = false;
            device->WaitIdle();
            fr->Rebuild(device->GetSwapchainRenderPass(), nextSDF);
            if (fr->fontRef && fr->fontRef->fontSize > 0) textScale = targetSize / fr->fontRef->fontSize;
        }

        NkMat4f view = NkMat4f::LookAt(NkVec3f(0, 2, 5), NkVec3f(0, 0, 0), NkVec3f(0, 1, 0));
        NkMat4f proj = NkMat4f::Perspective(NkAngle(60.f), (float)W / (float)H, 0.05f, 100.f);

        NkFrameContext frame;
        if (!device->BeginFrame(frame)) continue;
        W = device->GetSwapchainWidth();
        H = device->GetSwapchainHeight();
        if (!W || !H) { device->EndFrame(frame); continue; }

        NkFramebufferHandle fbo = device->GetSwapchainFramebuffer();
        NkRenderPassHandle  rp  = device->GetSwapchainRenderPass();

        cmd->Reset();
        if (!cmd->Begin()) { device->EndFrame(frame); continue; }
        if (!cmd->BeginRenderPass(rp, fbo, NkRect2D{0, 0, (int)W, (int)H})) {
            cmd->End(); device->EndFrame(frame); continue;
        }
        cmd->SetViewport(NkViewport(0, 0, (float)W, (float)H));
        cmd->SetScissor(NkRect2D(0u, 0u, W, H));

        fr->BeginFrame();
        fr->Clear3DNoTexVertices();

        // Texte 3D extrudé avec contours réels
        {
            float y = 1.5f + 0.12f * NkSin(NkToRadians(bob));
            NkMat4f w = NkMat4f::RotationY(NkAngle(rot)) * NkMat4f::Translation(NkVec3f(0, y, 0)) * NkMat4f::Scaling(NkVec3f(1.f, -1.5f, 1.f));
            fr->GenerateText3DMesh(fr->font3D, "NKENTSEU", w, 1.0f, 0.5f, 0.2f, 1.0f, 0.2f, vertexCallback, fr);
        }
        {
            NkMat4f w = NkMat4f::RotationX(NkAngle(0)) * NkMat4f::Translation(NkVec3f(0, -0.5f, 1.2f)) * NkMat4f::Scaling(NkVec3f(1.f, -1.5f, 1.f));
            fr->GenerateText3DMesh(fr->font3D, "NKFont V2.1", w, 0.2f, 0.6f, 1.0f, 1.0f, 1.0f, vertexCallback, fr);
        }

        // HUD 2D
        const char* modeStr = fr->isSDF ? "SDF (net a toute taille)" : "Normal (bitmap atlas)";
        const char* kindStr = (fr->profile.kind == NkFontKind::Bitmap) ? "Bitmap" : "Vectorielle TTF";
        float lineH = fr->fontRef ? fr->fontRef->lineAdvance * textScale : 20.f;
        float sdfSmooth = fr->isSDF ? 0.2f / (targetSize * textScale) : 0.f;

        {
            NkString s = NkFormat("FPS:{0:.0f} | {1} | {2} | {3:.0f}px | +/- taille | S=SDF", fpsDisp, kindStr, modeStr, targetSize);
            fr->DrawText2D(fr->fontRef, s.CStr(), 12, 12, (float)W, (float)H, 1, 1, 0.3f, 1, textScale);
        }
        fr->DrawText2D(fr->fontRef, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 12, 12 + lineH, (float)W, (float)H, 1, 1, 1, 1, textScale);
        fr->DrawText2D(fr->fontRef, "abcdefghijklmnopqrstuvwxyz 0123456789", 12, 12 + lineH*2, (float)W, (float)H, 0.9f,0.9f,0.9f,1, textScale);
        fr->DrawText2D(fr->fontRef, "Bonjour le monde! Hello World! @#$%", 12, 12 + lineH*3, (float)W, (float)H, 1,0.5f,0.5f,1, textScale);
        fr->DrawText2D(fr->fontRef, "Fix: advanceX/oversampleH + rasteŕiseur correct", 12, 12 + lineH*4, (float)W, (float)H, 0.6f,0.9f,1,1, textScale);
        fr->DrawText2D(fr->fontRef, "WOFF + OTF/CFF Type2 + SDF mode", 12, 12 + lineH*5, (float)W, (float)H, 0.7f,1,0.7f,1, textScale);
        fr->DrawText2D(fr->font2D, "ESC:quit  +/-:size  S:SDF mode", 12, (float)H - 26, (float)W, (float)H, 0.5f,0.5f,0.5f,0.8f, 1);

        fr->EndFrame(cmd, view, proj, sdfSmooth);
        cmd->EndRenderPass();
        cmd->End();
        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);
    }

    device->WaitIdle();
    fr->Shutdown();
    delete fr;
    device->DestroyCommandBuffer(cmd);
    NkDeviceFactory::Destroy(device);
    window.Close();
    return 0;
}

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================