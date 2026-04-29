// =============================================================================
// Demo06–10  — NKRenderer v4.0
// VFX, Offscreen RTT, PV3DE, Multi-Viewport Éditeur, Custom NkSL
// =============================================================================
#include "Demo.h"
#include <cmath>

namespace nkentseu {
namespace renderer {
namespace demo {

// =============================================================================
// Demo06_VFX — Particules, Trails, Decals
// =============================================================================
    bool Demo06_VFX::Init(NkIDevice* device, const NkSurfaceDesc& surface,
                            uint32 w, uint32 h) {
        mWidth=w; mHeight=h;
        auto cfg = NkRendererConfig::ForGame(NkGraphicsApi::NK_OPENGL, w, h);
        cfg.width=w; cfg.height=h;
        mRenderer = NkRenderer::Create(device, surface, cfg);
        if (!mRenderer) return false;

        auto* vfx    = mRenderer->GetVFX();
        auto* texLib = mRenderer->GetTextures();

        mFireTex  = texLib->Load("assets/vfx/fire_particle.png");
        mSmokeTex = texLib->Load("assets/vfx/smoke_particle.png");
        mSparkTex = texLib->Load("assets/vfx/spark.png");

        // Émetteur feu
        NkEmitterDesc fireDesc;
        fireDesc.shape      = NkEmitterShape::CONE;
        fireDesc.position   = {0, 0, 0};
        fireDesc.coneAngle  = 15.f;
        fireDesc.ratePerSec = 60.f;
        fireDesc.lifeMin    = 0.5f; fireDesc.lifeMax  = 1.2f;
        fireDesc.speedMin   = 1.5f; fireDesc.speedMax = 3.f;
        fireDesc.sizeStart  = 0.3f; fireDesc.sizeEnd  = 0.f;
        fireDesc.colorStart = {1.f,0.6f,0.1f,1.f};
        fireDesc.colorEnd   = {0.5f,0.1f,0.f,0.f};
        fireDesc.gravity    = {0.f, 0.5f, 0.f};   // monte
        fireDesc.velocityDir= {0, 1, 0};
        fireDesc.velocityRand=0.4f;
        fireDesc.texture    = mFireTex;
        fireDesc.blend      = NkBlendMode::NK_ADDITIVE;
        fireDesc.maxParticles=500;
        mFireEmitter = vfx->CreateEmitter(fireDesc);

        // Émetteur fumée
        NkEmitterDesc smokeDesc;
        smokeDesc.shape     = NkEmitterShape::SPHERE;
        smokeDesc.position  = {0, 0.8f, 0};
        smokeDesc.radius    = 0.1f;
        smokeDesc.ratePerSec= 15.f;
        smokeDesc.lifeMin   = 2.f; smokeDesc.lifeMax  = 4.f;
        smokeDesc.speedMin  = 0.3f; smokeDesc.speedMax= 0.8f;
        smokeDesc.sizeStart = 0.4f; smokeDesc.sizeEnd  = 1.5f;
        smokeDesc.colorStart= {0.3f,0.3f,0.3f,0.4f};
        smokeDesc.colorEnd  = {0.5f,0.5f,0.5f,0.f};
        smokeDesc.gravity   = {0.1f,0.8f,0.f};
        smokeDesc.texture   = mSmokeTex;
        smokeDesc.blend     = NkBlendMode::NK_ALPHA;
        smokeDesc.maxParticles=200;
        mSmokeEmitter = vfx->CreateEmitter(smokeDesc);

        // Émetteur étincelles
        NkEmitterDesc sparkDesc;
        sparkDesc.shape     = NkEmitterShape::POINT;
        sparkDesc.position  = {0, 0.2f, 0};
        sparkDesc.ratePerSec= 40.f;
        sparkDesc.lifeMin   = 0.3f; sparkDesc.lifeMax  = 0.8f;
        sparkDesc.speedMin  = 2.f;  sparkDesc.speedMax = 5.f;
        sparkDesc.sizeStart = 0.08f; sparkDesc.sizeEnd = 0.f;
        sparkDesc.colorStart= {1.f,0.9f,0.5f,1.f};
        sparkDesc.colorEnd  = {1.f,0.2f,0.f,0.f};
        sparkDesc.gravity   = {0,-5.f,0};
        sparkDesc.velocityRand=1.f;
        sparkDesc.texture   = mSparkTex;
        sparkDesc.blend     = NkBlendMode::NK_ADDITIVE;
        sparkDesc.maxParticles=300;
        mSparkEmitter = vfx->CreateEmitter(sparkDesc);

        // Trail
        NkTrailDesc trailDesc;
        trailDesc.width     = 0.08f;
        trailDesc.lifetime  = 0.8f;
        trailDesc.maxPoints = 128;
        trailDesc.colorStart= {0.f,0.8f,1.f,1.f};
        trailDesc.colorEnd  = {0.f,0.3f,0.5f,0.f};
        trailDesc.blend     = NkBlendMode::NK_ADDITIVE;
        mTrail = vfx->CreateTrail(trailDesc);

        // Decal brûlure
        NkDecalDesc decal;
        decal.transform = NkMat4f::Translate({0,0.01f,0}) * NkMat4f::Scale({1.5f,1.f,1.5f});
        decal.albedo    = texLib->Load("assets/vfx/scorch.png");
        decal.opacity   = 0.8f;
        decal.lifetime  = -1.f;
        mScorchDecal = vfx->SpawnDecal(decal);

        return true;
    }

