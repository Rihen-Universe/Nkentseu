// =============================================================================
// NkFontDemo.cpp
// Démo NKFont multi-backend — rendu de texte 2D
//
//   • Backends : OpenGL / Vulkan / DX11 / DX12 / Software / Metal
//     sélection via : --backend=opengl|vulkan|dx11|dx12|sw|metal
//   • Police   : --font=<chemin.ttf>  ou police système automatique
//   • Rendu    : "RIHEN", "NKENTSEU", "UNKENY" en 2D avec alpha blending
//   • Atlas    : Gray8 → RGBA8 uploadé comme texture GPU
//   • ESC      : quitter
//
// Dépendances : NKFont, NKRHI, NKWindow, NKMath
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkTime.h"

#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Software/NkSoftwareDevice.h"

#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"

// NKFont
#include "NKFont/NKFont.h"

#include "NKContainers/Sequential/NkVector.h"
#include <cstring>  // strncmp, memcpy
#include <cstdio>   // fopen, fread, fclose, fseek, ftell
#include <cstdlib>  // malloc, free

using namespace nkentseu;
using namespace nkentseu::math;

// =============================================================================
// Shaders GLSL (OpenGL)
// =============================================================================

// ── Background (quad plein-écran coloré) ─────────────────────────────────────
static const char* kGLSL_BgVS = R"(#version 460 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec3 aColor;
out vec4 vColor;
void main(){ vColor=vec4(aColor,1.0); gl_Position=vec4(aPos,0.0,1.0); })";
static const char* kGLSL_BgFS = R"(#version 460 core
in vec4 vColor; out vec4 fragColor;
void main(){ fragColor=vColor; })";

// ── Texte : ortho + atlas (alpha depuis canal A) ──────────────────────────────
static const char* kGLSL_TextVS = R"(#version 460 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec3 aColor;
layout(std140, binding=0) uniform TextUBO { mat4 uOrtho; };
out vec2 vUV; out vec3 vColor;
void main(){ vUV=aUV; vColor=aColor; gl_Position=uOrtho*vec4(aPos,0,1); })";
static const char* kGLSL_TextFS = R"(#version 460 core
in vec2 vUV; in vec3 vColor;
layout(binding=1) uniform sampler2D uAtlas;
out vec4 fragColor;
void main(){
    float alpha = texture(uAtlas, vUV).a;
    fragColor = vec4(vColor, alpha);
})";

// =============================================================================
// Shaders HLSL (DX11 / DX12)
// =============================================================================
static const char* kHLSL_BgVS = R"(
struct VI { float2 pos:POSITION; float3 col:COLOR; };
struct VO { float4 pos:SV_Position; float4 col:COLOR; };
VO VSMain(VI i){ VO o; o.pos=float4(i.pos,0,1); o.col=float4(i.col,1); return o; })";
static const char* kHLSL_BgPS = R"(
struct VO { float4 pos:SV_Position; float4 col:COLOR; };
float4 PSMain(VO i):SV_Target{ return i.col; })";

static const char* kHLSL_TextVS = R"(
cbuffer TextUBO : register(b0){ float4x4 uOrtho; };
struct VI { float2 pos:POSITION; float2 uv:TEXCOORD; float3 col:COLOR; };
struct VO { float4 pos:SV_Position; float2 uv:TEXCOORD; float3 col:COLOR; };
VO VSMain(VI i){
    VO o;
    o.pos = mul(uOrtho, float4(i.pos,0,1));
    o.uv  = i.uv; o.col = i.col; return o;
})";
static const char* kHLSL_TextPS = R"(
Texture2D uAtlas : register(t1);
SamplerState uSampler : register(s1);
struct VO { float4 pos:SV_Position; float2 uv:TEXCOORD; float3 col:COLOR; };
float4 PSMain(VO i):SV_Target{
    float alpha = uAtlas.Sample(uSampler, i.uv).a;
    return float4(i.col, alpha);
})";

// =============================================================================
// Structs
// =============================================================================

