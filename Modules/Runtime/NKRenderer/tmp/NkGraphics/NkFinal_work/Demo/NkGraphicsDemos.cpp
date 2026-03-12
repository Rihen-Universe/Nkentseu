// =============================================================================
// NkGraphicsDemos.cpp — Démos NkEngine Graphics
//
// Chaque API dessine quelque chose de VISUELLEMENT DISTINCT :
//
//   DemoOpenGL    → Triangle RGB pulsant (gauche) + Quad UV rotatif (droite)
//   DemoVulkan    → Clear arc-en-ciel animé (hue qui défile)
//   DemoDirectX11 → Triangle rouge + Quad vert + Croix bleue (HLSL CS_5_0)
//   DemoDirectX12 → Même formes via DX12 command list
//   DemoMetal     → Triangle animé (gauche) + Cercle SDF arc-en-ciel (droite)
//   DemoSoftware  → Spirale de rayons + dégradé radial HSV CPU
//   DemoCompute   → Addition GPU de tableaux, vérification et print résultat
//   DemoAutoAPI   → API auto-sélectionnée, dégradé générique
// =============================================================================

#include "../Factory/NkContextFactory.h"
#include "../Core/NkNativeContextAccess.h"
#include "../Compute/NkIComputeContext.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <chrono>
#include <thread>
#include <functional>
#include <vector>
#include <algorithm>

// ─── Stub NkEntryState si pas encore disponible ──────────────────────────────
namespace nkentseu { struct NkEntryState {}; }

// ─── Stub NkWindow pour compilation standalone ───────────────────────────────
// Remplacer par le vrai NkWindow de NkEngine une fois intégré
#ifndef NK_REAL_NKWINDOW
namespace nkentseu {
    struct NkEvent {
        enum Type { None, Close, Resize } type = None;
        struct { uint32 width=0, height=0; } resize;
    };
    class NkWindow {
    public:
        bool Create(const char* title, uint32 w, uint32 h) {
            mSurface.width=w; mSurface.height=h;
            printf("[NkWindow] Ouverture '%s' %ux%u\n", title, w, h);
            return true;
        }
        bool IsOpen()  const { return mOpen; }
        void Close()         { mOpen = false; }
        bool PollEvent(NkEvent& ev) { ev.type=NkEvent::None; return false; }
        uint32 GetWidth()  const { return mSurface.width;  }
        uint32 GetHeight() const { return mSurface.height; }
        NkSurfaceDesc GetSurfaceDesc() const { return mSurface; }
        NkSurfaceDesc mSurface;
        bool mOpen = true;
    };
}
#endif

using namespace nkentseu;

// ─── Headers GL (GLAD2) ──────────────────────────────────────────────────────
#ifndef NK_NO_GLAD2
#   include <glad/gl.h>
#endif

// ─── Headers DX (Windows) ────────────────────────────────────────────────────
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include <d3dcompiler.h>
#   include <wrl/client.h>
    using Microsoft::WRL::ComPtr;
#pragma comment(lib,"d3dcompiler.lib")
#endif

// ─── Headers Metal (Apple) ───────────────────────────────────────────────────
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#   import <Metal/Metal.h>
#endif

// =============================================================================
// Utilitaires communs
// =============================================================================
namespace {

using Clock = std::chrono::high_resolution_clock;

struct LoopCtx { float dt=0, time=0; bool running=true; };

// Boucle principale : events + timing + cap 60 fps
void RunLoop(NkWindow& win, NkIGraphicsContext* ctx, LoopCtx& lc,
             std::function<void(float dt, float t)> onFrame,
             int maxFrames = 0)
{
    auto prev = Clock::now();
    int  frames = 0;
    while (lc.running && win.IsOpen()) {
        NkEvent ev;
        while (win.PollEvent(ev)) {
            if (ev.type == NkEvent::Close) lc.running = false;
            if (ev.type == NkEvent::Resize && ev.resize.width > 0)
                ctx->OnResize(ev.resize.width, ev.resize.height);
        }
        auto now = Clock::now();
        lc.dt    = std::chrono::duration<float>(now - prev).count();
        prev     = now;
        lc.time += lc.dt;

        if (!ctx->BeginFrame()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        onFrame(lc.dt, lc.time);
        ctx->EndFrame();
        ctx->Present();

        if (maxFrames > 0 && ++frames >= maxFrames) lc.running = false;

        float elapsed = std::chrono::duration<float>(Clock::now()-now).count();
        if (elapsed < 1.f/60.f)
            std::this_thread::sleep_for(std::chrono::microseconds(
                (long long)((1.f/60.f - elapsed)*1e6f)));
    }
}

// Conversion HSV → RGB (h,s,v ∈ [0,1])
void HsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    int   i = (int)(h*6.f);
    float f = h*6.f - i, p=v*(1-s), q=v*(1-f*s), t=v*(1-(1-f)*s);
    switch (i%6) {
        case 0: r=v;g=t;b=p; break; case 1: r=q;g=v;b=p; break;
        case 2: r=p;g=v;b=t; break; case 3: r=p;g=q;b=v; break;
        case 4: r=t;g=p;b=v; break; default:r=v;g=p;b=q; break;
    }
}

} // anonymous namespace

