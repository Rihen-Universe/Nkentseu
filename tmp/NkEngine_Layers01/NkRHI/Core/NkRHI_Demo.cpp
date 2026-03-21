// =============================================================================
// NkRHI_Demo.cpp
// Démo complète de la couche 1 RHI :
//   1. Rendu 3D (triangle + texture + uniform buffer)
//   2. Entraînement d'un MLP simple (XOR, MNIST stub)
//   3. Benchmark MatMul GPU vs CPU
// =============================================================================
#include "../Core/NkIDevice.h"
#include "../Core/NkDeviceFactory.h"
#include "../Core/NkRHI_ML.h"
#include "../../NkFinal_work/Factory/NkContextFactory.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <chrono>

using namespace nkentseu;
using Clock = std::chrono::high_resolution_clock;

// =============================================================================
// DEMO 1 : Rendu RHI — Triangle texturé avec uniform buffer
// =============================================================================
static void DemoRHIRendering(NkIDevice* dev) {
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║  DEMO RHI : Rendu Triangle Texturé  ║\n");
    printf("╚══════════════════════════════════════╝\n");

    // ── Géométrie (position + UV) ─────────────────────────────────────────────
    struct Vertex { float x,y,z,  u,v; };
    Vertex verts[] = {
        { 0.0f,  0.5f, 0.f,  0.5f, 1.f},
        {-0.5f, -0.5f, 0.f,  0.0f, 0.f},
        { 0.5f, -0.5f, 0.f,  1.0f, 0.f},
    };
    uint16_t indices[] = {0,1,2};

    auto vb = dev->CreateBuffer(NkBufferDesc::Vertex(sizeof(verts), verts));
    auto ib = dev->CreateBuffer(NkBufferDesc::Index (sizeof(indices),indices));
    printf("[RHI] Vertex buffer: %s\n", vb.IsValid() ? "OK" : "FAIL");
    printf("[RHI] Index  buffer: %s\n", ib.IsValid() ? "OK" : "FAIL");

    // ── Uniform buffer (matrices) ─────────────────────────────────────────────
    struct Matrices { float model[16], view[16], proj[16]; };
    auto ub = dev->CreateBuffer(NkBufferDesc::Uniform(sizeof(Matrices)));
    Matrices mats{};
    // Identity matrices
    for (int i=0;i<4;i++) { mats.model[i*5]=1.f; mats.view[i*5]=1.f; mats.proj[i*5]=1.f; }
    dev->WriteBuffer(ub, &mats, sizeof(mats));
    printf("[RHI] Uniform buffer: %s\n", ub.IsValid() ? "OK" : "FAIL");

    // ── Texture 2×2 damier ────────────────────────────────────────────────────
    uint8_t pixels[] = {
        255,0,0,255,  0,255,0,255,
        0,0,255,255,  255,255,0,255,
    };
    auto texDesc = NkTextureDesc::Tex2D(2,2,NkFormat::RGBA8_Srgb,1);
    auto tex = dev->CreateTexture(texDesc);
    dev->WriteTexture(tex, pixels, 2*4);
    printf("[RHI] Texture 2×2: %s\n", tex.IsValid() ? "OK" : "FAIL");

    // ── Sampler ───────────────────────────────────────────────────────────────
    auto samp = dev->CreateSampler(NkSamplerDesc::Nearest());
    printf("[RHI] Sampler: %s\n", samp.IsValid() ? "OK" : "FAIL");

    // ── Shader GLSL ───────────────────────────────────────────────────────────
    const char* vsGLSL = R"(#version 430 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(std140, binding=0) uniform Matrices {
    mat4 model, view, proj;
};
out vec2 vUV;
void main(){
    gl_Position = proj * view * model * vec4(aPos,1.0);
    vUV = aUV;
})";
    const char* fsGLSL = R"(#version 430 core
