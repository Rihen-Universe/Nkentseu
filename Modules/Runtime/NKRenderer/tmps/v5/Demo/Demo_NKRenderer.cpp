// =============================================================================
// Demo_NKRenderer.cpp  — NKRenderer v4.0
//
// Point d'entrée + 10 démos utilisant NKRenderer au-dessus de NKRHI.
// Structure exactement identique à NkRHIDemoFull.cpp :
//   • NKPlatform/NkMain.h pour nkmain
//   • NkWindow (pas de pointeur, objet valeur)
//   • NkDeviceFactory::Create / Destroy
//   • NkEvents() pour les callbacks
//   • NkClock pour le delta time
//   • NkRenderer::Create / Destroy au-dessus de NkIDevice
//   • Cleanup explicite de toutes les ressources
//
// Sélection démo : argument ligne de commande --demo=N (0..9)
// Sélection backend: --backend=opengl|vulkan|dx11|dx12|sw|metal
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
#include "NKLogger/NkLog.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "../NkRenderer.h"
#include "../Core/NkRendererConfig.h"

namespace nkentseu { struct NkEntryState; }
using namespace nkentseu;
using namespace nkentseu::math;
using namespace nkentseu::renderer;

// =============================================================================
// Helpers de parsing
// =============================================================================
static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (size_t i=1;i<args.Size();i++){
        if(args[i]=="--backend=vulkan" ||args[i]=="-bvk")  return NkGraphicsApi::NK_VULKAN;
        if(args[i]=="--backend=dx11"   ||args[i]=="-bdx11") return NkGraphicsApi::NK_DX11;
        if(args[i]=="--backend=dx12"   ||args[i]=="-bdx12") return NkGraphicsApi::NK_DX12;
        if(args[i]=="--backend=metal"  ||args[i]=="-bmtl")  return NkGraphicsApi::NK_METAL;
        if(args[i]=="--backend=sw"     ||args[i]=="-bsw")   return NkGraphicsApi::NK_SOFTWARE;
    }
    return NkGraphicsApi::NK_OPENGL;
}

static int ParseDemo(const NkVector<NkString>& args) {
    for (size_t i=1;i<args.Size();i++){
        if(args[i].StartsWith("--demo=")){
            return atoi(args[i].SubStr(7).c_str());
        }
    }
    return 0;
}

