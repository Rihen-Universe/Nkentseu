// =============================================================================
// Demo02_Game2D.cpp  — NKRenderer v4.0
// Jeu 2D : sprites, tilemaps, texte, UI avec NKFont + NKUI
// =============================================================================
#include "Demo.h"

namespace nkentseu {
namespace renderer {
namespace demo {

    bool Demo02_Game2D::Init(NkIDevice* device, const NkSurfaceDesc& surface,
                               uint32 w, uint32 h) {
        mWidth=w; mHeight=h;
        auto cfg = NkRendererConfig::For2D(NkGraphicsApi::NK_OPENGL, w, h);
        mRenderer = NkRenderer::Create(device, surface, cfg);
        if (!mRenderer) return false;

        auto* texLib = mRenderer->GetTextures();
        auto* txtR   = mRenderer->GetTextRenderer();

        mSprite  = texLib->Load("assets/sprites/player.png");
        mTileset = texLib->Load("assets/sprites/tileset.png");
        mUI      = texLib->Load("assets/ui/panel.png");
        mFont    = txtR->LoadFont("assets/fonts/Roboto-Regular.ttf", 20.f, true/*SDF*/);

        mSpriteX = 100.f;
        mScore   = 0;
        return true;
    }

    void Demo02_Game2D::Update(float32 dt) {
        mTime    += dt;
        mSpriteX += 60.f * dt;
        if (mSpriteX > (float32)mWidth + 64.f) mSpriteX = -64.f;
        mScore   = (int32)(mTime * 10.f);
    }

    void Demo02_Game2D::Render() {
        if (!mRenderer->BeginFrame()) return;

        auto* r2d = mRenderer->GetRender2D();
        auto* txt = mRenderer->GetTextRenderer();

        r2d->Begin(mRenderer->GetCmd(), mWidth, mHeight);

        // ── Fond dégradé ciel/sol ──────────────────────────────────────────
        r2d->FillRectGradV({0,0,(float32)mWidth,(float32)mHeight*0.6f},
                             {0.3f,0.5f,0.9f,1.f},{0.6f,0.8f,1.f,1.f});
        r2d->FillRectGradV({0,(float32)mHeight*0.6f,(float32)mWidth,(float32)mHeight*0.4f},
                             {0.2f,0.7f,0.2f,1.f},{0.1f,0.4f,0.1f,1.f});

        // ── Tilemap (3×3 tiles) ─────────────────────────────────────────────
        for (int ty=0;ty<3;ty++) {
            for (int tx=0;tx<(int)(mWidth/64)+2;tx++) {
                float32 x = (float32)(tx*64) - (int32)(mTime*40)%64;
                float32 y = (float32)mHeight*0.6f - 64.f + ty*64.f;
                // UV tile dans tileset 4×4
                float32 tu = (float32)(ty%4)*0.25f, tv = 0.f;
                r2d->DrawSprite({x,y,64,64}, mTileset, {1,1,1,1}, {tu,tv,0.25f,0.25f});
            }
        }

        // ── Sprite joueur ──────────────────────────────────────────────────
        float32 bobY = sinf(mTime * 8.f) * 4.f;
        r2d->DrawSpriteRotated({mSpriteX-32.f,(float32)mHeight*0.6f-96.f+bobY,64,64},
                                 mSprite, sinf(mTime * 2.f) * 5.f);

        // ── Nuages (cercles semi-transparents) ─────────────────────────────
        for (int c=0;c<5;c++) {
            float32 cx = (mTime*25.f + c*220.f);
            cx = cx - (int32)(cx / (mWidth+200.f)) * (mWidth+200.f) - 100.f;
            r2d->FillCircle({cx, (float32)(50+c*30)}, 40.f + c*10.f, {1,1,1,0.6f});
            r2d->FillCircle({cx+30.f,(float32)(40+c*30)}, 30.f, {1,1,1,0.6f});
        }

        // ── Panel UI 9-slice ───────────────────────────────────────────────
        float32 pw=200.f, ph=80.f;
        float32 px=(float32)mWidth-pw-20.f, py=20.f;
        r2d->DrawNineSlice({px,py,pw,ph}, mUI, 16,16,16,16, {1,1,1,0.9f});

        // ── Score ──────────────────────────────────────────────────────────
        char scoreBuf[64];
        snprintf(scoreBuf, sizeof(scoreBuf), "SCORE: %06d", mScore);
        txt->DrawText({px+16.f, py+12.f}, scoreBuf, mFont, 22.f, 0xFFFFFFFF);
        txt->DrawText({px+16.f, py+42.f}, "LEVEL 1", mFont, 16.f, 0xFFDD44FF);

        // ── Titre ──────────────────────────────────────────────────────────
        txt->DrawTextCentered({0,0,(float32)mWidth,50.f},
                               "NKRenderer v4 — 2D Demo", mFont, 24.f, 0xFFFFFFFF);

        // ── Formes debug ───────────────────────────────────────────────────
        r2d->DrawRoundRect({20,mHeight-80.f,200,60}, {0,0,0,0.5f}, 8.f);
        r2d->DrawCircle({120.f,(float32)mHeight-50.f}, 20.f+sinf(mTime*3)*5.f,
                          {0.f,1.f,0.5f,1.f}, 2.f);
        r2d->DrawBezier({30,(float32)mHeight-40},{80,(float32)mHeight-80},
                          {140,(float32)mHeight-20},{200,(float32)mHeight-60},
                          {1.f,0.5f,0.f,1.f}, 3.f);

        r2d->End();

        mRenderer->GetOverlay()->DrawStats(mRenderer->GetStats());
        mRenderer->EndFrame();
        mRenderer->Present();
    }