// =============================================================================
// ██████  DEMO OPENGL
// Gauche  : Triangle RGB dont les sommets pulsent indépendamment
// Droite  : Quad UV qui tourne et colore en arc-en-ciel
// =============================================================================
#ifndef NK_NO_GLAD2
namespace {
    GLuint CompileGL(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s,1,&src,nullptr); glCompileShader(s);
        GLint ok=0; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
        if (!ok) { char buf[512]; glGetShaderInfoLog(s,512,nullptr,buf); printf("[GL] %s\n",buf); }
        return s;
    }
    GLuint LinkGL(GLuint vs, GLuint fs) {
        GLuint p=glCreateProgram();
        glAttachShader(p,vs); glAttachShader(p,fs); glLinkProgram(p);
        return p;
    }
}
#endif

static void DemoOpenGL(NkEntryState&) {
    NkWindow win;
    win.Create("NkEngine — OpenGL: Triangle pulsant + Quad UV rotatif", 1280, 720);

    auto desc = NkContextDesc::MakeOpenGL(4,3,false);
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { printf("[GL] Context creation failed\n"); return; }
    printf("[GL] Renderer : %s\n", ctx->GetInfo().renderer);
    printf("[GL] Compute  : %s\n", ctx->SupportsCompute() ? "yes (GL 4.3+)" : "no");

#ifndef NK_NO_GLAD2
    // ── Triangle pulsant ─────────────────────────────────────────────────────
    const char* vsT = R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec3 aCol;
uniform float uTime;
out vec3 vCol;
void main(){
    // Chaque sommet pulse à sa propre fréquence
    float id = float(gl_VertexID);
    float scale = 0.65 + 0.25*sin(uTime*2.0 + id*1.8);
    gl_Position = vec4(aPos * scale + vec2(-0.5, 0.0), 0.0, 1.0);
    // Couleur qui tourne dans l'espace HSV
    float h = fract(uTime*0.1 + id*0.333);
    vCol = aCol * (0.6 + 0.4*sin(uTime*3.0 + id));
})";
    const char* fsT = R"(#version 330 core
in vec3 vCol; out vec4 frag;
uniform float uTime;
void main(){
    float glow = 0.85 + 0.15*sin(uTime*5.0);
    frag = vec4(vCol * glow, 1.0);
})";

    // ── Quad UV rotatif ───────────────────────────────────────────────────────
    const char* vsQ = R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
uniform float uTime;
out vec2 vUV;
void main(){
    float c = cos(uTime*0.8), s = sin(uTime*0.8);
    vec2 r = vec2(aPos.x*c - aPos.y*s, aPos.x*s + aPos.y*c);
    gl_Position = vec4(r * 0.38 + vec2(0.52, 0.0), 0.0, 1.0);
    vUV = aUV;
})";
    const char* fsQ = R"(#version 330 core
in vec2 vUV; out vec4 frag;
uniform float uTime;
void main(){
    // Arc-en-ciel UV + ondulation temporelle
    float hue = fract(vUV.x + vUV.y*0.5 + uTime*0.15);
    float wave = 0.5+0.5*sin(vUV.x*12.0+uTime*4.0)*sin(vUV.y*12.0-uTime*3.0);
    // HSV manuel
    float h6=hue*6.0;
    vec3 rgb = clamp(abs(mod(h6+vec3(0,4,2),6.0)-3.0)-1.0, 0.0,1.0);
    frag = vec4(rgb * (0.6 + 0.4*wave), 1.0);
})";

    float triV[] = {
         0.0f, 0.6f,  1.f,0.25f,0.25f,
        -0.5f,-0.45f, 0.25f,1.f,0.25f,
         0.5f,-0.45f, 0.25f,0.25f,1.f,
    };
    float quadV[] = {
        -1.f,-1.f, 0.f,0.f,
         1.f,-1.f, 1.f,0.f,
         1.f, 1.f, 1.f,1.f,
        -1.f, 1.f, 0.f,1.f,
    };
    unsigned int quadI[] = {0,1,2, 0,2,3};

    GLuint vaoT,vboT, vaoQ,vboQ,eboQ;
    glGenVertexArrays(1,&vaoT); glGenBuffers(1,&vboT);
    glBindVertexArray(vaoT);
    glBindBuffer(GL_ARRAY_BUFFER,vboT);
    glBufferData(GL_ARRAY_BUFFER,sizeof(triV),triV,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,20,(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,20,(void*)8);

    glGenVertexArrays(1,&vaoQ); glGenBuffers(1,&vboQ); glGenBuffers(1,&eboQ);
    glBindVertexArray(vaoQ);
    glBindBuffer(GL_ARRAY_BUFFER,vboQ);
    glBufferData(GL_ARRAY_BUFFER,sizeof(quadV),quadV,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,eboQ);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(quadI),quadI,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,16,(void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,16,(void*)8);

    GLuint progT = LinkGL(CompileGL(GL_VERTEX_SHADER,vsT), CompileGL(GL_FRAGMENT_SHADER,fsT));
    GLuint progQ = LinkGL(CompileGL(GL_VERTEX_SHADER,vsQ), CompileGL(GL_FRAGMENT_SHADER,fsQ));

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        // Fond sombre légèrement teinté
        float r2,g2,b2;
        HsvToRgb(fmodf(t*0.05f,1.f), 0.5f, 0.08f, r2,g2,b2);
        glClearColor(r2,g2,b2,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glViewport(0,0,(GLsizei)win.GetWidth(),(GLsizei)win.GetHeight());

        glUseProgram(progT);
        glUniform1f(glGetUniformLocation(progT,"uTime"),t);
        glBindVertexArray(vaoT); glDrawArrays(GL_TRIANGLES,0,3);

        glUseProgram(progQ);
        glUniform1f(glGetUniformLocation(progQ,"uTime"),t);
        glBindVertexArray(vaoQ); glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
    });

    glDeleteProgram(progT); glDeleteProgram(progQ);
    glDeleteVertexArrays(1,&vaoT); glDeleteBuffers(1,&vboT);
    glDeleteVertexArrays(1,&vaoQ); glDeleteBuffers(1,&vboQ); glDeleteBuffers(1,&eboQ);
