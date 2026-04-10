// =============================================================================
// NkRHIDemoFull_NkSL.cpp  — v3.0 (NkSL shader language)
//
// Même démo que v2.0 mais tous les shaders sont écrits en NkSL.
// Le compilateur NkSL traduit automatiquement vers la cible de l'API active :
//   OpenGL   → GLSL 4.50
//   Vulkan   → SPIR-V (via glslang embarqué)
//   DX11/12  → HLSL SM5
//   Metal    → MSL 2.0 (via SPIRV-Cross si dispo, sinon natif)
//   Software → C++ lambdas
//
// Plus aucun #include de .inl SPIR-V précompilé, plus aucun bloc HLSL/MSL/GLSL
// inline. Un seul source NkSL par shader, compilé à l'exécution.
//
// Pour tester NkSL, lancer avec n'importe quel --backend= :
//   ./NkRHIDemoFull_NkSL --backend=opengl
//   ./NkRHIDemoFull_NkSL --backend=vulkan
//   ./NkRHIDemoFull_NkSL --backend=dx11
//   ./NkRHIDemoFull_NkSL --backend=dx12
//   ./NkRHIDemoFull_NkSL --backend=metal
//   ./NkRHIDemoFull_NkSL --backend=sw
//
// En cas d'échec de compilation NkSL, le log affiche le code généré pour debug :
//   [NkSL] Compiled shader.nksl stage=vertex target=GLSL (1234 bytes)
//   [NkSL] Error line 12: undeclared identifier 'aPos'
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKTime/NkTime.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Software/NkSoftwareDevice.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"
#include "NKImage/Core/NkImage.h"
#include "NKFont/NKFont.h"

// ── NkSL : le seul include shader nécessaire ─────────────────────────────────
#include "NKRHI/SL/NkSLIntegration.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    struct NkEntryState;
}

using namespace nkentseu;
using namespace nkentseu::math;

// =============================================================================
// Constante de tiling UV pour le plan
// =============================================================================
static constexpr float kPlaneUVScale = 3.0f;

// =============================================================================
// Sources NkSL
//
// Syntaxe NkSL = GLSL-like avec annotations @ pour les bindings/stages.
// Le compilateur gère automatiquement toutes les différences entre APIs :
//   - séparation texture/sampler (HLSL/MSL)
//   - semantics HLSL (SV_Position, TEXCOORD0, etc.)
//   - address spaces MSL (constant, device, thread)
//   - coordonnées Z NDC ([-1,1] vs [0,1])
//   - column-major / row-major matrices
// =============================================================================

// ── Shader principal : Phong + shadow PCF + texture albedo ───────────────────
//
// Layout :
//   binding 0 = MainUBO (matrices + lumière)
//   binding 1 = sampler2DShadow uShadowMap
//   binding 2 = sampler2D       uAlbedoMap
//
// Vertex inputs :
//   location 0 = aPos     (vec3)
//   location 1 = aNormal  (vec3)
//   location 2 = aColor   (vec4 — w = hasTexture flag)
//   location 3 = aUV      (vec2)
// =============================================================================
static constexpr const char* kNkSL_MainVert = R"NKSL(
@binding(set=0, binding=0)
uniform MainUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  lightDirW;
    vec4  eyePosW;
    float ndcZScale;
    float ndcZOffset;
    float _pad0;
    float _pad1;
} ubo;

@location(0) in vec3 aPos;
@location(1) in vec3 aNormal;
@location(2) in vec4 aColor;   // xyz=couleur, w=hasTexture (0 ou 1)
@location(3) in vec2 aUV;

@location(0) out vec3 vWorldPos;
@location(1) out vec3 vNormal;
@location(2) out vec4 vColor;
@location(3) out vec4 vShadowCoord;
@location(4) out vec2 vUV;

@stage(vertex)
@entry
void main() {
    vec4 worldPos    = ubo.model * vec4(aPos, 1.0);
    vWorldPos        = worldPos.xyz;
    mat3 normalMat   = transpose(inverse(mat3(ubo.model)));
    vNormal          = normalize(normalMat * aNormal);
    vColor           = aColor;
    vShadowCoord     = ubo.lightVP * worldPos;
    vUV              = aUV;
    gl_Position      = ubo.proj * ubo.view * worldPos;
}
)NKSL";

static constexpr const char* kNkSL_MainFrag = R"NKSL(
@binding(set=0, binding=0)
uniform MainUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  lightDirW;
    vec4  eyePosW;
    float ndcZScale;
    float ndcZOffset;
    float _pad0;
    float _pad1;
} ubo;

@binding(set=0, binding=1)
uniform sampler2DShadow uShadowMap;

@binding(set=0, binding=2)
uniform sampler2D uAlbedoMap;

@location(0) in vec3 vWorldPos;
@location(1) in vec3 vNormal;
@location(2) in vec4 vColor;
@location(3) in vec4 vShadowCoord;
@location(4) in vec2 vUV;

@location(0) out vec4 fragColor;

@stage(fragment)
@entry
void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-ubo.lightDirW.xyz);
    vec3 V = normalize(ubo.eyePosW.xyz - vWorldPos);
    vec3 H = normalize(L + V);

    // Coordonnées shadow : NDC → [0,1] avec scale/offset selon l'API
    vec3 proj  = vShadowCoord.xyz / vShadowCoord.w;
    float su   = proj.x * 0.5 + 0.5;
    float sv   = proj.y * 0.5 + 0.5;
    float sz   = proj.z * ubo.ndcZScale + ubo.ndcZOffset;

    float cosA = max(dot(N, L), 0.0);
    float bias = mix(0.005, 0.0005, cosA);
    sz -= bias;

    float shadow = 1.0;
    if (su >= 0.0 && su <= 1.0 && sv >= 0.0 && sv <= 1.0 && sz <= 1.0) {
        vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
        shadow = 0.0;
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                float ox = float(x) * texelSize.x;
                float oy = float(y) * texelSize.y;
                shadow += texture(uShadowMap, vec3(su + ox, sv + oy, sz));
            }
        }
        shadow /= 9.0;
    }
    shadow = max(shadow, 0.35);

    // Couleur de base + texture albedo si hasTexture (vColor.w > 0.5)
    vec3 baseColor = vColor.rgb;
    if (vColor.w > 0.5) {
        baseColor *= texture(uAlbedoMap, clamp(vUV, 0.0, 1.0)).rgb;
    }

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 32.0);

    vec3 ambient  = 0.15 * baseColor;
    vec3 diffuse  = shadow * diff * baseColor;
    vec3 specular = shadow * spec * vec3(0.4);
    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
)NKSL";

// ── Shadow depth pass : position seule ───────────────────────────────────────
static constexpr const char* kNkSL_ShadowVert = R"NKSL(
@binding(set=0, binding=0)
uniform ShadowUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  lightDirW;
    vec4  eyePosW;
    float ndcZScale;
    float ndcZOffset;
    float _pad0;
    float _pad1;
} ubo;

@location(0) in vec3 aPos;

@stage(vertex)
@entry
void main() {
    gl_Position = ubo.lightVP * ubo.model * vec4(aPos, 1.0);
}
)NKSL";

static constexpr const char* kNkSL_ShadowFrag = R"NKSL(
@stage(fragment)
@entry
void main() {
    // Depth-only pass : aucune sortie couleur
}
)NKSL";

// ── Shader texte 2D/3D ────────────────────────────────────────────────────────
//   binding 0 = TextUBO { mat4 mvp; vec4 color; }
//   binding 1 = sampler2D uText (atlas Gray8)
static constexpr const char* kNkSL_TextVert = R"NKSL(
@binding(set=0, binding=0)
uniform TextUBO {
    mat4 mvp;
    vec4 color;
} ubo;

@location(0) in vec2 aPos;
@location(1) in vec2 aUV;

@location(0) out vec2 vUV;

@stage(vertex)
@entry
void main() {
    vUV         = aUV;
    gl_Position = ubo.mvp * vec4(aPos, 0.0, 1.0);
}
)NKSL";

static constexpr const char* kNkSL_TextFrag = R"NKSL(
@binding(set=0, binding=0)
uniform TextUBO {
    mat4 mvp;
    vec4 color;
} ubo;

@binding(set=0, binding=1)
uniform sampler2D uText;

@location(0) in vec2 vUV;

@location(0) out vec4 outColor;

@stage(fragment)
@entry
void main() {
    float a  = texture(uText, vUV).r;
    outColor = vec4(ubo.color.rgb, ubo.color.a * a);
}
)NKSL";

// =============================================================================
// UBO (std140-compatible, 16-byte aligned)
// =============================================================================
struct alignas(16) UboData {
    float model[16];
    float view[16];
    float proj[16];
    float lightVP[16];
    float lightDirW[4];
    float eyePosW[4];
    float ndcZScale;
    float ndcZOffset;
    float _pad[2];
};

// =============================================================================
// Géométrie — identique à v2.0 (Vtx3D.color = vec4, w = hasTexture)
// =============================================================================
struct Vtx3D {
    NkVec3f pos;
    NkVec3f normal;
    NkVec4f color;   // xyz=couleur, w=hasTexture (0 ou 1)
    NkVec2f uv;
};

static void Mat4ToArray(const NkMat4f& m, float out[16]) {
    mem::NkCopy(out, m.data, 16 * sizeof(float));
}

static NkVector<Vtx3D> MakeCube(float r=1.f, float g=0.45f, float b=0.2f) {
    static const float P=0.5f, N=-0.5f;
    struct Face { float vx[4][3]; float uv[4][2]; float nx,ny,nz; };
    static const Face faces[6] = {
        {{{P,N,P},{P,P,P},{N,P,P},{N,N,P}},{{1,0},{1,1},{0,1},{0,0}}, 0, 0, 1},
        {{{N,N,N},{N,P,N},{P,P,N},{P,N,N}},{{1,0},{1,1},{0,1},{0,0}}, 0, 0,-1},
        {{{N,N,N},{P,N,N},{P,N,P},{N,N,P}},{{0,1},{1,1},{1,0},{0,0}}, 0,-1, 0},
        {{{N,P,P},{P,P,P},{P,P,N},{N,P,N}},{{0,1},{1,1},{1,0},{0,0}}, 0, 1, 0},
        {{{N,N,N},{N,N,P},{N,P,P},{N,P,N}},{{0,0},{1,0},{1,1},{0,1}},-1, 0, 0},
        {{{P,N,P},{P,N,N},{P,P,N},{P,P,P}},{{0,0},{1,0},{1,1},{0,1}}, 1, 0, 0},
    };
    static const int idx[6]={0,1,2,0,2,3};
    NkVector<Vtx3D> v; v.Reserve(36);
    for (const auto& f : faces)
        for (int i : idx)
            v.PushBack({NkVec3f(f.vx[i][0],f.vx[i][1],f.vx[i][2]),
                        NkVec3f(f.nx,f.ny,f.nz),
                        NkVec4f(r,g,b,0.f),   // w=0 : pas de texture
                        NkVec2f(f.uv[i][0],f.uv[i][1])});
    return v;
}