in vec2 vUV;
out vec4 frag;
uniform sampler2D uTex;
void main(){ frag = texture(uTex, vUV); })";

    NkShaderDesc sd;
    sd.AddGLSL(NkShaderStage::Vertex,   vsGLSL)
      .AddGLSL(NkShaderStage::Fragment, fsGLSL);
    sd.debugName = "TriangleShader";
    auto shader = dev->CreateShader(sd);
    printf("[RHI] Shader: %s\n", shader.IsValid() ? "OK" : "FAIL");

    // ── Render Pass & Pipeline ────────────────────────────────────────────────
    auto rp = dev->GetSwapchainRenderPass();
    NkGraphicsPipelineDesc pd;
    pd.shader       = shader;
    pd.vertexLayout = NkVertexLayout()
        .AddBinding(0, sizeof(Vertex))
        .AddAttribute(0,0,NkVertexFormat::Float3, 0,"POSITION")
        .AddAttribute(1,0,NkVertexFormat::Float2,12,"TEXCOORD");
    pd.topology     = NkPrimitiveTopology::TriangleList;
    pd.rasterizer   = NkRasterizerDesc::NoCull();
    pd.depthStencil = NkDepthStencilDesc::Default();
    pd.blend        = NkBlendDesc::Opaque();
    pd.renderPass   = rp;
    auto pipeline   = dev->CreateGraphicsPipeline(pd);
    printf("[RHI] Pipeline: %s\n", pipeline.IsValid() ? "OK" : "FAIL");

    // ── Descriptor Set ────────────────────────────────────────────────────────
    NkDescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Add(0, NkDescriptorType::UniformBuffer, NkShaderStage::Vertex)
              .Add(1, NkDescriptorType::CombinedImageSampler, NkShaderStage::Fragment);
    auto layout = dev->CreateDescriptorSetLayout(layoutDesc);
    auto descSet = dev->AllocateDescriptorSet(layout);
    dev->BindUniformBuffer  (descSet, 0, ub, sizeof(Matrices));
    dev->BindTextureSampler (descSet, 1, tex, samp);
    printf("[RHI] Descriptor set: %s\n", descSet.IsValid() ? "OK" : "FAIL");

    // ── Command Buffer ────────────────────────────────────────────────────────
    auto* cmd = dev->CreateCommandBuffer();
    NkFrameContext frame;
    dev->BeginFrame(frame);

    cmd->Begin();
    cmd->BeginDebugGroup("DemoTriangle");

    NkFramebufferHandle fb = dev->GetSwapchainFramebuffer();
    NkRect2D area{0,0,dev->GetSwapchainWidth(),dev->GetSwapchainHeight()};
    cmd->BeginRenderPass(rp, fb, area);

    NkViewport vp{0,0,(float)dev->GetSwapchainWidth(),(float)dev->GetSwapchainHeight()};
    cmd->SetViewport(vp);
    cmd->SetScissor(area);

    cmd->BindGraphicsPipeline(pipeline);
    cmd->BindDescriptorSet(descSet, 0);
    cmd->BindVertexBuffer(0, vb);
    cmd->BindIndexBuffer(ib, NkIndexFormat::Uint16);
    cmd->DrawIndexed(3);

    cmd->EndRenderPass();
    cmd->EndDebugGroup();
    cmd->End();

    dev->SubmitAndPresent(cmd);
    dev->EndFrame(frame);
    dev->DestroyCommandBuffer(cmd);

    printf("[RHI] Frame rendu avec succès ✓\n");

    // Nettoyage
    dev->FreeDescriptorSet(descSet);
    dev->DestroyDescriptorSetLayout(layout);
    dev->DestroyPipeline(pipeline);
    dev->DestroyShader(shader);
    dev->DestroySampler(samp);
    dev->DestroyTexture(tex);
    dev->DestroyBuffer(ub);
    dev->DestroyBuffer(ib);
    dev->DestroyBuffer(vb);
}