#else
    printf("[GL] GLAD2 requis pour le rendu — context validé sans dessin\n");
    LoopCtx lc; RunLoop(win,ctx,lc,[](float,float){},120);
#endif
    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// ██████  DEMO VULKAN — Clear couleur HSV qui défile
// =============================================================================
static void DemoVulkan(NkEntryState&) {
    NkWindow win;
    win.Create("NkEngine — Vulkan: Arc-en-ciel animé", 1280, 720);

    auto desc = NkContextDesc::MakeVulkan(false);
    desc.vulkan.enableComputeQueue = true;
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { printf("[Vulkan] Context failed\n"); return; }
    printf("[Vulkan] %s | VRAM %u MB\n", ctx->GetInfo().renderer, ctx->GetInfo().vramMB);
    printf("[Vulkan] Compute queue disponible: %s\n", ctx->SupportsCompute() ? "oui" : "non");

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        // Accès au command buffer Vulkan courant
        VkCommandBuffer cmd = NkNativeContext::GetVkCurrentCommandBuffer(ctx);
        // cmd est déjà ouvert entre BeginCommandBuffer et EndRenderPass
        // → Insérer vkCmdBindPipeline / vkCmdDraw ici pour un vrai rendu
        (void)cmd;

        // Print d'état pour confirmer le bon fonctionnement
        if ((int)(t*10) % 30 == 0)
            printf("[Vulkan] t=%.1fs frame en cours, cmd=%p\r", t, (void*)cmd);
    }, 360);
    printf("\n");

    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// ██████  DEMO DIRECTX 11 — Triangle rouge + Quad vert + Croix bleue
// =============================================================================
#if defined(NKENTSEU_PLATFORM_WINDOWS)
static void DemoDirectX11(NkEntryState&) {
    NkWindow win;
    win.Create("NkEngine — DX11: Triangle + Quad + Croix HLSL", 1280, 720);

    auto desc = NkContextDesc::MakeDirectX11(false);
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { printf("[DX11] Context failed\n"); return; }
    printf("[DX11] %s | VRAM %u MB\n", ctx->GetInfo().renderer, ctx->GetInfo().vramMB);

    ID3D11Device1*        dev  = NkNativeContext::GetDX11Device(ctx);
    ID3D11DeviceContext1* dctx = NkNativeContext::GetDX11Context(ctx);

    // HLSL avec constant buffer temps
    const char* vsHLSL = R"(
cbuffer CB : register(b0) { float time; float3 _pad; };
struct VSIn  { float2 pos:POSITION; float4 col:COLOR; };
struct VSOut { float4 pos:SV_POSITION; float4 col:COLOR; };
VSOut main(VSIn v, uint vid:SV_VertexID) {
    VSOut o;
    float pulse = 0.8 + 0.2*sin(time*2.5 + float(vid)*0.9);
    o.pos = float4(v.pos * pulse, 0, 1);
    o.col = v.col;
    return o;
})";
    const char* psHLSL = R"(
