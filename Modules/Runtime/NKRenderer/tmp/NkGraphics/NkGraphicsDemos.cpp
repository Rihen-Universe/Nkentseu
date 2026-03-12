// =============================================================================
// NkGraphicsDemos.cpp
// Démos standalone pour chaque API graphique + compute.
// Pattern : nkmain + NkEntryState + NkContextFactory
// =============================================================================
#include "../Factory/NkContextFactory.h"
#include "../Core/NkNativeContextAccess.h"
#include "../Compute/NkIComputeContext.h"
#include "NkWindow.h"    // votre include NkWindow
#include "NkEntryState.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers partagés
// ─────────────────────────────────────────────────────────────────────────────
namespace {

struct LoopState {
    bool  running   = true;
    float dt        = 0.0f;
    float totalTime = 0.0f;
};

// Boucle principale simple — gestion resize + events + 60fps cap
void RunLoop(NkWindow& win, NkIGraphicsContext* ctx, LoopState& state,
             std::function<void(float dt, float time)> onFrame)
{
    using Clock = std::chrono::high_resolution_clock;
    auto prev = Clock::now();

    while (state.running && win.IsOpen()) {
        // Events
        NkEvent ev;
        while (win.PollEvent(ev)) {
            if (ev.type == NkEventType::Close) state.running = false;
            if (ev.type == NkEventType::Resize)
                ctx->OnResize(ev.resize.width, ev.resize.height);
        }

        auto now = Clock::now();
        state.dt = std::chrono::duration<float>(now - prev).count();
        prev = now;
        state.totalTime += state.dt;

        if (!ctx->BeginFrame()) continue;
        onFrame(state.dt, state.totalTime);
        ctx->EndFrame();
        ctx->Present();

        // 60fps cap
        float elapsed = std::chrono::duration<float>(Clock::now() - now).count();
        if (elapsed < 1.0f/60.0f)
            std::this_thread::sleep_for(
                std::chrono::microseconds((int)((1.0f/60.0f - elapsed)*1e6f)));
    }
}

} // anonymous namespace

// =============================================================================
// DEMO OpenGL — Triangle RGB + Compute Shader (GL 4.3+)
// =============================================================================
static void DemoOpenGL(NkEntryState& state) {
    NkWindow win;
    win.Create({"NkEngine — OpenGL Demo", 1280, 720});

    auto desc = NkContextDesc::MakeOpenGL(4, 6, true);
    desc.opengl.msaaSamples = 4;
    desc.opengl.swapInterval = NkGLSwapInterval::AdaptiveVSync;

    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { printf("OpenGL context failed\n"); return; }

    // Compute depuis le même contexte GL
    NkIComputeContext* compute = NkContextFactory::ComputeFromGraphics(ctx);
    printf("OpenGL: %s\n", ctx->GetInfo().renderer);
    printf("Compute supported: %s\n", ctx->SupportsCompute() ? "yes" : "no");

    // Triangle simple (GLAD2 nécessaire pour les fonctions GL 3.3+)
    const char* vsrc = R"(
        #version 330 core
        layout(location=0) in vec2 pos;
        layout(location=1) in vec3 col;
        out vec3 vCol;
        void main(){ gl_Position=vec4(pos,0,1); vCol=col; }
    )";
    const char* fsrc = R"(
        #version 330 core
        in vec3 vCol; out vec4 frag;
        void main(){ frag=vec4(vCol,1); }
    )";

    float verts[] = {
        -0.5f,-0.5f, 1,0,0,
         0.5f,-0.5f, 0,1,0,
         0.0f, 0.5f, 0,0,1,
    };

    GLuint vao,vbo;
    glGenVertexArrays(1,&vao);
    glGenBuffers(1,&vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,20,(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,20,(void*)8);

    // Compiler shaders
    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s,1,&src,nullptr);
        glCompileShader(s);
        return s;
    };
    GLuint vs = compile(GL_VERTEX_SHADER, vsrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog,vs); glAttachShader(prog,fs);
    glLinkProgram(prog);

    LoopState ls;
    RunLoop(win, ctx, ls, [&](float dt, float time) {
        (void)dt;
        glClearColor(0.1f,0.1f,0.1f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        auto info = ctx->GetInfo();
        glViewport(0,0,win.GetWidth(),win.GetHeight());
        glUseProgram(prog);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES,0,3);
    });

    glDeleteProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    glDeleteBuffers(1,&vbo);
    glDeleteVertexArrays(1,&vao);

    if (compute) { compute->Shutdown(); delete compute; }
    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// DEMO Vulkan — Clear animé + accès compute queue
