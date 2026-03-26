/**
 * @File    NkPAApp.cpp
 * @Brief   Implémentation de la boucle principale NKPA.
 */

#include "NkPAApp.h"

#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkGraphicsApi.h"

#include "NKLogger/NkLog.h"

#include <cstdlib>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::nkpa;

// ─────────────────────────────────────────────────────────────────────────────
//  Init
// ─────────────────────────────────────────────────────────────────────────────

bool NkPAApp::Init(int32 width, int32 height, NkGraphicsApi api) {
    mWidth  = width;
    mHeight = height;
    mApi    = api;

    // ── Fenêtre ───────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title    = "NKPA — Procedural Animation";
    winCfg.width    = mWidth;
    winCfg.height   = mHeight;
    winCfg.centered = true;
    winCfg.resizable= true;

    if (!mWindow.Create(winCfg)) {
        logger_src.Error("[NKPA] Echec création fenêtre");
        return false;
    }

    // ── Device RHI ────────────────────────────────────────────────────────────
    NkDeviceInitInfo init{};
    init.api     = mApi;
    init.surface = mWindow.GetSurfaceDesc();
    init.width   = (uint32)mWidth;
    init.height  = (uint32)mHeight;
    if (mApi == NkGraphicsApi::NK_API_SOFTWARE) {
        init.context.software.threading = true;
        init.context.software.useSSE    = true;
    }

    logger_src.Info("[NKPA] Backend: {0}", NkGraphicsApiName(mApi));
    mDevice = NkDeviceFactory::Create(init);
    if (!mDevice || !mDevice->IsValid()) {
        logger_src.Error("[NKPA] Echec création device {0}", NkGraphicsApiName(mApi));
        mWindow.Close();
        return false;
    }

    // ── Shader ────────────────────────────────────────────────────────────────
    NkShaderDesc shaderDesc;
    mShader = mDevice->CreateShader(shaderDesc);
    if (!mShader.IsValid()) {
        logger_src.Error("[NKPA] Echec création shader");
        Shutdown();
        return false;
    }

    // ── Pipeline (triangle list, couleur par sommet) ──────────────────────────
    NkVertexLayout layout;
    layout
        .AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT,  0,              "POSITION", 0)
        .AddAttribute(1, 0, NkGPUFormat::NK_RGBA32_FLOAT, 3*sizeof(float32), "COLOR", 0)
        .AddBinding(0, sizeof(PaVertex));

    NkGraphicsPipelineDesc pd{};
    pd.shader       = mShader;
    pd.vertexLayout = layout;
    pd.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
    pd.rasterizer   = NkRasterizerDesc::Default();
    pd.depthStencil = NkDepthStencilDesc::NoDepth();
    pd.blend        = NkBlendDesc::Alpha();
    pd.renderPass   = mDevice->GetSwapchainRenderPass();

    mPipeline = mDevice->CreateGraphicsPipeline(pd);
    if (!mPipeline.IsValid()) {
        logger_src.Error("[NKPA] Echec création pipeline");
        Shutdown();
        return false;
    }

    // ── Command buffer ────────────────────────────────────────────────────────
    mCmd = mDevice->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
    if (!mCmd || !mCmd->IsValid()) {
        logger_src.Error("[NKPA] Echec création command buffer");
        Shutdown();
        return false;
    }

    // ── Événements fenêtre ───────────────────────────────────────────────────
    NkEventSystem& ev = NkEvents();
    ev.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) {
        mRunning = false;
    });
    ev.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if (e->GetKey() == NkKey::NK_ESCAPE) mRunning = false;
        if (e->GetKey() == NkKey::NK_SPACE)  mUIState.paused = !mUIState.paused;
        if (e->GetKey() == NkKey::NK_F1)     mUIState.showUI = !mUIState.showUI;
    });
    ev.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        mWidth  = e->GetWidth();
        mHeight = e->GetHeight();
    });

    // ── NKUI ─────────────────────────────────────────────────────────────────
    mUI.Init(mWidth, mHeight);

    // Callbacks souris → alimentation NKUI (dans NkPAApp.cpp car inclut NKWindow)
    ev.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent* e) {
        mUI.SetMousePos((float32)e->GetX(), (float32)e->GetY());
    });
    ev.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent* e) {
        int32 btn = (e->GetButton() == NkMouseButton::NK_MB_LEFT)   ? 0
                  : (e->GetButton() == NkMouseButton::NK_MB_RIGHT)  ? 1
                  : (e->GetButton() == NkMouseButton::NK_MB_MIDDLE) ? 2 : -1;
        if (btn >= 0) mUI.SetMouseButton(btn, true);
    });
    ev.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent* e) {
        int32 btn = (e->GetButton() == NkMouseButton::NK_MB_LEFT)   ? 0
                  : (e->GetButton() == NkMouseButton::NK_MB_RIGHT)  ? 1
                  : (e->GetButton() == NkMouseButton::NK_MB_MIDDLE) ? 2 : -1;
        if (btn >= 0) mUI.SetMouseButton(btn, false);
    });
    ev.AddEventCallback<NkMouseWheelVerticalEvent>([&](NkMouseWheelVerticalEvent* e) {
        mUI.AddMouseWheel((float32)e->GetDeltaY());
    });

    // ── Environnement ─────────────────────────────────────────────────────────
    mEnv.Init((float32)mWidth, (float32)mHeight);
    float32 wY  = mEnv.WaterY();
    float32 wH  = (float32)mHeight;
    float32 wW  = (float32)mWidth;

    // ── Créatures ─────────────────────────────────────────────────────────────
    srand(12345u);

    // Marines — zone eau [wY+20 .. wH-20]
    auto RandW  = [&]() -> float32 { return 80.f + (float32)rand()/(float32)RAND_MAX * (wW - 160.f); };
    auto RandWY = [&]() -> float32 { return wY + 30.f + (float32)rand()/(float32)RAND_MAX * (wH - wY - 50.f); };

    for (int32 i = 0; i < NUM_FISH;      ++i) mFish[i].Init({RandW(), RandWY()}, mWidth, mHeight);
    for (int32 i = 0; i < NUM_SHARK;     ++i) mSharks[i].Init({RandW(), RandWY()}, mWidth, mHeight);
    for (int32 i = 0; i < NUM_EEL;       ++i) mEels[i].Init({RandW(), RandWY()}, mWidth, mHeight);
    for (int32 i = 0; i < NUM_JELLYFISH; ++i) mJellyfish[i].Init({RandW(), RandWY()}, mWidth, mHeight);

    // Terrestres — zone terre [wY+15 .. wY+80]
    auto RandLY = [&]() -> float32 { return wY + 20.f + (float32)rand()/(float32)RAND_MAX * 60.f; };

    for (int32 i = 0; i < NUM_SNAKE;       ++i) mSnakes[i].Init({RandW(), RandLY()}, mWidth, mHeight);
    for (int32 i = 0; i < NUM_CATERPILLAR; ++i) mCaterpillars[i].Init({RandW(), RandLY()}, mWidth, mHeight);
    for (int32 i = 0; i < NUM_CENTIPEDE;   ++i) mCentipedes[i].Init({RandW(), RandLY()}, mWidth, mHeight);
    for (int32 i = 0; i < NUM_WORM;        ++i) mWorms[i].Init({RandW(), RandLY()}, mWidth, mHeight);

    // Mixtes — toute la scène
    for (int32 i = 0; i < NUM_LIZARD; ++i) mLizards[i].Init({RandW(), RandLY()}, mWidth, mHeight);
    for (int32 i = 0; i < NUM_TURTLE; ++i) mTurtles[i].Init({RandW(), RandWY()}, mWidth, mHeight);
    for (int32 i = 0; i < NUM_CAT;    ++i) mCats[i].Init({RandW(), RandLY()}, mWidth, mHeight);
    for (int32 i = 0; i < NUM_BIRD;   ++i) {
        // Oiseaux dans le ciel
        float32 birdY = 40.f + (float32)rand()/(float32)RAND_MAX * (wY * 0.5f);
        mBirds[i].Init({RandW(), birdY}, mWidth, mHeight);
    }

    mRunning = true;
    mChrono  = NkChrono{};
    logger_src.Info("[NKPA] Démarrage — 12 espèces, 20 individus");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Run