// =============================================================================
// Demo 01 — PBR 3D Basic
// Cube rotatif + sphères + sol, IBL, 3 lumières, ombres CSM, SSAO, Bloom
// =============================================================================
static int RunDemo01_Basic3D(NkIDevice* device, NkWindow& window,
                               NkGraphicsApi api) {
    logger.Info("[Demo01] Démarrage — PBR 3D Basic\n");
    logger.Info("[Demo01] Contrôles: WASD=cam | Flèches=lumière | ESC=quit | TAB=démo suivante\n");

    // ── NkRenderer ───────────────────────────────────────────────────────────
    uint32 W = (uint32)window.GetSize().width;
    uint32 H = (uint32)window.GetSize().height;

    NkSurfaceDesc surface = window.GetSurfaceDesc();
    NkRendererConfig cfg  = NkRendererConfig::ForGame(api, W, H);
    cfg.shadow.enabled      = true;
    cfg.postProcess.ssao    = true;
    cfg.postProcess.bloom   = true;
    cfg.postProcess.aces    = true;
    cfg.postProcess.fxaa    = true;

    NkRenderer* renderer = NkRenderer::Create(device, surface, cfg);
    if (!renderer) {
        logger.Info("[Demo01] Échec NkRenderer::Create\n");
        return 1;
    }

    // ── Ressources ───────────────────────────────────────────────────────────
    auto* meshSys  = renderer->GetMeshSystem();
    auto* matSys   = renderer->GetMaterials();
    auto* texLib   = renderer->GetTextures();
    auto* r3d      = renderer->GetRender3D();
    auto* r2d      = renderer->GetRender2D();
    auto* txtR     = renderer->GetTextRenderer();
    auto* overlay  = renderer->GetOverlay();
    auto* pp       = renderer->GetPostProcess();

    NkMeshHandle meshSphere = meshSys->GetIcosphere();
    NkMeshHandle meshCube   = meshSys->GetCube();
    NkMeshHandle meshPlane  = meshSys->GetPlane();

    // Matériau or métallique
    NkMaterialInstance* matGold = matSys->CreateInstance(matSys->DefaultPBR());
    matGold->SetAlbedo({1.f,0.766f,0.336f})->SetMetallic(1.f)->SetRoughness(0.2f);

    // Matériau plastique rouge
    NkMaterialInstance* matRed = matSys->CreateInstance(matSys->DefaultPBR());
    matRed->SetAlbedo({0.8f,0.1f,0.1f})->SetMetallic(0.f)->SetRoughness(0.6f);

    // Sol pierre (fallback blanc si pas de texture)
    NkMaterialInstance* matFloor = matSys->CreateInstance(matSys->DefaultPBR());
    matFloor->SetAlbedo({0.5f,0.5f,0.5f})->SetRoughness(0.8f);
    NkTexHandle floorAlb = texLib->Load("assets/textures/stone_albedo.png");
    NkTexHandle floorNrm = texLib->Load("assets/textures/stone_normal.png");
    if(floorAlb.IsValid()) matFloor->SetAlbedoMap(floorAlb);
    if(floorNrm.IsValid()) matFloor->SetNormalMap(floorNrm);

    NkTexHandle envMap = texLib->LoadHDR("assets/hdri/studio_small_09_4k.hdr");
    NkFontHandle font  = txtR->GetDefaultFont();

    // ── État simulation ───────────────────────────────────────────────────────
    bool    running   = true;
    float32 rotAngle  = 0.f;
    float32 camYaw    = 0.f;
    float32 camPitch  = 20.f;
    float32 camDist   = 5.f;
    float32 lightYaw  = -45.f;
    float32 lightPitch= -30.f;
    float32 totalTime = 0.f;
    bool    keys[512] = {};
    int     nextDemo  = -1;

    NkClock clock;
    NkEventSystem& events = NkEvents();

    // ── Callbacks événements ──────────────────────────────────────────────────
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running=false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if((uint32)e->GetKey()<512) keys[(uint32)e->GetKey()]=true;
        if(e->GetKey()==NkKey::NK_ESCAPE) running=false;
        if(e->GetKey()==NkKey::NK_TAB)    nextDemo=2;
        for(int d=0;d<10;d++)
            if(e->GetKey()==(NkKey)(NkKey::NK_F1+d)) nextDemo=d;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e){
        if((uint32)e->GetKey()<512) keys[(uint32)e->GetKey()]=false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e){
        W=(uint32)e->GetWidth(); H=(uint32)e->GetHeight();
        if(renderer) renderer->OnResize(W,H);
    });

    // ── Boucle principale ─────────────────────────────────────────────────────
    while (running && nextDemo < 0) {
        events.PollEvents();
        if(!running) break;

        float32 dt = clock.Tick().delta;
        if(dt<=0.f||dt>0.25f) dt=1.f/60.f;
        totalTime += dt;

        // Contrôles
        const float32 camSpd=60.f, lightSpd=90.f;
        if(keys[(uint32)NkKey::NK_A]) camYaw   -=camSpd*dt;
        if(keys[(uint32)NkKey::NK_D]) camYaw   +=camSpd*dt;
        if(keys[(uint32)NkKey::NK_W]) camPitch +=camSpd*dt;
        if(keys[(uint32)NkKey::NK_S]) camPitch -=camSpd*dt;
        if(keys[(uint32)NkKey::NK_LEFT])  lightYaw  -=lightSpd*dt;
        if(keys[(uint32)NkKey::NK_RIGHT]) lightYaw  +=lightSpd*dt;
        if(keys[(uint32)NkKey::NK_UP])    lightPitch+=lightSpd*dt;
        if(keys[(uint32)NkKey::NK_DOWN])  lightPitch-=lightSpd*dt;
        camPitch = NkClamp(camPitch,-80.f,80.f);
        rotAngle += 45.f*dt;

        if(W==0||H==0) continue;

        // ── Caméra ───────────────────────────────────────────────────────────
        float32 ex=camDist*NkCos(NkToRadians(camPitch))*NkSin(NkToRadians(camYaw));
        float32 ey=camDist*NkSin(NkToRadians(camPitch));
        float32 ez=camDist*NkCos(NkToRadians(camPitch))*NkCos(NkToRadians(camYaw));

        NkCamera3DData camData;
        camData.position={ex,ey,ez}; camData.target={0,0,0};
        camData.fovY=65.f; camData.aspect=(float32)W/(float32)H;
        camData.nearPlane=0.1f; camData.farPlane=100.f;
        NkCamera3D cam(camData);

        // ── Lumières ──────────────────────────────────────────────────────────
        NkVector<NkLightDesc> lights;
        NkLightDesc sun; sun.type=NkLightType::NK_DIRECTIONAL;
        float32 lx=NkCos(NkToRadians(lightPitch))*NkSin(NkToRadians(lightYaw));
        float32 ly=NkSin(NkToRadians(lightPitch));
        float32 lz=NkCos(NkToRadians(lightPitch))*NkCos(NkToRadians(lightYaw));
        sun.direction={lx,ly,lz}; sun.color={1.f,0.95f,0.85f};
        sun.intensity=3.f; sun.castShadow=true;
        lights.PushBack(sun);

        NkLightDesc ptRed; ptRed.type=NkLightType::NK_POINT;
        ptRed.position={2.f,1.5f,0.f}; ptRed.color={1.f,0.2f,0.1f};
        ptRed.intensity=5.f; ptRed.range=6.f;
        lights.PushBack(ptRed);

        // ── Begin frame ──────────────────────────────────────────────────────
        if(!renderer->BeginFrame()) continue;

        // ── Scène 3D ─────────────────────────────────────────────────────────
        NkSceneContext ctx;
        ctx.camera=cam; ctx.lights=lights;
        ctx.envMap=envMap; ctx.ambientIntensity=0.15f; ctx.time=totalTime;
        r3d->BeginScene(ctx);

        // Sol
        NkDrawCall3D dcFloor;
        dcFloor.mesh=meshPlane; dcFloor.material={};
        dcFloor.transform=NkMat4f::Scale({10.f,1.f,10.f});
        dcFloor.aabb={{-5,0,-5},{5,0.01f,5}}; dcFloor.castShadow=false;
        r3d->Submit(dcFloor);

        // Grille de sphères 4×4
        for(int row=0;row<4;row++) for(int col=0;col<4;col++){
            float32 x=(col-1.5f)*1.2f, z=(row-1.5f)*1.2f;
            NkDrawCall3D dc;
            dc.mesh=meshSphere; dc.material={};
            dc.transform=NkMat4f::Translate({x,0.5f,z})*NkMat4f::Scale({0.45f,0.45f,0.45f});
            dc.aabb={{x-0.25f,0.25f,z-0.25f},{x+0.25f,0.75f,z+0.25f}};
            dc.tint={(float32)col/3.f,(float32)row/3.f,0.7f};
            r3d->Submit(dc);
        }

        // Cube central
        NkDrawCall3D dcCube;
        float32 y=0.5f+NkSin(totalTime*1.5f)*0.2f;
        dcCube.mesh=meshCube; dcCube.material={};
        dcCube.transform=NkMat4f::Translate({0,y,0})
                         *NkMat4f::RotateY(NkAngle(rotAngle))
                         *NkMat4f::Scale({0.6f,0.6f,0.6f});
        dcCube.aabb={{-0.35f,0.1f,-0.35f},{0.35f,0.9f,0.35f}};
        r3d->Submit(dcCube);

        // Gizmos debug
        r3d->DrawDebugAxes(NkMat4f::Identity(), 1.f);
        r3d->DrawDebugGrid({0,0,0}, 1.f, 20, {0.3f,0.3f,0.3f,1.f});

        // ── Post-process dynamique ────────────────────────────────────────────
        pp->GetConfig().exposure = 1.f + NkSin(totalTime*0.3f)*0.1f;

        // ── Overlay HUD ───────────────────────────────────────────────────────
        overlay->BeginOverlay(renderer->GetCmd(), W, H);
        overlay->DrawStats(renderer->GetStats());
        overlay->DrawText({10.f,30.f},
            "Demo 01 — PBR 3D | WASD=cam Fleches=lumiere TAB=suivant ESC=quitter");
        overlay->DrawText({10.f,50.f},
            "Backend: %s | %.1f FPS | Triangles: %u",
            NkGraphicsApiName(api),
            1.f/fmaxf(dt,0.001f),
            renderer->GetStats().triangles);
        overlay->EndOverlay();

        // ── End frame ─────────────────────────────────────────────────────────
        renderer->EndFrame();
        renderer->Present();
    }

    // ── Nettoyage ─────────────────────────────────────────────────────────────
    device->WaitIdle();
    if(matGold)  matSys->DestroyInstance(matGold);
    if(matRed)   matSys->DestroyInstance(matRed);
    if(matFloor) matSys->DestroyInstance(matFloor);
    NkRenderer::Destroy(renderer);

    logger.Info("[Demo01] Terminé proprement.\n");
    return (nextDemo>=0) ? nextDemo : 0;
}

