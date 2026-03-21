// =============================================================================
// NkRHIDemo.cpp
// Démo graphique : fenêtre NkWindow + OpenGL via GLAD
//   • Figures 2D : triangle coloré, rectangle
//   • Figures 3D : cube rotatif, sphère, plan (sol)
//   • Éclairage  : Phong directionnel (ambiant + diffus + spéculaire)
//   • Ombres     : shadow mapping directionnel (1024×1024)
//   • Contrôles  : ESC = quitter, WASD = caméra, Flèches = lumière
// =============================================================================

// ── Détection plateforme avant tout le reste ──────────────────────────────────
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKMath/NKMath.h"
#include "NKMath/NkAngle.h"
#include "NKTime/NkTime.h"

// ── GLAD ─────────────────────────────────────────────────────────────────────
#if defined(__has_include)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       if __has_include(<glad/wgl.h>) && __has_include(<glad/gl.h>)
#           define NK_RHIDEMO_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if __has_include(<glad/gl.h>)
#           define NK_RHIDEMO_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       if __has_include(<glad/gles2.h>)
#           define NK_RHIDEMO_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       if __has_include(<glad/gles2.h>)
#           define NK_RHIDEMO_HAS_GLAD 1
#       endif
#   endif
#endif

#if defined(NK_RHIDEMO_HAS_GLAD)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       include <glad/wgl.h>
#       include <glad/gl.h>
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if defined(__has_include) && __has_include(<glad/glx.h>)
#           include <glad/glx.h>
#       endif
#       include <glad/gl.h>
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       include <glad/gles2.h>
#   endif
#endif

#if defined(Bool)
#   undef Bool
#endif

// ── NkEngine headers ─────────────────────────────────────────────────────────
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKWindow/Events/NkWindowEvent.h"
#include "NKWindow/Events/NkKeyboardEvent.h"
#include "NKContext/Factory/NkContextFactory.h"
#include "NKContext/Core/NkContextDesc.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKContext/Core/NkOpenGLDesc.h"
#include "NKTime/NkChrono.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::math;

// =============================================================================
// Application data
// =============================================================================
nkentseu::NkAppData appData = [] {
    nkentseu::NkAppData d{};
    d.appName    = "NkRHI Demo";
    d.appVersion = "1.0.0";
    return d;
}();
NKENTSEU_APP_DATA_DEFINED(appData);

// =============================================================================
// Chargement GLAD depuis le contexte
// =============================================================================
static bool LoadGL(NkIGraphicsContext* ctx) {
#if defined(NK_RHIDEMO_HAS_GLAD)
    auto loader = NkNativeContext::GetOpenGLProcAddressLoader(ctx);
    if (!loader) { printf("[RHIDemo] OpenGL loader introuvable\n"); return false; }
#if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    int ver = gladLoadGLES2((GLADloadfunc)loader);
#else
    int ver = gladLoadGL((GLADloadfunc)loader);
#endif
    if (!ver) { printf("[RHIDemo] gladLoadGL échoué\n"); return false; }
    printf("[RHIDemo] OpenGL %s  GLSL %s\n",
        glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
#endif
    return true;
}

// =============================================================================
// Compilation de shader
// =============================================================================
static GLuint CompileShader(const char* vsrc, const char* fsrc) {
    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log);
            printf("[RHIDemo] Shader error:\n%s\n", log);
        }
        return s;
    };
    GLuint vs = compile(GL_VERTEX_SHADER, vsrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { char log[1024]; glGetProgramInfoLog(prog, 1024, nullptr, log); printf("[RHIDemo] Link error:\n%s\n", log); }
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// =============================================================================
// Shaders
// =============================================================================

// ── Passe ombres (depth only) ─────────────────────────────────────────────────
static const char* kShadowVS = R"(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uLightMVP;
void main(){ gl_Position = uLightMVP * vec4(aPos,1.0); }
)";
static const char* kShadowFS = R"(#version 330 core
void main(){}
)";