// Vertex background (NDC)
struct VtxBg { float x,y, r,g,b; };
// Vertex texte (coordonnées écran + UV atlas + couleur)
struct VtxText { float x,y, u,v, r,g,b; };

// UBO texte — ortho matrix (GPU) + pointeur atlas (SW uniquement)
struct alignas(16) TextUbo {
    float         ortho[16];
    NkSWTexture*  swAtlas;      // SW seulement — nullptr sur GPU
    uint8         _pad[8];
};

static constexpr uint32 kMaxTextVerts = 2048;
static VtxText gTextVerts[kMaxTextVerts];
static uint32  gTextVertCount = 0;

// =============================================================================
// Quad background NDC
// =============================================================================
static const VtxBg kBgVerts[] = {
    {-1,-1, 0.04f,0.04f,0.12f}, { 1,-1, 0.04f,0.04f,0.12f},
    { 1, 1, 0.08f,0.08f,0.20f}, {-1,-1, 0.04f,0.04f,0.12f},
    { 1, 1, 0.08f,0.08f,0.20f}, {-1, 1, 0.08f,0.08f,0.20f},
};

// =============================================================================
// Math helpers
// =============================================================================
static void Mat4Identity(float m[16])
    { for(int i=0;i<16;i++) m[i]=0; m[0]=m[5]=m[10]=m[15]=1.f; }

static void Mat4Ortho(float m[16],float l,float r,float b,float t,float n,float f){
    for(int i=0;i<16;i++) m[i]=0;
    m[0]=2.f/(r-l);  m[5]=2.f/(t-b);  m[10]=-2.f/(f-n);
    m[12]=-(r+l)/(r-l); m[13]=-(t+b)/(t-b); m[14]=-(f+n)/(f-n); m[15]=1.f;
}

static void MulMV4(const float m[16],float x,float y,float z,float w,
                   float& ox,float& oy,float& oz,float& ow){
    ox=m[0]*x+m[4]*y+m[8]*z+m[12]*w;
    oy=m[1]*x+m[5]*y+m[9]*z+m[13]*w;
    oz=m[2]*x+m[6]*y+m[10]*z+m[14]*w;
    ow=m[3]*x+m[7]*y+m[11]*z+m[15]*w;
}

// =============================================================================
// Sélection du backend
// =============================================================================
static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (usize i = 1; i < args.Size(); i++) {
        const char* s = args[i].CStr();
        if (::strncmp(s,"--backend=vulkan",16)==0||::strncmp(s,"-bvk",4)==0)   return NkGraphicsApi::NK_GFX_API_VULKAN;
        if (::strncmp(s,"--backend=dx11",14)==0||::strncmp(s,"-bdx11",6)==0)   return NkGraphicsApi::NK_GFX_API_D3D11;
        if (::strncmp(s,"--backend=dx12",14)==0||::strncmp(s,"-bdx12",6)==0)   return NkGraphicsApi::NK_GFX_API_D3D12;
        if (::strncmp(s,"--backend=metal",15)==0||::strncmp(s,"-bmtl",5)==0)   return NkGraphicsApi::NK_GFX_API_METAL;
        if (::strncmp(s,"--backend=sw",12)==0||::strncmp(s,"-bsw",4)==0)       return NkGraphicsApi::NK_GFX_API_SOFTWARE;
        if (::strncmp(s,"--backend=opengl",16)==0||::strncmp(s,"-bgl",4)==0)   return NkGraphicsApi::NK_GFX_API_OPENGL;
    }
    return NkGraphicsApi::NK_GFX_API_SOFTWARE;
}

static const char* ApiName(NkGraphicsApi a) {
    switch(a){
        case NkGraphicsApi::NK_GFX_API_VULKAN:    return "Vulkan";
        case NkGraphicsApi::NK_GFX_API_D3D11: return "DX11";
        case NkGraphicsApi::NK_GFX_API_D3D12: return "DX12";
        case NkGraphicsApi::NK_GFX_API_METAL:     return "Metal";
        case NkGraphicsApi::NK_GFX_API_SOFTWARE:  return "Software";
        case NkGraphicsApi::NK_GFX_API_OPENGL:    return "OpenGL";
        default: return "Unknown";
    }
}