    void Demo06_VFX::Update(float32 dt) {
        mTime += dt;
        mTrailAngle += dt * 2.f;

        // Mouvoir le trail en cercle
        mTrailPos = {cosf(mTrailAngle)*1.5f, 0.8f + sinf(mTime*3.f)*0.3f,
                      sinf(mTrailAngle)*1.5f};
        mRenderer->GetVFX()->AddTrailPoint(mTrail, mTrailPos);

        // Mettre à jour le VFX
        NkCamera3DData cam;
        cam.position={0,3,5}; cam.target={0,0.5f,0};
        mRenderer->GetVFX()->Update(dt, cam);
    }

    void Demo06_VFX::Render() {
        if (!mRenderer->BeginFrame()) return;
        auto* r3d = mRenderer->GetRender3D();
        auto* vfx = mRenderer->GetVFX();

        NkCamera3DData camData;
        camData.position={3.f,2.5f,4.f}; camData.target={0,0.5f,0};
        camData.fovY=70.f; camData.aspect=(float32)mWidth/(float32)mHeight;

        NkLightDesc l; l.type=NkLightType::NK_POINT; l.position={0,0.5f,0};
        l.color={1.f,0.5f,0.1f}; l.intensity=8.f+sinf(mTime*10)*2.f; l.range=5.f;

        NkSceneContext ctx;
        ctx.camera=NkCamera3D(camData); ctx.lights.PushBack(l); ctx.time=mTime;
        r3d->BeginScene(ctx);

        // Sol
        auto* meshSys=mRenderer->GetMeshSystem();
        NkDrawCall3D dcFloor; dcFloor.mesh=meshSys->GetPlane();
        dcFloor.transform=NkMat4f::Scale({8,1,8}); dcFloor.aabb={{-4,0,-4},{4,0,4}};
        r3d->Submit(dcFloor);

        r3d->DrawDebugGrid({0,0,0},0.5f,16,{0.2f,0.2f,0.2f,0.5f});

        // Render VFX par-dessus
        vfx->Render(mRenderer->GetCmd(), camData);

        auto* ov = mRenderer->GetOverlay();
        ov->DrawText({10,10},"Demo 06 — VFX: Fire + Smoke + Sparks + Trail + Decal");
        ov->DrawText({10,30},"Particles: %u  Emitters: %u",
                      vfx->GetActiveParticleCount(), vfx->GetActiveEmitterCount());

        mRenderer->EndFrame();
        mRenderer->Present();
    }

    void Demo06_VFX::Shutdown() {
        if (mRenderer) {
            auto* vfx = mRenderer->GetVFX();
            vfx->DestroyEmitter(mFireEmitter);
            vfx->DestroyEmitter(mSmokeEmitter);
            vfx->DestroyEmitter(mSparkEmitter);
            vfx->DestroyTrail(mTrail);
            vfx->DestroyDecal(mScorchDecal);
            NkRenderer::Destroy(mRenderer);
        }
    }

// =============================================================================
// Demo07_Offscreen — Minimap RTT + Export image
// =============================================================================
    bool Demo07_Offscreen::Init(NkIDevice* device, const NkSurfaceDesc& surface,
                                  uint32 w, uint32 h) {
        mWidth=w; mHeight=h;
        auto cfg = NkRendererConfig::ForGame(NkGraphicsApi::NK_OPENGL, w, h);
        cfg.width=w; cfg.height=h;
        mRenderer = NkRenderer::Create(device, surface, cfg);
        if (!mRenderer) return false;

        // Minimap 256×256 (vue de dessus)
        NkOffscreenDesc mmDesc;
        mmDesc.width=256; mmDesc.height=256; mmDesc.name="Minimap";
        mMinimap = mRenderer->CreateOffscreen(mmDesc);

        // Portail 512×512 (vue alternative)
        NkOffscreenDesc portalDesc;
        portalDesc.width=512; portalDesc.height=512;
        portalDesc.hdr=true; portalDesc.name="Portal";
        mPortal = mRenderer->CreateOffscreen(portalDesc);

        auto* meshSys = mRenderer->GetMeshSystem();
        auto* matSys  = mRenderer->GetMaterials();
        mMeshScene = meshSys->GetCube();
        mMatScene  = matSys->CreateInstance(matSys->DefaultPBR());
        mMatScene->SetAlbedo({0.4f,0.6f,0.9f})->SetRoughness(0.4f);

        return true;
    }