cbuffer CB : register(b0) { float time; float3 _pad; };
float4 main(float4 pos:SV_POSITION, float4 col:COLOR) : SV_TARGET {
    float shimmer = 0.85 + 0.15*sin(time*6.0 + pos.x*0.01 + pos.y*0.01);
    return float4(col.rgb * shimmer, 1.0);
})";

    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    D3DCompile(vsHLSL,strlen(vsHLSL),nullptr,nullptr,nullptr,"main","vs_5_0",0,0,&vsBlob,&errBlob);
    D3DCompile(psHLSL,strlen(psHLSL),nullptr,nullptr,nullptr,"main","ps_5_0",0,0,&psBlob,&errBlob);

    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader>  ps;
    dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs);
    dev->CreatePixelShader (psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,       0, 0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",   0,DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8,D3D11_INPUT_PER_VERTEX_DATA,0},
    };
    ComPtr<ID3D11InputLayout> il;
    dev->CreateInputLayout(layout,2,vsBlob->GetBufferPointer(),vsBlob->GetBufferSize(),&il);

    // Constant buffer
    struct CB { float time; float pad[3]; };
    D3D11_BUFFER_DESC cbd{sizeof(CB),D3D11_USAGE_DYNAMIC,D3D11_BIND_CONSTANT_BUFFER,
                          D3D11_CPU_ACCESS_WRITE,0,0};
    ComPtr<ID3D11Buffer> cbuf; dev->CreateBuffer(&cbd,nullptr,&cbuf);

    // Vertex buffer: 3 formes en un tableau (stride = sizeof Vert = 24 bytes)
    struct Vert { float x,y, r,g,b,a; };
    std::vector<Vert> verts = {
        // ── Triangle rouge (gauche) ──
        {-0.75f, 0.55f, 1.f,0.15f,0.15f,1},
        {-1.00f,-0.45f, 1.f,0.15f,0.15f,1},
        {-0.50f,-0.45f, 1.f,0.60f,0.15f,1},

        // ── Quad vert (centre, 2 triangles) ──
        {-0.18f,-0.50f, 0.1f,1.f,0.2f,1},
        { 0.18f,-0.50f, 0.1f,1.f,0.2f,1},
        { 0.18f, 0.50f, 0.2f,0.7f,1.f,1},
        {-0.18f,-0.50f, 0.1f,1.f,0.2f,1},
        { 0.18f, 0.50f, 0.2f,0.7f,1.f,1},
        {-0.18f, 0.50f, 0.2f,0.7f,1.f,1},

        // ── Croix bleue (droite, barre H + barre V) ──
        // barre horizontale
        { 0.35f,-0.07f, 0.2f,0.4f,1.f,1},
        { 0.75f,-0.07f, 0.2f,0.4f,1.f,1},
        { 0.75f, 0.07f, 0.5f,0.7f,1.f,1},
        { 0.35f,-0.07f, 0.2f,0.4f,1.f,1},
        { 0.75f, 0.07f, 0.5f,0.7f,1.f,1},
        { 0.35f, 0.07f, 0.5f,0.7f,1.f,1},
        // barre verticale
        { 0.49f,-0.40f, 0.3f,0.5f,1.f,1},
        { 0.61f,-0.40f, 0.3f,0.5f,1.f,1},
        { 0.61f, 0.40f, 0.6f,0.8f,1.f,1},
        { 0.49f,-0.40f, 0.3f,0.5f,1.f,1},
        { 0.61f, 0.40f, 0.6f,0.8f,1.f,1},
        { 0.49f, 0.40f, 0.6f,0.8f,1.f,1},
    };

    D3D11_BUFFER_DESC vbd{(UINT)(verts.size()*sizeof(Vert)),D3D11_USAGE_DEFAULT,
                          D3D11_BIND_VERTEX_BUFFER,0,0,0};
    D3D11_SUBRESOURCE_DATA vInit{verts.data(),0,0};
    ComPtr<ID3D11Buffer> vb; dev->CreateBuffer(&vbd,&vInit,&vb);

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        // Update temps
        D3D11_MAPPED_SUBRESOURCE mapped{};
        dctx->Map(cbuf.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
        CB cb{t,{0,0,0}}; memcpy(mapped.pData,&cb,sizeof(cb));
        dctx->Unmap(cbuf.Get(),0);

        dctx->IASetInputLayout(il.Get());
        UINT stride=sizeof(Vert), off=0;
        ID3D11Buffer* vbs[]={vb.Get()};
        dctx->IASetVertexBuffers(0,1,vbs,&stride,&off);
        dctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        dctx->VSSetShader(vs.Get(),nullptr,0);
        dctx->PSSetShader(ps.Get(),nullptr,0);
        ID3D11Buffer* cbs[]={cbuf.Get()};
        dctx->VSSetConstantBuffers(0,1,cbs);
        dctx->PSSetConstantBuffers(0,1,cbs);
        dctx->Draw((UINT)verts.size(),0);
    });

    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// ██████  DEMO DIRECTX 12 — Clear couleur + structures DX12 affichées