// =============================================================================
// Chargement de fichier en mémoire
// =============================================================================
static uint8* LoadFileToMemory(const char* path, usize& outSize) {
    FILE* f = ::fopen(path, "rb");
    if (!f) return nullptr;
    ::fseek(f, 0, SEEK_END);
    outSize = (usize)::ftell(f);
    ::rewind(f);
    uint8* buf = (uint8*)::malloc(outSize);
    if (!buf) { ::fclose(f); outSize = 0; return nullptr; }
    if (::fread(buf, 1, outSize, f) != outSize) {
        ::free(buf); ::fclose(f); outSize = 0; return nullptr;
    }
    ::fclose(f);
    return buf;
}

// Chemins de polices — Resources/ en priorité, puis polices système
static const char* sFontPaths[] = {
    // ── Polices du projet (relatives au répertoire de travail) ──────────────
    "Resources/Fonts/Roboto/Roboto-Regular.ttf",
    "Resources/Fonts/opensans/OpenSans-Regular.ttf",
    "Resources/Fonts/Antonio-Regular.ttf",
    "Resources/Fonts/OpenSans-Light.ttf",
    // ── Polices système Windows ─────────────────────────────────────────────
    "C:/Windows/Fonts/arial.ttf",
    "C:/Windows/Fonts/Arial.ttf",
    "C:/Windows/Fonts/consola.ttf",
    // ── Polices système Linux ────────────────────────────────────────────────
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    // ── Polices système macOS ────────────────────────────────────────────────
    "/Library/Fonts/Arial.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    nullptr
};

// =============================================================================
// Construction du maillage texte
// =============================================================================
// Construit les quads pour un mot en ASCII.
// pen_x/pen_y = position de la baseline en pixels écran (Y-down).
// Retourne le nombre de vertices ajoutés.
static uint32 BuildWord(NkFontFace* face, const char* text,
                         float pen_x, float pen_y,
                         float cr, float cg, float cb,
                         VtxText* buf, uint32 maxV) {
    uint32 n = 0;
    for (const char* p = text; *p && n + 6 <= maxV; ++p) {
        uint32 cp = (uint32)(uint8)*p;
        NkGlyph glyph{};
        if (!face->GetGlyph(cp, glyph)) {
            // glyphe absent — avance quand même si possibles métriques
            pen_x += 8.f;
            continue;
        }
        if (glyph.isEmpty) {
            pen_x += glyph.metrics.xAdvance.ToFloat();
            continue;
        }
        if (!glyph.inAtlas) {
            pen_x += glyph.metrics.xAdvance.ToFloat();
            continue;
        }

        float x0 = pen_x + (float)glyph.metrics.bearingX;
        float y0 = pen_y - (float)glyph.metrics.bearingY;   // Y-down
        float x1 = x0 + (float)glyph.metrics.width;
        float y1 = y0 + (float)glyph.metrics.height;
        float u0 = glyph.uv.u0, v0 = glyph.uv.v0;
        float u1 = glyph.uv.u1, v1 = glyph.uv.v1;

        // Triangle 1
        buf[n++] = {x0,y0,u0,v0,cr,cg,cb};
        buf[n++] = {x1,y0,u1,v0,cr,cg,cb};
        buf[n++] = {x1,y1,u1,v1,cr,cg,cb};
        // Triangle 2
        buf[n++] = {x0,y0,u0,v0,cr,cg,cb};
        buf[n++] = {x1,y1,u1,v1,cr,cg,cb};
        buf[n++] = {x0,y1,u0,v1,cr,cg,cb};

        pen_x += glyph.metrics.xAdvance.ToFloat();
    }
    return n;
}