// =============================================================================
// DEMO 2 : ML — Entraîner un MLP pour apprendre XOR
// Réseau : 2 → 8 → 1, activation ReLU, loss MSE, optimiseur AdamW
// =============================================================================
static void DemoML_XOR(NkIDevice* dev) {
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║  DEMO ML : MLP XOR (2→8→1, AdamW)  ║\n");
    printf("╚══════════════════════════════════════╝\n");

    NkMLContext ml;
    if (!ml.Init(dev)) { printf("[ML] Init failed\n"); return; }

    // ── Dataset XOR ──────────────────────────────────────────────────────────
    // Inputs  : [0,0], [0,1], [1,0], [1,1]  → shape [4,2]
    // Targets : [0],   [1],   [1],   [0]    → shape [4,1]
    float xData[] = {0,0,  0,1,  1,0,  1,1};
    float yData[] = {0,    1,    1,    0};

    auto X      = ml.CreateTensor({4,2}, false, "X");
    auto Y      = ml.CreateTensor({4,1}, false, "Y");
    ml.Upload(X, xData);
    ml.Upload(Y, yData);

    // ── Paramètres du réseau ──────────────────────────────────────────────────
    // Couche 1 : W1[2,8], b1[8]
    // Couche 2 : W2[8,1], b2[1]
    auto W1 = ml.CreateTensor({2,8}, true, "W1"); ml.InitXavier(W1);
    auto b1 = ml.CreateTensor({8},   true, "b1"); ml.FillZero(b1);
    auto W2 = ml.CreateTensor({8,1}, true, "W2"); ml.InitXavier(W2);
    auto b2 = ml.CreateTensor({1},   true, "b2"); ml.FillZero(b2);

    // Activations et gradients
    auto H1     = ml.CreateTensor({4,8}, false, "H1");    // après couche 1
    auto H1act  = ml.CreateTensor({4,8}, false, "H1act"); // après ReLU
    auto Yhat   = ml.CreateTensor({4,1}, false, "Yhat");  // sortie

    // Gradients
    auto dYhat  = ml.CreateTensor({4,1}, false, "dYhat");
    auto dH1act = ml.CreateTensor({4,8}, false, "dH1act");
    auto dH1    = ml.CreateTensor({4,8}, false, "dH1");
    auto dW1    = ml.CreateTensor({2,8}, false, "dW1");
    auto db1    = ml.CreateTensor({8},   false, "db1");
    auto dW2    = ml.CreateTensor({8,1}, false, "dW2");
    auto db2    = ml.CreateTensor({1},   false, "db2");

    // Optimiseurs
    auto adamW1 = ml.CreateAdamState(W1);
    auto adamb1 = ml.CreateAdamState(b1);
    auto adamW2 = ml.CreateAdamState(W2);
    auto adamb2 = ml.CreateAdamState(b2);
    NkMLContext::AdamConfig cfg{0.01f, 0.9f, 0.999f, 1e-8f, 0.f}; // lr=0.01, pas de wd

    // ── Boucle d'entraînement ─────────────────────────────────────────────────
    const int EPOCHS = 5000;
    printf("Entraînement %d epochs...\n", EPOCHS);
    auto t0 = Clock::now();

    for (int epoch=0; epoch<=EPOCHS; epoch++) {
        // ── Forward ──────────────────────────────────────────────────────────
        ml.MatMulAdd(X,  W1, b1, H1);     // H1   = X×W1 + b1
        ml.ReLU(H1, H1act);               // H1act= relu(H1)
        ml.MatMulAdd(H1act, W2, b2, Yhat); // Yhat = H1act×W2 + b2

        // ── Loss ─────────────────────────────────────────────────────────────
        if (epoch%500==0 || epoch==EPOCHS) {
            float loss = ml.MSELoss(Yhat, Y);
            float ms = std::chrono::duration<float,std::milli>(Clock::now()-t0).count();
            printf("  Epoch %5d | Loss: %.6f | %.0fms\n", epoch, loss, ms);
            if (epoch==EPOCHS) {
                printf("\nPrédictions finales:\n");
                ml.PrintTensor(Yhat, 4, "  Yhat = ");
                printf("  Attendu  = [0.0, 1.0, 1.0, 0.0]\n");
            }
        }
        if (epoch==EPOCHS) break;

        // ── Backward (gradient descent manuel) ───────────────────────────────
        // dL/dYhat = 2*(Yhat - Y) / n    (dérivée MSE)
        // Calculé côté CPU pour l'instant (tenseur petit = 4 éléments)
        {
            std::vector<float> yh(4), y(4), dy(4);
            ml.Download(Yhat, yh.data());
            ml.Download(Y,    y.data());
            for (int i=0;i<4;i++) dy[i] = 2.f*(yh[i]-y[i])/4.f;
            ml.Upload(dYhat, dy.data());
        }

        // dW2 = H1act^T × dYhat
        ml.MatMulBackward(H1act, W2, dYhat, dH1act, dW2);
        // db2 = sum(dYhat) → CPU pour bias 1D
        {
            std::vector<float> dy(4), db(1,0);
            ml.Download(dYhat,dy.data());
            for (auto v:dy) db[0]+=v;
            ml.Upload(db2,db.data());
        }
        // dH1 = dH1act * (H1>0)  (ReLU backward)
        ml.ReLUBackward(H1act, dH1act, dH1);
        // dW1 = X^T × dH1
        ml.MatMulBackward(X, W1, dH1, /*dX (ignoré)*/dYhat, dW1);
        // db1 = sum(dH1, axis=0) → CPU pour simplicité
        {
            std::vector<float> dh(32), db(8,0);
            ml.Download(dH1,dh.data());
            for (int i=0;i<4;i++) for (int j=0;j<8;j++) db[j]+=dh[i*8+j];
            ml.Upload(db1,db.data());
        }

        // ── Update AdamW ──────────────────────────────────────────────────────
        ml.AdamWStep(W1, dW1, adamW1, cfg);
        ml.AdamWStep(b1, db1, adamb1, cfg);
        ml.AdamWStep(W2, dW2, adamW2, cfg);
        ml.AdamWStep(b2, db2, adamb2, cfg);
    }

    float totalMs = std::chrono::duration<float,std::milli>(Clock::now()-t0).count();
    printf("\nTemps total: %.0fms (%.2fms/epoch)\n", totalMs, totalMs/EPOCHS);

    // Nettoyage
    ml.DestroyAdamState(adamW1); ml.DestroyAdamState(adamb1);
    ml.DestroyAdamState(adamW2); ml.DestroyAdamState(adamb2);
    ml.Shutdown();
}