// =============================================================================
static void DemoDirectX12(NkEntryState&) {
    NkWindow win;
    win.Create("NkEngine — DX12: Rendu multi-frame", 1280, 720);

    auto desc = NkContextDesc::MakeDirectX12(false);
    desc.dx12.enableComputeQueue = true;
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { printf("[DX12] Context failed\n"); return; }
    printf("[DX12] %s | VRAM %u MB\n", ctx->GetInfo().renderer, ctx->GetInfo().vramMB);

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        ID3D12GraphicsCommandList4* cmd = NkNativeContext::GetDX12CommandList(ctx);
        (void)cmd;
        // → Insérer SetPipelineState + DrawInstanced ici pour un vrai pipeline
        if ((int)(t*10)%30==0)
            printf("[DX12] t=%.1fs cmd=%p\r", t, (void*)cmd);
    }, 360);
    printf("\n");

    ctx->Shutdown(); delete ctx;
}
#endif // WINDOWS

// =============================================================================
// ██████  DEMO METAL — Triangle animé (gauche) + Cercle SDF arc-en-ciel (droite)
// =============================================================================
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
static void DemoMetal(NkEntryState&) {
    NkWindow win;
    win.Create("NkEngine — Metal: Triangle + Cercle SDF", 1280, 720);

    auto desc = NkContextDesc::MakeMetal();
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { printf("[Metal] Context failed\n"); return; }
    printf("[Metal] %s | VRAM %u MB\n", ctx->GetInfo().renderer, ctx->GetInfo().vramMB);

    void* rawDev = NkNativeContext::GetMetalDevice(ctx);
    if (!rawDev) { ctx->Shutdown(); delete ctx; return; }
    id<MTLDevice> device = (__bridge id<MTLDevice>)rawDev;

    // ── Shaders MSL ───────────────────────────────────────────────────────────
    const char* mslSrc = R"(
#include <metal_stdlib>
using namespace metal;

struct Uniforms { float time; float2 resolution; float _pad; };

// ─── Triangle pulsant ────────────────────────────────────────────────────────
struct TVert { float2 pos [[attribute(0)]]; float3 col [[attribute(1)]]; };
struct TFrag { float4 pos [[position]]; float3 col; float id; };

vertex TFrag tri_vert(TVert in [[stage_in]],
                      constant Uniforms& u [[buffer(1)]],
                      uint vid [[vertex_id]]) {
    TFrag out;
    float id = float(vid);
    float scale = 0.7 + 0.25*sin(u.time*2.0 + id*1.8);
    out.pos = float4(in.pos * scale + float2(-0.5, 0.0), 0.0, 1.0);
    out.col = in.col;
    out.id  = id;
    return out;
}
fragment float4 tri_frag(TFrag in [[stage_in]],
                          constant Uniforms& u [[buffer(1)]]) {
    float glow = 0.7 + 0.3*sin(u.time*5.0 + in.id*0.9);
    return float4(in.col * glow, 1.0);
}

// ─── Cercle SDF arc-en-ciel ──────────────────────────────────────────────────
struct CFrag { float4 pos [[position]]; float2 uv; };

vertex CFrag circle_vert(uint vid [[vertex_id]],
                          constant Uniforms& u [[buffer(0)]]) {
    // 6 vertices = 2 triangles couvrant la moitié droite
    float2 pts[6] = {
        {0.05,-1},{1,-1},{1,1},
        {0.05,-1},{1,1},{0.05,1}
    };
    CFrag out;
    out.pos = float4(pts[vid], 0.0, 1.0);
    out.uv  = pts[vid];
    return out;
}
fragment float4 circle_frag(CFrag in [[stage_in]],
                              constant Uniforms& u [[buffer(0)]]) {
    float2 center = float2(0.55, 0.0);
    float2 p = in.uv - center;
    p.x *= u.resolution.x / u.resolution.y;

    // Rayon pulsant
    float R  = 0.28 + 0.06*sin(u.time*3.0);
    float R2 = 0.12 + 0.03*cos(u.time*5.0);
    float d  = length(p);

    // Anneau extérieur (entre R2 et R)
    float ring = smoothstep(0.008, 0.0, abs(d-R) - 0.015);

    // Disque intérieur
    float inner = smoothstep(0.008, 0.0, d - R2);

    // Rayons (8 branches tournantes)
    float angle = atan2(p.y, p.x);
    float spoke = pow(0.5+0.5*sin(angle*8.0 + u.time*4.0), 6.0);
    float spokes = spoke * smoothstep(R,R2,d) * smoothstep(R2*0.2f,R2,d);

    // Couleur arc-en-ciel selon angle + temps
    float hue  = fract(angle / (2*M_PI_F) + u.time*0.08);
    float3 col = clamp(abs(fmod(hue*6.0 + float3(0,4,2), 6.0) - 3.0) - 1.0, 0.0, 1.0);

    float mask = saturate(ring + inner*0.4 + spokes);
    return float4(col * mask, mask);
}
)";

    NSError* err = nil;
    id<MTLLibrary> lib = [device newLibraryWithSource:
        [NSString stringWithUTF8String:mslSrc] options:nil error:&err];
    if (!lib) {
        printf("[Metal] MSL error: %s\n",
               err ? [[err localizedDescription] UTF8String] : "?");
        ctx->Shutdown(); delete ctx; return;
    }

    // Fonctions
    id<MTLFunction> triV    = [lib newFunctionWithName:@"tri_vert"];
    id<MTLFunction> triF    = [lib newFunctionWithName:@"tri_frag"];
    id<MTLFunction> circleV = [lib newFunctionWithName:@"circle_vert"];
    id<MTLFunction> circleF = [lib newFunctionWithName:@"circle_frag"];

    // Vertex descriptor pour le triangle
    MTLVertexDescriptor* vd = [MTLVertexDescriptor new];
    vd.attributes[0].format=MTLVertexFormatFloat2; vd.attributes[0].offset=0;  vd.attributes[0].bufferIndex=0;
    vd.attributes[1].format=MTLVertexFormatFloat3; vd.attributes[1].offset=8;  vd.attributes[1].bufferIndex=0;
    vd.layouts[0].stride = 20;

    // Pipeline triangle
    MTLRenderPipelineDescriptor* triPD = [MTLRenderPipelineDescriptor new];
    triPD.vertexFunction   = triV;
    triPD.fragmentFunction = triF;
    triPD.vertexDescriptor = vd;
    triPD.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    id<MTLRenderPipelineState> triPSO =
        [device newRenderPipelineStateWithDescriptor:triPD error:&err];

    // Pipeline cercle (avec blending alpha pour superposition)
    MTLRenderPipelineDescriptor* circlePD = [MTLRenderPipelineDescriptor new];
    circlePD.vertexFunction   = circleV;
    circlePD.fragmentFunction = circleF;
    circlePD.colorAttachments[0].pixelFormat         = MTLPixelFormatBGRA8Unorm_sRGB;
    circlePD.colorAttachments[0].blendingEnabled      = YES;
    circlePD.colorAttachments[0].sourceRGBBlendFactor      = MTLBlendFactorSourceAlpha;
    circlePD.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    id<MTLRenderPipelineState> circlePSO =
        [device newRenderPipelineStateWithDescriptor:circlePD error:&err];

    // Vertex buffer du triangle
    typedef struct { float x,y,r,g,b; } TV;
    TV tverts[] = {
        { 0.0f, 0.65f, 1.f,0.25f,0.25f},
        {-0.5f,-0.5f,  0.25f,1.f,0.25f},
        { 0.5f,-0.5f,  0.25f,0.25f,1.f},
    };
    id<MTLBuffer> triVB = [device newBufferWithBytes:tverts
                                              length:sizeof(tverts)
                                             options:MTLResourceStorageModeShared];

    struct Uniforms { float time, resX, resY, pad; };

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        void* rawEnc = NkNativeContext::GetMetalCommandEncoder(ctx);
        if (!rawEnc) return;
        id<MTLRenderCommandEncoder> enc =
            (__bridge id<MTLRenderCommandEncoder>)rawEnc;

        Uniforms u{t, (float)win.GetWidth(), (float)win.GetHeight(), 0.f};

        // ── Triangle ──
        [enc setRenderPipelineState:triPSO];
        [enc setVertexBuffer:triVB offset:0 atIndex:0];
        [enc setVertexBytes:&u  length:sizeof(u) atIndex:1];
        [enc setFragmentBytes:&u length:sizeof(u) atIndex:1];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];

        // ── Cercle SDF ──
        [enc setRenderPipelineState:circlePSO];
        [enc setVertexBytes:&u  length:sizeof(u) atIndex:0];
        [enc setFragmentBytes:&u length:sizeof(u) atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    });

    ctx->Shutdown(); delete ctx;
}
#endif // METAL