// ── Rendu 3D avec Phong + ombres ──────────────────────────────────────────────
static const char* kPhongVS = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNorm;
layout(location=2) in vec3 aColor;
out vec3 vWorldPos;
out vec3 vNorm;
out vec3 vColor;
out vec4 vShadowCoord;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat4 uLightMVP;
void main(){
    vec4 wp = uModel * vec4(aPos,1.0);
    vWorldPos   = wp.xyz;
    vNorm       = normalize(mat3(transpose(inverse(uModel))) * aNorm);
    vColor      = aColor;
    vShadowCoord= uLightMVP * vec4(aPos,1.0);
    gl_Position = uMVP * vec4(aPos,1.0);
}
)";
static const char* kPhongFS = R"(#version 330 core
in vec3 vWorldPos;
in vec3 vNorm;
in vec3 vColor;
in vec4 vShadowCoord;
out vec4 fragColor;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uCameraPos;
uniform sampler2DShadow uShadowMap;

float ShadowFactor(){
    vec3 proj = vShadowCoord.xyz / vShadowCoord.w;
    proj = proj * 0.5 + 0.5;
    if(proj.x<0||proj.x>1||proj.y<0||proj.y>1||proj.z>1) return 1.0;
    float bias = max(0.005*(1.0-dot(vNorm,uLightDir)),0.001);
    proj.z -= bias;
    float shadow=0.0;
    float texelSize=1.0/1024.0;
    for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++){
        vec2 off=vec2(x,y)*texelSize;
        shadow+=texture(uShadowMap,vec3(proj.xy+off,proj.z));
    }
    return shadow/9.0;
}

void main(){
    vec3 n    = normalize(vNorm);
    vec3 l    = normalize(uLightDir);
    vec3 v    = normalize(uCameraPos - vWorldPos);
    vec3 h    = normalize(l+v);
    float diff  = max(dot(n,l),0.0);
    float spec  = pow(max(dot(n,h),0.0),64.0);
    float shadow= ShadowFactor();
    vec3 ambient  = 0.15 * vColor;
    vec3 diffuse  = shadow * diff * uLightColor * vColor;
    vec3 specular = shadow * spec * uLightColor * 0.4;
    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
)";

// ── Rendu 2D coloré (overlay sans profondeur) ─────────────────────────────────
static const char* k2DVS = R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec3 aColor;
out vec3 vColor;
uniform mat4 uOrtho;
void main(){
    vColor      = aColor;
    gl_Position = uOrtho * vec4(aPos,0.0,1.0);
}
)";
static const char* k2DFS = R"(#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main(){ fragColor = vec4(vColor,1.0); }
)";

// =============================================================================
// Génération de géométrie — NkVec3f remplace Vec3
// =============================================================================

struct Vertex3D { NkVec3f pos; NkVec3f normal; NkVec3f color; };