static NkVector<Vtx3D> MakeSphere(int stacks=16, int slices=24,
                                    float r=0.2f, float g=0.55f, float b=0.9f) {
    NkVector<Vtx3D> v;
    const float pi=(float)NkPi;
    for (int i=0;i<stacks;i++) {
        float phi0=(float)i/stacks*pi, phi1=(float)(i+1)/stacks*pi;
        for (int j=0;j<slices;j++) {
            float th0=(float)j/slices*2.f*pi, th1=(float)(j+1)/slices*2.f*pi;
            auto mk=[&](float phi,float th)->Vtx3D{
                float x=NkSin(phi)*NkCos(th), y=NkCos(phi), z=NkSin(phi)*NkSin(th);
                return {NkVec3f(x*.5f,y*.5f,z*.5f),NkVec3f(x,y,z),
                        NkVec4f(r,g,b,0.f),NkVec2f(th/(2.f*pi),phi/pi)};
            };
            Vtx3D a=mk(phi0,th0),b2=mk(phi0,th1),c=mk(phi1,th0),d=mk(phi1,th1);
            v.PushBack(a); v.PushBack(b2); v.PushBack(d);
            v.PushBack(a); v.PushBack(d);  v.PushBack(c);
        }
    }
    return v;
}

static NkVector<Vtx3D> MakePlane(float sz=3.f,
                                   float r=0.35f, float g=0.65f, float b=0.35f) {
    const float h=sz*.5f, uM=kPlaneUVScale, vM=kPlaneUVScale;
    NkVector<Vtx3D> v; v.Reserve(6);
    v.PushBack({NkVec3f(-h,0.f, h),NkVec3f(0,1,0),NkVec4f(r,g,b,1.f),NkVec2f(0.f, 0.f)});
    v.PushBack({NkVec3f( h,0.f, h),NkVec3f(0,1,0),NkVec4f(r,g,b,1.f),NkVec2f(uM,  0.f)});
    v.PushBack({NkVec3f( h,0.f,-h),NkVec3f(0,1,0),NkVec4f(r,g,b,1.f),NkVec2f(uM,  vM )});
    v.PushBack({NkVec3f(-h,0.f, h),NkVec3f(0,1,0),NkVec4f(r,g,b,1.f),NkVec2f(0.f, 0.f)});
    v.PushBack({NkVec3f( h,0.f,-h),NkVec3f(0,1,0),NkVec4f(r,g,b,1.f),NkVec2f(uM,  vM )});
    v.PushBack({NkVec3f(-h,0.f,-h),NkVec3f(0,1,0),NkVec4f(r,g,b,1.f),NkVec2f(0.f, vM )});
    return v;
}

// =============================================================================
// Utilitaires
// =============================================================================
static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (size_t i=1;i<args.Size();i++) {
        if (args[i]=="--backend=vulkan" ||args[i]=="-bvk")  return NkGraphicsApi::NK_API_VULKAN;
        if (args[i]=="--backend=dx11"   ||args[i]=="-bdx11") return NkGraphicsApi::NK_API_DIRECTX11;
        if (args[i]=="--backend=dx12"   ||args[i]=="-bdx12") return NkGraphicsApi::NK_API_DIRECTX12;
        if (args[i]=="--backend=metal"  ||args[i]=="-bmtl")  return NkGraphicsApi::NK_API_METAL;
        if (args[i]=="--backend=sw"     ||args[i]=="-bsw")   return NkGraphicsApi::NK_API_SOFTWARE;
        if (args[i]=="--backend=opengl" ||args[i]=="-bgl")   return NkGraphicsApi::NK_API_OPENGL;
    }
    return NkGraphicsApi::NK_API_OPENGL;
}

static bool HasArg(const NkVector<NkString>& args,const char* ln,const char* sn=nullptr){
    for (size_t i=1;i<args.Size();++i){
        if(args[i]==ln)return true;
        if(sn&&args[i]==sn)return true;
    }
    return false;
}

static bool IsSupportedImageExtension(const std::string& e){
    return e==".png"||e==".jpg"||e==".jpeg"||e==".bmp"||e==".tga"||e==".hdr"||
           e==".ppm"||e==".pgm"||e==".pbm"||e==".qoi"||e==".gif"||e==".ico"||
           e==".webp"||e==".svg";
}

static std::string ToLowerCopy(std::string s){
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return(char)std::tolower(c);});
    return s;
}

static int TextureCandidateScore(const std::filesystem::path& p){
    const std::string fl=ToLowerCopy(p.filename().string());
    const std::string pl=ToLowerCopy(p.string());
    int score=0;
    if(fl.find("checker") !=std::string::npos)score+=200;
    if(fl.find("albedo")  !=std::string::npos)score+=850;
    if(fl.find("diffuse") !=std::string::npos)score+=900;
    if(fl.find("basecolor")!=std::string::npos)score+=820;
    if(fl.find("color")   !=std::string::npos)score+=300;
    if(fl.find("brick")   !=std::string::npos)score+=120;
    if(fl.find("ao")      !=std::string::npos||fl.find("ambient")!=std::string::npos)score-=800;
    if(fl.find("normal")  !=std::string::npos)score-=900;
    if(fl.find("rough")   !=std::string::npos)score-=900;
    if(fl.find("metal")   !=std::string::npos)score-=900;
    if(fl.find("spec")    !=std::string::npos)score-=700;
    if(fl.find("height")  !=std::string::npos||fl.find("disp")!=std::string::npos)score-=700;
    if(pl.find("resources\\textures")!=std::string::npos||
       pl.find("resources/textures") !=std::string::npos)score+=120;
    const std::string ext=ToLowerCopy(p.extension().string());
    if(ext==".jpg"||ext==".jpeg")score+=40;
    if(ext==".png")score+=20;
    return score;
}

static std::string FindTextureInResources(){
    namespace fs=std::filesystem;
    std::vector<fs::path> roots;
    auto pushRoot=[&](const fs::path& p){
        auto n=p.lexically_normal();
        for(auto& e:roots)if(e==n)return;
        roots.push_back(n);
    };
    pushRoot(fs::path("Resources/Textures"));
    pushRoot(fs::path("Resources"));
    std::error_code ec;
    fs::path cursor=fs::current_path(ec);
    if(!ec)for(int i=0;i<8;++i){
        pushRoot(cursor/"Resources/Textures");
        pushRoot(cursor/"Resources");
        if(!cursor.has_parent_path()||cursor.parent_path()==cursor)break;
        cursor=cursor.parent_path();
    }
    struct Cand{std::string path;int score=0;};
    std::vector<Cand> cands;
    for(const fs::path& root:roots){
        std::error_code e;
        if(!fs::exists(root,e)||!fs::is_directory(root,e))continue;
        for(fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,e),end;
            it!=end;it.increment(e)){
            if(e)break;
            if(!it->is_regular_file())continue;
            std::string ext=ToLowerCopy(it->path().extension().string());
            if(IsSupportedImageExtension(ext)){
                Cand c; std::error_code ae;
                auto abs=fs::absolute(it->path(),ae);
                c.path=(ae?it->path():abs).lexically_normal().string();
                c.score=TextureCandidateScore(it->path());
                cands.push_back(c);
            }
        }
        if(!cands.empty())break;
    }
    if(cands.empty())return{};
    std::sort(cands.begin(),cands.end(),[](const Cand& a,const Cand& b){
        return a.score!=b.score?a.score>b.score:a.path<b.path;
    });
    return cands.front().path;
}

static bool SaveImageVariants(const NkImage& img,
                               const std::filesystem::path& dir,
                               const std::string& stem){
    namespace fs=std::filesystem;
    std::error_code ec;
    fs::create_directories(dir,ec);
    if(ec)return false;
    const std::string b=(dir/stem).string();
    bool ok=false;
    ok|=img.SavePNG ((b+".png").c_str());
    ok|=img.SaveJPEG((b+".jpg").c_str(),92);
    ok|=img.SaveBMP ((b+".bmp").c_str());
    ok|=img.SaveTGA ((b+".tga").c_str());
    ok|=img.SavePPM ((b+".ppm").c_str());
    ok|=img.SaveQOI ((b+".qoi").c_str());
    ok|=img.SaveHDR ((b+".hdr").c_str());
    return ok;
}

static bool SaveSoftwareScene(NkSoftwareDevice* swDev,
                               const std::filesystem::path& dir,
                               const std::string& stem){
    if(!swDev)return false;
    const uint8* px=swDev->BackbufferPixels();
    uint32 w=swDev->BackbufferWidth(),h=swDev->BackbufferHeight();
    if(!px||!w||!h)return false;
    NkImage* img=NkImage::Wrap(const_cast<uint8*>(px),(int32)w,(int32)h,NkImagePixelFormat::NK_RGBA32);
    if(!img||!img->IsValid()){if(img)img->Free();return false;}
    bool ok=SaveImageVariants(*img,dir,stem);
    img->Free();
    return ok;
}