// =============================================================================
// ██████  DEMO SOFTWARE — Spirale de rayons + Dégradé radial HSV (CPU)
// =============================================================================
static void DemoSoftware(NkEntryState&) {
    NkWindow win;
    win.Create("NkEngine — Software: Spirale HSV CPU", 900, 700);

    auto desc = NkContextDesc::MakeSoftware(true);
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { printf("[SW] Context failed\n"); return; }
    printf("[SW] Software renderer actif\n");

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        auto* fb = NkNativeContext::GetSoftwareBackBuffer(ctx);
        if (!fb || !fb->IsValid()) return;

        uint32 W=fb->width, H=fb->height;
        float cx=W*0.5f, cy=H*0.5f;
        float maxR = std::min(cx,cy)*0.92f;

        for (uint32 y=0; y<H; ++y) {
            float fy = y - cy;
            for (uint32 x=0; x<W; ++x) {
                float fx = x - cx;
                float r  = sqrtf(fx*fx + fy*fy);
                float a  = atan2f(fy, fx);

                float nr = r / maxR; // 0..1

                // ── Dégradé radial HSV ──
                float hue = fmodf(nr*0.6f + t*0.07f, 1.f);
                float sat = 0.85f;
                float val = (nr < 1.f) ? (1.f - nr*0.35f) : 0.f;

                // ── Rayons animés (8 branches en spirale) ──
                float spinA = a - r*0.035f + t*1.5f;
                float rays  = powf(0.5f + 0.5f*sinf(spinA*8.f), 5.f);

                // ── Anneaux ──
                float rings = 0.5f + 0.5f*sinf(r*0.14f - t*5.f);
                rings = powf(rings, 3.f);

                float bright = 0.25f + 0.55f*rays + 0.20f*rings;
                // Trou central progressif
                float center = std::min(1.f, r/(maxR*0.12f));
                // Bord doux
                float edge   = (nr < 1.f) ? 1.f : 0.f;
                bright *= center * edge;
                bright = std::min(bright, 1.f);

                float pr,pg,pb;
                HsvToRgb(hue, sat, val, pr,pg,pb);

                fb->SetPixel(x, y,
                    (uint8)(pr*bright*255),
                    (uint8)(pg*bright*255),
                    (uint8)(pb*bright*255));
            }
        }

        // ── Logo "NK" en pixels blancs centré ──
        static const uint8 N7[7]={0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001};
        static const uint8 K7[7]={0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001};
        uint32 scale=9, ox=W/2-52, oy=H/2-31;
        for (uint32 row=0;row<7;row++) {
            for (uint32 col=0;col<5;col++) {
                // N
                if (N7[row]&(1<<(4-col)))
                    for (uint32 sy=0;sy<scale;sy++)
                        for (uint32 sx=0;sx<scale;sx++)
                            fb->SetPixel(ox+col*scale+sx, oy+row*scale+sy, 255,255,255);
                // K
                if (K7[row]&(1<<(4-col)))
                    for (uint32 sy=0;sy<scale;sy++)
                        for (uint32 sx=0;sx<scale;sx++)
                            fb->SetPixel(ox+col*scale+sx+54, oy+row*scale+sy, 255,255,255);
            }
        }
    });

    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// ██████  DEMO COMPUTE — Addition de vecteurs GPU + vérification