// =============================================================================
// Demo 02 — 2D Game
// Sprites, tilemaps, texte SDF, UI 9-slice, formes géométriques, camera 2D
// =============================================================================
static int RunDemo02_Game2D(NkIDevice* device, NkWindow& window, NkGraphicsApi api) {
    logger.Info("[Demo02] Démarrage — 2D Game\n");

    uint32 W=(uint32)window.GetSize().width;
    uint32 H=(uint32)window.GetSize().height;
    NkSurfaceDesc surface = window.GetSurfaceDesc();

    NkRendererConfig cfg = NkRendererConfig::For2D(api, W, H);
    NkRenderer* renderer = NkRenderer::Create(device, surface, cfg);
    if(!renderer){ logger.Info("[Demo02] Échec NkRenderer::Create\n"); return 1; }

    auto* r2d    = renderer->GetRender2D();
    auto* txtR   = renderer->GetTextRenderer();
    auto* texLib = renderer->GetTextures();

    NkFontHandle fontBig  = txtR->LoadFont("assets/fonts/Roboto-Regular.ttf",  28.f, true);
    NkFontHandle fontSmall= txtR->LoadFont("assets/fonts/Roboto-Regular.ttf",  16.f, true);
    if(!fontBig.IsValid())   fontBig  = txtR->GetDefaultFont();
    if(!fontSmall.IsValid()) fontSmall= txtR->GetDefaultFont();

    NkTexHandle texSprite = texLib->Load("assets/sprites/player.png");
    NkTexHandle texTile   = texLib->Load("assets/sprites/tileset.png");
    NkTexHandle texPanel  = texLib->Load("assets/ui/panel.png");

    bool    running     = true;
    float32 spriteX     = 100.f;
    float32 totalTime   = 0.f;
    int32   score       = 0;
    int     nextDemo    = -1;
    bool    keys[512]   = {};
    NkClock clock;
    NkEventSystem& events = NkEvents();

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*){ running=false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e){
        if((uint32)e->GetKey()<512) keys[(uint32)e->GetKey()]=true;
        if(e->GetKey()==NkKey::NK_ESCAPE) running=false;
        if(e->GetKey()==NkKey::NK_TAB)    nextDemo=3;
        for(int d=0;d<10;d++) if(e->GetKey()==(NkKey)(NkKey::NK_F1+d)) nextDemo=d;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e){
        if((uint32)e->GetKey()<512) keys[(uint32)e->GetKey()]=false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e){
        W=(uint32)e->GetWidth(); H=(uint32)e->GetHeight();
        if(renderer) renderer->OnResize(W,H);
    });

    while(running && nextDemo<0){
        events.PollEvents();
        float32 dt=clock.Tick().delta;
        if(dt<=0.f||dt>0.25f) dt=1.f/60.f;
        totalTime+=dt;

        spriteX+=80.f*dt;
        if(spriteX>(float32)W+64.f) spriteX=-64.f;
        score=(int32)(totalTime*10.f);

        if(W==0||H==0) continue;
        if(!renderer->BeginFrame()) continue;

        auto* cmd=renderer->GetCmd();
        r2d->Begin(cmd, W, H);

        float32 fw=(float32)W, fh=(float32)H;

        // Fond dégradé ciel
        r2d->FillRectGradV({0,0,fw,fh*0.6f},{0.3f,0.5f,0.9f,1.f},{0.6f,0.8f,1.f,1.f});
        // Sol herbeux
        r2d->FillRectGradV({0,fh*0.6f,fw,fh*0.4f},{0.2f,0.7f,0.2f,1.f},{0.1f,0.4f,0.1f,1.f});

        // Nuages
        for(int c=0;c<5;c++){
            float32 cx=(totalTime*25.f+c*220.f);
            float32 rem=(fw+200.f); cx=cx-(int32)(cx/rem)*rem-100.f;
            r2d->FillCircle({cx,(float32)(50+c*30)},40.f+c*10.f,{1,1,1,0.6f});
            r2d->FillCircle({cx+30.f,(float32)(40+c*30)},30.f,{1,1,1,0.6f});
        }

        // Tilemap
        for(int ty=0;ty<3;ty++) for(int tx=0;tx<(int)(fw/64)+2;tx++){
            float32 tx2=(float32)(tx*64)-(int32)(totalTime*40)%64;
            float32 ty2=fh*0.6f-64.f+ty*64.f;
            float32 tu=(float32)(ty%4)*0.25f;
            r2d->DrawSprite({tx2,ty2,64,64}, texTile, {1,1,1,1}, {tu,0,0.25f,0.25f});
        }

        // Sprite joueur (bob)
        float32 bobY=NkSin(totalTime*8.f)*4.f;
        r2d->DrawSpriteRotated({spriteX-32.f,fh*0.6f-96.f+bobY,64,64},
                                 texSprite, NkSin(totalTime*2.f)*5.f);

        // Panel UI 9-slice
        float32 pw=220.f,ph=88.f,px=fw-pw-20.f,py=20.f;
        if(texPanel.IsValid())
            r2d->DrawNineSlice({px,py,pw,ph},texPanel,16,16,16,16,{1,1,1,0.9f});
        else
            r2d->FillRoundRect({px,py,pw,ph},{0.1f,0.1f,0.2f,0.85f},8.f);

        // Score
        char scoreBuf[64];
        snprintf(scoreBuf,64,"SCORE: %06d",score);
        txtR->DrawText({px+16.f,py+12.f},scoreBuf,fontBig,22.f,0xFFFFFFFF);
        txtR->DrawText({px+16.f,py+48.f},"LEVEL 1",fontSmall,16.f,0xFFDD44FF);

        // Titre
        txtR->DrawTextCentered({0,0,fw,50.f},"NKRenderer v4 — Demo 02 : 2D Game",
                                fontSmall,18.f,0xFFFFFFFF);

        // Formes debug
        r2d->FillRoundRect({20.f,fh-90.f,210.f,70.f},{0,0,0,0.5f},6.f);
        r2d->DrawCircle({120.f,fh-55.f},20.f+NkSin(totalTime*3)*5.f,
                          {0.f,1.f,0.5f,1.f},2.f);
        r2d->DrawBezier({30.f,fh-44.f},{80.f,fh-84.f},{140.f,fh-24.f},{200.f,fh-64.f},
                          {1.f,0.5f,0.f,1.f},3.f);
        r2d->DrawArc({340.f,fh-50.f},30.f,0.f,270.f,{0.8f,0.2f,1.f,1.f},2.f);

        r2d->End();

        renderer->EndFrame();
        renderer->Present();
    }

    device->WaitIdle();
    NkRenderer::Destroy(renderer);
    logger.Info("[Demo02] Terminé proprement.\n");
    return (nextDemo>=0)?nextDemo:0;
}