// ─────────────────────────────────────────────────────────────────────────────

void NkPAApp::Run() {
    NkEventSystem& ev = NkEvents();

    while (mRunning) {
        mUI.BeginInput();   // reset transients avant les événements
        ev.PollEvents();
        if (!mRunning) break;

        uint32 sw = mDevice->GetSwapchainWidth();
        uint32 sh = mDevice->GetSwapchainHeight();

        if (sw == 0 || sh == 0) continue;
        if (sw != (uint32)mWidth || sh != (uint32)mHeight) {
            mWidth  = (int32)sw;
            mHeight = (int32)sh;
            mDevice->OnResize(sw, sh);
            mEnv.Init((float32)mWidth, (float32)mHeight);
        }

        NkElapsedTime elapsed = mChrono.Reset();
        float32 dt = (float32)elapsed.seconds;
        if (dt > 0.05f) dt = 0.05f;
        if (dt < 0.0001f) dt = 0.016f;

        // NKUI input + widgets
        mUI.BeginInput();
        mUI.BuildFrame(dt, mUIState);

        if (!mUIState.paused) Update(dt * mUIState.speedScale);

        Render();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update
// ─────────────────────────────────────────────────────────────────────────────

void NkPAApp::Update(float32 dt) {
    mEnv.Update(dt);

    for (int32 i = 0; i < NUM_FISH;        ++i) mFish[i].Update(dt);
    for (int32 i = 0; i < NUM_SHARK;       ++i) mSharks[i].Update(dt);
    for (int32 i = 0; i < NUM_EEL;         ++i) mEels[i].Update(dt);
    for (int32 i = 0; i < NUM_JELLYFISH;   ++i) mJellyfish[i].Update(dt);

    for (int32 i = 0; i < NUM_SNAKE;       ++i) mSnakes[i].Update(dt);
    for (int32 i = 0; i < NUM_CATERPILLAR; ++i) mCaterpillars[i].Update(dt);
    for (int32 i = 0; i < NUM_CENTIPEDE;   ++i) mCentipedes[i].Update(dt);
    for (int32 i = 0; i < NUM_WORM;        ++i) mWorms[i].Update(dt);

    for (int32 i = 0; i < NUM_LIZARD;      ++i) mLizards[i].Update(dt);
    for (int32 i = 0; i < NUM_TURTLE;      ++i) mTurtles[i].Update(dt);
    for (int32 i = 0; i < NUM_CAT;         ++i) mCats[i].Update(dt);
    for (int32 i = 0; i < NUM_BIRD;        ++i) mBirds[i].Update(dt);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render
// ─────────────────────────────────────────────────────────────────────────────

void NkPAApp::Render() {
    NkFrameContext frame{};
    if (!mDevice->BeginFrame(frame)) return;

    mMesh.Clear();

    // 1. Environnement (fond)
    mEnv.Draw(mMesh);

    // 2. Créatures marines (dans l'eau)
    for (int32 i = 0; i < NUM_JELLYFISH; ++i) mJellyfish[i].Draw(mMesh);
    for (int32 i = 0; i < NUM_EEL;       ++i) mEels[i].Draw(mMesh);
    for (int32 i = 0; i < NUM_FISH;      ++i) mFish[i].Draw(mMesh);
    for (int32 i = 0; i < NUM_SHARK;     ++i) mSharks[i].Draw(mMesh);
    for (int32 i = 0; i < NUM_TURTLE;    ++i) mTurtles[i].Draw(mMesh);

    // 3. Créatures terrestres
    for (int32 i = 0; i < NUM_WORM;        ++i) mWorms[i].Draw(mMesh);
    for (int32 i = 0; i < NUM_CATERPILLAR; ++i) mCaterpillars[i].Draw(mMesh);
    for (int32 i = 0; i < NUM_CENTIPEDE;   ++i) mCentipedes[i].Draw(mMesh);
    for (int32 i = 0; i < NUM_SNAKE;       ++i) mSnakes[i].Draw(mMesh);
    for (int32 i = 0; i < NUM_LIZARD;      ++i) mLizards[i].Draw(mMesh);
    for (int32 i = 0; i < NUM_CAT;         ++i) mCats[i].Draw(mMesh);

    // 4. Oiseaux (au-dessus de tout)
    for (int32 i = 0; i < NUM_BIRD;        ++i) mBirds[i].Draw(mMesh);

    // 5. UI overlay NKUI
    mUI.RenderToMesh(mMesh);

    if (mMesh.IsEmpty()) {
        mDevice->SubmitAndPresent(mCmd);
        mDevice->EndFrame(frame);
        return;
    }

    // Conversion pixels → NDC
    NkVector<PaVertex> ndc = mMesh.BuildNDC((float32)mWidth, (float32)mHeight);

    NkBufferHandle vbuf = mDevice->CreateBuffer(
        NkBufferDesc::Vertex((uint64)ndc.Size() * sizeof(PaVertex), ndc.Begin()));

    mCmd->Begin();
    NkRect2D area{ 0, 0, mWidth, mHeight };
    if (mCmd->BeginRenderPass(mDevice->GetSwapchainRenderPass(),
                              mDevice->GetSwapchainFramebuffer(), area)) {
        NkViewport vp;
        vp.x = 0.f; vp.y = 0.f;
        vp.width    = (float32)mWidth;
        vp.height   = (float32)mHeight;
        vp.minDepth = 0.f;
        vp.maxDepth = 1.f;
        mCmd->SetViewport(vp);
        mCmd->SetScissor(area);
        mCmd->BindGraphicsPipeline(mPipeline);
        mCmd->BindVertexBuffer(0, vbuf);
        mCmd->Draw((uint32)ndc.Size());
        mCmd->EndRenderPass();
    }
    mCmd->End();

    mDevice->SubmitAndPresent(mCmd);
    mDevice->EndFrame(frame);

    if (vbuf.IsValid()) mDevice->DestroyBuffer(vbuf);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Shutdown
// ─────────────────────────────────────────────────────────────────────────────

void NkPAApp::Shutdown() {
    if (mCmd)     { mDevice->DestroyCommandBuffer(mCmd); mCmd = nullptr; }
    if (mPipeline.IsValid()) { mDevice->DestroyPipeline(mPipeline); }
    if (mShader.IsValid())   { mDevice->DestroyShader(mShader);   }
    if (mDevice)  { NkDeviceFactory::Destroy(mDevice); mDevice = nullptr; }
    mWindow.Close();
}