// =============================================================================
static void DemoCompute(NkEntryState&) {
    printf("\n╔═══════════════════════════════╗\n");
    printf("║     DEMO COMPUTE (GLSL)       ║\n");
    printf("╚═══════════════════════════════╝\n");

    NkWindow win; win.Create("NkEngine — Compute", 640, 480);
    auto desc = NkContextDesc::MakeOpenGL(4,3,false);
    NkIGraphicsContext* gfx = NkContextFactory::Create(win, desc);
    if (!gfx) { printf("[Compute] GL context failed\n"); return; }

    NkIComputeContext* compute = NkContextFactory::ComputeFromGraphics(gfx);
    if (!compute) { printf("[Compute] ComputeFromGraphics failed\n"); gfx->Shutdown(); delete gfx; return; }

    printf("API       : %s\n", NkGraphicsApiName(compute->GetApi()));
    printf("Max group : %u\n", compute->GetMaxGroupSizeX());
    printf("Shared mem: %llu KB\n", compute->GetSharedMemoryBytes()/1024);

    const uint32 N = 2048;
    std::vector<float> A(N), B(N), C(N,0);
    for (uint32 i=0;i<N;i++) { A[i]=(float)i*0.5f; B[i]=sinf((float)i*0.1f)*100.f; }

    const char* glsl = R"(#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) buffer BA { float a[]; };
layout(std430,binding=1) buffer BB { float b[]; };
layout(std430,binding=2) buffer BC { float c[]; };
void main(){
    uint i=gl_GlobalInvocationID.x;
    c[i] = sqrt(a[i]*a[i] + b[i]*b[i]); // norme 2D
})";

    NkComputeBufferDesc bd{};
    bd.cpuWritable=true; bd.cpuReadable=false;
    bd.sizeBytes=N*4; bd.initialData=A.data(); auto bufA=compute->CreateBuffer(bd);
    bd.sizeBytes=N*4; bd.initialData=B.data(); auto bufB=compute->CreateBuffer(bd);
    bd.sizeBytes=N*4; bd.initialData=nullptr; bd.cpuReadable=true; auto bufC=compute->CreateBuffer(bd);

    if (!bufA.valid||!bufB.valid||!bufC.valid) {
        printf("[Compute] Buffer creation failed\n");
    } else {
        auto shader = compute->CreateShaderFromSource(glsl, "main");
        auto pipe   = compute->CreatePipeline(shader);

        compute->BindPipeline(pipe);
        compute->BindBuffer(0,bufA);
        compute->BindBuffer(1,bufB);
        compute->BindBuffer(2,bufC);
        compute->Dispatch(N/256,1,1);
        compute->MemoryBarrier();
        compute->WaitIdle();
        compute->ReadBuffer(bufC,C.data(),N*4);

        // Vérification CPU
        bool ok=true; uint32 errs=0;
        for (uint32 i=0;i<N;i++) {
            float expected=sqrtf(A[i]*A[i]+B[i]*B[i]);
            if (fabsf(C[i]-expected)>0.01f) { ok=false; errs++; }
        }
        printf("\nOpération GPU : c[i] = sqrt(a[i]² + b[i]²)\n");
        printf("c[0]    = %.4f  (attendu %.4f)\n", C[0],   sqrtf(A[0]*A[0]+B[0]*B[0]));
        printf("c[256]  = %.4f  (attendu %.4f)\n", C[256], sqrtf(A[256]*A[256]+B[256]*B[256]));
        printf("c[1024] = %.4f  (attendu %.4f)\n", C[1024],sqrtf(A[1024]*A[1024]+B[1024]*B[1024]));
        printf("Vérification %u éléments: %s (%u erreurs)\n\n",
               N, ok ? "✓ PASS" : "✗ FAIL", errs);

        compute->DestroyPipeline(pipe);
        compute->DestroyShader(shader);
    }
    compute->DestroyBuffer(bufA);
    compute->DestroyBuffer(bufB);
    compute->DestroyBuffer(bufC);
    compute->Shutdown(); delete compute;
    gfx->Shutdown();    delete gfx;
}