    void Demo07_Offscreen::Update(float32 dt) {
        mTime+=dt; mAngle+=dt*0.7f;
    }

    void Demo07_Offscreen::Render() {
        if (!mRenderer->BeginFrame()) return;
        auto* cmd     = mRenderer->GetCmd();
        auto* r3d     = mRenderer->GetRender3D();
        auto* meshSys = mRenderer->GetMeshSystem();

        NkLightDesc sun; sun.type=NkLightType::NK_DIRECTIONAL;
        sun.direction={-0.5f,-1,-0.3f}; sun.color={1,0.95f,0.9f}; sun.intensity=3.f;

        // ── Rendu minimap (top-down) ───────────────────────────────────────
        if (mMinimap && mMinimap->IsValid()) {
            mMinimap->BeginCapture(cmd, true, {0.1f,0.1f,0.15f,1.f});
            NkCamera3DData topCam;
            topCam.position={0,10,0}; topCam.target={0,0,0};
            topCam.up={0,0,-1}; topCam.fovY=60.f; topCam.aspect=1.f;
            NkSceneContext ctx; ctx.camera=NkCamera3D(topCam);
            ctx.lights.PushBack(sun); ctx.time=mTime;
            r3d->BeginScene(ctx);
            for(int i=-3;i<=3;i++) for(int j=-3;j<=3;j++) {
                NkDrawCall3D dc; dc.mesh=mMeshScene;
                dc.transform=NkMat4f::Translate({(float32)i*1.5f,0,(float32)j*1.5f})
                              *NkMat4f::Scale({0.4f,0.4f,0.4f});
                dc.aabb={{(float32)i*1.5f-0.2f,-0.2f,(float32)j*1.5f-0.2f},
                          {(float32)i*1.5f+0.2f, 0.2f,(float32)j*1.5f+0.2f}};
                r3d->Submit(dc);
            }
            r3d->Flush(cmd);
            mMinimap->EndCapture(cmd);
        }

        // ── Rendu principal ───────────────────────────────────────────────
        NkCamera3DData cam;
        cam.position={cosf(mAngle)*5,2,sinf(mAngle)*5};
        cam.target={0,0,0}; cam.fovY=65.f;
        cam.aspect=(float32)mWidth/(float32)mHeight;
        NkSceneContext ctx; ctx.camera=NkCamera3D(cam);
        ctx.lights.PushBack(sun); ctx.time=mTime;
        r3d->BeginScene(ctx);

        NkDrawCall3D dc; dc.mesh=mMeshScene;
        dc.transform=NkMat4f::RotateY(mAngle)*NkMat4f::Scale({1.5f,1.5f,1.5f});
        dc.aabb={{-0.8f,-0.8f,-0.8f},{0.8f,0.8f,0.8f}};
        r3d->Submit(dc);
        r3d->DrawDebugGrid({0,0,0},1.f,10,{0.2f,0.2f,0.2f,1.f});

        // ── Afficher minimap en overlay ────────────────────────────────────
        auto* r2d = mRenderer->GetRender2D();
        r2d->Begin(cmd, mWidth, mHeight);
        if (mMinimap)
            r2d->DrawImage(mMinimap->GetColorResult(),
                             {(float32)mWidth-270.f,10,256,256});
        r2d->DrawRect({(float32)mWidth-272.f,8,260,260},{1,1,0,1},2.f);
        r2d->End();

        auto* ov = mRenderer->GetOverlay();
        ov->DrawText({10,10},"Demo 07 — Offscreen RTT: Minimap + Portal");
        ov->DrawStats(mRenderer->GetStats());

        mRenderer->EndFrame();
        mRenderer->Present();
    }