    void Demo02_Game2D::Shutdown() {
        if (mRenderer) NkRenderer::Destroy(mRenderer);
    }

// =============================================================================
// Demo03_Film.cpp  — Rendu cinématique (film d'animation)
// =============================================================================
    bool Demo03_Film::Init(NkIDevice* device, const NkSurfaceDesc& surface,
                             uint32 w, uint32 h) {
        mWidth=w; mHeight=h;
        auto cfg = NkRendererConfig::ForFilm(NkGraphicsApi::NK_VULKAN, w, h);
        // Réduire pour la démo (4K → 1920)
        cfg.width=w; cfg.height=h;
        mRenderer = NkRenderer::Create(device, surface, cfg);
        if (!mRenderer) return false;

        auto* meshSys = mRenderer->GetMeshSystem();
        auto* matSys  = mRenderer->GetMaterials();

        mMeshChar = meshSys->Import("assets/models/mannequin.glb");
        mMatSkin  = matSys->CreateInstance(matSys->DefaultSkin());
        mMatSkin->SetAlbedo({0.9f,0.7f,0.6f})
               ->SetSubsurface(0.4f, {1.f,0.4f,0.25f})
               ->SetRoughness(0.5f)
               ->SetMetallic(0.0f);

        return true;
    }

    void Demo03_Film::Update(float32 dt) {
        mTime   += dt;
        mCamYaw += dt * 0.3f;
        mCamPos  = {cosf(mCamYaw)*3.f, 1.5f, sinf(mCamYaw)*3.f};
    }

    void Demo03_Film::Render() {
        if (!mRenderer->BeginFrame()) return;
        auto* r3d = mRenderer->GetRender3D();
        auto* pp  = mRenderer->GetPostProcess();

        // Activer DOF pour l'effet cinématique
        pp->GetConfig().dof         = true;
        pp->GetConfig().dofFocusDist= 3.f;
        pp->GetConfig().dofAperture = 0.15f;
        pp->GetConfig().motionBlur  = true;
        pp->GetConfig().colorGrading= true;
        pp->GetConfig().vignette    = true;

        NkCamera3DData cam;
        cam.position  = mCamPos;
        cam.target    = {0,1.f,0};
        cam.fovY      = 50.f;
        cam.aspect    = (float32)mWidth/(float32)mHeight;

        NkLightDesc key;
        key.type = NkLightType::NK_DIRECTIONAL; key.direction={-1,-1,-0.5f};
        key.color={1.f,0.95f,0.9f}; key.intensity=4.f; key.castShadow=true;
        NkLightDesc fill;
        fill.type=NkLightType::NK_POINT; fill.position={-2,2,2};
        fill.color={0.4f,0.5f,0.8f}; fill.intensity=2.f; fill.range=10.f;

        NkSceneContext ctx;
        ctx.camera={NkCamera3D(cam)}; ctx.lights.PushBack(key); ctx.lights.PushBack(fill);
        ctx.time=mTime;
        r3d->BeginScene(ctx);

        if (mMeshChar.IsValid()) {
            NkDrawCall3D dc;
            dc.mesh=mMeshChar; dc.transform=NkMat4f::Identity();
            dc.aabb={{-0.5f,0,-0.5f},{0.5f,2.f,0.5f}};
            r3d->Submit(dc);
        }

        mRenderer->GetOverlay()->DrawText({10,10}, "Demo 03 — Film | DOF + MotionBlur + SSR");
        mRenderer->EndFrame();
        mRenderer->Present();
    }