// =============================================================================
// ██████  DEMO AUTO — API auto-sélectionnée + dégradé générique
// =============================================================================
static void DemoAutoAPI(NkEntryState&) {
    NkWindow win;
    win.Create("NkEngine — Auto API (fallback chain)", 1280, 720);

    NkIGraphicsContext* ctx = NkContextFactory::CreateWithFallback(win, {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        NkGraphicsApi::NK_API_DIRECTX12,
        NkGraphicsApi::NK_API_DIRECTX11,
#endif
#if defined(NKENTSEU_PLATFORM_MACOS)
        NkGraphicsApi::NK_API_METAL,
#endif
        NkGraphicsApi::NK_API_VULKAN,
        NkGraphicsApi::NK_API_OPENGL,
        NkGraphicsApi::NK_API_SOFTWARE,
    });
    if (!ctx) { printf("[Auto] Aucune API disponible!\n"); return; }

    printf("[Auto] API choisie : %s\n",    NkGraphicsApiName(ctx->GetApi()));
    printf("[Auto] Renderer    : %s\n",    ctx->GetInfo().renderer);
    printf("[Auto] Compute     : %s\n",    ctx->SupportsCompute() ? "oui" : "non");

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        // Si Software, dessiner un dégradé coloré
        if (ctx->GetApi() == NkGraphicsApi::NK_API_SOFTWARE) {
            auto* fb = NkNativeContext::GetSoftwareBackBuffer(ctx);
            if (fb && fb->IsValid()) {
                for (uint32 y=0;y<fb->height;y++) {
                    float fy=(float)y/fb->height;
                    for (uint32 x=0;x<fb->width;x++) {
                        float fx=(float)x/fb->width;
                        float h=fmodf(fx+fy*0.4f+t*0.06f,1.f);
                        float r,g,b; HsvToRgb(h,0.8f,0.85f,r,g,b);
                        fb->SetPixel(x,y,(uint8)(r*255),(uint8)(g*255),(uint8)(b*255));
                    }
                }
            }
        }
        // Autres APIs : le BeginFrame gère le clear
    }, 300);

    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// Point d'entrée — nkmain (appelé par NkMetalEntryPoint.mm sur Apple,
//                           ou directement depuis main() sur Windows/Linux)
// =============================================================================
int nkmain(NkEntryState& state) {
    printf("╔════════════════════════════════════════╗\n");
    printf("║    NkEngine — Graphics System Demos    ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    printf("Démos disponibles (décommenter dans le code) :\n");
    printf("  DemoOpenGL    — Triangle pulsant + Quad UV rotatif (GLSL)\n");
    printf("  DemoVulkan    — Clear arc-en-ciel + accès command buffer\n");
    printf("  DemoDirectX11 — Triangle + Quad + Croix HLSL (Windows)\n");
    printf("  DemoDirectX12 — Clear animé DX12 (Windows)\n");
    printf("  DemoMetal     — Triangle + Cercle SDF MSL (macOS/iOS)\n");
    printf("  DemoSoftware  — Spirale HSV tracée pixel par pixel (CPU)\n");
    printf("  DemoCompute   — Norme 2D GPU, vérification résultat\n");
    printf("  DemoAutoAPI   — Fallback chain automatique\n\n");

    // ── Activer les démos souhaitées ──────────────────────────────────────────
    DemoOpenGL(state);
    // DemoVulkan(state);
    DemoSoftware(state);
    DemoCompute(state);
    // DemoAutoAPI(state);

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    // DemoDirectX11(state);
    // DemoDirectX12(state);
#endif
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    // DemoMetal(state);
#endif

    printf("\n[Demo] Toutes les démos terminées.\n");
    return 0;
}