// =============================================================================
// Demo 05 — Animation System généraliste
// Bones + UV scroll + sprite flipbook + couleur animée + camera animée
// =============================================================================
static int RunDemo05_Animation(NkIDevice* device, NkWindow& window, NkGraphicsApi api) {
    logger.Info("[Demo05] Démarrage — Animation System\n");

    uint32 W=(uint32)window.GetSize().width;
    uint32 H=(uint32)window.GetSize().height;
    NkSurfaceDesc surface = window.GetSurfaceDesc();

    NkRendererConfig cfg = NkRendererConfig::ForEditor(api, W, H);
    NkRenderer* renderer = NkRenderer::Create(device, surface, cfg);
    if(!renderer){ logger.Info("[Demo05] Échec\n"); return 1; }

    auto* animSys  = renderer->GetAnimation();
    auto* r3d      = renderer->GetRender3D();
    auto* r2d      = renderer->GetRender2D();
    auto* txtR     = renderer->GetTextRenderer();
    auto* meshSys  = renderer->GetMeshSystem();
    auto* matSys   = renderer->GetMaterials();
    auto* texLib   = renderer->GetTextures();
    auto* overlay  = renderer->GetOverlay();

    NkFontHandle font = txtR->GetDefaultFont();

    // ── Clips d'animation ─────────────────────────────────────────────────────

    // Clip 1 : Rotation objet
    NkAnimationClip* clipSpin = NkAnimationClip::MakeSpinClip("Spin", 30.f);

    // Clip 2 : Pulsation couleur
    NkAnimationClip* clipColor = NkAnimationClip::MakeColorFade(
        "ColorPulse",
        {0.2f,0.4f,0.9f,1.f}, {1.f,0.3f,0.1f,1.f},
        2.f, NkInterpMode::NK_EASE_IN_OUT);

    // Clip 3 : Skeletal procédural
    NkAnimationClip* clipWalk = NkAnimationClip::MakeProceduralWalk(8, 1.5f);

    // Clip 4 : Caméra animée (dolly)
    NkAnimationClip* clipCam = animSys->CreateClip("CameraDolly");
    clipCam->cameraPosition.AddKey(0.f,  {0,1.5f,4.f}, NkInterpMode::NK_EASE_IN_OUT);
    clipCam->cameraPosition.AddKey(2.f,  {2,2.f, 3.f}, NkInterpMode::NK_EASE_IN_OUT);
    clipCam->cameraPosition.AddKey(4.f,  {-2,1.f,3.f}, NkInterpMode::NK_EASE_IN_OUT);
    clipCam->cameraPosition.AddKey(6.f,  {0,1.5f,4.f}, NkInterpMode::NK_EASE_IN_OUT);
    clipCam->cameraTarget.AddKey(0.f, {0,0.5f,0});
    clipCam->cameraFOV.AddKey(0.f, 65.f); clipCam->cameraFOV.AddKey(3.f, 80.f);
    clipCam->cameraFOV.AddKey(6.f, 65.f);
    clipCam->RecalcDuration();

    // Clip 5 : Sprite flipbook (16 frames d'explosion)
    NkAnimationClip* clipSprite = animSys->CreateClip("Explosion");
    clipSprite->spriteAtlasCols=4; clipSprite->spriteAtlasRows=4;
    clipSprite->BuildSpriteFlipBook(16, 8.f);

    // Clip 6 : UV scrolling (lave)
    NkAnimationClip* clipUV = animSys->CreateClip("LavaFlow");
    clipUV->uvOffset.AddKey(0.f,  {0.f,0.f},   NkInterpMode::NK_LINEAR);
    clipUV->uvOffset.AddKey(10.f, {2.f,0.5f},  NkInterpMode::NK_LINEAR);
    clipUV->RecalcDuration();

    // Clip 7 : Pulsation lumière
    NkAnimationClip* clipLight = NkAnimationClip::MakeLightPulse("FireFlicker",0.5f,3.f,8.f);

    // ── Players ───────────────────────────────────────────────────────────────
    NkAnimationPlayer* playerSpin   = animSys->CreatePlayer("Spin");
    NkAnimationPlayer* playerColor  = animSys->CreatePlayer("Color");
    NkAnimationPlayer* playerWalk   = animSys->CreatePlayer("Walk");
    NkAnimationPlayer* playerCam    = animSys->CreatePlayer("Camera");
    NkAnimationPlayer* playerSprite = animSys->CreatePlayer("Sprite");
    NkAnimationPlayer* playerUV     = animSys->CreatePlayer("UV");
    NkAnimationPlayer* playerLight  = animSys->CreatePlayer("Light");

    playerSpin  ->SetClip(clipSpin);    playerSpin  ->Play(NkPlayMode::NK_LOOP);
    playerColor ->SetClip(clipColor);   playerColor ->Play(NkPlayMode::NK_PING_PONG);
    playerWalk  ->SetClip(clipWalk);    playerWalk  ->Play(NkPlayMode::NK_LOOP);
    playerCam   ->SetClip(clipCam);     playerCam   ->Play(NkPlayMode::NK_LOOP);
    playerSprite->SetClip(clipSprite);  playerSprite->Play(NkPlayMode::NK_LOOP);
    playerUV    ->SetClip(clipUV);      playerUV    ->Play(NkPlayMode::NK_LOOP);
    playerLight ->SetClip(clipLight);   playerLight ->Play(NkPlayMode::NK_LOOP);

    // ── Meshes & matériaux ────────────────────────────────────────────────────
    NkMeshHandle meshCube   = meshSys->GetCube();
    NkMeshHandle meshSphere = meshSys->GetSphere();
    NkMeshHandle meshChar   = meshSys->Import("assets/models/character_skinned.glb");
    if(!meshChar.IsValid()) meshChar=meshSys->GetCapsule();

    NkMaterialInstance* matObj  = matSys->CreateInstance(matSys->DefaultPBR());
    NkMaterialInstance* matChar = matSys->CreateInstance(matSys->DefaultPBR());
    matChar->SetAlbedo({0.2f,0.4f,0.8f})->SetRoughness(0.6f);

    NkTexHandle texExplosion = texLib->Load("assets/vfx/explosion_atlas.png");
    NkTexHandle texLava      = texLib->Load("assets/textures/lava_albedo.png");

    // Squelette (hiérarchie simple 8 os)
    int32 boneParents[8]={-1,0,1,1,1,3,4,0};

    // ── State ─────────────────────────────────────────────────────────────────
    bool    running  = true;
    int     nextDemo = -1;
    bool    showOnion= true;
    bool    showSkel = true;
    bool    showSprite=true;
    bool    keys[512]= {};
    NkClock clock;
    NkEventSystem& events=NkEvents();

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*){ running=false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e){
        if((uint32)e->GetKey()<512) keys[(uint32)e->GetKey()]=true;
        if(e->GetKey()==NkKey::NK_ESCAPE) running=false;
        if(e->GetKey()==NkKey::NK_TAB)    nextDemo=6;
        if(e->GetKey()==NkKey::NK_O)      showOnion=!showOnion;
        if(e->GetKey()==NkKey::NK_J)      showSkel =!showSkel;
        if(e->GetKey()==NkKey::NK_P)      showSprite=!showSprite;
        for(int d=0;d<10;d++) if(e->GetKey()==(NkKey)(NkKey::NK_F1+d)) nextDemo=d;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e){
        if((uint32)e->GetKey()<512) keys[(uint32)e->GetKey()]=false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e){
        W=(uint32)e->GetWidth(); H=(uint32)e->GetHeight();
        if(renderer) renderer->OnResize(W,H);
    });

    while(running && nextDemo<0){
        events.PollEvents();
        float32 dt=clock.Tick().delta;
        if(dt<=0.f||dt>0.25f) dt=1.f/60.f;

        if(W==0||H==0) continue;

        // ── Update animation ──────────────────────────────────────────────────
        animSys->Update(dt);

        if(!renderer->BeginFrame()) continue;
        auto* cmd=renderer->GetCmd();

        // ── Caméra animée ─────────────────────────────────────────────────────
        NkCamera3D cam;
        animSys->ApplyCamera(cam, *playerCam);
        cam.SetAspect(W,H);

        // ── Lumière animée ────────────────────────────────────────────────────
        NkLightDesc fireLight; fireLight.type=NkLightType::NK_POINT;
        fireLight.position={0,1.f,0};
        animSys->ApplyLight(fireLight, *playerLight);

        NkLightDesc sun; sun.type=NkLightType::NK_DIRECTIONAL;
        sun.direction={-0.5f,-1.f,-0.3f}; sun.color={1,1,1}; sun.intensity=2.f;

        NkVector<NkLightDesc> lights; lights.PushBack(sun); lights.PushBack(fireLight);

        NkSceneContext ctx;
        ctx.camera=cam; ctx.lights=lights; ctx.time=0.f;
        r3d->BeginScene(ctx);

        // ── Objet 1 : cube avec rotation + couleur animées ────────────────────
        NkMat4f worldCube=animSys->ApplyTransform(*playerSpin,NkMat4f::Translate({-2,0.5f,0}));
        animSys->ApplyMaterial(matObj, *playerColor);
        NkDrawCall3D dc1; dc1.mesh=meshCube; dc1.material={};
        dc1.transform=worldCube; dc1.aabb={{-0.6f,0,-0.6f},{0.6f,1.f,0.6f}};
        r3d->Submit(dc1);

        // ── Objet 2 : sphère avec UV scroll (lava) ───────────────────────────
        NkVec4f spriteUV=animSys->GetAnimatedSpriteUV(*playerUV);
        NkDrawCall3D dc2; dc2.mesh=meshSphere; dc2.material={};
        dc2.transform=NkMat4f::Translate({0,0.5f,0}); dc2.aabb={{-0.6f,0,-0.6f},{0.6f,1.f,0.6f}};
        r3d->Submit(dc2);

        // ── Objet 3 : personnage skinné avec onion skin ───────────────────────
        if(showOnion){
            int32 offsets[6]={-3,-2,-1,1,2,3};
            animSys->ApplyOnionSkin(meshChar,{},NkMat4f::Translate({2,0,0}),
                                     *playerWalk, offsets,6);
        } else {
            animSys->ApplySkinnedMesh(meshChar,{},NkMat4f::Translate({2,0,0}),
                                       *playerWalk);
        }

        // ── Squelette debug ───────────────────────────────────────────────────
        if(showSkel)
            animSys->DrawSkeleton(*playerWalk, NkMat4f::Translate({2,0,0}),
                                   boneParents, {1.f,0.8f,0.f,1.f}, 0.04f);

        // Grille
        r3d->DrawDebugAxes(NkMat4f::Identity(), 0.5f);
        r3d->DrawDebugGrid({0,0,0}, 0.5f, 14, {0.25f,0.25f,0.25f,1.f});

        // ── Sprite flip-book en 2D ────────────────────────────────────────────
        if(showSprite){
            r2d->Begin(cmd, W, H);
            const auto& ss=playerSprite->GetState();
            if(texExplosion.IsValid())
                r2d->DrawSprite({10.f,(float32)H-150.f,128,128},
                                  texExplosion,{1,1,1,1},
                                  {ss.spriteUV.x,ss.spriteUV.y,
                                   ss.spriteUV.z-ss.spriteUV.x,
                                   ss.spriteUV.w-ss.spriteUV.y});
            else {
                // Fake flipbook avec couleur
                float32 hue=ss.spriteFrame*22.5f/360.f;
                r2d->FillRoundRect({10.f,(float32)H-150.f,128,128},
                                     {hue,1.f-hue,0.5f,1.f},8.f);
                char fbuf[32]; snprintf(fbuf,32,"Frame %d",ss.spriteFrame);
                txtR->DrawText({16.f,(float32)H-90.f},fbuf,font,16.f,0xFFFFFFFF);
            }
            r2d->End();
        }

        // ── Overlay ───────────────────────────────────────────────────────────
        overlay->BeginOverlay(cmd, W, H);
        overlay->DrawStats(renderer->GetStats());
        overlay->DrawText({10,10},"Demo 05 — Animation System Generaliste");
        overlay->DrawText({10,30},"O=onion skin J=squelette P=sprite | TAB=suivant");
        overlay->DrawText({10,50},
            "Spin:%.1fs Color:%.2f Walk:F%d Cam:%.1fs Light:%.2f UV:%.2f,%.2f",
            playerSpin->GetTime(),
            playerColor->GetState().albedo.x,
            playerWalk->GetFrame(),
            playerCam->GetTime(),
            playerLight->GetState().lightIntensity,
            playerUV->GetState().uvOffset.x,
            playerUV->GetState().uvOffset.y);
        overlay->EndOverlay();

        renderer->EndFrame();
        renderer->Present();
    }

    // ── Nettoyage ─────────────────────────────────────────────────────────────
    device->WaitIdle();
    if(matObj)  matSys->DestroyInstance(matObj);
    if(matChar) matSys->DestroyInstance(matChar);
    // Les clips alloués par MakeXxx doivent être détruits manuellement
    delete clipSpin; delete clipColor; delete clipWalk;
    delete clipLight;
    NkRenderer::Destroy(renderer);

    logger.Info("[Demo05] Terminé proprement.\n");
    return (nextDemo>=0)?nextDemo:0;
}