// =============================================================================
// NkSL shader compilation helpers
//
// CompileNkSLShader : compile un source NkSL pour le backend actif et remplit
// le NkShaderDesc. En cas d'erreur, log détaillé avec les messages du compilateur
// ET le code source généré (pour diagnostiquer les bugs NkSL ↔ backend).
// =============================================================================
static bool CompileNkSLShader(
    NkIDevice*          device,
    NkGraphicsApi       api,
    const char*         nkslVertSrc,
    const char*         nkslFragSrc,
    const char*         debugName,
    NkShaderDesc&       outDesc,
    NkVector<NkString>& outSourceStorage,  // garde les sources NkString vivantes
    NkSLReflection*     outReflect = nullptr)
{
    NkSLCompiler& compiler = nksl::GetCompiler();
    NkSLTarget target = nksl::ApiToTarget(api);

    NkSLCompileOptions opts;
    opts.glslVersion    = 450;
    opts.hlslShaderModel= 50;
    opts.mslVersion     = 200;
    opts.vulkanVersion  = 100;
    opts.optimize       = false;  // désactivé pour faciliter le debug NkSL
    opts.debugInfo      = true;
    opts.preferSpirvCrossForMSL = true;

    outDesc.debugName = debugName;
    bool ok = true;

    // ── Vertex ───────────────────────────────────────────────────────────────
    auto vertResult = compiler.CompileWithReflection(
        NkString(nkslVertSrc),
        NkSLStage::NK_VERTEX,
        target, opts,
        NkString(debugName) + ".vert.nksl");

    if (!vertResult.result.success) {
        logger.Errorf("[NkSL] %s vertex compilation FAILED (target=%s):\n",
                      debugName, NkSLTargetName(target));
        for (auto& e : vertResult.result.errors)
            logger.Errorf("  line %u: %s\n", e.line, e.message.CStr());
        ok = false;
    } else {
        logger.Infof("[NkSL] %s vertex OK (%zu bytes source, %zu bytes bytecode) target=%s\n",
                     debugName,
                     (size_t)vertResult.result.source.Size(),
                     (size_t)vertResult.result.bytecode.Size(),
                     NkSLTargetName(target));
        // Log du code généré en mode debug
        if (!vertResult.result.source.Empty())
            logger.Infof("[NkSL] Generated vertex source:\n%s\n",
                         vertResult.result.source.CStr());
    }

    // ── Fragment ─────────────────────────────────────────────────────────────
    auto fragResult = compiler.CompileWithReflection(
        NkString(nkslFragSrc),
        NkSLStage::NK_FRAGMENT,
        target, opts,
        NkString(debugName) + ".frag.nksl");

    if (!fragResult.result.success) {
        logger.Errorf("[NkSL] %s fragment compilation FAILED (target=%s):\n",
                      debugName, NkSLTargetName(target));
        for (auto& e : fragResult.result.errors)
            logger.Errorf("  line %u: %s\n", e.line, e.message.CStr());
        ok = false;
    } else {
        logger.Infof("[NkSL] %s fragment OK (%zu bytes source) target=%s\n",
                     debugName,
                     (size_t)fragResult.result.source.Size(),
                     NkSLTargetName(target));
        if (!fragResult.result.source.Empty())
            logger.Infof("[NkSL] Generated fragment source:\n%s\n",
                         fragResult.result.source.CStr());
    }

    if (!ok) return false;

    // ── Remplir le NkShaderDesc selon la cible ────────────────────────────────
    // IMPORTANT : res.source est un NkString local → copier dans outSourceStorage
    // pour que le const char* reste valide après le retour de cette fonction.
    auto storeSource = [&](const NkString& src) -> const char* {
        outSourceStorage.PushBack(src);
        return outSourceStorage.Back().CStr();
    };
    auto fillStage = [&](const NkSLCompileResult& res, NkShaderStage stage) {
        NkShaderStageDesc sd{};
        sd.stage      = stage;
        sd.entryPoint = "main";
        switch (target) {
            case NkSLTarget::NK_GLSL:
                sd.glslSource = storeSource(res.source);
                break;
            case NkSLTarget::NK_SPIRV:
                if (!res.IsText()) {
                    sd.spirvBinary.Resize((uint32)res.bytecode.Size());
                    memcpy(sd.spirvBinary.Data(), res.bytecode.Data(), (size_t)res.bytecode.Size());
                } else {
                    // Pas de compilateur SPIR-V embarqué → fallback GLSL text
                    logger.Infof("[NkSL] No SPIR-V compiler — falling back to GLSL text\n");
                    sd.glslSource = storeSource(res.source);
                }
                break;
            case NkSLTarget::NK_HLSL:
                sd.hlslSource = storeSource(res.source);
                break;
            case NkSLTarget::NK_MSL:
            case NkSLTarget::NK_MSL_SPIRV_CROSS:
                sd.mslSource = storeSource(res.source);
                break;
            case NkSLTarget::NK_CPLUSPLUS:
                // Software : les shaders CPU sont définis plus bas via NkSWShader
                // On stocke le source comme référence de debug seulement
                sd.glslSource = storeSource(res.source);
                break;
            default:
                break;
        }
        outDesc.AddStage(sd);
    };

    fillStage(vertResult.result, NkShaderStage::NK_VERTEX);
    fillStage(fragResult.result, NkShaderStage::NK_FRAGMENT);

    // Reflection (optionnelle)
    if (outReflect) *outReflect = vertResult.reflection;

    return true;
}

// =============================================================================
// Texte
// =============================================================================
struct TextVtx { float x, y, u, v; };
struct alignas(16) TextUboData { float mvp[16]; float color[4]; };

struct NkTextQuad {
    NkTextureHandle hTex;
    NkSamplerHandle hSampler;
    NkBufferHandle  hVBO;
    NkBufferHandle  hUBO;
    NkDescSetHandle hDesc;
    uint32 texW=0,texH=0,vtxCount=0;
};

static std::string FindFontFile(){
    namespace fs=std::filesystem;
    std::vector<fs::path> roots;
    auto pushRoot=[&](const fs::path& p){
        auto n=p.lexically_normal();
        for(auto& e:roots)if(e==n)return;
        roots.push_back(n);
    };
    pushRoot(fs::path("Resources/Fonts"));
    std::error_code ec;
    fs::path cursor=fs::current_path(ec);
    if(!ec)for(int i=0;i<8;++i){
        pushRoot(cursor/"Resources/Fonts");
        if(!cursor.has_parent_path()||cursor.parent_path()==cursor)break;
        cursor=cursor.parent_path();
    }
    const std::vector<std::string> pref={
        "Roboto-Regular.ttf","Roboto-Light.ttf","OpenSans-Light.ttf",
        "Antonio-Regular.ttf","Antonio-Light.ttf","OCRAEXT.TTF"
    };
    for(const fs::path& root:roots){
        std::error_code e;
        if(!fs::exists(root,e)||!fs::is_directory(root,e))continue;
        for(const auto& p:pref){
            auto c=root/p;
            if(fs::exists(c,e)&&fs::is_regular_file(c,e)){
                auto abs=fs::absolute(c,e);
                return(e?c:abs).lexically_normal().string();
            }
        }
        for(fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,e),end;
            it!=end;it.increment(e)){
            if(e)break;
            if(!it->is_regular_file())continue;
            auto ext=ToLowerCopy(it->path().extension().string());
            if(ext==".ttf"||ext==".otf"){
                auto abs=fs::absolute(it->path(),e);
                return(e?it->path():abs).lexically_normal().string();
            }
        }
    }
    return{};
}

static std::vector<uint8_t> LoadFileBytes(const std::string& path){
    FILE* f=fopen(path.c_str(),"rb");
    if(!f)return{};
    fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);
    if(sz<=0){fclose(f);return{};}
    std::vector<uint8_t> buf((size_t)sz);
    fread(buf.data(),1,(size_t)sz,f);fclose(f);
    return buf;
}

static std::vector<uint8_t> BuildStringBitmap(NkFontFace* face,const char* text, uint32& outW,uint32& outH){
    if(!face||!text||!*text){outW=outH=0;return{};}
    const int32 asc=face->GetAscender(),desc=face->GetDescender();
    const int32 rowH=asc-desc;
    if(rowH<=0){outW=outH=0;return{};}
    uint32 totalW=0;
    // for(const char* p=text;*p;++p){NkGlyph g{};if(face->GetGlyph((uint8_t)*p,g))totalW+=(uint32)g.xAdvance;}
    // if(!totalW)totalW=1;
    // outW=totalW;outH=(uint32)rowH;
    std::vector<uint8_t> bmp(outW*outH,0u);
    // int32 penX=0;
    // for(const char* p=text;*p;++p){
    //     NkGlyph g{};
    //     if(!face->GetGlyph((uint8_t)*p,g)||g.isEmpty||!g.bitmap){
    //         if(face->GetGlyph((uint8_t)*p,g))penX+=g.xAdvance;
    //         continue;
    //     }
    //     int32 dstX=penX+g.bearingX,dstY=asc-g.bearingY;
    //     for(int32 gy=0;gy<g.height;++gy){
    //         int32 dy=dstY+gy;
    //         if(dy<0||dy>=(int32)outH)continue;
    //         const uint8_t* sr=g.bitmap+gy*g.pitch;
    //         uint8_t* dr=bmp.data()+dy*outW;
    //         for(int32 gx=0;gx<g.width;++gx){
    //             int32 dx=dstX+gx;
    //             if(dx<0||dx>=(int32)outW)continue;
    //             uint32 s=sr[gx],d=dr[dx];
    //             dr[dx]=(uint8_t)(d+s-(d*s)/255u);
    //         }
    //     }
    //     penX+=g.xAdvance;
    // }
    return bmp;
}

static void Ortho2DMVP(float x,float y,float scaleX,float scaleY,
                        float W,float H,bool depthZeroToOne,float out[16]){
    const float l=0.f,r=W,t=H,b=0.f,nearZ=-1.f,farZ=1.f;
    const float rl=1.f/(r-l),tb=1.f/(t-b),fn=1.f/(farZ-nearZ);
    const float ortho[16]={
        2.f*rl,     0.f,        0.f,                                  0.f,
        0.f,        2.f*tb,     0.f,                                  0.f,
        0.f,        0.f,        (depthZeroToOne?fn:-2.f*fn),          0.f,
        -(r+l)*rl, -(t+b)*tb,  (depthZeroToOne?-nearZ*fn:(farZ+nearZ)*fn),1.f
    };
    const float model[16]={
        scaleX,0.f,0.f,0.f, 0.f,scaleY,0.f,0.f, 0.f,0.f,1.f,0.f, x,y,0.f,1.f
    };
    for(int col=0;col<4;col++)
        for(int row=0;row<4;row++){
            float s=0.f;
            for(int k=0;k<4;k++)s+=ortho[k*4+row]*model[col*4+k];
            out[col*4+row]=s;
        }
}