    void Demo07_Offscreen::Shutdown() {
        if (mRenderer) {
            if (mMinimap) mRenderer->DestroyOffscreen(mMinimap);
            if (mPortal)  mRenderer->DestroyOffscreen(mPortal);
            if (mMatScene) mRenderer->GetMaterials()->DestroyInstance(mMatScene);
            NkRenderer::Destroy(mRenderer);
        }
    }

// =============================================================================
// Demo08_PV3DE — Patient Virtuel 3D Émotif
// =============================================================================
    bool Demo08_PV3DE::Init(NkIDevice* device, const NkSurfaceDesc& surface,
                              uint32 w, uint32 h) {
        mWidth=w; mHeight=h;
        auto cfg = NkRendererConfig::ForFilm(NkGraphicsApi::NK_OPENGL, w, h);
        cfg.width=w; cfg.height=h;
        cfg.postProcess.ssao=true; cfg.postProcess.bloom=true;
        mRenderer = NkRenderer::Create(device, surface, cfg);
        if (!mRenderer) return false;

        auto* meshSys = mRenderer->GetMeshSystem();
        auto* matSys  = mRenderer->GetMaterials();
        auto* vfx     = mRenderer->GetVFX();

        mMeshHead = meshSys->Import("assets/models/head_hires.glb");
        mMeshBody = meshSys->Import("assets/models/body.glb");

        // Matériau peau SSS
        mMatSkin = matSys->CreateInstance(matSys->DefaultSkin());
        mMatSkin->SetAlbedo({0.88f,0.65f,0.5f})
                ->SetSubsurface(0.45f, {1.f,0.4f,0.25f})
                ->SetRoughness(0.55f)->SetMetallic(0.f);

        // Matériau yeux
        mMatEyes = matSys->CreateInstance(matSys->DefaultPBR());
        mMatEyes->SetAlbedo({0.05f,0.05f,0.05f})
                ->SetRoughness(0.f)->SetMetallic(0.f)
                ->SetEmissive({0.4f,0.6f,1.f}, 0.3f);

        // Larmes (émetteur désactivé par défaut)
        NkEmitterDesc tearDesc;
        tearDesc.shape     = NkEmitterShape::POINT;
        tearDesc.position  = {-0.03f,1.55f,0.08f};
        tearDesc.ratePerSec= 0.f;
        tearDesc.lifeMin   = 0.4f; tearDesc.lifeMax=0.8f;
        tearDesc.speedMin  = 0.03f; tearDesc.speedMax=0.08f;
        tearDesc.velocityDir={0,-1,0}; tearDesc.velocityRand=0.05f;
        tearDesc.sizeStart = 0.004f; tearDesc.sizeEnd=0.002f;
        tearDesc.colorStart= {0.7f,0.85f,1.f,0.9f};
        tearDesc.colorEnd  = {0.7f,0.85f,1.f,0.f};
        tearDesc.gravity   = {0,-3.f,0};
        tearDesc.maxParticles=30;
        mTearEmitter = vfx->CreateEmitter(tearDesc);

        // Bones tête (simplifiés)
        mBones.Resize(4);
        for(auto& b:mBones) b=NkMat4f::Identity();

        // État initial neutre
        mBS      = {};
        mEmotion = {};

        return true;
    }