// =============================================================================
// Demo 08 — PV3DE (Patient Virtuel 3D Émotif)
// Utilise NkAnimationSystem pour animer les paramètres peau + caméra + lumière
// =============================================================================
static int RunDemo08_PV3DE(NkIDevice* device, NkWindow& window, NkGraphicsApi api) {
    logger.Info("[Demo08] Démarrage — PV3DE\n");

    uint32 W=(uint32)window.GetSize().width;
    uint32 H=(uint32)window.GetSize().height;
    NkSurfaceDesc surface=window.GetSurfaceDesc();

    NkRendererConfig cfg=NkRendererConfig::ForFilm(api,W,H);
    cfg.width=W; cfg.height=H;
    cfg.postProcess.ssao=true;
    cfg.postProcess.bloom=true;
    cfg.postProcess.dof=false;

    NkRenderer* renderer=NkRenderer::Create(device,surface,cfg);
    if(!renderer){ logger.Info("[Demo08] Échec\n"); return 1; }

    auto* animSys = renderer->GetAnimation();
    auto* sim     = renderer->GetSimulation();
    auto* r3d     = renderer->GetRender3D();
    auto* r2d     = renderer->GetRender2D();
    auto* txtR    = renderer->GetTextRenderer();
    auto* meshSys = renderer->GetMeshSystem();
    auto* matSys  = renderer->GetMaterials();
    auto* texLib  = renderer->GetTextures();
    auto* vfx     = renderer->GetVFX();
    auto* overlay = renderer->GetOverlay();

    NkFontHandle font=txtR->GetDefaultFont();

    // ── Clips émotionnels (chacun anime des paramètres matériau + post-process) ─
    auto makeEmotionClip = [&](const char* name, NkVec4f albedo, float32 emissI,
                                 NkVec3f emissColor, float32 bloom,
                                 float32 exposure, float32 duration) {
        NkAnimationClip* c=animSys->CreateClip(name);
        c->albedoColor.AddKey(0.f, albedo, NkInterpMode::NK_EASE_IN_OUT);
        c->albedoColor.AddKey(duration, albedo, NkInterpMode::NK_EASE_IN_OUT);
        c->emissiveStrength.AddKey(0.f, emissI); c->emissiveStrength.AddKey(duration, emissI);
        NkVec4f ec4={emissColor.x,emissColor.y,emissColor.z,1.f};
        c->emissiveColor.AddKey(0.f, {ec4.x,ec4.y,ec4.z}); c->emissiveColor.AddKey(duration, {ec4.x,ec4.y,ec4.z});
        c->ppBloomStrength.AddKey(0.f, bloom); c->ppBloomStrength.AddKey(duration, bloom);
        c->ppExposure.AddKey(0.f, exposure); c->ppExposure.AddKey(duration, exposure);
        c->duration=duration;
        return c;
    };

    NkAnimationClip* clipNeutre  = makeEmotionClip("Neutre",  {0.88f,0.65f,0.5f,1.f},0.f,{0,0,0},0.04f,1.f,3.f);
    NkAnimationClip* clipJoie    = makeEmotionClip("Joie",    {0.95f,0.6f,0.55f,1.f},0.2f,{1,.5f,.2f},0.08f,1.2f,3.f);
    NkAnimationClip* clipTristesse=makeEmotionClip("Triste",  {0.8f,0.68f,0.6f,1.f},0.f,{0,0,0},0.02f,0.8f,3.f);
    NkAnimationClip* clipColere  = makeEmotionClip("Colere",  {0.96f,0.45f,0.4f,1.f},0.1f,{1,.1f,.1f},0.06f,1.1f,3.f);
    NkAnimationClip* clipPeur    = makeEmotionClip("Peur",    {0.8f,0.75f,0.78f,1.f},0.05f,{0.5f,0.5f,1.f},0.03f,0.7f,3.f);

    // Clip caméra qui tourne autour du personnage
    NkAnimationClip* clipCam=animSys->CreateClip("CamOrbit");
    for(int i=0;i<=32;i++){
        float32 t=(float32)i/32*12.f;
        float32 a=t/12.f*2.f*3.14159f;
        clipCam->cameraPosition.AddKey(t,{NkCos(NkAngle(a*180.f/3.14159f))*0.8f,1.6f,NkSin(NkAngle(a*180.f/3.14159f))*0.8f});
        clipCam->cameraTarget.AddKey(t,{0,1.5f,0});
        clipCam->cameraFOV.AddKey(t,50.f+NkSin(NkAngle(a*90.f))*5.f);
    }
    clipCam->RecalcDuration();

    NkAnimationPlayer* playerSkin = animSys->CreatePlayer("Skin");
    NkAnimationPlayer* playerCam  = animSys->CreatePlayer("Cam");
    playerSkin->SetClip(clipNeutre); playerSkin->Play(NkPlayMode::NK_LOOP);
    playerCam->SetClip(clipCam);     playerCam->Play(NkPlayMode::NK_LOOP);

    // Squelette tête simplifié
    NkAnimationClip* clipWalk=NkAnimationClip::MakeProceduralWalk(4,2.f);
    NkAnimationPlayer* playerBones=animSys->CreatePlayer("Bones");
    playerBones->SetClip(clipWalk); playerBones->Play(NkPlayMode::NK_LOOP);

    NkMeshHandle meshHead=meshSys->Import("assets/models/head_hires.glb");
    if(!meshHead.IsValid()) meshHead=meshSys->GetIcosphere();
    NkMeshHandle meshBody=meshSys->Import("assets/models/body.glb");
    if(!meshBody.IsValid()) meshBody=meshSys->GetCapsule();

    NkMaterialInstance* matSkin=matSys->CreateInstance(matSys->DefaultSkin());
    matSkin->SetAlbedo({0.88f,0.65f,0.5f})->SetSubsurface(0.45f,{1.f,0.4f,0.25f})->SetRoughness(0.55f);

    // Émetteur larmes (off par défaut)
    NkEmitterDesc tearDesc;
    tearDesc.position={-0.03f,1.55f,0.08f}; tearDesc.ratePerSec=0.f;
    tearDesc.lifeMin=0.3f; tearDesc.lifeMax=0.6f;
    tearDesc.speedMin=0.02f; tearDesc.speedMax=0.05f;
    tearDesc.velocityDir={0,-1,0}; tearDesc.gravity={0,-3.f,0};
    tearDesc.sizeStart=0.004f; tearDesc.sizeEnd=0.001f;
    tearDesc.colorStart={0.7f,0.85f,1.f,0.9f}; tearDesc.colorEnd={0.7f,0.85f,1.f,0.f};
    tearDesc.maxParticles=40; tearDesc.blend=NkBlendMode::NK_ALPHA;
    NkEmitterId tearEmitter=vfx->CreateEmitter(tearDesc);

    // Séquence d'émotions
    NkAnimationClip* emotionClips[]={clipNeutre,clipJoie,clipTristesse,clipColere,clipPeur};
    const char* emotionNames[]={"Neutre","Joie","Tristesse","Colère","Peur"};
    int emotionIdx=0;
    float32 emotionTimer=0.f;
    const float32 emotionDur=4.f;

    bool    running  =true;
    int     nextDemo =-1;
    bool    keys[512]={};
    float32 totalTime=0.f;
    NkClock clock;
    NkEventSystem& events=NkEvents();

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*){ running=false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e){
        if((uint32)e->GetKey()<512) keys[(uint32)e->GetKey()]=true;
        if(e->GetKey()==NkKey::NK_ESCAPE) running=false;
        if(e->GetKey()==NkKey::NK_TAB)    nextDemo=9;
        // Changer d'émotion manuellement
        if(e->GetKey()==NkKey::NK_1){emotionIdx=0;playerSkin->SetClip(clipNeutre);   playerSkin->Play();}
        if(e->GetKey()==NkKey::NK_2){emotionIdx=1;playerSkin->SetClip(clipJoie);     playerSkin->Play();}
        if(e->GetKey()==NkKey::NK_3){emotionIdx=2;playerSkin->SetClip(clipTristesse);playerSkin->Play();}
        if(e->GetKey()==NkKey::NK_4){emotionIdx=3;playerSkin->SetClip(clipColere);   playerSkin->Play();}
        if(e->GetKey()==NkKey::NK_5){emotionIdx=4;playerSkin->SetClip(clipPeur);     playerSkin->Play();}
        for(int d=0;d<10;d++) if(e->GetKey()==(NkKey)(NkKey::NK_F1+d)) nextDemo=d;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e){
        if((uint32)e->GetKey()<512) keys[(uint32)e->GetKey()]=false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e){
        W=(uint32)e->GetWidth(); H=(uint32)e->GetHeight();
        if(renderer) renderer->OnResize(W,H);
    });

    while(running && nextDemo<0){
        events.PollEvents();
        float32 dt=clock.Tick().delta;
        if(dt<=0.f||dt>0.25f) dt=1.f/60.f;
        totalTime+=dt;

        if(W==0||H==0) continue;

        // ── Séquence auto ─────────────────────────────────────────────────────
        emotionTimer+=dt;
        if(emotionTimer>=emotionDur){
            emotionTimer=0.f;
            emotionIdx=(emotionIdx+1)%5;
            playerSkin->BlendTo(emotionClips[emotionIdx], 0.5f);
        }

        // Larmes si tristesse
        if(emotionIdx==2){
            if(auto* d=vfx->GetEmitterDesc(tearEmitter)) d->ratePerSec=8.f;
        } else {
            if(auto* d=vfx->GetEmitterDesc(tearEmitter)) d->ratePerSec=0.f;
        }

        NkCamera3DData camDummy; camDummy.position={0,1.5f,0.6f}; camDummy.target={0,1.5f,0};
        vfx->Update(dt, camDummy);
        animSys->Update(dt);

        if(!renderer->BeginFrame()) continue;
        auto* cmd=renderer->GetCmd();

        // ── Caméra depuis animation ───────────────────────────────────────────
        NkCamera3D cam;
        animSys->ApplyCamera(cam, *playerCam);
        cam.SetAspect(W,H);

        // ── Éclairage studio animé ────────────────────────────────────────────
        NkLightDesc key; key.type=NkLightType::NK_DIRECTIONAL;
        key.direction={-0.5f,-0.8f,-0.3f}; key.color={1.f,0.98f,0.95f}; key.intensity=4.f;
        NkLightDesc fill; fill.type=NkLightType::NK_POINT;
        fill.position={-1.f,2.f,0.5f}; fill.color={0.5f,0.6f,0.9f};
        fill.intensity=1.5f+NkSin(NkAngle(totalTime*90.f))*0.2f;
        fill.range=4.f;

        NkVector<NkLightDesc> lights; lights.PushBack(key); lights.PushBack(fill);
        NkSceneContext ctx; ctx.camera=cam; ctx.lights=lights; ctx.time=totalTime;
        r3d->BeginScene(ctx);

        // Appliquer matériau animé à la peau
        animSys->ApplyMaterial(matSkin, *playerSkin);

        // Appliquer post-process animé
        animSys->ApplyPostProcess(renderer->GetPostProcess()->GetConfig(), *playerSkin);

        // Soumettre personnage via NkSimulationRenderer
        NkBlendShapeState bs{}; NkEmotionState em{};
        if(emotionIdx==1){em.joy=0.8f;bs.blush=0.3f;}
        else if(emotionIdx==2){em.sadness=0.9f;bs.tears=0.7f;}
        else if(emotionIdx==3){em.anger=0.9f;bs.blush=0.5f;}
        else if(emotionIdx==4){em.fear=0.9f;bs.pallor=0.5f;}

        const auto& boneState=playerBones->GetState();
        sim->SubmitCharacter(meshHead,{},NkMat4f::Identity(),
                              boneState.boneMatrices.Empty()?nullptr:boneState.boneMatrices.Data(),
                              (uint32)boneState.boneMatrices.Size(), bs, em);

        NkDrawCall3D dcBody; dcBody.mesh=meshBody;
        dcBody.transform=NkMat4f::Translate({0,-0.5f,0});
        dcBody.aabb={{-0.3f,-0.5f,-0.2f},{0.3f,1.f,0.2f}};
        r3d->Submit(dcBody);

        vfx->Render(cmd, camDummy);

        // ── UI barres émotions ────────────────────────────────────────────────
        r2d->Begin(cmd, W, H);
        float32 bx=10.f, by=70.f;
        const float32 bw=160.f, bh=18.f;

        auto drawBar=[&](const char* label, float32 val, NkVec4f color){
            r2d->FillRect({bx,by,bw,bh},{0.08f,0.08f,0.08f,0.75f});
            r2d->FillRect({bx,by,bw*NkClamp(val,0.f,1.f),bh},color);
            r2d->DrawRect({bx,by,bw,bh},{0.4f,0.4f,0.4f,1.f});
            by+=bh+2.f;
        };
        r2d->FillRoundRect({5.f,55.f,172.f,170.f},{0,0,0,0.65f},6.f);
        drawBar("Joie",     em.joy,     {1.f,0.9f,0.2f,1.f});
        drawBar("Tristesse",em.sadness, {0.3f,0.4f,1.f,1.f});
        drawBar("Peur",     em.fear,    {0.7f,0.3f,1.f,1.f});
        drawBar("Colere",   em.anger,   {1.f,0.2f,0.1f,1.f});
        drawBar("Blush",    bs.blush,   {1.f,0.5f,0.5f,1.f});
        drawBar("Pallor",   bs.pallor,  {0.8f,0.9f,1.f,1.f});
        drawBar("Larmes",   bs.tears,   {0.6f,0.8f,1.f,1.f});
        r2d->End();

        overlay->BeginOverlay(cmd, W, H);
        overlay->DrawStats(renderer->GetStats());
        overlay->DrawText({10,10},"Demo 08 — PV3DE: Patient Virtuel 3D Emotif");
        overlay->DrawText({10,30},"1-5=emotion | TAB=suivant | ESC=quitter");
        overlay->DrawText({10,50},"Emotion: %s | Blend: %s",
                           emotionNames[emotionIdx],
                           playerSkin->IsBlending()?"OUI":"NON");
        overlay->EndOverlay();

        renderer->EndFrame();
        renderer->Present();
    }

    device->WaitIdle();
    vfx->DestroyEmitter(tearEmitter);
    if(matSkin) matSys->DestroyInstance(matSkin);
    NkRenderer::Destroy(renderer);
    logger.Info("[Demo08] Terminé proprement.\n");
    return (nextDemo>=0)?nextDemo:0;
}