static void Billboard3DMVP(const NkVec3f& worldPos,float worldW,float worldH,
                             const NkMat4f& view,const NkMat4f& proj,
                             bool flipBillY,float out[16]){
    float rx=view.data[0],ry=view.data[4],rz=view.data[8];
    float ux=view.data[1],uy=view.data[5],uz=view.data[9];
    if(flipBillY){ux=-ux;uy=-uy;uz=-uz;}
    NkMat4f model=NkMat4f::Identity();
    model.data[0]=rx*worldW;model.data[1]=ry*worldW;model.data[2]=rz*worldW;
    model.data[4]=ux*worldH;model.data[5]=uy*worldH;model.data[6]=uz*worldH;
    model.data[12]=worldPos.x;model.data[13]=worldPos.y;model.data[14]=worldPos.z;
    NkMat4f mvp=proj*view*model;
    mem::NkCopy(out,mvp.data,16*sizeof(float));
}

static NkTextQuad CreateTextQuad(NkIDevice* device,NkDescSetHandle hTextLayout,
                                  NkFontFace* face,const char* text,bool flipV=false){
    NkTextQuad q{};
    if(!device||!face||!text||!*text)return q;
    uint32 bmpW=0,bmpH=0;
    auto bmp=BuildStringBitmap(face,text,bmpW,bmpH);
    if(bmp.empty()||!bmpW||!bmpH)return q;
    NkTextureDesc td=NkTextureDesc::Tex2D(bmpW,bmpH,NkGPUFormat::NK_R8_UNORM,1);
    td.bindFlags=NkBindFlags::NK_SHADER_RESOURCE;td.debugName=text;
    q.hTex=device->CreateTexture(td);
    if(!q.hTex.IsValid())return q;
    device->WriteTexture(q.hTex,bmp.data(),bmpW);
    NkSamplerDesc sd{};
    sd.magFilter=NkFilter::NK_LINEAR;sd.minFilter=NkFilter::NK_LINEAR;
    sd.mipFilter=NkMipFilter::NK_NONE;
    sd.addressU=sd.addressV=sd.addressW=NkAddressMode::NK_CLAMP_TO_EDGE;
    q.hSampler=device->CreateSampler(sd);
    if(!q.hSampler.IsValid()){device->DestroyTexture(q.hTex);q.hTex={};return q;}
    float v0=flipV?1.f:0.f,v1=flipV?0.f:1.f;
    const TextVtx verts[6]={
        {0.f,0.f,0.f,v0},{1.f,0.f,1.f,v0},{1.f,1.f,1.f,v1},
        {0.f,0.f,0.f,v0},{1.f,1.f,1.f,v1},{0.f,1.f,0.f,v1},
    };
    q.hVBO=device->CreateBuffer(NkBufferDesc::Vertex(sizeof(verts),verts));
    q.hUBO=device->CreateBuffer(NkBufferDesc::Uniform(sizeof(TextUboData)));
    q.texW=bmpW;q.texH=bmpH;q.vtxCount=6;
    if(hTextLayout.IsValid()){
        q.hDesc=device->AllocateDescriptorSet(hTextLayout);
        if(q.hDesc.IsValid()){
            NkDescriptorWrite w[2]{};
            w[0].set=q.hDesc;w[0].binding=0;w[0].type=NkDescriptorType::NK_UNIFORM_BUFFER;
            w[0].buffer=q.hUBO;w[0].bufferOffset=0;w[0].bufferRange=sizeof(TextUboData);
            w[1].set=q.hDesc;w[1].binding=1;w[1].type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            w[1].texture=q.hTex;w[1].sampler=q.hSampler;
            w[1].textureLayout=NkResourceState::NK_SHADER_READ;
            device->UpdateDescriptorSets(w,2);
        }
    }
    return q;
}

static void DestroyTextQuad(NkIDevice* device,NkTextQuad& q){
    if(!device)return;
    if(q.hDesc.IsValid())   device->FreeDescriptorSet(q.hDesc);
    if(q.hUBO.IsValid())    device->DestroyBuffer(q.hUBO);
    if(q.hVBO.IsValid())    device->DestroyBuffer(q.hVBO);
    if(q.hSampler.IsValid())device->DestroySampler(q.hSampler);
    if(q.hTex.IsValid())    device->DestroyTexture(q.hTex);
    q={};
}

static void RenderTextQuad(NkICommandBuffer* cmd,NkIDevice* device,
                            const NkTextQuad& q,NkPipelineHandle hTextPipe,
                            const float mvp[16],float r,float g,float b,float a){
    if(!cmd||!device||!q.hVBO.IsValid()||!q.hUBO.IsValid()||!hTextPipe.IsValid())return;
    TextUboData ubo{};
    mem::NkCopy(ubo.mvp,mvp,16*sizeof(float));
    ubo.color[0]=r;ubo.color[1]=g;ubo.color[2]=b;ubo.color[3]=a;
    device->WriteBuffer(q.hUBO,&ubo,sizeof(ubo));
    cmd->BindGraphicsPipeline(hTextPipe);
    if(q.hDesc.IsValid())cmd->BindDescriptorSet(q.hDesc,0);
    cmd->BindVertexBuffer(0,q.hVBO);
    cmd->Draw(q.vtxCount);
}