    void Demo08_PV3DE::Update(float32 dt) {
        mTime       += dt;
        mEmotionTime+= dt;

        // Séquence d'émotions démonstrative (cycle 20s)
        float32 t = fmodf(mEmotionTime, 20.f);

        if      (t < 3.f)  { // Neutre
            mEmotion={0,0,0,0,0,0,0,0};
            mBS.eyeBlinkL=mBS.eyeBlinkR=fmaxf(0.f,1.f-fabsf(fmodf(t,3.f)-1.5f)*4.f);
        }
        else if (t < 6.f)  { // Joie
            float32 p=(t-3.f)/3.f;
            mEmotion.joy=p;
            mBS.mouthSmileL=mBS.mouthSmileR=p*0.8f;
            mBS.cheekPuffL=mBS.cheekPuffR=p*0.3f;
            mBS.blush=p*0.3f;
        }
        else if (t < 10.f) { // Tristesse + larmes
            float32 p=(t-6.f)/4.f;
            mEmotion.sadness=p; mEmotion.joy=0;
            mBS.mouthFrownL=mBS.mouthFrownR=p*0.7f;
            mBS.mouthSmileL=mBS.mouthSmileR=0;
            mBS.browInnerUp=p*0.6f;
            mBS.eyeSquintL=mBS.eyeSquintR=p*0.5f;
            mBS.tears=p;
            mBS.pallor=p*0.2f;
            // Activer larmes
            if (auto* d=mRenderer->GetVFX()->GetEmitterDesc(mTearEmitter))
                d->ratePerSec=p*5.f;
        }
        else if (t < 13.f) { // Peur
            float32 p=(t-10.f)/3.f;
            mEmotion.fear=p; mEmotion.sadness=0;
            mBS.browOuterUpL=mBS.browOuterUpR=p;
            mBS.eyeWideL=mBS.eyeWideR=p*0.8f;
            mBS.tears=0; mBS.pallor=p*0.5f;
            if(auto*d=mRenderer->GetVFX()->GetEmitterDesc(mTearEmitter)) d->ratePerSec=0;
        }
        else if (t < 16.f) { // Colère
            float32 p=(t-13.f)/3.f;
            mEmotion.anger=p; mEmotion.fear=0;
            mBS.browDownL=mBS.browDownR=p*0.8f;
            mBS.eyeWideL=mBS.eyeWideR=0;
            mBS.mouthFrownL=mBS.mouthFrownR=p*0.5f;
            mBS.blush=p*0.5f;
        }
        else { // Retour neutre
            float32 p=1.f-(t-16.f)/4.f;
            mEmotion.anger*=p; mEmotion.joy*=p;
            mBS.blush*=p; mBS.pallor*=p;
        }

        // Animation head-bob subtile
        mBones[0]=NkMat4f::Translate({0,1.5f,0})*NkMat4f::RotateY(sinf(mTime*0.5f)*0.05f);
        mBones[1]=mBones[0]*NkMat4f::Translate({0,0.2f,0});
        mBones[2]=mBones[1]*NkMat4f::Translate({0,0.1f,0});
        mBones[3]=mBones[0]*NkMat4f::RotateX(sinf(mTime*0.3f)*0.03f);

        NkCamera3DData cam; cam.position={0,1.5f,0.6f}; cam.target={0,1.5f,0};
        mRenderer->GetVFX()->Update(dt, cam);
    }

    void Demo08_PV3DE::Render() {
        if (!mRenderer->BeginFrame()) return;
        auto* sim = mRenderer->GetSimulation();
        auto* r3d = mRenderer->GetRender3D();
        auto* vfx = mRenderer->GetVFX();
        auto* r2d = mRenderer->GetRender2D();

        NkCamera3DData camData;
        camData.position={0,1.5f,0.6f}; camData.target={0,1.5f,0};
        camData.fovY=50.f; camData.aspect=(float32)mWidth/(float32)mHeight;

        // Éclairage studio
        NkLightDesc key; key.type=NkLightType::NK_DIRECTIONAL;
        key.direction={-0.5f,-0.8f,-0.3f}; key.color={1.f,0.98f,0.95f};
        key.intensity=4.f; key.castShadow=true;
        NkLightDesc fill; fill.type=NkLightType::NK_POINT;
        fill.position={-1.f,2.f,0.5f}; fill.color={0.5f,0.6f,0.9f};
        fill.intensity=1.5f; fill.range=4.f;
        NkLightDesc rim; rim.type=NkLightType::NK_POINT;
        rim.position={0,2.f,-1.f}; rim.color={0.9f,0.9f,1.f};
        rim.intensity=2.f; rim.range=3.f;

        NkSceneContext ctx;
        ctx.camera=NkCamera3D(camData);
        ctx.lights.PushBack(key); ctx.lights.PushBack(fill); ctx.lights.PushBack(rim);
        ctx.time=mTime;
        r3d->BeginScene(ctx);

        // Soumettre tête + corps via NkSimulationRenderer
        if (mMeshHead.IsValid())
            sim->SubmitCharacter(mMeshHead, {}, NkMat4f::Identity(),
                                  mBones.Data(), (uint32)mBones.Size(),
                                  mBS, mEmotion);
        if (mMeshBody.IsValid()) {
            NkDrawCall3D dc; dc.mesh=mMeshBody;
            dc.transform=NkMat4f::Translate({0,-0.5f,0});
            dc.aabb={{-0.3f,-0.5f,-0.2f},{0.3f,1.f,0.2f}};
            r3d->Submit(dc);
        }

        // VFX larmes
        vfx->Render(mRenderer->GetCmd(), camData);

        // ── UI émotions ───────────────────────────────────────────────────
        r2d->Begin(mRenderer->GetCmd(), mWidth, mHeight);
        float32 bw=160.f, bh=18.f, bx=10.f, by=60.f;
        auto DrawBar=[&](const char* label, float32 val, NkVec4f color) {
            r2d->FillRect({bx,by,bw,bh},{0.1f,0.1f,0.1f,0.7f});
            r2d->FillRect({bx,by,bw*val,bh},color);
            r2d->DrawRect({bx,by,bw,bh},{0.5f,0.5f,0.5f,1.f});
            by+=bh+2.f;
        };
        r2d->FillRoundRect({5,55,170,200},{0,0,0,0.6f},6.f);
        DrawBar("Joy",     mEmotion.joy,     {1.f,0.9f,0.2f,1.f});
        DrawBar("Sadness", mEmotion.sadness, {0.3f,0.4f,1.f,1.f});
        DrawBar("Fear",    mEmotion.fear,    {0.7f,0.3f,1.f,1.f});
        DrawBar("Anger",   mEmotion.anger,   {1.f,0.2f,0.1f,1.f});
        DrawBar("Pain",    mEmotion.pain,    {1.f,0.4f,0.f,1.f});
        DrawBar("Fatigue", mEmotion.fatigue, {0.4f,0.4f,0.5f,1.f});
        DrawBar("Blush",   mBS.blush,        {1.f,0.5f,0.5f,1.f});
        DrawBar("Tears",   mBS.tears,        {0.6f,0.8f,1.f,1.f});
        r2d->End();

        auto* ov = mRenderer->GetOverlay();
        ov->DrawText({10,10},"Demo 08 — PV3DE: Patient Virtuel 3D Emotif");
        ov->DrawStats(mRenderer->GetStats());

        mRenderer->EndFrame();
        mRenderer->Present();
    }