static NkVector<Vertex3D> MakeCube(float r, float g, float b) {
    struct Face { NkVec3f n; NkVec3f v[4]; };
    Face faces[6] = {
        { {0,0,1},  { {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f} } },
        { {0,0,-1}, { {.5f,-.5f,-.5f},{-.5f,-.5f,-.5f},{-.5f,.5f,-.5f},{.5f,.5f,-.5f} } },
        { {1,0,0},  { {.5f,-.5f,.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{.5f,.5f,.5f} } },
        { {-1,0,0}, { {-.5f,-.5f,-.5f},{-.5f,-.5f,.5f},{-.5f,.5f,.5f},{-.5f,.5f,-.5f} } },
        { {0,1,0},  { {-.5f,.5f,.5f},{.5f,.5f,.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f} } },
        { {0,-1,0}, { {-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,-.5f,.5f},{-.5f,-.5f,.5f} } }
    };
    NkVector<Vertex3D> verts;
    for (auto& f : faces) {
        int idx[6] = {0,1,2,0,2,3};
        for (int i : idx) {
            verts.PushBack({f.v[i], f.n, NkVec3f(r,g,b)});
        }
    }
    return verts;
}

static NkVector<Vertex3D> MakeSphere(int stacks, int slices, float r, float g, float b) {
    NkVector<Vertex3D> v;
    for (int i = 0; i < stacks; i++) {
        float phi0 = (float)i / stacks * (float)NkPi;
        float phi1 = (float)(i+1) / stacks * (float)NkPi;
        for (int j = 0; j < slices; j++) {
            float th0 = (float)j / slices * 2.f * (float)NkPi;
            float th1 = (float)(j+1) / slices * 2.f * (float)NkPi;
            auto mk = [&](float phi, float th) -> Vertex3D {
                float x = NkSin(phi) * NkCos(th);
                float y = NkCos(phi);
                float z = NkSin(phi) * NkSin(th);
                return {NkVec3f(x*.5f,y*.5f,z*.5f), NkVec3f(x,y,z), NkVec3f(r,g,b)};
            };
            Vertex3D a=mk(phi0,th0), b2=mk(phi0,th1), c=mk(phi1,th0), d=mk(phi1,th1);
            v.PushBack(a); v.PushBack(b2); v.PushBack(d);
            v.PushBack(a); v.PushBack(d);  v.PushBack(c);
        }
    }
    return v;
}

static NkVector<Vertex3D> MakePlane(float sz, float r, float g, float b) {
    float h = sz * .5f;
    NkVector<Vertex3D> v;
    v.PushBack({NkVec3f(-h,0,-h), NkVec3f(0,1,0), NkVec3f(r,g,b)}); v.PushBack({NkVec3f(h,0,-h), NkVec3f(0,1,0), NkVec3f(r,g,b)});
    v.PushBack({NkVec3f(h,0,h),   NkVec3f(0,1,0), NkVec3f(r,g,b)}); v.PushBack({NkVec3f(-h,0,-h), NkVec3f(0,1,0), NkVec3f(r,g,b)});
    v.PushBack({NkVec3f(h,0,h),   NkVec3f(0,1,0), NkVec3f(r,g,b)}); v.PushBack({NkVec3f(-h,0,h),  NkVec3f(0,1,0), NkVec3f(r,g,b)});
    return v;
}

// =============================================================================
// Helper VAO/VBO — NkVector remplace std::vector
// =============================================================================
struct Mesh {
    GLuint vao=0, vbo=0;
    int    count=0;
    void Upload(const NkVector<Vertex3D>& v) {
        count = (int)v.Size();
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, v.Size()*sizeof(Vertex3D), v.Data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)(3*4));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)(6*4));
        glBindVertexArray(0);
    }
    void Draw()  const { glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES,0,count); glBindVertexArray(0); }
    void Free()  { if (vao) { glDeleteVertexArrays(1,&vao); glDeleteBuffers(1,&vbo); vao=vbo=0; } }
};

struct Mesh2D {
    GLuint vao=0, vbo=0;
    int    count=0;
    struct V2D { float x,y,r,g,b; };
    void Upload(const NkVector<V2D>& v) {
        count = (int)v.Size();
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, v.Size()*sizeof(V2D), v.Data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V2D), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V2D), (void*)(2*4));
        glBindVertexArray(0);
    }
    void Draw() const { glBindVertexArray(vao); glDrawArrays(GL_TRIANGLES,0,count); glBindVertexArray(0); }
    void Free() { if (vao) { glDeleteVertexArrays(1,&vao); glDeleteBuffers(1,&vbo); vao=vbo=0; } }
};

// =============================================================================
// Shadow Map FBO
// =============================================================================
static const int kShadowRes = 1024;
struct ShadowMap {
    GLuint fbo=0, tex=0;
    bool Init() {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, kShadowRes, kShadowRes, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float border[] = {1,1,1,1}; glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
        glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
        bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return ok;
    }
    void Bind()         { glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0,0,kShadowRes,kShadowRes); glClear(GL_DEPTH_BUFFER_BIT); }
    void Unbind(int w, int h) { glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0,0,w,h); }
    void Free()         { if (fbo) { glDeleteFramebuffers(1,&fbo); glDeleteTextures(1,&tex); fbo=tex=0; } }
};