    void Demo03_Film::Shutdown() {
        if (mRenderer) NkRenderer::Destroy(mRenderer);
    }

// =============================================================================
// Demo04_Archviz
// =============================================================================
    bool Demo04_Archviz::Init(NkIDevice* device, const NkSurfaceDesc& surface,
                                uint32 w, uint32 h) {
        mWidth=w; mHeight=h;
        auto cfg = NkRendererConfig::ForArchviz(NkGraphicsApi::NK_OPENGL, w, h);
        cfg.width=w; cfg.height=h;
        mRenderer = NkRenderer::Create(device, surface, cfg);
        if (!mRenderer) return false;

        auto* meshSys = mRenderer->GetMeshSystem();
        auto* matSys  = mRenderer->GetMaterials();
        auto* texLib  = mRenderer->GetTextures();

        mMeshRoom = meshSys->Import("assets/models/room.glb");

        // Matériau mur (archviz)
        mMatWall = matSys->CreateInstance(matSys->DefaultArchviz());
        mMatWall->SetAlbedo({0.92f,0.90f,0.88f})->SetRoughness(0.8f);

        // Verre fenêtre
        mMatGlass = matSys->CreateInstance(
            matSys->RegisterTemplate({NkMaterialType::NK_GLASS,"Glass",
                                       NkRenderQueue::NK_TRANSPARENT,
                                       NkBlendMode::NK_ALPHA}));
        mMatGlass->SetAlbedo({0.8f,0.9f,1.f,0.2f})->SetRoughness(0.05f)->SetMetallic(0.f);

        mHDRI = texLib->LoadHDR("assets/hdri/interior_workshop_4k.hdr");
        return true;
    }

    void Demo04_Archviz::Update(float32 dt) { mTime+=dt; }

    void Demo04_Archviz::Render() {
        if (!mRenderer->BeginFrame()) return;
        auto* r3d = mRenderer->GetRender3D();

        NkCamera3DData cam;
        cam.position={cosf(mTime*0.05f)*4.f,1.6f,sinf(mTime*0.05f)*4.f};
        cam.target={0,1.2f,0}; cam.fovY=55.f;
        cam.aspect=(float32)mWidth/(float32)mHeight;

        NkLightDesc sun; sun.type=NkLightType::NK_DIRECTIONAL;
        sun.direction={-0.3f,-1.f,-0.5f}; sun.color={1.f,0.98f,0.95f};
        sun.intensity=5.f; sun.castShadow=true;

        NkSceneContext ctx;
        ctx.camera=NkCamera3D(cam); ctx.lights.PushBack(sun);
        ctx.envMap=mHDRI; ctx.ambientIntensity=0.3f; ctx.time=mTime;
        r3d->BeginScene(ctx);

        if (mMeshRoom.IsValid()) {
            NkDrawCall3D dc; dc.mesh=mMeshRoom;
            dc.transform=NkMat4f::Identity();
            dc.aabb={{-5,0,-5},{5,3.f,5}}; r3d->Submit(dc);
        }

        mRenderer->GetOverlay()->DrawText({10,10}, "Demo 04 — Archviz | SSR + HBAO + TAA");
        mRenderer->EndFrame();
        mRenderer->Present();
    }

    void Demo04_Archviz::Shutdown() {
        if (mRenderer) NkRenderer::Destroy(mRenderer);
    }

// =============================================================================
// Demo05_Animation — Skinning GPU + Onion skin + Squelette
// =============================================================================
    bool Demo05_Animation::Init(NkIDevice* device, const NkSurfaceDesc& surface,
                                  uint32 w, uint32 h) {
        mWidth=w; mHeight=h;
        auto cfg = NkRendererConfig::ForEditor(NkGraphicsApi::NK_OPENGL, w, h);
        cfg.width=w; cfg.height=h;
        mRenderer = NkRenderer::Create(device, surface, cfg);
        if (!mRenderer) return false;

        auto* meshSys = mRenderer->GetMeshSystem();
        auto* matSys  = mRenderer->GetMaterials();

        mMeshChar = meshSys->Import("assets/models/character_skinned.glb");
        mMatChar  = matSys->CreateInstance(matSys->DefaultPBR());
        mMatChar->SetAlbedo({0.2f,0.4f,0.8f})->SetRoughness(0.6f);

        // Créer un squelette simple 8 os (bras, tronc, tête)
        mBoneCount = 8;
        mBones.Resize(mBoneCount);
        for (uint32 i=0;i<mBoneCount;i++) mBones[i]=NkMat4f::Identity();
        // Hiérarchie : 0=hanche, 1=torse, 2=tête, 3=épauleg, 4=épauled, 5=avant-bg, 6=avant-bd, 7=bassin
        int32 parents[8]={-1,0,1,1,1,3,4,0};
        for (uint32 i=0;i<8;i++) mParents[i]=parents[i];

        mCurrentFrame = 0;
        return true;
    }