// Convertit un atlas Gray8 (avec stride) en RGBA8 contigu.
// R=G=B=255, A=coverage.  Appelant libère avec ::free().
static uint8* AtlasToRGBA8(const NkBitmap& bm) {
    const int32 w = bm.width, h = bm.height;
    uint8* rgba = (uint8*)::malloc((usize)w * h * 4);
    if (!rgba) return nullptr;
    for (int32 y = 0; y < h; ++y) {
        const uint8* src = bm.RowPtr(y);
        uint8*       dst = rgba + y * w * 4;
        for (int32 x = 0; x < w; ++x) {
            dst[x*4+0] = 255;
            dst[x*4+1] = 255;
            dst[x*4+2] = 255;
            dst[x*4+3] = src[x];   // couverture → alpha
        }
    }
    return rgba;
}

// =============================================================================
// SW CPU shaders
// =============================================================================
static NkVertexSoftwareShader MakeBgVertSW() {
    return [](const void* vdata, uint32 idx, const void*) -> NkVertexSoftware {
        const VtxBg* v = static_cast<const VtxBg*>(vdata) + idx;
        NkVertexSoftware out; out.position={v->x,v->y,0,1}; out.color={v->r,v->g,v->b,1};
        return out;
    };
}
static NkSWPixelShader MakeBgFragSW() {
    return [](const NkVertexSoftware& v, const void*, const void*) -> NkSWColor {
        return v.color;
    };
}

static NkVertexSoftwareShader MakeTextVertSW() {
    return [](const void* vdata, uint32 idx, const void* udata) -> NkVertexSoftware {
        const VtxText* v  = static_cast<const VtxText*>(vdata) + idx;
        const TextUbo* ub = static_cast<const TextUbo*>(udata);
        NkVertexSoftware out;
        float cx,cy,cz,cw;
        if (ub) {
            MulMV4(ub->ortho, v->x, v->y, 0.f, 1.f, cx,cy,cz,cw);
        } else {
            cx=v->x; cy=v->y; cz=0; cw=1;
        }
        out.position = {cx,cy,cz,cw};
        out.uv       = {v->u, v->v};
        out.color    = {v->r, v->g, v->b, 1.f};
        return out;
    };
}
static NkSWPixelShader MakeTextFragSW() {
    return [](const NkVertexSoftware& v, const void* udata, const void*) -> NkSWColor {
        const TextUbo* ub = static_cast<const TextUbo*>(udata);
        if (!ub || !ub->swAtlas) return {v.color.r, v.color.g, v.color.b, 1.f};
        NkSWColor tex = ub->swAtlas->Sample(v.uv.x, v.uv.y, 0);
        return {v.color.r, v.color.g, v.color.b, tex.a};   // alpha = coverage
    };
}

// =============================================================================
// Shaders multi-backend
// =============================================================================
static NkShaderHandle MakeBgShader(NkIDevice* dev, NkGraphicsApi api) {
    NkShaderDesc sd; sd.debugName = "FontBg";
    // Déclarés à portée de fonction pour rester valides jusqu'à CreateShader.
    NkVertexSoftwareShader vs; 
    NkSWPixelShader fs;
    if (api == NkGraphicsApi::NK_GFX_API_D3D11 || api == NkGraphicsApi::NK_GFX_API_D3D12) {
        sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_BgVS, "VSMain");
        sd.AddHLSL(NkShaderStage::NK_FRAGMENT, kHLSL_BgPS, "PSMain");
    } else if (api == NkGraphicsApi::NK_GFX_API_SOFTWARE) {
        vs = MakeBgVertSW(); fs = MakeBgFragSW();
        NkShaderStageDesc svs; svs.stage=NkShaderStage::NK_VERTEX;   svs.cpuVertFn=&vs; sd.stages.PushBack(svs);
        NkShaderStageDesc sfs; sfs.stage=NkShaderStage::NK_FRAGMENT; sfs.cpuFragFn=&fs; sd.stages.PushBack(sfs);
    } else {
        sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_BgVS);
        sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kGLSL_BgFS);
    }
    return dev->CreateShader(sd);
}