    void Demo08_PV3DE::Shutdown() {
        if (mRenderer) {
            mRenderer->GetVFX()->DestroyEmitter(mTearEmitter);
            if (mMatSkin)  mRenderer->GetMaterials()->DestroyInstance(mMatSkin);
            if (mMatEyes)  mRenderer->GetMaterials()->DestroyInstance(mMatEyes);
            NkRenderer::Destroy(mRenderer);
        }
    }

// =============================================================================
// Demo09_MultiViewport — Éditeur 4 viewports (top/front/side/perspective)
// =============================================================================
    bool Demo09_MultiVP::Init(NkIDevice* device, const NkSurfaceDesc& surface,
                                uint32 w, uint32 h) {
        mWidth=w; mHeight=h;
        auto cfg = NkRendererConfig::ForEditor(NkGraphicsApi::NK_OPENGL, w, h);
        cfg.width=w; cfg.height=h;
        mRenderer = NkRenderer::Create(device, surface, cfg);
        if (!mRenderer) return false;

        uint32 vw=w/2, vh=h/2;
        NkOffscreenDesc vd; vd.width=vw; vd.height=vh;

        vd.name="VP_Persp";  mViewports[0]=mRenderer->CreateOffscreen(vd);
        vd.name="VP_Top";    mViewports[1]=mRenderer->CreateOffscreen(vd);
        vd.name="VP_Front";  mViewports[2]=mRenderer->CreateOffscreen(vd);
        vd.name="VP_Side";   mViewports[3]=mRenderer->CreateOffscreen(vd);

        auto* meshSys = mRenderer->GetMeshSystem();
        auto* matSys  = mRenderer->GetMaterials();
        mMeshScene = meshSys->GetIcosphere();
        mMat       = matSys->CreateInstance(matSys->DefaultPBR());
        mMat->SetAlbedo({0.2f,0.5f,0.9f})->SetRoughness(0.3f)->SetMetallic(0.6f);

        // Configurer les 4 caméras
        NkCamera3DData d;
        d.nearPlane=0.01f; d.farPlane=1000.f; d.aspect=(float32)vw/(float32)vh;

        // Perspective
        d.position={3,2,3}; d.target={0,0,0}; d.fovY=65.f; mCamPersp.SetData(d);
        // Top (orthographique)
        d.position={0,10,0}; d.target={0,0,0}; d.up={0,0,-1};
        d.ortho=true; d.orthoSize=5.f; mCamTop.SetData(d);
        // Front
        d.position={0,0,10}; d.target={0,0,0}; d.up={0,1,0};
        d.ortho=true; mCamFront.SetData(d);
        // Side
        d.position={10,0,0}; d.target={0,0,0};
        mCamSide.SetData(d);

        return true;
    }

    void Demo09_MultiVP::Update(float32 dt) { mTime+=dt; }