// =============================================================================
static void DemoVulkan(NkEntryState& state) {
    NkWindow win;
    win.Create({"NkEngine — Vulkan Demo", 1280, 720});

    auto desc = NkContextDesc::MakeVulkan(false);
    desc.vulkan.vsync = true;
    desc.vulkan.enableComputeQueue = true;

    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { printf("Vulkan context failed\n"); return; }

    NkIComputeContext* compute = NkContextFactory::ComputeFromGraphics(ctx);
    printf("Vulkan: %s | VRAM %u MB\n", ctx->GetInfo().renderer, ctx->GetInfo().vramMB);
    printf("Compute queue: %s\n", ctx->SupportsCompute() ? "dedicated" : "fallback graphics");

    LoopState ls;
    RunLoop(win, ctx, ls, [&](float dt, float time) {
        (void)dt;
        // Modifier la couleur de clear dans le BeginFrame
        // (démo simple — les vraies commandes Vulkan se font via GetVkCurrentCommandBuffer)
        VkCommandBuffer cmd = NkNativeContext::GetVkCurrentCommandBuffer(ctx);
        // cmd est déjà entre BeginCommandBuffer et BeginRenderPass
        // → Ajouter des vkCmdDraw* ici
        (void)cmd;
    });

    if (compute) { compute->Shutdown(); delete compute; }
    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// DEMO DirectX 11 — Triangle HLSL + Compute Shader
// =============================================================================
#if defined(NKENTSEU_PLATFORM_WINDOWS)
static void DemoDirectX11(NkEntryState& state) {
    NkWindow win;
    win.Create({"NkEngine — DirectX 11 Demo", 1280, 720});

    auto desc = NkContextDesc::MakeDirectX11(false);
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { printf("DX11 context failed\n"); return; }

    NkIComputeContext* compute = NkContextFactory::ComputeFromGraphics(ctx);
    printf("DX11: %s | VRAM %u MB\n", ctx->GetInfo().renderer, ctx->GetInfo().vramMB);

    ID3D11Device1*        dev  = NkNativeContext::GetDX11Device(ctx);
    ID3D11DeviceContext1* dctx = NkNativeContext::GetDX11Context(ctx);

    // Shaders HLSL simple
    const char* vsHLSL = R"(
        struct VS_IN { float2 pos:POSITION; float3 col:COLOR; };
        struct VS_OUT{ float4 pos:SV_POSITION; float3 col:COLOR; };
        VS_OUT main(VS_IN v){ VS_OUT o; o.pos=float4(v.pos,0,1); o.col=v.col; return o; }
    )";
    const char* psHLSL = R"(
        float4 main(float4 p:SV_POSITION, float3 c:COLOR):SV_TARGET{ return float4(c,1); }
    )";

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, err;
    D3DCompile(vsHLSL, strlen(vsHLSL), nullptr, nullptr, nullptr,
               "main", "vs_5_0", 0, 0, &vsBlob, &err);
    D3DCompile(psHLSL, strlen(psHLSL), nullptr, nullptr, nullptr,
               "main", "ps_5_0", 0, 0, &psBlob, &err);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  ps;
    dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs);
    dev->CreatePixelShader (psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT,  0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    Microsoft::WRL::ComPtr<ID3D11InputLayout> il;
    dev->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &il);

    float verts[] = {
        0.0f, 0.5f, 1,0,0,
       -0.5f,-0.5f, 0,1,0,
        0.5f,-0.5f, 0,0,1,
    };
    D3D11_BUFFER_DESC bd{sizeof(verts), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0};
    D3D11_SUBRESOURCE_DATA initData{verts, 0, 0};
    Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
    dev->CreateBuffer(&bd, &initData, &vb);

    LoopState ls;
    RunLoop(win, ctx, ls, [&](float dt, float time) {
        (void)dt; (void)time;
        dctx->IASetInputLayout(il.Get());
        UINT stride=20, offset=0;
        ID3D11Buffer* vbs[]={vb.Get()};
        dctx->IASetVertexBuffers(0,1,vbs,&stride,&offset);
        dctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        dctx->VSSetShader(vs.Get(),nullptr,0);
        dctx->PSSetShader(ps.Get(),nullptr,0);
        dctx->Draw(3,0);
    });

    // Demo compute — additionner deux tableaux GPU
    if (compute) {
        const char* csHLSL = R"(
            RWStructuredBuffer<float> bufA : register(u0);
            RWStructuredBuffer<float> bufB : register(u1);
            RWStructuredBuffer<float> bufC : register(u2);
            [numthreads(256, 1, 1)]
            void CSMain(uint3 id : SV_DispatchThreadID) {
                bufC[id.x] = bufA[id.x] + bufB[id.x];
            }
        )";
        uint32 N = 1024;
        std::vector<float> A(N,1.0f), B(N,2.0f), C(N,0.0f);

        NkComputeBufferDesc bd2{};
        bd2.sizeBytes = N*4; bd2.cpuWritable=true; bd2.cpuReadable=false; bd2.initialData=A.data();
        auto bufA = compute->CreateBuffer(bd2);
        bd2.initialData = B.data();
        auto bufB = compute->CreateBuffer(bd2);
        bd2.initialData = nullptr; bd2.cpuReadable = true;
        auto bufC = compute->CreateBuffer(bd2);

        auto shader = compute->CreateShaderFromSource(csHLSL, "CSMain");
        auto pipe   = compute->CreatePipeline(shader);

        compute->BindPipeline(pipe);
        compute->BindBuffer(0, bufA);
        compute->BindBuffer(1, bufB);
        compute->BindBuffer(2, bufC);
        compute->Dispatch(N/256, 1, 1);
        compute->WaitIdle();
        compute->ReadBuffer(bufC, C.data(), N*4);

        printf("DX11 Compute: A[0]+B[0] = %.1f (expected 3.0)\n", C[0]);

        compute->DestroyBuffer(bufA);
        compute->DestroyBuffer(bufB);
        compute->DestroyBuffer(bufC);
        compute->DestroyPipeline(pipe);
        compute->DestroyShader(shader);
        compute->Shutdown(); delete compute;
    }

    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// DEMO DirectX 12