static NkShaderHandle MakeTextShader(NkIDevice* dev, NkGraphicsApi api) {
    NkShaderDesc sd; sd.debugName = "FontText";
    // Déclarés à portée de fonction pour rester valides jusqu'à CreateShader.
    NkVertexSoftwareShader vs; 
    NkSWPixelShader fs;
    if (api == NkGraphicsApi::NK_GFX_API_D3D11 || api == NkGraphicsApi::NK_GFX_API_D3D12) {
        sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_TextVS, "VSMain");
        sd.AddHLSL(NkShaderStage::NK_FRAGMENT, kHLSL_TextPS, "PSMain");
    } else if (api == NkGraphicsApi::NK_GFX_API_SOFTWARE) {
        vs = MakeTextVertSW(); fs = MakeTextFragSW();
        NkShaderStageDesc svs; svs.stage=NkShaderStage::NK_VERTEX;   svs.cpuVertFn=&vs; sd.stages.PushBack(svs);
        NkShaderStageDesc sfs; sfs.stage=NkShaderStage::NK_FRAGMENT; sfs.cpuFragFn=&fs; sd.stages.PushBack(sfs);
    } else {
        sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_TextVS);
        sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kGLSL_TextFS);
    }
    return dev->CreateShader(sd);
}

// =============================================================================
// nkmain
// =============================================================================
int nkmain(const NkEntryState& state) {
    NkGraphicsApi api = ParseBackend(state.args);
    logger_src.Infof("[FontDemo] Backend : %s\n", ApiName(api));

    const bool isSW = (api == NkGraphicsApi::NK_GFX_API_SOFTWARE);

    // ── Recherche de police ────────────────────────────────────────────────
    const char* fontPath = nullptr;
    for (usize i = 1; i < state.args.Size(); i++) {
        const char* s = state.args[i].CStr();
        if (::strncmp(s, "--font=", 7) == 0) { fontPath = s + 7; break; }
    }
    if (!fontPath) {
        for (int k = 0; sFontPaths[k]; ++k) {
            FILE* f = ::fopen(sFontPaths[k], "rb");
            if (f) { ::fclose(f); fontPath = sFontPaths[k]; break; }
        }
    }
    if (!fontPath) {
        logger_src.Errorf("[FontDemo] Aucune police trouvee. Utilisez --font=<chemin.ttf>\n");
        return 1;
    }
    logger_src.Infof("[FontDemo] Police : %s\n", fontPath);

    // ── Chargement de la police ────────────────────────────────────────────
    usize fontDataSize = 0;
    logger_src.Infof("[FontDemo] Lecture fichier...\n");
    uint8* fontData = LoadFileToMemory(fontPath, fontDataSize);
    if (!fontData) {
        logger_src.Errorf("[FontDemo] Impossible de lire %s\n", fontPath);
        return 1;
    }
    logger_src.Infof("[FontDemo] Fichier lu : %u octets\n", (uint32)fontDataSize);

    // ── Initialisation NkFontLibrary ──────────────────────────────────────
    NkFontLibrary lib;
    logger_src.Infof("[FontDemo] NkFontLibrary::Init...\n");
    if (!lib.Init()) {
        logger_src.Errorf("[FontDemo] NkFontLibrary::Init echoue\n");
        ::free(fontData); return 1;
    }
    logger_src.Infof("[FontDemo] NkFontLibrary::Init OK\n");

    // Charger 3 tailles pour 3 mots
    logger_src.Infof("[FontDemo] LoadFont 48px...\n");
    NkFontFace* faceRihen   = lib.LoadFont(fontData, fontDataSize, 48);
    logger_src.Infof("[FontDemo] LoadFont 64px...\n");
    NkFontFace* faceNkentseu= lib.LoadFont(fontData, fontDataSize, 64);
    logger_src.Infof("[FontDemo] LoadFont 40px...\n");
    NkFontFace* faceUnkeny  = lib.LoadFont(fontData, fontDataSize, 40);

    if (!faceRihen || !faceNkentseu || !faceUnkeny) {
        logger_src.Errorf("[FontDemo] Echec chargement NkFontFace (r=%p n=%p u=%p)\n",
                          (void*)faceRihen, (void*)faceNkentseu, (void*)faceUnkeny);
        ::free(fontData); lib.Destroy(); return 1;
    }
    logger_src.Infof("[FontDemo] Faces chargees OK\n");

    // ── Préchargement des glyphes → atlas ─────────────────────────────────
    // fontData DOIT rester valide jusqu'ici : rawData est un pointeur non-propriétaire.
    logger_src.Infof("[FontDemo] PreloadASCII...\n");
    faceRihen->PreloadASCII();
    faceNkentseu->PreloadASCII();
    faceUnkeny->PreloadASCII();
    logger_src.Infof("[FontDemo] PreloadASCII OK\n");

    // Libération du buffer TTF après rastérisation complète.
    ::free(fontData); fontData = nullptr;

    // ── Fenêtre ────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title     = "NkFontDemo — RIHEN / NKENTSEU / UNKENY (ESC=quit)";
    winCfg.width     = 900;
    winCfg.height    = 500;
    winCfg.resizable = true;
    NkWindow window;
    if (!window.Create(winCfg)) {
        logger_src.Errorf("[FontDemo] Echec creation fenetre\n");
        lib.Destroy(); return 1;
    }
    uint32 W = winCfg.width, H = winCfg.height;

    // ── Device RHI ────────────────────────────────────────────────────────
    NkDeviceInitInfo di;
    di.api    = api;
    di.surface= window.GetSurfaceDesc();
    di.width  = W; di.height = H;
    di.context.vulkan.appName    = "NkFontDemo";
    di.context.vulkan.engineName = "Unkenty";
    di.context.software.threading= true;
    di.context.software.useSSE   = true;
    NkIDevice* device = NkDeviceFactory::Create(di);
    if (!device || !device->IsValid()) {
        logger_src.Errorf("[FontDemo] Echec creation device\n");
        lib.Destroy(); window.Close(); return 1;
    }

    // ── Construction du maillage texte ────────────────────────────────────
    // NOTE: les coordonnées Y sont pour un ortho(0,W, H,0) → Y-down
    gTextVertCount = 0;
    uint32 n;
    n = BuildWord(faceRihen,    "RIHEN",    60.f, 200.f, 1.00f,0.75f,0.15f,
                  gTextVerts + gTextVertCount, kMaxTextVerts - gTextVertCount);
    gTextVertCount += n;
    n = BuildWord(faceNkentseu, "NKENTSEU", 60.f, 310.f, 0.20f,0.80f,1.00f,
                  gTextVerts + gTextVertCount, kMaxTextVerts - gTextVertCount);
    gTextVertCount += n;
    n = BuildWord(faceUnkeny,   "UNKENY",   60.f, 420.f, 0.75f,1.00f,0.20f,
                  gTextVerts + gTextVertCount, kMaxTextVerts - gTextVertCount);
    gTextVertCount += n;

    logger_src.Infof("[FontDemo] %u vertices texte construits\n", gTextVertCount);

    // ── Atlas GPU : Gray8 → RGBA8 ─────────────────────────────────────────
    // Les 3 faces partagent le même atlas (celui de NkFontLibrary)
    NkTextureAtlas* atlas = lib.GetAtlas();
    const int32 atlasW = atlas->bitmap.width;
    const int32 atlasH = atlas->bitmap.height;
    uint8* atlasRGBA = AtlasToRGBA8(atlas->bitmap);
    if (!atlasRGBA) {
        logger_src.Errorf("[FontDemo] Allocation atlas RGBA echouee\n");
        NkDeviceFactory::Destroy(device); lib.Destroy(); window.Close(); return 1;
    }

    NkTextureDesc atlasTd = NkTextureDesc::Tex2D(
        (uint32)atlasW, (uint32)atlasH, NkGPUFormat::NK_RGBA8_UNORM);
    atlasTd.initialData = atlasRGBA;
    atlasTd.debugName   = "FontAtlas";
    NkTextureHandle hAtlasTex = device->CreateTexture(atlasTd);
    ::free(atlasRGBA); atlasRGBA = nullptr;

    if (!hAtlasTex.IsValid()) {
        logger_src.Errorf("[FontDemo] Echec creation texture atlas %dx%d\n", atlasW, atlasH);
        NkDeviceFactory::Destroy(device); lib.Destroy(); window.Close(); return 1;
    }
    logger_src.Infof("[FontDemo] Atlas GPU %dx%d OK\n", atlasW, atlasH);

    // ── Sampler atlas (non-SW) ────────────────────────────────────────────
    NkSamplerHandle hAtlasSampler;
    if (!isSW) {
        NkSamplerDesc sd; sd.minFilter=NkFilter::NK_LINEAR; sd.magFilter=NkFilter::NK_LINEAR;
        hAtlasSampler = device->CreateSampler(sd);
    }

    // ── Buffers ───────────────────────────────────────────────────────────
    NkBufferHandle hVBBg   = device->CreateBuffer(NkBufferDesc::Vertex(sizeof(kBgVerts), kBgVerts));
    NkBufferHandle hVBText;
    if (gTextVertCount > 0)
        hVBText = device->CreateBuffer(NkBufferDesc::Vertex(
            (uint64)gTextVertCount * sizeof(VtxText), gTextVerts));

    NkBufferHandle hUBOText = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(TextUbo)));

    // ── Descriptor set layouts ────────────────────────────────────────────
    // Layout A : background (pas de sampler)
    NkDescriptorSetLayoutDesc dslA;
    dslA.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
    NkDescSetHandle hDSLA = device->CreateDescriptorSetLayout(dslA);

    // Layout B : texte — UBO + atlas texture (non-SW)
    NkDescriptorSetLayoutDesc dslB;
    dslB.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
    if (!isSW)
        dslB.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
    NkDescSetHandle hDSLB = device->CreateDescriptorSetLayout(dslB);

    NkDescSetHandle hDSText = device->AllocateDescriptorSet(hDSLB);

    // Bind UBO texte
    {
        NkDescriptorWrite w; w.set=hDSText; w.binding=0;
        w.type=NkDescriptorType::NK_UNIFORM_BUFFER; w.buffer=hUBOText;
        device->UpdateDescriptorSets(&w,1);
    }
    // Bind atlas texture (non-SW)
    if (!isSW && hAtlasTex.IsValid() && hAtlasSampler.IsValid()) {
        NkDescriptorWrite w; w.set=hDSText; w.binding=1;
        w.type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
        w.texture=hAtlasTex; w.sampler=hAtlasSampler;
        w.textureLayout=NkResourceState::NK_SHADER_READ;
        device->UpdateDescriptorSets(&w,1);
    }

    // ── Shaders ───────────────────────────────────────────────────────────
    NkShaderHandle hShBg   = MakeBgShader  (device, api);
    NkShaderHandle hShText = MakeTextShader(device, api);

    // ── Swapchain ─────────────────────────────────────────────────────────
    NkRenderPassHandle  hRPSwap = device->GetSwapchainRenderPass();
    NkFramebufferHandle hFBSwap = device->GetSwapchainFramebuffer();

    // ── Pipelines ─────────────────────────────────────────────────────────
    auto makeVL1 = [](uint32 stride) {
        NkVertexLayout vl;
        vl.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT,  0, "POSITION", 0);
        vl.AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT, 8, "COLOR",    0);
        vl.AddBinding(0, stride);
        return vl;
    };
    auto makeVL2 = [](uint32 stride) {
        NkVertexLayout vl;
        vl.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT,  0,  "POSITION", 0);
        vl.AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT,  8,  "TEXCOORD", 0);
        vl.AddAttribute(2, 0, NkGPUFormat::NK_RGB32_FLOAT, 16, "COLOR",    0);
        vl.AddBinding(0, stride);
        return vl;
    };

    auto makePipe = [&](NkShaderHandle sh, NkVertexLayout vl,
                        bool blend, NkDescSetHandle dsl) -> NkPipelineHandle {
        NkGraphicsPipelineDesc pd;
        pd.shader       = sh;
        pd.vertexLayout = vl;
        pd.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        pd.rasterizer   = NkRasterizerDesc::Default();
        pd.depthStencil = NkDepthStencilDesc::NoDepth();
        pd.blend        = blend ? NkBlendDesc::Alpha() : NkBlendDesc::Opaque();
        pd.renderPass   = hRPSwap;
        if (dsl.IsValid()) pd.descriptorSetLayouts.PushBack(dsl);
        return device->CreateGraphicsPipeline(pd);
    };

    NkPipelineHandle hPipeBg   = makePipe(hShBg,   makeVL1(sizeof(VtxBg)),   false, hDSLA);
    NkPipelineHandle hPipeText = makePipe(hShText,  makeVL2(sizeof(VtxText)), true,  hDSLB);

    // ── Pointer atlas SW ──────────────────────────────────────────────────
    NkSWTexture* swAtlas = nullptr;
    if (isSW) {
        auto* swDev = static_cast<NkSoftwareDevice*>(device);
        if (hAtlasTex.IsValid()) swAtlas = swDev->GetTex(hAtlasTex.id);
    }

    // ── Command buffer ────────────────────────────────────────────────────
    NkICommandBuffer* cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);

    bool running = true;
    NkEventSystem& events = NkEvents();
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = e->GetWidth(); H = e->GetHeight();
    });

    // ── Boucle principale ─────────────────────────────────────────────────
    while (running) {
        events.PollEvents();
        if (!running) break;
        if (W == 0 || H == 0) continue;
        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight()) {
            device->OnResize(W, H);
            hRPSwap = device->GetSwapchainRenderPass();
            hFBSwap = device->GetSwapchainFramebuffer();
        }

        NkFrameContext frame;
        device->BeginFrame(frame);

        // ── Mise à jour UBO texte ────────────────────────────────────────
        {
            TextUbo ub;
            Mat4Ortho(ub.ortho, 0.f,(float)W, (float)H,0.f, -1.f,1.f);
            ub.swAtlas = swAtlas;  // nullptr sur GPU
            device->WriteBuffer(hUBOText, &ub, sizeof(ub), 0);
        }

        cmd->Begin();
        {
            NkRect2D area{0,0,(int32)W,(int32)H};
            cmd->BeginRenderPass(hRPSwap, hFBSwap, area);
            NkViewport vp{0.f,0.f,(float)W,(float)H,0.f,1.f};
            cmd->SetViewport(vp); cmd->SetScissor(area);

            // Background
            cmd->BindGraphicsPipeline(hPipeBg);
            uint64 off=0; cmd->BindVertexBuffer(0, hVBBg, off);
            cmd->Draw(6, 1, 0, 0);

            // Texte
            if (gTextVertCount > 0 && hVBText.IsValid()) {
                cmd->BindGraphicsPipeline(hPipeText);
                cmd->BindDescriptorSet(hDSText, 0, nullptr, 0);
                cmd->BindVertexBuffer(0, hVBText, off);
                cmd->Draw(gTextVertCount, 1, 0, 0);
            }

            cmd->EndRenderPass();
        }
        cmd->End();
        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);
    }

    // ── Cleanup ───────────────────────────────────────────────────────────
    device->WaitIdle();
    device->DestroyCommandBuffer(cmd);
    device->DestroyBuffer(hVBBg);
    if (hVBText.IsValid()) device->DestroyBuffer(hVBText);
    device->DestroyBuffer(hUBOText);
    device->DestroyShader(hShBg); device->DestroyShader(hShText);
    device->DestroyPipeline(hPipeBg); device->DestroyPipeline(hPipeText);
    device->DestroyTexture(hAtlasTex);
    if (hAtlasSampler.IsValid()) device->DestroySampler(hAtlasSampler);
    device->FreeDescriptorSet(hDSText);
    device->DestroyDescriptorSetLayout(hDSLA);
    device->DestroyDescriptorSetLayout(hDSLB);
    NkDeviceFactory::Destroy(device);
    lib.Destroy();
    window.Close();
    return 0;
}