    void Demo09_MultiVP::Render() {
        if (!mRenderer->BeginFrame()) return;
        auto* cmd=mRenderer->GetCmd();
        auto* r3d=mRenderer->GetRender3D();
        auto* r2d=mRenderer->GetRender2D();

        NkLightDesc sun; sun.type=NkLightType::NK_DIRECTIONAL;
        sun.direction={-0.5f,-1,-0.3f}; sun.color={1,1,1}; sun.intensity=3.f;

        NkCamera3D cams[4]={mCamPersp,mCamTop,mCamFront,mCamSide};
        const char* labels[4]={"Perspective","Top","Front","Side"};

        // ── Rendu des 4 viewports ─────────────────────────────────────────
        for (int v=0;v<4;v++) {
            if (!mViewports[v]||!mViewports[v]->IsValid()) continue;
            mViewports[v]->BeginCapture(cmd, true, {0.12f,0.12f,0.15f,1.f});

            NkSceneContext ctx;
            ctx.camera=cams[v]; ctx.lights.PushBack(sun); ctx.time=mTime;
            r3d->BeginScene(ctx);

            // Grille + axes
            r3d->DrawDebugGrid({0,0,0},0.5f,20,{0.3f,0.3f,0.3f,0.5f});
            r3d->DrawDebugAxes(NkMat4f::Identity(), 1.5f);

            // Objet central
            NkDrawCall3D dc; dc.mesh=mMeshScene;
            dc.transform=NkMat4f::RotateY(mTime*0.5f)*NkMat4f::Scale({0.8f,0.8f,0.8f});
            dc.aabb={{-0.5f,-0.5f,-0.5f},{0.5f,0.5f,0.5f}};
            r3d->Submit(dc);

            r3d->Flush(cmd);
            mViewports[v]->EndCapture(cmd);
        }

        // ── Composer les 4 viewports en 2×2 ─────────────────────────────
        r2d->Begin(cmd, mWidth, mHeight);
        float32 vw=(float32)mWidth*0.5f, vh=(float32)mHeight*0.5f;
        NkVec2f origins[4]={{0,0},{vw,0},{0,vh},{vw,vh}};
        NkVec4f borderColors[4]={{1.f,0.5f,0.f,1.f},{0.f,0.5f,1.f,1.f},
                                   {0.f,0.8f,0.3f,1.f},{0.8f,0.3f,1.f,1.f}};
        for (int v=0;v<4;v++) {
            if (!mViewports[v]) continue;
            r2d->DrawImage(mViewports[v]->GetColorResult(),
                             {origins[v].x,origins[v].y,vw,vh});
            r2d->DrawRect({origins[v].x,origins[v].y,vw,vh},borderColors[v],2.f);
        }
        // Croix centrale
        r2d->DrawLine({vw-1,0},{vw-1,(float32)mHeight},{0.4f,0.4f,0.4f,1.f},2.f);
        r2d->DrawLine({0,vh-1},{(float32)mWidth,vh-1},{0.4f,0.4f,0.4f,1.f},2.f);
        r2d->End();

        auto* txt=mRenderer->GetTextRenderer();
        auto  fnt=txt->GetDefaultFont();
        auto* r2d2=mRenderer->GetRender2D();
        r2d2->Begin(cmd,mWidth,mHeight);
        for (int v=0;v<4;v++)
            txt->DrawText({origins[v].x+8,origins[v].y+8},
                           labels[v], fnt, 14.f, 0xFFFFFFFF);
        r2d2->End();

        auto* ov=mRenderer->GetOverlay();
        ov->DrawText({10,10},"Demo 09 — Multi-Viewport Editor (4 views)");

        mRenderer->EndFrame();
        mRenderer->Present();
    }