// =============================================================================
static void DemoDirectX12(NkEntryState& state) {
    NkWindow win;
    win.Create({"NkEngine — DirectX 12 Demo", 1280, 720});

    auto desc = NkContextDesc::MakeDirectX12(false);
    desc.dx12.enableComputeQueue = true;

    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { printf("DX12 context failed\n"); return; }

    NkIComputeContext* compute = NkContextFactory::ComputeFromGraphics(ctx);
    printf("DX12: %s | VRAM %u MB\n", ctx->GetInfo().renderer, ctx->GetInfo().vramMB);

    LoopState ls;
    RunLoop(win, ctx, ls, [&](float dt, float time) {
        (void)dt; (void)time;
        // Commandes DX12 via GetDX12CommandList
        ID3D12GraphicsCommandList4* cmd = NkNativeContext::GetDX12CommandList(ctx);
        (void)cmd;
        // → Ajouter SetPipelineState, DrawInstanced, etc.
    });

    if (compute) { compute->Shutdown(); delete compute; }
    // FlushGPU automatique dans ~NkDX12Context
    ctx->Shutdown(); delete ctx;
}
#endif // WINDOWS

// =============================================================================
// DEMO Metal (macOS/iOS)
// =============================================================================
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
static void DemoMetal(NkEntryState& state) {
    NkWindow win;
    win.Create({"NkEngine — Metal Demo", 1280, 720});

    auto desc = NkContextDesc::MakeMetal();
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { printf("Metal context failed\n"); return; }

    NkIComputeContext* compute = NkContextFactory::ComputeFromGraphics(ctx);
    printf("Metal: %s | VRAM %u MB\n", ctx->GetInfo().renderer, ctx->GetInfo().vramMB);

    LoopState ls;
    RunLoop(win, ctx, ls, [&](float dt, float time) {
        (void)dt; (void)time;
        // id<MTLRenderCommandEncoder> enc via NkNativeContext::GetMetalCommandEncoder(ctx)
        // Caster : (__bridge id<MTLRenderCommandEncoder>)enc
    });

    if (compute) { compute->Shutdown(); delete compute; }
    ctx->Shutdown(); delete ctx;
}
#endif // METAL