    void Demo05_Animation::Update(float32 dt) {
        mTime     += dt;
        mAnimTime += dt;
        mCurrentFrame = (int32)(mAnimTime * 30.f); // 30 fps

        // Animer les bones (rotation simple sur l'axe Y pour la démo)
        for (uint32 i=0;i<mBoneCount;i++) {
            float32 a = mAnimTime * (1.f + i*0.3f);
            NkVec3f pos = {(float32)i*0.15f, (float32)i*0.2f, 0};
            mBones[i] = NkMat4f::Translate(pos) * NkMat4f::RotateY(a*0.5f);
        }
    }

    void Demo05_Animation::Render() {
        if (!mRenderer->BeginFrame()) return;

        auto* anim = mRenderer->GetAnimation();
        auto* r3d  = mRenderer->GetRender3D();

        NkCamera3DData cam;
        cam.position={0,1.5f,3.5f}; cam.target={0,0.8f,0};
        cam.fovY=65.f; cam.aspect=(float32)mWidth/(float32)mHeight;

        NkLightDesc l; l.type=NkLightType::NK_DIRECTIONAL;
        l.direction={-0.5f,-1,-0.5f}; l.color={1,1,1}; l.intensity=3.f;

        NkSceneContext ctx;
        ctx.camera=NkCamera3D(cam); ctx.lights.PushBack(l); ctx.time=mTime;
        r3d->BeginScene(ctx);

        // ── Onion skinning (6 fantômes) ──────────────────────────────────────
        if (mShowOnion && mMeshChar.IsValid()) {
            int32 offsets[6] = {-3,-2,-1,1,2,3};

            auto getBones = [this](int32 frame, NkMat4f* out, uint32 count) {
                // Simuler les bones au frame donné
                float32 t = frame / 30.f;
                for (uint32 i=0;i<count;i++) {
                    float32 a = t * (1.f + i*0.3f);
                    NkVec3f pos={i*0.15f,(float32)i*0.2f,0};
                    out[i] = NkMat4f::Translate(pos) * NkMat4f::RotateY(a*0.5f);
                }
            };

            anim->SubmitOnionSkin(mMeshChar, {},
                                   NkMat4f::Identity(), mBoneCount,
                                   mCurrentFrame, offsets, 6,
                                   getBones,
                                   {1.f,0.3f,0.3f,0.4f},
                                   {0.3f,0.3f,1.f,0.4f});
        }

        // ── Mesh principal skinné ─────────────────────────────────────────────
        if (mMeshChar.IsValid())
            anim->SubmitSkinnedMesh(mMeshChar, {},
                                     NkMat4f::Identity(),
                                     mBones.Data(), mBoneCount);

        // ── Squelette debug ───────────────────────────────────────────────────
        if (mShowSkeleton)
            anim->DrawSkeleton(mBones.Data(), mBoneCount, mParents,
                                {1.f,0.8f,0.f,1.f}, 0.04f);

        r3d->DrawDebugAxes(NkMat4f::Identity(), 0.5f);
        r3d->DrawDebugGrid({0,0,0}, 0.5f, 10, {0.25f,0.25f,0.25f,1.f});

        auto* ov = mRenderer->GetOverlay();
        ov->DrawText({10,10}, "Demo 05 — Skinning GPU + Onion Skin");
        ov->DrawText({10,30}, "Frame: %d  |  Bones: %d  |  Onion: %s  |  Skeleton: %s",
                      mCurrentFrame, mBoneCount,
                      mShowOnion?"ON":"OFF", mShowSkeleton?"ON":"OFF");

        mRenderer->EndFrame();
        mRenderer->Present();
    }

    void Demo05_Animation::Shutdown() {
        if (mRenderer) {
            if (mMatChar) mRenderer->GetMaterials()->DestroyInstance(mMatChar);
            NkRenderer::Destroy(mRenderer);
        }
    }

} // namespace demo
} // namespace renderer
} // namespace nkentseu