    void Demo09_MultiVP::Shutdown() {
        if (mRenderer) {
            for (int v=0;v<4;v++)
                if (mViewports[v]) mRenderer->DestroyOffscreen(mViewports[v]);
            if (mMat) mRenderer->GetMaterials()->DestroyInstance(mMat);
            NkRenderer::Destroy(mRenderer);
        }
    }

// =============================================================================
// Demo10_NkSL — Shader custom NkSL (wave distortion)
// =============================================================================
    bool Demo10_NkSL::Init(NkIDevice* device, const NkSurfaceDesc& surface,
                             uint32 w, uint32 h) {
        mWidth=w; mHeight=h;
        auto cfg = NkRendererConfig::ForGame(NkGraphicsApi::NK_OPENGL, w, h);
        cfg.width=w; cfg.height=h;
        mRenderer = NkRenderer::Create(device, surface, cfg);
        if (!mRenderer) return false;

        // Shader NkSL custom : wave distortion sur la géométrie
        const char* nkslSrc = R"nksl(
@uniform 0 CameraUBO { mat4 viewProj; vec4 camPos; float time; float _p[3]; };
@uniform 1 ObjectUBO { mat4 model; vec4 tint; float waveAmplitude; float waveFreq; float _p[2]; };
@texture(2) tAlbedo;

@vertex {
    @in vec3 aPos    : 0;
    @in vec3 aNormal : 1;
    @in vec2 aUV     : 2;
    @out vec3 vNormal: 0;
    @out vec2 vUV    : 1;
    @out vec3 vWPos  : 2;

    void main() {
        vec4 wp = model * vec4(aPos, 1.0);
        // Wave displacement
        float wave = sin(wp.x * waveFreq + time * 3.0) *
                     cos(wp.z * waveFreq + time * 2.0) * waveAmplitude;
        wp.y += wave;
        vWPos  = wp.xyz;
        vNormal= aNormal;
        vUV    = aUV;

        @target GL  { gl_Position = viewProj * wp; }
        @target VK  { gl_Position = viewProj * wp; gl_Position.y = -gl_Position.y; }
        @target DX11{ SV_Position = mul(viewProj, wp); }
    }
}

@fragment {
    @in vec3 vNormal : 0;
    @in vec2 vUV     : 1;
    @in vec3 vWPos   : 2;
    @out vec4 fragColor : 0;

    void main() {
        vec4 albedo = texture(tAlbedo, vUV + vNormal.xy * 0.02) * tint;
        // Iridescent color based on normal + time
        float iri = sin(dot(normalize(vNormal), vec3(0,1,0)) * 6.28 + time) * 0.5 + 0.5;
        vec3 iriColor = vec3(iri, 1.0 - iri, sin(iri * 3.14) * 0.5 + 0.5);
        fragColor = vec4(albedo.rgb * 0.6 + iriColor * 0.4, albedo.a);
    }
}
)nksl";

        // Enregistrer le template custom
        NkMaterialTemplateDesc tmplDesc;
        tmplDesc.type     = NkMaterialType::NK_CUSTOM;
        tmplDesc.name     = "WaveIridescent";
        tmplDesc.nkslSource = nkslSrc;

        auto* matSys = mRenderer->GetMaterials();
        mCustomTemplate = matSys->RegisterTemplate(tmplDesc);
        mCustomMat      = matSys->CreateInstance(mCustomTemplate);
        mCustomMat->SetAlbedo({0.8f,0.6f,1.f})
                  ->SetFloat("waveAmplitude", 0.1f)
                  ->SetFloat("waveFreq",      3.f);

        mMesh = mRenderer->GetMeshSystem()->GetIcosphere();
        return true;
    }

    void Demo10_NkSL::Update(float32 dt) {
        mTime     += dt;
        mWaveTime += dt;
        // Faire varier l'amplitude de la vague
        float32 amp = 0.08f + sinf(mWaveTime) * 0.05f;
        mCustomMat->SetFloat("waveAmplitude", amp);
    }

    void Demo10_NkSL::Render() {
        if (!mRenderer->BeginFrame()) return;
        auto* r3d = mRenderer->GetRender3D();

        NkCamera3DData cam;
        cam.position={0,1.5f,3.f}; cam.target={0,0,0};
        cam.fovY=60.f; cam.aspect=(float32)mWidth/(float32)mHeight;

        NkLightDesc l; l.type=NkLightType::NK_DIRECTIONAL;
        l.direction={-1,-1,-0.5f}; l.color={1,1,1}; l.intensity=3.f;

        NkSceneContext ctx;
        ctx.camera=NkCamera3D(cam); ctx.lights.PushBack(l); ctx.time=mTime;
        r3d->BeginScene(ctx);

        NkDrawCall3D dc; dc.mesh=mMesh;
        dc.transform=NkMat4f::RotateY(mTime*0.4f)*NkMat4f::Scale({1.2f,1.2f,1.2f});
        dc.aabb={{-0.7f,-0.7f,-0.7f},{0.7f,0.7f,0.7f}};
        r3d->Submit(dc);

        r3d->DrawDebugGrid({0,0,0},0.5f,14,{0.2f,0.2f,0.2f,0.5f});

        auto* ov = mRenderer->GetOverlay();
        ov->DrawText({10,10},"Demo 10 — Custom NkSL Shader: Wave + Iridescent");
        ov->DrawText({10,30},"NkSL transpiles to: GL / VK / DX11 / DX12 / MSL");
        ov->DrawText({10,50},"Wave amp=%.3f  freq=3.0  time=%.1f",
                      0.08f+sinf(mWaveTime)*0.05f, mTime);

        mRenderer->EndFrame();
        mRenderer->Present();
    }

    void Demo10_NkSL::Shutdown() {
        if (mRenderer) {
            if (mCustomMat) mRenderer->GetMaterials()->DestroyInstance(mCustomMat);
            NkRenderer::Destroy(mRenderer);
        }
    }

} // namespace demo
} // namespace renderer
} // namespace nkentseu