// =============================================================================
// Helpers uniforms — NkMat4f remplace Mat4
// =============================================================================
static void SetMat4(GLuint prog, const char* name, const NkMat4f& m) {
    glUniformMatrix4fv(glGetUniformLocation(prog, name), 1, GL_FALSE, m.data);
}
static void SetVec3(GLuint prog, const char* name, float x, float y, float z) {
    glUniform3f(glGetUniformLocation(prog, name), x, y, z);
}
static void SetInt(GLuint prog, const char* name, int v) {
    glUniform1i(glGetUniformLocation(prog, name), v);
}

// =============================================================================
// nkmain — point d'entrée
// =============================================================================
int nkmain(const nkentseu::NkEntryState&)
{
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  NkRHI Demo — 2D/3D + Phong + Shadow Mapping    ║\n");
    printf("║  ESC=Quitter | WASD=Caméra | ↑↓←→=Lumière       ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    // ── Fenêtre ──────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title     = "NkRHI Demo — 2D/3D + Phong + Ombres";
    winCfg.width     = 1280;
    winCfg.height    = 720;
    winCfg.centered  = true;
    winCfg.resizable = true;

    // ── Contexte OpenGL 3.3 ──────────────────────────────────────────────────
    NkContextDesc desc;
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    desc.api           = NkGraphicsApi::NK_API_WEBGL;
    desc.opengl.majorVersion = 3;
    desc.opengl.minorVersion = 0;
    desc.opengl.profile      = NkGLProfile::ES;
#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
    desc = NkContextDesc::MakeOpenGLES(3, 0);
#else
    desc.api    = NkGraphicsApi::NK_API_OPENGL;
    desc.opengl = NkOpenGLDesc::Desktop46(false);
    desc.opengl.majorVersion = 3;
    desc.opengl.minorVersion = 3;
#endif
    desc.opengl.msaaSamples        = 4;
    desc.opengl.srgbFramebuffer    = true;
    desc.opengl.swapInterval       = NkGLSwapInterval::AdaptiveVSync;
    desc.opengl.runtime.autoLoadEntryPoints  = false;
    desc.opengl.runtime.validateVersion      = true;
    desc.opengl.runtime.installDebugCallback = false;

    NkWindow window(winCfg);
    if (!window.IsOpen()) { printf("[RHIDemo] Fenêtre échouée\n"); return -1; }

    auto ctx = NkContextFactory::Create(window, desc);
    if (!ctx)  { printf("[RHIDemo] Contexte GL échoué\n"); window.Close(); return -2; }
    if (!LoadGL(ctx)) { ctx->Shutdown(); window.Close(); return -3; }

    // ── Shaders ──────────────────────────────────────────────────────────────
    GLuint progShadow = CompileShader(kShadowVS, kShadowFS);
    GLuint progPhong  = CompileShader(kPhongVS,  kPhongFS);
    GLuint prog2D     = CompileShader(k2DVS,     k2DFS);

    // ── Géométries ───────────────────────────────────────────────────────────
    Mesh cube, sphere, plane3d;
    cube.Upload(MakeCube(0.8f, 0.3f, 0.2f));
    sphere.Upload(MakeSphere(20, 20, 0.3f, 0.6f, 0.9f));
    plane3d.Upload(MakePlane(10.f, 0.6f, 0.65f, 0.5f));

    // Formes 2D (overlay coins écran)
    Mesh2D tri2d, quad2d;
    {
        using V = Mesh2D::V2D;
        NkVector<V> tv;
        tv.PushBack({20,60,1,0.8f,0}); tv.PushBack({80,60,0,1,0.2f}); tv.PushBack({50,110,0.2f,0.4f,1});
        tri2d.Upload(tv);

        NkVector<V> qv;
        qv.PushBack({1180,60,1,1,0}); qv.PushBack({1260,60,1,0,1}); qv.PushBack({1260,130,0,1,1});
        qv.PushBack({1180,60,1,1,0}); qv.PushBack({1260,130,0,1,1}); qv.PushBack({1180,130,0.5f,0,0.5f});
        quad2d.Upload(qv);
    }

    // ── Shadow map ───────────────────────────────────────────────────────────
    ShadowMap shadowMap;
    if (!shadowMap.Init()) printf("[RHIDemo] Shadow FBO incomplet (continuons quand même)\n");

    // ── État caméra ──────────────────────────────────────────────────────────
    float camYaw=0.3f, camPitch=0.4f, camDist=6.f;
    NkVec3f camTarget(0.f, 0.f, 0.f);

    // ── État lumière ─────────────────────────────────────────────────────────
    float lightYaw=0.8f, lightPitch=1.1f;

    auto LightDir = [&]() -> NkVec3f {
        return NkVec3f(NkCos(lightPitch)*NkSin(lightYaw),
                       NkSin(lightPitch),
                       NkCos(lightPitch)*NkCos(lightYaw)).Normalized();
    };

    // ── Boucle principale ─────────────────────────────────────────────────────
    NkClock    clock;
    auto& events = NkEvents();
    float time=0.f;
    bool running=true;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    while (running && window.IsOpen())
    {
        float dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f/60.f;
        time += dt;

        // ── Événements ───────────────────────────────────────────────────────
        NkEvent* ev = nullptr;
        while ((ev = events.PollEvent()) != nullptr) {
            if (ev->Is<NkWindowCloseEvent>()) { running=false; break; }
            if (auto* k = ev->As<NkKeyPressEvent>()) {
                if (k->GetKey() == NkKey::NK_ESCAPE) running=false;
            }
        }

        if (NkInput.IsKeyDown(NkKey::NK_A))     camYaw      -= 0.05f;
        if (NkInput.IsKeyDown(NkKey::NK_D))     camYaw      += 0.05f;
        if (NkInput.IsKeyDown(NkKey::NK_W))     camPitch    += 0.05f;
        if (NkInput.IsKeyDown(NkKey::NK_S))     camPitch    -= 0.05f;
        if (NkInput.IsKeyDown(NkKey::NK_Q))     camDist     -= 0.05f;
        if (NkInput.IsKeyDown(NkKey::NK_E))     camDist     += 0.05f;

        if (NkInput.IsKeyDown(NkKey::NK_LEFT))  lightYaw    -= 0.1f;
        if (NkInput.IsKeyDown(NkKey::NK_RIGHT)) lightYaw    += 0.1f;
        if (NkInput.IsKeyDown(NkKey::NK_UP))    { lightPitch += 0.1f; if (lightPitch > 1.5f) lightPitch = 1.5f; }
        if (NkInput.IsKeyDown(NkKey::NK_DOWN))  { lightPitch -= 0.1f; if (lightPitch < 0.1f) lightPitch = 0.1f; }

        if (!running) break;

        // ── Calcul matrices ──────────────────────────────────────────────────
        auto surf = window.GetSurfaceDesc();
        int W = surf.width, H = surf.height;
        if (W <= 0 || H <= 0) { NkClock::SleepMilliseconds(8); continue; }

        float aspect = (float)W / (float)H;
        NkMat4f proj = NkMat4f::Perspective(NkAngle(45.f), aspect, 0.1f, 100.f);

        // Caméra orbitale
        NkVec3f camEye(
            camTarget.x + camDist * NkCos(camPitch) * NkSin(camYaw),
            camTarget.y + camDist * NkSin(camPitch),
            camTarget.z + camDist * NkCos(camPitch) * NkCos(camYaw)
        );
        NkMat4f view = NkMat4f::LookAt(camEye, camTarget, NkVec3f(0.f,1.f,0.f));
        NkMat4f vp   = proj * view;

        // Lumière
        NkVec3f ld       = LightDir();
        NkVec3f lightPos = ld * 8.f;
        NkMat4f lightView = NkMat4f::LookAt(lightPos, camTarget, NkVec3f(0.f,1.f,0.f));
        NkMat4f lightProj = NkMat4f::Orthogonal(NkVec2f(-6.f,-6.f), NkVec2f(6.f,6.f), 0.5f, 30.f);
        NkMat4f lightVP   = lightProj * lightView;

        // Modèles 3D
        NkMat4f mCube  = NkMat4f::Translation(NkVec3f(0.f,0.5f,0.f))
                       * NkMat4f::RotationY(NkAngle(time*0.8f))
                       * NkMat4f::RotationX(NkAngle(time*0.3f));
        NkMat4f mSph   = NkMat4f::Translation(NkVec3f(2.5f*NkSin(time*0.5f), 0.5f, 2.5f*NkCos(time*0.5f)))
                       * NkMat4f::Scaling(NkVec3f(0.7f,0.7f,0.7f));
        NkMat4f mPlane = NkMat4f::Identity();

        // ── Passe ombre ──────────────────────────────────────────────────────
        if (shadowMap.fbo) {
            shadowMap.Bind();
            glCullFace(GL_FRONT);
            glUseProgram(progShadow);
            auto DrawShadow = [&](const Mesh& mesh, const NkMat4f& model) {
                SetMat4(progShadow, "uLightMVP", lightVP * model);
                mesh.Draw();
            };
            DrawShadow(cube,    mCube);
            DrawShadow(sphere,  mSph);
            DrawShadow(plane3d, mPlane);
            shadowMap.Unbind(W, H);
            glCullFace(GL_BACK);
        }

        // ── Passe principale ─────────────────────────────────────────────────
        if (ctx->BeginFrame()) {
            glViewport(0, 0, W, H);
            glClearColor(0.12f, 0.14f, 0.18f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);

            glUseProgram(progPhong);
            SetVec3(progPhong, "uLightDir",   ld.x, ld.y, ld.z);
            SetVec3(progPhong, "uLightColor", 1.f, 0.95f, 0.85f);
            SetVec3(progPhong, "uCameraPos",  camEye.x, camEye.y, camEye.z);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, shadowMap.tex);
            SetInt(progPhong, "uShadowMap", 0);

            auto Draw3D = [&](const Mesh& mesh, const NkMat4f& model) {
                SetMat4(progPhong, "uMVP",      vp * model);
                SetMat4(progPhong, "uModel",    model);
                SetMat4(progPhong, "uLightMVP", lightVP * model);
                mesh.Draw();
            };
            Draw3D(plane3d, mPlane);
            Draw3D(cube,    mCube);
            Draw3D(sphere,  mSph);

            // ── Overlay 2D ───────────────────────────────────────────────────
            glDisable(GL_DEPTH_TEST);
            glUseProgram(prog2D);
            NkMat4f ortho = NkMat4f::Orthogonal(NkVec2f(0.f,(float)H), NkVec2f((float)W,0.f), -1.f, 1.f);
            SetMat4(prog2D, "uOrtho", ortho);
            tri2d.Draw();
            quad2d.Draw();
            glEnable(GL_DEPTH_TEST);

            ctx->EndFrame();
            ctx->Present();
        }

        NkClock::SleepMilliseconds(16);
    }

    // ── Nettoyage ────────────────────────────────────────────────────────────
    cube.Free(); sphere.Free(); plane3d.Free();
    tri2d.Free(); quad2d.Free();
    shadowMap.Free();
    glDeleteProgram(progShadow);
    glDeleteProgram(progPhong);
    glDeleteProgram(prog2D);

    ctx->Shutdown();
    window.Close();
    printf("[RHIDemo] Terminé proprement.\n");
    return 0;
}