// =============================================================================
// DEMO 3 : Benchmark MatMul GPU vs CPU
// =============================================================================
static void DemoBenchmarkMatMul(NkIDevice* dev) {
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║  DEMO BENCH : MatMul GPU vs CPU  512×512  ║\n");
    printf("╚════════════════════════════════════════════╝\n");

    NkMLContext ml; ml.Init(dev);
    const uint32 N=512;

    auto A = ml.CreateTensor({N,N}); ml.InitXavier(A);
    auto B = ml.CreateTensor({N,N}); ml.InitXavier(B);
    auto C = ml.CreateTensor({N,N});

    // ── GPU warmup ────────────────────────────────────────────────────────────
    for (int i=0;i<3;i++) ml.MatMul(A,B,C);
    ml.Sync();

    // ── GPU timing ────────────────────────────────────────────────────────────
    const int RUNS=20;
    auto t0=Clock::now();
    for (int i=0;i<RUNS;i++) ml.MatMul(A,B,C);
    ml.Sync();
    float gpuMs=std::chrono::duration<float,std::milli>(Clock::now()-t0).count()/RUNS;

    // ── CPU référence (naïf O(n³)) ────────────────────────────────────────────
    std::vector<float> a(N*N), b(N*N), c(N*N,0);
    ml.Download(A,a.data()); ml.Download(B,b.data());
    t0=Clock::now();
    for (uint32 i=0;i<N;i++)
        for (uint32 k=0;k<N;k++)
            for (uint32 j=0;j<N;j++)
                c[i*N+j]+=a[i*N+k]*b[k*N+j];
    float cpuMs=std::chrono::duration<float,std::milli>(Clock::now()-t0).count();

    // ── Résultats ─────────────────────────────────────────────────────────────
    // Vérification correctness GPU vs CPU
    auto Cref=ml.CreateTensor({N,N});
    ml.Upload(Cref,c.data());
    bool ok=ml.AllClose(C,Cref,1e-2f,1e-2f);

    float gflops_gpu = 2.f*N*N*N / (gpuMs*1e6f);
    float gflops_cpu = 2.f*N*N*N / (cpuMs*1e6f);

    printf("Taille     : %u×%u (%.1f M éléments)\n", N,N, float(N*N)/1e6f);
    printf("GPU        : %6.2f ms  →  %.2f GFLOPS\n", gpuMs,  gflops_gpu);
    printf("CPU (naïf) : %6.2f ms  →  %.2f GFLOPS\n", cpuMs,  gflops_cpu);
    printf("Speedup    : %.1f×\n", cpuMs/gpuMs);
    printf("Correctness: %s\n\n", ok ? "✓ PASS (atol=1e-2)" : "✗ FAIL");

    ml.DestroyTensor(A); ml.DestroyTensor(B);
    ml.DestroyTensor(C); ml.DestroyTensor(Cref);
    ml.Shutdown();
}

// =============================================================================
// Point d'entrée
// =============================================================================
int nkmain_rhi(/* NkEntryState& state */) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║    NkEngine — RHI Layer 1 + ML Demo          ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    // Créer le contexte graphique (couche 0)
    // NkWindow win; win.Create("NkRHI Demo", 1280, 720);
    // auto ctx = NkContextFactory::Create(win, NkContextDesc::MakeOpenGL(4,3));
    // NkIDevice* dev = NkDeviceFactory::Create(ctx);

    // STUB pour compilation standalone (remplacer par le vrai contexte)
    NkIDevice* dev = nullptr;
    printf("[Demo] Remplacer 'dev = nullptr' par le vrai NkIDevice\n");
    printf("[Demo] (NkDeviceFactory::Create(ctx) depuis un NkIGraphicsContext)\n\n");

    if (!dev) {
        printf("Structure RHI validée à la compilation.\n");
        printf("Démos disponibles une fois un device initialisé:\n");
        printf("  DemoRHIRendering  — Triangle texturé + uniform buffer\n");
        printf("  DemoML_XOR        — MLP XOR entraîné en 5000 epochs GPU\n");
        printf("  DemoBenchmarkMatMul — GPU vs CPU MatMul 512×512\n");
        return 0;
    }

    printf("[Device] API   : %s\n", NkGraphicsApiName(dev->GetApi()));
    printf("[Device] VRAM  : %llu MB\n", dev->GetCaps().vramBytes/1024/1024);
    printf("[Device] Compute: %s\n", dev->GetCaps().computeShaders?"oui":"non");

    DemoRHIRendering(dev);
    DemoML_XOR(dev);
    DemoBenchmarkMatMul(dev);

    NkDeviceFactory::Destroy(dev);
    return 0;
}