// =============================================================================
// nkmain — point d'entrée unifié
// =============================================================================
int nkmain(const NkEntryState& state) {

    // ── Backend & démo ─────────────────────────────────────────────────────────
    NkGraphicsApi api = ParseBackend(state.GetArgs());
    int demoIdx       = ParseDemo(state.GetArgs());
    const char* apiName = NkGraphicsApiName(api);

    logger.Info("=============================================================\n");
    logger.Info(" NKRenderer v4.0 — Demo {0} — Backend: {1}\n", demoIdx, apiName);
    logger.Info(" F1-F10=changer demo | TAB=demo suivante | ESC=quitter\n");
    logger.Info("=============================================================\n");

    // ── Fenêtre ────────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title     = NkFormat("NKRenderer v4.0 — Demo {0} — {1}", demoIdx, apiName);
    winCfg.width     = 1280;
    winCfg.height    = 720;
    winCfg.centered  = true;
    winCfg.resizable = true;

    NkWindow window;
    if(!window.Create(winCfg)){
        logger.Info("[Main] Échec création fenêtre\n");
        return 1;
    }

    // ── Device RHI ────────────────────────────────────────────────────────────
    NkSurfaceDesc surface = window.GetSurfaceDesc();
    NkDeviceInitInfo devInfo;
    devInfo.api             = api;
    devInfo.surface         = surface;
    devInfo.width           = (uint32)window.GetSize().width;
    devInfo.height          = (uint32)window.GetSize().height;
    devInfo.context.vulkan.appName    = "NKRenderer_Demo";
    devInfo.context.vulkan.engineName = "Unkeny";

    NkIDevice* device = NkDeviceFactory::Create(devInfo);
    if(!device||!device->IsValid()){
        logger.Info("[Main] Échec NkDeviceFactory::Create ({0})\n", apiName);
        window.Close();
        return 1;
    }
    logger.Info("[Main] Device {0} créé. VRAM: {1} Mo\n",
                apiName, (uint64)(device->GetCaps().vramBytes>>20));

    // ── Sélection + boucle de démo ────────────────────────────────────────────
    int nextDemo = demoIdx;
    while(nextDemo >= 0){
        // Mettre à jour le titre
        char title[128];
        snprintf(title,128,"NKRenderer v4.0 — Demo %d — %s", nextDemo, apiName);
        window.SetTitle(title);

        int result = 0;
        switch(nextDemo){
            case 0: case 1:  result=RunDemo01_Basic3D(device,window,api);  break;
            case 2:          result=RunDemo02_Game2D(device,window,api);   break;
            case 4:          result=RunDemo05_Animation(device,window,api);break;
            case 7:          result=RunDemo08_PV3DE(device,window,api);    break;
            default:         result=RunDemo01_Basic3D(device,window,api);  break;
        }

        // Si result == -1 → fermeture totale, sinon changer de démo
        if(result<=0||result==nextDemo) break;
        nextDemo=result;
    }

    // ── Nettoyage final ───────────────────────────────────────────────────────
    device->WaitIdle();
    NkDeviceFactory::Destroy(device);
    window.Close();

    logger.Info("[Main] Terminé proprement.\n");
    return 0;
}