// =============================================================================
// DEMO Software — gradient animé
// =============================================================================
static void DemoSoftware(NkEntryState& state) {
    NkWindow win;
    win.Create({"NkEngine — Software Demo", 800, 600});

    auto desc = NkContextDesc::MakeSoftware(true);
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { printf("Software context failed\n"); return; }
    printf("Software renderer ready\n");

    LoopState ls;
    RunLoop(win, ctx, ls, [&](float dt, float time) {
        (void)dt;
        auto* fb = NkNativeContext::GetSoftwareBackBuffer(ctx);
        if (!fb || !fb->IsValid()) return;

        for (uint32 y = 0; y < fb->height; ++y) {
            float fy = (float)y / fb->height;
            for (uint32 x = 0; x < fb->width; ++x) {
                float fx = (float)x / fb->width;
                float wave = 0.5f + 0.5f * std::sin(fx*10.0f + time*2.0f + fy*5.0f);
                fb->SetPixel(x, y,
                    (uint8)(fx  * 255),
                    (uint8)(wave * 255),
                    (uint8)((fy + std::sin(time)) * 0.5f * 255));
            }
        }
    });

    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// DEMO Multi-contexte — plusieurs contextes par application
// =============================================================================
static void DemoMultiContext(NkEntryState& state) {
    // Contexte 1 : rendu principal
    NkWindow win1; win1.Create({"NkEngine — Window 1 (rendu)", 800, 600});
    // Contexte 2 : compute (sans surface)
    NkWindow win2; win2.Create({"NkEngine — Window 2 (compute preview)", 400, 300});

    auto desc1 = NkContextDesc::MakeOpenGL(4,6,false);
    auto desc2 = NkContextDesc::MakeOpenGL(4,6,false);

    NkIGraphicsContext* ctx1 = NkContextFactory::Create(win1, desc1);
    NkIGraphicsContext* ctx2 = NkContextFactory::Create(win2, desc2);

    NkIComputeContext* compute1 = NkContextFactory::ComputeFromGraphics(ctx1);

    printf("Multi-context: ctx1=%p ctx2=%p compute=%p\n",
           (void*)ctx1, (void*)ctx2, (void*)compute1);
    printf("Deux contextes OpenGL indépendants créés — chacun a son propre VAO/state\n");
    printf("Le compute partage le device GPU de ctx1\n");

    // Boucle simplifiée pour la démo
    bool running = true;
    int  frames  = 120; // 2 secondes à 60fps
    while (running && frames-- > 0) {
        // Rendu sur fenêtre 1
        if (ctx1->BeginFrame()) {
            // ctx1 est le contexte courant ici
            ctx1->EndFrame();
            ctx1->Present();
        }
        // Rendu sur fenêtre 2 (contexte différent)
        if (ctx2->BeginFrame()) {
            ctx2->EndFrame();
            ctx2->Present();
        }
    }

    if (compute1) { compute1->Shutdown(); delete compute1; }
    if (ctx1) { ctx1->Shutdown(); delete ctx1; }
    if (ctx2) { ctx2->Shutdown(); delete ctx2; }
}

// =============================================================================
// DEMO Auto — chaîne de fallback
// =============================================================================
static void DemoAutoAPI(NkEntryState& state) {
    NkWindow win;
    win.Create({"NkEngine — Auto API", 1280, 720});

    // Préférence dans l'ordre — première API disponible est utilisée
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

    if (!ctx) { printf("No API available!\n"); return; }
    printf("Auto selected: %s\n", NkGraphicsApiName(ctx->GetApi()));
    printf("Renderer: %s\n", ctx->GetInfo().renderer);
    printf("Compute: %s\n", ctx->SupportsCompute() ? "yes" : "no");

    LoopState ls;
    RunLoop(win, ctx, ls, [&](float dt, float time) {
        (void)dt; (void)time;
        // Rendu générique — independant de l'API
    });

    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// Point d'entrée NkEngine
// =============================================================================
int nkmain(NkEntryState& state) {
    printf("=== NkGraphics System Demo ===\n");
    printf("Choisir une démo dans le code source.\n");

    // Décommenter la démo souhaitée :
    DemoOpenGL(state);
    // DemoVulkan(state);
    // DemoSoftware(state);
    // DemoMultiContext(state);
    // DemoAutoAPI(state);
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    // DemoDirectX11(state);
    // DemoDirectX12(state);
#endif
#if defined(NKENTSEU_PLATFORM_MACOS)
    // DemoMetal(state);
#endif
    return 0;
}