// =============================================================================
// nkmain
// =============================================================================
int nkmain(const NkEntryState& state) {

    NkGraphicsApi targetApi = ParseBackend(state.GetArgs());
    const char*   apiName   = NkGraphicsApiName(targetApi);
    const NkSLTarget slTarget = nksl::ApiToTarget(targetApi);

    logger.Infof("[RHIFullDemo-NkSL] Backend: %s  NkSL target: %s\n",
                 apiName, NkSLTargetName(slTarget));

    // ── Initialiser le compilateur NkSL avec cache disque ────────────────────
    // Le cache évite de recompiler à chaque lancement (utile pour tests)
    nksl::InitCompiler("./nksl_cache");
    logger.Infof("[NkSL] Compiler initialized, cache: ./nksl_cache\n");

    // ── Fenêtre ───────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title     = NkFormat("NkRHI — NkSL Demo ({0})", apiName);
    winCfg.width     = 1280;
    winCfg.height    = 720;
    winCfg.centered  = true;
    winCfg.resizable = true;
    NkWindow window;
    if(!window.Create(winCfg)){
        logger.Errorf("[RHIFullDemo-NkSL] Window creation failed\n");
        nksl::ShutdownCompiler();
        return 1;
    }

    // ── Device ────────────────────────────────────────────────────────────────
    NkSurfaceDesc surface=window.GetSurfaceDesc();
    NkDeviceInitInfo dii;
    dii.api=targetApi;dii.surface=surface;
    dii.height=window.GetSize().height;dii.width=window.GetSize().width;
    dii.context.vulkan.appName="NkRHIDemoNkSL";
    dii.context.vulkan.engineName="Unkeny";
    NkIDevice* device=NkDeviceFactory::Create(dii);
    if(!device||!device->IsValid()){
        logger.Errorf("[RHIFullDemo-NkSL] Device creation failed (%s)\n",apiName);
        nksl::ShutdownCompiler();
        window.Close();return 1;
    }

    uint32 W=device->GetSwapchainWidth(), H=device->GetSwapchainHeight();

    const bool depthZeroToOne=
        targetApi==NkGraphicsApi::NK_API_VULKAN    ||
        targetApi==NkGraphicsApi::NK_API_DIRECTX11 ||
        targetApi==NkGraphicsApi::NK_API_DIRECTX12 ||
        targetApi==NkGraphicsApi::NK_API_METAL;
    const float ndcZScale  = depthZeroToOne?1.f:0.5f;
    const float ndcZOffset = depthZeroToOne?0.f:0.5f;

    // =========================================================================
    // Compilation des shaders NkSL
    // =========================================================================
    logger.Infof("[NkSL] Compiling shaders for target=%s ...\n", NkSLTargetName(slTarget));

    // Storage pour garder les sources NkSL compilées vivantes jusqu'à CreateShader
    NkVector<NkString> mainSrcs, shadowSrcs, textSrcs;

    // ── Shader principal ─────────────────────────────────────────────────────
    NkShaderDesc mainShaderDesc; mainShaderDesc.debugName="Phong3D_NkSL";
    NkSLReflection mainReflect;
    if(!CompileNkSLShader(device, targetApi,
                          kNkSL_MainVert, kNkSL_MainFrag,
                          "Phong3D", mainShaderDesc, mainSrcs, &mainReflect)){
        logger.Errorf("[NkSL] Main shader compilation failed — aborting\n");
        NkDeviceFactory::Destroy(device);
        nksl::ShutdownCompiler();
        window.Close();return 1;
    }

    // Log de la reflection (utile pour vérifier que NkSL détecte correctement les bindings)
    logger.Infof("[NkSL] Main shader reflection: %u resources, %u vertex inputs\n",
                 (unsigned)mainReflect.resources.Size(),
                 (unsigned)mainReflect.vertexInputs.Size());
    for(auto& r:mainReflect.resources)
        logger.Infof("[NkSL]   resource set=%u binding=%u name=%s\n",r.set,r.binding,r.name.CStr());
    for(auto& vi:mainReflect.vertexInputs)
        logger.Infof("[NkSL]   vertex input location=%u name=%s components=%u\n",
                     vi.location,vi.name.CStr(),vi.components);

    // ── Shadow shader ─────────────────────────────────────────────────────────
    NkShaderDesc shadowShaderDesc; shadowShaderDesc.debugName="Shadow_NkSL";
    bool shadowShaderOk=false;
    // Metal n'a pas de shadow pass dans cette démo (Metal 3 mesh shaders requis)
    if(targetApi != NkGraphicsApi::NK_API_METAL){
        shadowShaderOk = CompileNkSLShader(device, targetApi,
                                           kNkSL_ShadowVert, kNkSL_ShadowFrag,
                                           "Shadow", shadowShaderDesc, shadowSrcs, nullptr);
        if(!shadowShaderOk)
            logger.Warnf("[NkSL] Shadow shader failed — shadow map disabled\n");
    }

    // ── Shader texte ──────────────────────────────────────────────────────────
    NkShaderDesc textShaderDesc; textShaderDesc.debugName="Text_NkSL";
    bool textShaderOk = CompileNkSLShader(device, targetApi,
                                          kNkSL_TextVert, kNkSL_TextFrag,
                                          "Text", textShaderDesc, textSrcs, nullptr);
    if(!textShaderOk)
        logger.Warnf("[NkSL] Text shader failed — text overlay disabled\n");

    logger.Infof("[NkSL] Shader compilation complete.\n");

    // ── Créer les objets shader RHI depuis les NkShaderDesc compilés ──────────
    NkShaderHandle hShader = device->CreateShader(mainShaderDesc);
    if(!hShader.IsValid()){
        logger.Errorf("[NkSL] NkIDevice::CreateShader failed for main shader\n");
        NkDeviceFactory::Destroy(device);nksl::ShutdownCompiler();window.Close();return 1;
    }

    NkShaderHandle hShadowShader;
    if(shadowShaderOk&&!shadowShaderDesc.stages.IsEmpty()){
        hShadowShader=device->CreateShader(shadowShaderDesc);
        if(!hShadowShader.IsValid()){
            logger.Warnf("[NkSL] Shadow shader RHI creation failed\n");
            shadowShaderOk=false;
        }
    }

    NkShaderHandle hTextShader;
    if(textShaderOk&&!textShaderDesc.stages.IsEmpty()){
        hTextShader=device->CreateShader(textShaderDesc);
        if(!hTextShader.IsValid()){
            logger.Warnf("[NkSL] Text shader RHI creation failed\n");
            textShaderOk=false;
        }
    }

    // ── Vertex Layout principal ───────────────────────────────────────────────
    NkVertexLayout vtxLayout;
    vtxLayout
        .AddAttribute(0,0,NkGPUFormat::NK_RGB32_FLOAT,  0,               "POSITION",0)
        .AddAttribute(1,0,NkGPUFormat::NK_RGB32_FLOAT,  3*sizeof(float), "NORMAL",  0)
        .AddAttribute(2,0,NkGPUFormat::NK_RGBA32_FLOAT, 6*sizeof(float), "TEXCOORD",2)
        .AddAttribute(3,0,NkGPUFormat::NK_RG32_FLOAT,  10*sizeof(float), "TEXCOORD",3)
        .AddBinding(0,sizeof(Vtx3D));

    NkRenderPassHandle hRP=device->GetSwapchainRenderPass();

    // ── Descriptor set layout principal ──────────────────────────────────────
    const bool shaderNeedsShadowSampler=
        targetApi==NkGraphicsApi::NK_API_OPENGL    ||
        targetApi==NkGraphicsApi::NK_API_VULKAN    ||
        targetApi==NkGraphicsApi::NK_API_DIRECTX11 ||
        targetApi==NkGraphicsApi::NK_API_DIRECTX12;
    const bool shaderNeedsAlbedoSampler= targetApi!=NkGraphicsApi::NK_API_SOFTWARE;
    const bool wantsShadowResources=
        shaderNeedsShadowSampler || targetApi==NkGraphicsApi::NK_API_SOFTWARE;
    static constexpr uint32 kShadowSize=2048;

    NkDescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Add(0,NkDescriptorType::NK_UNIFORM_BUFFER,NkShaderStage::NK_ALL_GRAPHICS);
    if(shaderNeedsShadowSampler)
        layoutDesc.Add(1,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
    if(shaderNeedsAlbedoSampler)
        layoutDesc.Add(2,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
    NkDescSetHandle hLayout=device->CreateDescriptorSetLayout(layoutDesc);

    // ── Pipeline principal ────────────────────────────────────────────────────
    NkGraphicsPipelineDesc pipeDesc;
    pipeDesc.shader=hShader;pipeDesc.vertexLayout=vtxLayout;
    pipeDesc.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
    pipeDesc.rasterizer=NkRasterizerDesc::Default();
    pipeDesc.depthStencil=NkDepthStencilDesc::Default();
    pipeDesc.blend=NkBlendDesc::Opaque();
    pipeDesc.renderPass=hRP;pipeDesc.debugName="Pipeline_NkSL_Phong";
    if(hLayout.IsValid())pipeDesc.descriptorSetLayouts.PushBack(hLayout);
    NkPipelineHandle hPipe=device->CreateGraphicsPipeline(pipeDesc);
    if(!hPipe.IsValid()){
        logger.Errorf("[NkSL] Main pipeline creation failed\n");
        device->DestroyShader(hShader);
        NkDeviceFactory::Destroy(device);nksl::ShutdownCompiler();window.Close();return 1;
    }

    // ── Géométrie ─────────────────────────────────────────────────────────────
    auto cubeVerts  =MakeCube();
    auto sphereVerts=MakeSphere();
    auto planeVerts =MakePlane();
    auto uploadVBO=[&](const NkVector<Vtx3D>& v)->NkBufferHandle{
        return device->CreateBuffer(NkBufferDesc::Vertex(v.Size()*sizeof(Vtx3D),v.Begin()));
    };
    NkBufferHandle hCube  =uploadVBO(cubeVerts);
    NkBufferHandle hSphere=uploadVBO(sphereVerts);
    NkBufferHandle hPlane =uploadVBO(planeVerts);

    NkBufferHandle hUBO[3];
    for(int i=0;i<3;i++)hUBO[i]=device->CreateBuffer(NkBufferDesc::Uniform(sizeof(UboData)));

    NkDescSetHandle hDescSet[3];
    for(int i=0;i<3;i++){
        hDescSet[i]=device->AllocateDescriptorSet(hLayout);
        if(hLayout.IsValid()&&hDescSet[i].IsValid()&&hUBO[i].IsValid()){
            NkDescriptorWrite w{};w.set=hDescSet[i];w.binding=0;
            w.type=NkDescriptorType::NK_UNIFORM_BUFFER;
            w.buffer=hUBO[i];w.bufferOffset=0;w.bufferRange=sizeof(UboData);
            device->UpdateDescriptorSets(&w,1);
        }
    }

    // ── Shadow map ────────────────────────────────────────────────────────────
    NkTextureHandle     hShadowTex;
    NkSamplerHandle     hShadowSampler;
    NkRenderPassHandle  hShadowRP;
    NkFramebufferHandle hShadowFBO;
    NkPipelineHandle    hShadowPipe;
    bool hasShadowMap=false, useRealShadowPass=false;

    if(wantsShadowResources){
        NkTextureDesc std2=NkTextureDesc::DepthStencil(
            kShadowSize,kShadowSize,NkGPUFormat::NK_D32_FLOAT,NkSampleCount::NK_S1);
        std2.bindFlags=NkBindFlags::NK_DEPTH_STENCIL|NkBindFlags::NK_SHADER_RESOURCE;
        std2.debugName="ShadowDepthTex";
        hShadowTex=device->CreateTexture(std2);

        NkSamplerDesc ssd=NkSamplerDesc::Shadow();
        ssd.magFilter=NkFilter::NK_LINEAR;ssd.minFilter=NkFilter::NK_LINEAR;
        ssd.mipFilter=NkMipFilter::NK_NONE;ssd.minLod=ssd.maxLod=0.f;
        hShadowSampler=device->CreateSampler(ssd);
        hShadowRP=device->CreateRenderPass(NkRenderPassDesc::ShadowMap());

        NkFramebufferDesc fboD{};
        fboD.renderPass=hShadowRP;fboD.depthAttachment=hShadowTex;
        fboD.width=kShadowSize;fboD.height=kShadowSize;fboD.debugName="ShadowFBO";
        hShadowFBO=device->CreateFramebuffer(fboD);

        hasShadowMap=hShadowTex.IsValid()&&hShadowSampler.IsValid()&&
                     hShadowRP.IsValid()&&hShadowFBO.IsValid();

        if(hasShadowMap&&shadowShaderOk&&hShadowShader.IsValid()&&
           !HasArg(state.GetArgs(),"--safe-shadows")){
            NkVertexLayout svl;
            svl.AddAttribute(0,0,NkGPUFormat::NK_RGB32_FLOAT,0,"POSITION",0)
               .AddBinding(0,sizeof(Vtx3D));
            NkGraphicsPipelineDesc spd{};
            spd.shader=hShadowShader;spd.vertexLayout=svl;
            spd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
            spd.rasterizer=NkRasterizerDesc::ShadowMap();
            spd.depthStencil=NkDepthStencilDesc::Default();
            spd.blend=NkBlendDesc::Opaque();
            spd.renderPass=hShadowRP;spd.debugName="ShadowPipeline_NkSL";
            if(hLayout.IsValid())spd.descriptorSetLayouts.PushBack(hLayout);
            hShadowPipe=device->CreateGraphicsPipeline(spd);
            useRealShadowPass=hShadowPipe.IsValid();
        }

        for(int i=0;i<3;i++){
            if(hDescSet[i].IsValid()&&hShadowTex.IsValid()&&
               hShadowSampler.IsValid()&&shaderNeedsShadowSampler){
                NkDescriptorWrite sw{};sw.set=hDescSet[i];sw.binding=1;
                sw.type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                sw.texture=hShadowTex;sw.sampler=hShadowSampler;
                sw.textureLayout=NkResourceState::NK_DEPTH_READ;
                device->UpdateDescriptorSets(&sw,1);
            }
        }
    }

    if(shaderNeedsShadowSampler&&!hasShadowMap){
        logger.Errorf("[NkSL] Shadow sampler required but unavailable (%s)\n",apiName);
        device->DestroyPipeline(hPipe);device->DestroyShader(hShader);
        NkDeviceFactory::Destroy(device);nksl::ShutdownCompiler();window.Close();return 1;
    }

    // ── Texture albedo ────────────────────────────────────────────────────────
    NkTextureHandle hAlbedoTex;
    NkSamplerHandle hAlbedoSampler;
    NkImage* loadedTextureImage=nullptr;
    std::string loadedTexturePath=FindTextureInResources();

    if(!loadedTexturePath.empty())
        loadedTextureImage = NkImage::Load(loadedTexturePath.c_str(), 4);
        // loadedTextureImage = NkImage::LoadSTB(loadedTexturePath.c_str(), 4);

    if(!loadedTextureImage||!loadedTextureImage->IsValid()){
        if(loadedTextureImage){loadedTextureImage->Free();loadedTextureImage=nullptr;}
        loadedTextureImage=NkImage::Alloc(256,256,NkImagePixelFormat::NK_RGBA32,true);
        if(loadedTextureImage&&loadedTextureImage->IsValid()){
            for(int y=0;y<loadedTextureImage->Height();++y){
                uint8* row=loadedTextureImage->RowPtr(y);
                for(int x=0;x<loadedTextureImage->Width();++x){
                    uint8 c=(((x/32)+(y/32))&1)?220:40;
                    row[x*4+0]=c;row[x*4+1]=c;row[x*4+2]=c;row[x*4+3]=255;
                }
            }
            loadedTexturePath="<procedural_checkerboard>";
        }
    }

    if(loadedTextureImage&&loadedTextureImage->IsValid()){
        NkTextureDesc ad=NkTextureDesc::Tex2D(
            (uint32)loadedTextureImage->Width(),
            (uint32)loadedTextureImage->Height(),
            NkGPUFormat::NK_RGBA8_UNORM,1);
        ad.bindFlags=NkBindFlags::NK_SHADER_RESOURCE;
        hAlbedoTex=device->CreateTexture(ad);
        if(hAlbedoTex.IsValid())
            device->WriteTexture(hAlbedoTex,loadedTextureImage->Pixels(),
                                 (uint32)loadedTextureImage->Stride());
        NkSamplerDesc asd{};
        asd.magFilter=NkFilter::NK_LINEAR;asd.minFilter=NkFilter::NK_LINEAR;
        asd.mipFilter=NkMipFilter::NK_NONE;
        asd.addressU=asd.addressV=asd.addressW=NkAddressMode::NK_REPEAT;
        hAlbedoSampler=device->CreateSampler(asd);
        for(int i=0;i<3;++i){
            if(!shaderNeedsAlbedoSampler)break;
            if(!hDescSet[i].IsValid()||!hAlbedoTex.IsValid()||!hAlbedoSampler.IsValid())continue;
            NkDescriptorWrite tw{};tw.set=hDescSet[i];tw.binding=2;
            tw.type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            tw.texture=hAlbedoTex;tw.sampler=hAlbedoSampler;
            tw.textureLayout=NkResourceState::NK_SHADER_READ;
            device->UpdateDescriptorSets(&tw,1);
        }
    }

    // ── Software backend : shaders CPU ────────────────────────────────────────
    // Pour le Software backend, NkSL génère du C++ mais NkSWShader utilise des
    // lambdas directement. On définit les lambdas CPU ici pour avoir le rendu SW.
    if(targetApi==NkGraphicsApi::NK_API_SOFTWARE){
        NkSoftwareDevice* swDev=static_cast<NkSoftwareDevice*>(device);
        if(hShadowShader.IsValid()){
            NkSWShader* swSh=swDev->GetShader(hShadowShader.id);
            if(swSh){
                swSh->vertFn=[](const void* vdata,uint32 idx,const void* udata)->NkVertexSoftware{
                    const Vtx3D* v=static_cast<const Vtx3D*>(vdata)+idx;
                    const UboData* ubo=static_cast<const UboData*>(udata);
                    NkVertexSoftware out;
                    auto mul4=[](const float m[16],float x,float y,float z,float w)->NkVec4f{
                        return NkVec4f(m[0]*x+m[4]*y+m[8]*z+m[12]*w,
                                       m[1]*x+m[5]*y+m[9]*z+m[13]*w,
                                       m[2]*x+m[6]*y+m[10]*z+m[14]*w,
                                       m[3]*x+m[7]*y+m[11]*z+m[15]*w);
                    };
                    NkVec4f wp=mul4(ubo->model,v->pos.x,v->pos.y,v->pos.z,1.f);
                    // out.position=mul4(ubo->lightVP,wp.x,wp.y,wp.z,wp.w);
                    return out;
                };
                swSh->fragFn=nullptr;
            }
        }

        NkSWShader*  sw          =swDev->GetShader(hShader.id);
        NkSWTexture* swShadowTex =hasShadowMap?swDev->GetTex(hShadowTex.id):nullptr;
        NkSWTexture* swAlbedoTex =hAlbedoTex.IsValid()?swDev->GetTex(hAlbedoTex.id):nullptr;

        if(sw){
            sw->vertFn=[](const void* vdata,uint32 idx,const void* udata)->NkVertexSoftware{
                const Vtx3D* v=static_cast<const Vtx3D*>(vdata)+idx;
                const UboData* ubo=static_cast<const UboData*>(udata);
                NkVertexSoftware out;
                auto mul4=[](const float m[16],float x,float y,float z,float w)->NkVec4f{
                    return NkVec4f(m[0]*x+m[4]*y+m[8]*z+m[12]*w,
                                   m[1]*x+m[5]*y+m[9]*z+m[13]*w,
                                   m[2]*x+m[6]*y+m[10]*z+m[14]*w,
                                   m[3]*x+m[7]*y+m[11]*z+m[15]*w);
                };
                NkVec4f wp=mul4(ubo->model, v->pos.x,v->pos.y,v->pos.z,1.f);
                NkVec4f vp=mul4(ubo->view,  wp.x,wp.y,wp.z,wp.w);
                // out.position=mul4(ubo->proj,vp.x,vp.y,vp.z,vp.w);
                // float nx=ubo->model[0]*v->normal.x+ubo->model[4]*v->normal.y+ubo->model[8]*v->normal.z;
                // float ny=ubo->model[1]*v->normal.x+ubo->model[5]*v->normal.y+ubo->model[9]*v->normal.z;
                // float nz=ubo->model[2]*v->normal.x+ubo->model[6]*v->normal.y+ubo->model[10]*v->normal.z;
                // float nl=NkSqrt(nx*nx+ny*ny+nz*nz);
                // if(nl>0.001f){nx/=nl;ny/=nl;nz/=nl;}
                // out.normal={nx,ny,nz};
                // out.color={v->color.x,v->color.y,v->color.z,1.f};
                // out.attrs[0]=wp.x;out.attrs[1]=wp.y;out.attrs[2]=wp.z;
                // NkVec4f lsc=mul4(ubo->lightVP,wp.x,wp.y,wp.z,wp.w);
                // out.attrs[3]=lsc.x;out.attrs[4]=lsc.y;
                // out.attrs[5]=lsc.z;out.attrs[6]=lsc.w;
                // out.attrs[7]=v->uv.x;out.attrs[8]=v->uv.y;
                // out.attrs[9]=v->color.w; // hasTexture
                return out;
            };
            sw->fragFn=[swShadowTex,swAlbedoTex](const NkVertexSoftware& frag,const void* udata,const void*)->math::NkVec4f{
                // const UboData* ubo=static_cast<const UboData*>(udata);
                // float nx=frag.normal.x,ny=frag.normal.y,nz=frag.normal.z;
                // float lx=-ubo->lightDirW[0],ly=-ubo->lightDirW[1],lz=-ubo->lightDirW[2];
                // float ll2=lx*lx+ly*ly+lz*lz;
                // if(ll2>1e-6f){float il=1.f/NkSqrt(ll2);lx*=il;ly*=il;lz*=il;}
                // float diff=nx*lx+ny*ly+nz*lz;if(diff<0.f)diff=0.f;
                // float vx=ubo->eyePosW[0]-frag.attrs[0],vy=ubo->eyePosW[1]-frag.attrs[1],vz=ubo->eyePosW[2]-frag.attrs[2];
                // float vl2=vx*vx+vy*vy+vz*vz;
                // if(vl2>1e-6f){float iv=1.f/NkSqrt(vl2);vx*=iv;vy*=iv;vz*=iv;}
                // float hx=lx+vx,hy=ly+vy,hz=lz+vz,hl2=hx*hx+hy*hy+hz*hz;
                // if(hl2>1e-6f){float ih=1.f/NkSqrt(hl2);hx*=ih;hy*=ih;hz*=ih;}
                // float sp=nx*hx+ny*hy+nz*hz;if(sp<0.f)sp=0.f;
                // float spec=sp*sp;spec*=spec;spec*=spec;spec*=spec;
                // float br=frag.color.x,bg=frag.color.y,bb=frag.color.z;
                // if(swAlbedoTex&&frag.attrs[9]>0.5f){
                //     float u=NkFmod(NkClamp(frag.attrs[7],0.f,1.f)*kPlaneUVScale,1.f);
                //     float v=NkFmod(NkClamp(frag.attrs[8],0.f,1.f)*kPlaneUVScale,1.f);
                //     NkVec4f tex=swAlbedoTex->Sample(u,v,0);
                //     br*=tex.x;bg*=tex.y;bb*=tex.z;
                // }
                // float shadow=1.f;
                // if(swShadowTex){
                //     float sw4=frag.attrs[6],invW=(sw4!=0.f)?1.f/sw4:1.f;
                //     float su=frag.attrs[3]*invW*.5f+.5f;
                //     float sv=-frag.attrs[4]*invW*.5f+.5f;
                //     float sz=frag.attrs[5]*invW*.5f+.5f-.002f;
                //     if(su>=0.f&&su<=1.f&&sv>=0.f&&sv<=1.f&&sz<=1.f){
                //         uint32 px=(uint32)(su*(float)swShadowTex->Width());
                //         uint32 py=(uint32)(sv*(float)swShadowTex->Height());
                //         if(px>=swShadowTex->Width())px=swShadowTex->Width()-1;
                //         if(py>=swShadowTex->Height())py=swShadowTex->Height()-1;
                //         shadow=(sz<=swShadowTex->Read(px,py).r)?1.f:0.f;
                //     }
                // }
                // shadow=shadow<.35f?.35f:shadow;
                // float r=.15f*br+shadow*(diff*br+spec*.4f);
                // float g=.15f*bg+shadow*(diff*bg+spec*.4f);
                // float b=.15f*bb+shadow*(diff*bb+spec*.4f);
                // r=r<0.f?0.f:(r>1.f?1.f:r);g=g<0.f?0.f:(g>1.f?1.f:g);b=b<0.f?0.f:(b>1.f?1.f:b);
                // return{r,g,b,1.f};
                return {0, 0, 0, 1.0f};
            };
        }
    }

    // ── Pipelines texte ───────────────────────────────────────────────────────
    NkDescriptorSetLayoutDesc textLayoutDesc;
    textLayoutDesc.Add(0,NkDescriptorType::NK_UNIFORM_BUFFER,NkShaderStage::NK_ALL_GRAPHICS);
    textLayoutDesc.Add(1,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
    NkDescSetHandle hTextLayout=device->CreateDescriptorSetLayout(textLayoutDesc);

    NkVertexLayout textVtxLayout;
    textVtxLayout
        .AddAttribute(0,0,NkGPUFormat::NK_RG32_FLOAT,0,              "POSITION",0)
        .AddAttribute(1,0,NkGPUFormat::NK_RG32_FLOAT,2*sizeof(float),"TEXCOORD",0)
        .AddBinding(0,sizeof(TextVtx));

    NkPipelineHandle hTextPipe;
    if(textShaderOk&&hTextShader.IsValid()&&hTextLayout.IsValid()){
        NkGraphicsPipelineDesc tpd;
        tpd.shader=hTextShader;tpd.vertexLayout=textVtxLayout;
        tpd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
        tpd.rasterizer=NkRasterizerDesc::Default();
        tpd.depthStencil=NkDepthStencilDesc::NoDepth();
        tpd.blend=NkBlendDesc::Alpha();
        tpd.renderPass=hRP;tpd.debugName="Pipeline_NkSL_Text";
        tpd.descriptorSetLayouts.PushBack(hTextLayout);
        hTextPipe=device->CreateGraphicsPipeline(tpd);
    }

    // ── Police + quads texte ─────────────────────────────────────────────────
    NkFontLibrary ftLib;
    NkFontFace*   ftFace18=nullptr, *ftFace32=nullptr;
    std::vector<uint8_t> fontBytes;
    {
        const std::string fp=FindFontFile();
        if(!fp.empty())fontBytes=LoadFileBytes(fp);
        if(!fontBytes.empty()){
            ftLib.Init();
            ftFace18=ftLib.LoadFont(fontBytes.data(),(usize)fontBytes.size(),18);
            ftFace32=ftLib.LoadFont(fontBytes.data(),(usize)fontBytes.size(),32);
        }
    }

    NkTextQuad tqBackend={},tqFPS={},tqCube={},tqSphere={},tqFloor={};
    // Ligne de statut NkSL : indique la cible de compilation
    NkTextQuad tqNkSL={};
    const bool textOk=hTextPipe.IsValid()&&hTextLayout.IsValid();
    if(textOk){
        // NkFontFace* f18=(ftFace18&&ftFace18->IsValid())?ftFace18:nullptr;
        // NkFontFace* f32=(ftFace32&&ftFace32->IsValid())?ftFace32:nullptr;
        // if(f32){
        //     NkString backendLabel=NkFormat("NkSL → {0}",NkSLTargetName(slTarget));
        //     tqBackend=CreateTextQuad(device,hTextLayout,f32,apiName,false);
        //     tqNkSL   =CreateTextQuad(device,hTextLayout,f32,backendLabel.CStr(),false);
        //     tqCube   =CreateTextQuad(device,hTextLayout,f32,"CUBE",  true);
        //     tqSphere =CreateTextQuad(device,hTextLayout,f32,"SPHERE",true);
        //     tqFloor  =CreateTextQuad(device,hTextLayout,f32,"FLOOR", true);
        // }
        // if(f18)tqFPS=CreateTextQuad(device,hTextLayout,f18,"FPS: --",false);
    }

    float fpsTimer=0.f;uint32 fpsFrameCount=0;

    // ── Command buffer ────────────────────────────────────────────────────────
    NkICommandBuffer* cmd=device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
    if(!cmd||!cmd->IsValid()){
        logger.Errorf("[NkSL] Command buffer creation failed\n");
        NkDeviceFactory::Destroy(device);nksl::ShutdownCompiler();window.Close();return 1;
    }

    bool  running=true;
    float rotAngle=0.f,camYaw=0.f,camPitch=20.f,camDist=4.f;
    float lightYaw=-45.f,lightPitch=-30.f;
    bool  keys[512]={};
    bool  reqSaveScene=false,reqSaveSource=false;
    uint32 saveSceneCnt=0,saveSourceCnt=0;
    const std::filesystem::path capturesDir=
        std::filesystem::path("Build")/"Captures"/"NkRHIDemoNkSL";

    NkClock clock;
    NkEventSystem& events=NkEvents();
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*){running=false;});
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e){
        if((uint32)e->GetKey()<512)keys[(uint32)e->GetKey()]=true;
        if(e->GetKey()==NkKey::NK_ESCAPE)running=false;
        if(e->GetKey()==NkKey::NK_F5)reqSaveScene=true;
        if(e->GetKey()==NkKey::NK_F6)reqSaveSource=true;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e){
        if((uint32)e->GetKey()<512)keys[(uint32)e->GetKey()]=false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e){
        W=(uint32)e->GetWidth();H=(uint32)e->GetHeight();
    });

    logger.Infof("[RHIFullDemo-NkSL] Running. ESC=quit, WASD=camera, Arrows=light, F5=save\n");

    // =========================================================================
    // Boucle principale
    // =========================================================================
    while(running){
        events.PollEvents();
        if(!running)break;
        if(!W||!H)continue;
        if(W!=device->GetSwapchainWidth()||H!=device->GetSwapchainHeight())
            device->OnResize(W,H);

        float dt=clock.Tick().delta;
        if(dt<=0.f||dt>0.25f)dt=1.f/60.f;

        fpsFrameCount++;fpsTimer+=dt;
        if(fpsTimer>=0.5f&&textOk){
            float fps=(fpsTimer>0.f)?(float)fpsFrameCount/fpsTimer:0.f;
            fpsFrameCount=0;fpsTimer=0.f;
            // if(ftFace18&&ftFace18->IsValid()&&hTextLayout.IsValid()){
            //     char buf[32];snprintf(buf,sizeof(buf),"FPS: %.0f",fps);
            //     DestroyTextQuad(device,tqFPS);
            //     tqFPS=CreateTextQuad(device,hTextLayout,ftFace18,buf,false);
            // }
        }

        const float camSpd=60.f,lightSpd=90.f;
        if(keys[(uint32)NkKey::NK_A])camYaw   -=camSpd*dt;
        if(keys[(uint32)NkKey::NK_D])camYaw   +=camSpd*dt;
        if(keys[(uint32)NkKey::NK_W])camPitch +=camSpd*dt;
        if(keys[(uint32)NkKey::NK_S])camPitch -=camSpd*dt;
        if(keys[(uint32)NkKey::NK_LEFT]) lightYaw  -=lightSpd*dt;
        if(keys[(uint32)NkKey::NK_RIGHT])lightYaw  +=lightSpd*dt;
        if(keys[(uint32)NkKey::NK_UP])   lightPitch+=lightSpd*dt;
        if(keys[(uint32)NkKey::NK_DOWN]) lightPitch-=lightSpd*dt;
        camPitch=NkClamp(camPitch,-80.f,80.f);
        rotAngle+=45.f*dt;

        float aspect=(H>0)?(float)W/(float)H:1.f;
        float eyeX=camDist*NkCos(NkToRadians(camPitch))*NkSin(NkToRadians(camYaw));
        float eyeY=camDist*NkSin(NkToRadians(camPitch));
        float eyeZ=camDist*NkCos(NkToRadians(camPitch))*NkCos(NkToRadians(camYaw));
        NkVec3f eye(eyeX,eyeY,eyeZ);
        NkMat4f matView=NkMat4f::LookAt(eye,NkVec3f(0,0,0),NkVec3f(0,1,0));
        NkMat4f matProj=NkMat4f::Perspective(NkAngle(60.f),aspect,0.1f,100.f);

        float lx=NkCos(NkToRadians(lightPitch))*NkSin(NkToRadians(lightYaw));
        float ly=NkSin(NkToRadians(lightPitch));
        float lz=NkCos(NkToRadians(lightPitch))*NkCos(NkToRadians(lightYaw));
        NkVec3f lightDir(lx,ly,lz);
        NkVec3f lightPos(-lx*10.f,-ly*10.f,-lz*10.f);
        NkVec3f lightUp=(NkFabs(ly)>0.9f)?NkVec3f(1,0,0):NkVec3f(0,1,0);
        NkMat4f matLightView=NkMat4f::LookAt(lightPos,NkVec3f(0,0,0),lightUp);
        NkMat4f matLightProj=NkMat4f::Orthogonal(NkVec2f(-5,-5),NkVec2f(5,5),1.f,20.f,depthZeroToOne);
        NkMat4f matLightVP=matLightProj*matLightView;

        NkFrameContext frame;
        if(!device->BeginFrame(frame))continue;
        W=device->GetSwapchainWidth();H=device->GetSwapchainHeight();
        if(!W||!H){device->EndFrame(frame);continue;}

        NkRenderPassHandle latestRP=device->GetSwapchainRenderPass();
        if(latestRP.IsValid()&&latestRP.id!=hRP.id){
            hRP=latestRP;pipeDesc.renderPass=hRP;
            if(hPipe.IsValid())device->DestroyPipeline(hPipe);
            hPipe=device->CreateGraphicsPipeline(pipeDesc);
            if(!hPipe.IsValid()){device->EndFrame(frame);continue;}
        }

        NkFramebufferHandle hFBO=device->GetSwapchainFramebuffer();
        cmd->Reset();
        if(!cmd->Begin()){device->EndFrame(frame);continue;}

        // ── Passe 1 : Shadow ────────────────────────────────────────────────
        if(hasShadowMap&&hShadowFBO.IsValid()&&hShadowRP.IsValid()){
            NkRect2D sa{0,0,(int32)kShadowSize,(int32)kShadowSize};
            bool ok=cmd->BeginRenderPass(hShadowRP,hShadowFBO,sa);
            if(ok&&useRealShadowPass&&hShadowPipe.IsValid()){
                NkViewport svp{0.f,0.f,(float)kShadowSize,(float)kShadowSize,0.f,1.f};
                cmd->SetViewport(svp);cmd->SetScissor(sa);
                cmd->BindGraphicsPipeline(hShadowPipe);
                auto fillShadow=[&](UboData& su,const NkMat4f& mm){
                    NkMat4f id=NkMat4f::Identity();
                    Mat4ToArray(mm,su.model);Mat4ToArray(matLightVP,su.lightVP);
                    Mat4ToArray(id,su.view);Mat4ToArray(id,su.proj);
                    su.ndcZScale=ndcZScale;su.ndcZOffset=ndcZOffset;
                };
                {NkMat4f mm=NkMat4f::RotationY(NkAngle(rotAngle))*NkMat4f::RotationX(NkAngle(rotAngle*.5f));
                 UboData su{};fillShadow(su,mm);
                 device->WriteBuffer(hUBO[0],&su,sizeof(su));
                 if(hDescSet[0].IsValid())cmd->BindDescriptorSet(hDescSet[0],0);
                 cmd->BindVertexBuffer(0,hCube);cmd->Draw((uint32)cubeVerts.Size());}
                {NkMat4f mm=NkMat4f::Translation(NkVec3f(2,0,0));
                 UboData su{};fillShadow(su,mm);
                 device->WriteBuffer(hUBO[1],&su,sizeof(su));
                 if(hDescSet[1].IsValid())cmd->BindDescriptorSet(hDescSet[1],0);
                 cmd->BindVertexBuffer(0,hSphere);cmd->Draw((uint32)sphereVerts.Size());}
                {NkMat4f mm=NkMat4f::Translation(NkVec3f(0,-1,0));
                 UboData su{};fillShadow(su,mm);
                 device->WriteBuffer(hUBO[2],&su,sizeof(su));
                 if(hDescSet[2].IsValid())cmd->BindDescriptorSet(hDescSet[2],0);
                 cmd->BindVertexBuffer(0,hPlane);cmd->Draw((uint32)planeVerts.Size());}
            }
            if(ok)cmd->EndRenderPass();
            if(ok&&hShadowTex.IsValid()){
                NkTextureBarrier b{};
                b.texture=hShadowTex;
                b.stateBefore=NkResourceState::NK_DEPTH_WRITE;
                b.stateAfter=NkResourceState::NK_DEPTH_READ;
                b.srcStage=NkPipelineStage::NK_LATE_FRAGMENT;
                b.dstStage=NkPipelineStage::NK_FRAGMENT_SHADER;
                cmd->Barrier(nullptr,0,&b,1);
            }
        }

        // ── Passe 2 : Rendu principal ────────────────────────────────────────
        NkRect2D area{0,0,(int32)W,(int32)H};
        if(!cmd->BeginRenderPass(hRP,hFBO,area)){
            cmd->End();
            if(targetApi==NkGraphicsApi::NK_API_VULKAN&&W>0&&H>0)device->OnResize(W,H);
            device->EndFrame(frame);continue;
        }
        NkViewport vp{0.f,0.f,(float)W,(float)H,0.f,1.f};
        cmd->SetViewport(vp);cmd->SetScissor(area);
        cmd->BindGraphicsPipeline(hPipe);

        auto fillMain=[&](UboData& ubo,const NkMat4f& model){
            Mat4ToArray(model,ubo.model);Mat4ToArray(matView,ubo.view);
            Mat4ToArray(matProj,ubo.proj);Mat4ToArray(matLightVP,ubo.lightVP);
            ubo.lightDirW[0]=lx;ubo.lightDirW[1]=ly;
            ubo.lightDirW[2]=lz;ubo.lightDirW[3]=0.f;
            ubo.eyePosW[0]=eye.x;ubo.eyePosW[1]=eye.y;
            ubo.eyePosW[2]=eye.z;ubo.eyePosW[3]=0.f;
            ubo.ndcZScale=ndcZScale;ubo.ndcZOffset=ndcZOffset;
        };

        {NkMat4f mm=NkMat4f::RotationY(NkAngle(rotAngle))*NkMat4f::RotationX(NkAngle(rotAngle*.5f));
         UboData ubo{};fillMain(ubo,mm);
         device->WriteBuffer(hUBO[0],&ubo,sizeof(ubo));
         if(hDescSet[0].IsValid())cmd->BindDescriptorSet(hDescSet[0],0);
         cmd->BindVertexBuffer(0,hCube);cmd->Draw((uint32)cubeVerts.Size());}

        {NkMat4f mm=NkMat4f::Translation(NkVec3f(2,0,0));
         UboData ubo{};fillMain(ubo,mm);
         device->WriteBuffer(hUBO[1],&ubo,sizeof(ubo));
         if(hDescSet[1].IsValid())cmd->BindDescriptorSet(hDescSet[1],0);
         cmd->BindVertexBuffer(0,hSphere);cmd->Draw((uint32)sphereVerts.Size());}

        {NkMat4f mm=NkMat4f::Translation(NkVec3f(0,-1,0));
         UboData ubo{};fillMain(ubo,mm);
         device->WriteBuffer(hUBO[2],&ubo,sizeof(ubo));
         if(hDescSet[2].IsValid())cmd->BindDescriptorSet(hDescSet[2],0);
         cmd->BindVertexBuffer(0,hPlane);cmd->Draw((uint32)planeVerts.Size());}

        // ── Texte 3D billboard ───────────────────────────────────────────────
        if(textOk){
            const bool flipBillY=(targetApi==NkGraphicsApi::NK_API_VULKAN||
                                  targetApi==NkGraphicsApi::NK_API_DIRECTX12);
            float bbMVP[16];
            if(tqCube.vtxCount>0){
                Billboard3DMVP(NkVec3f(0,.9f,0),1.f,.25f,matView,matProj,flipBillY,bbMVP);
                RenderTextQuad(cmd,device,tqCube,hTextPipe,bbMVP,1.f,.85f,.3f,1.f);
            }
            if(tqSphere.vtxCount>0){
                Billboard3DMVP(NkVec3f(2,.7f,0),1.4f,.25f,matView,matProj,flipBillY,bbMVP);
                RenderTextQuad(cmd,device,tqSphere,hTextPipe,bbMVP,.4f,.85f,1.f,1.f);
            }
            if(tqFloor.vtxCount>0){
                Billboard3DMVP(NkVec3f(-1.5f,-.7f,1.5f),1.2f,.25f,matView,matProj,flipBillY,bbMVP);
                RenderTextQuad(cmd,device,tqFloor,hTextPipe,bbMVP,.4f,1.f,.5f,1.f);
            }
        }

        // ── Texte 2D overlay ─────────────────────────────────────────────────
        if(textOk){
            float oMVP[16];
            const float fw=(float)W,fh=(float)H;
            // Ligne 1 : nom du backend (ex: "OpenGL")
            if(tqBackend.vtxCount>0){
                float tw=(float)tqBackend.texW,th=(float)tqBackend.texH;
                Ortho2DMVP(10.f,fh-th-10.f,tw,th,fw,fh,depthZeroToOne,oMVP);
                RenderTextQuad(cmd,device,tqBackend,hTextPipe,oMVP,1.f,1.f,0.f,1.f);
            }
            // Ligne 2 : cible NkSL (ex: "NkSL → GLSL")
            if(tqNkSL.vtxCount>0){
                float tw=(float)tqNkSL.texW,th=(float)tqNkSL.texH;
                float lineHeight=(tqBackend.vtxCount>0)?(float)tqBackend.texH+4.f:0.f;
                Ortho2DMVP(10.f,fh-th-10.f-lineHeight,tw,th,fw,fh,depthZeroToOne,oMVP);
                RenderTextQuad(cmd,device,tqNkSL,hTextPipe,oMVP,.4f,1.f,1.f,1.f);
            }
            // FPS en haut à droite
            if(tqFPS.vtxCount>0){
                float tw=(float)tqFPS.texW,th=(float)tqFPS.texH;
                Ortho2DMVP(fw-tw-10.f,fh-th-10.f,tw,th,fw,fh,depthZeroToOne,oMVP);
                RenderTextQuad(cmd,device,tqFPS,hTextPipe,oMVP,0.f,1.f,0.f,1.f);
            }
        }

        cmd->EndRenderPass();
        cmd->End();
        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);

        if(reqSaveSource){
            reqSaveSource=false;
            if(loadedTextureImage&&loadedTextureImage->IsValid()){
                ++saveSourceCnt;
                std::string stem=NkFormat("source_{0}",(unsigned long long)saveSourceCnt).CStr();
                SaveImageVariants(*loadedTextureImage,capturesDir,stem);
            }
        }
        if(reqSaveScene){
            reqSaveScene=false;++saveSceneCnt;
            std::string stem=NkFormat("scene_{0}",(unsigned long long)saveSceneCnt).CStr();
            if(targetApi==NkGraphicsApi::NK_API_SOFTWARE)
                SaveSoftwareScene(static_cast<NkSoftwareDevice*>(device),capturesDir,stem);
        }
    }

    // =========================================================================
    // Nettoyage
    // =========================================================================
    device->WaitIdle();
    device->DestroyCommandBuffer(cmd);
    for(int i=0;i<3;i++)if(hDescSet[i].IsValid())device->FreeDescriptorSet(hDescSet[i]);
    if(hLayout.IsValid())      device->DestroyDescriptorSetLayout(hLayout);
    for(int i=0;i<3;i++)if(hUBO[i].IsValid())device->DestroyBuffer(hUBO[i]);
    if(hPlane.IsValid())       device->DestroyBuffer(hPlane);
    if(hSphere.IsValid())      device->DestroyBuffer(hSphere);
    if(hCube.IsValid())        device->DestroyBuffer(hCube);
    device->DestroyPipeline(hPipe);
    device->DestroyShader(hShader);
    if(hShadowPipe.IsValid())   device->DestroyPipeline(hShadowPipe);
    if(hShadowShader.IsValid()) device->DestroyShader(hShadowShader);
    if(hShadowFBO.IsValid())    device->DestroyFramebuffer(hShadowFBO);
    if(hShadowRP.IsValid())     device->DestroyRenderPass(hShadowRP);
    if(hShadowSampler.IsValid())device->DestroySampler(hShadowSampler);
    if(hShadowTex.IsValid())    device->DestroyTexture(hShadowTex);
    if(hAlbedoSampler.IsValid())device->DestroySampler(hAlbedoSampler);
    if(hAlbedoTex.IsValid())    device->DestroyTexture(hAlbedoTex);
    if(loadedTextureImage){loadedTextureImage->Free();loadedTextureImage=nullptr;}
    DestroyTextQuad(device,tqFPS);
    DestroyTextQuad(device,tqNkSL);
    DestroyTextQuad(device,tqBackend);
    DestroyTextQuad(device,tqFloor);
    DestroyTextQuad(device,tqSphere);
    DestroyTextQuad(device,tqCube);
    if(hTextPipe.IsValid())    device->DestroyPipeline(hTextPipe);
    if(hTextShader.IsValid())  device->DestroyShader(hTextShader);
    if(hTextLayout.IsValid())  device->DestroyDescriptorSetLayout(hTextLayout);
    // if(ftFace18)ftLib.FreeFont(ftFace18);
    // if(ftFace32)ftLib.FreeFont(ftFace32);
    ftLib.Destroy();

    // ── Arrêt du compilateur NkSL (flush du cache disque) ────────────────────
    nksl::ShutdownCompiler();
    logger.Infof("[NkSL] Compiler shut down, cache flushed.\n");

    NkDeviceFactory::Destroy(device);
    window.Close();
    logger.Infof("[RHIFullDemo-NkSL] Terminated cleanly.\n");
    return 0;
}