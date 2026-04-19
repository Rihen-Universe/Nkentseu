// =============================================================================
// Example_MultiStyle.cpp
// Exemples variés : Anime/Toon, Rendu 2D pur, Offscreen RTT, Archviz
// =============================================================================
#include "Core/NkRenderer.h"
#include "Materials/NkMaterialSystem.h"
#include "Tools/Offscreen/NkOffscreenTarget.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;

// =============================================================================
// EXEMPLE 1 : Rendu Anime / Cel-shading (style manga/UPBGE EEVEE)
// =============================================================================
struct AnimeScene {
    NkRenderer* renderer = nullptr;

    void Init(NkIDevice* dev, uint32 w, uint32 h) {
        // Config orientée anime : pas de SSAO, pas de bloom physique,
        // FXAA pour lisser les outlines
        NkRendererConfig cfg = NkRendererConfig::ForGame(dev->GetApi(), w, h);
        cfg.postProcess.ssao = false;
        cfg.postProcess.bloom = false;
        cfg.postProcess.fxaa  = true;
        cfg.shadow.enabled    = true;
        cfg.shadow.softShadows= false;  // ombres dures = style toon

        renderer = NkRenderer::Create(dev, cfg);

        auto* tex  = renderer->GetTextures();
        auto* mats = renderer->GetMaterials();
        auto* mesh = renderer->GetMeshCache();

        // Texture cel-shading pour le personnage
        NkTexId charAlbedo = tex->Load("Assets/Anime/character_albedo.png", true, false);
        NkTexId charNormal = tex->Load("Assets/Anime/character_normal.png", false, false);

        // Matériau Anime (cel-shading avec rim light et outline)
        NkMatId matChar = mats->CreateAnime(
            charAlbedo,
            {0.7f, 0.7f, 0.85f, 1.f},  // couleur d'ombre froide
            0.5f                         // rim light strength
        );

        // Personnaliser l'outline
        auto* m = mats->Get(matChar);
        m->toon.outlineWidth = 0.004f;
        m->toon.outlineColor = {0.05f, 0.05f, 0.1f, 1.f}; // presque noir
        mats->MarkDirty(matChar);

        // Matériau Toon simple pour le background
        NkMatId matBg = mats->CreateToon(
            {0.9f, 0.9f, 1.f, 1.f},     // lumière
            {0.4f, 0.4f, 0.6f, 1.f},    // ombre
            2                             // 2 bandes = style 2 tons
        );

        // ── Rendu ─────────────────────────────────────────────────────────────────
        auto* r3d = renderer->GetRender3D();
        r3d->SetCamera({
            .position   = {0,1.5f,4.f},
            .target     = {0,1.f,0},
            .fovY       = 50.f,
        });
        r3d->SetSun({-0.5f,-1.f,-0.3f}, {1,1,0.9f}, 3.f, true);

        // Le rendu se fait normalement dans la boucle principale...
        // renderer->BeginFrame();
        // r3d->DrawMesh(characterMesh, transform, matChar);
        // renderer->EndFrame();
    }

    void Destroy() { NkRenderer::Destroy(renderer); }
};

// =============================================================================
// EXEMPLE 2 : Jeu 2D pur (top-down, sprites, tilemap)
// =============================================================================
struct TopDown2DScene {
    NkRenderer*     renderer = nullptr;
    NkSpriteAtlas   tileAtlas;
    NkSpriteAtlas   charAtlas;
    NkCamera2D      camera;

    void Init(NkIDevice* dev, uint32 w, uint32 h) {
        // Config 2D : pas de shadow, pas de SSAO
        NkRendererConfig cfg = NkRendererConfig::For2D(dev->GetApi(), w, h);
        renderer = NkRenderer::Create(dev, cfg);

        auto* tex = renderer->GetTextures();

        // Chargement des atlases
        NkTexId tilesTex = tex->Load("Assets/2D/tileset.png", false, false);
        NkTexId charTex  = tex->Load("Assets/2D/character.png", false, false);

        tileAtlas = NkSpriteAtlas(tilesTex, 32, 32, 16, 16, 512, 512);
        charAtlas = NkSpriteAtlas(charTex,  48, 64, 8,  4,  384, 256);

        camera.viewSize  = {(float32)w, (float32)h};
        camera.zoom      = 2.f;
        camera.position  = {0, 0};
    }

    void Render(float32 time) {
        if (!renderer->BeginFrame()) return;

        auto& r2d = *renderer->GetRender2D();
        r2d.Begin(camera);

        // ── Tilemap (background) ──────────────────────────────────────────────────
        static const uint32 kMap[10*10] = {
            0,0,1,1,1,1,0,0,0,0,
            0,1,2,2,2,2,1,0,0,0,
            1,2,2,2,2,2,2,1,0,0,
            1,2,2,2,2,2,2,1,0,0,
            1,2,2,2,2,2,2,1,0,0,
            0,1,2,2,2,2,1,0,0,0,
            0,0,1,1,1,1,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,
        };

        NkRender2D::Tilemap tilemap;
        tilemap.tiles  = kMap;
        tilemap.width  = 10;
        tilemap.height = 10;
        tilemap.atlas  = &tileAtlas;
        tilemap.tileWidth  = 32.f;
        tilemap.tileHeight = 32.f;

        r2d.DrawTilemap(tilemap, {-160.f, -160.f});

        // ── Sprites animés ────────────────────────────────────────────────────────
        // Animation du personnage (frame basée sur le temps)
        uint32 frame = (uint32)(time * 8.f) % 4; // 4 frames à 8 FPS
        NkSprite charSprite = charAtlas.GetSprite(frame, 0); // row 0 = idle down

        NkDrawSpriteParams params;
        params.position = {0.f, 0.f};
        params.scale    = {2.f, 2.f};
        params.tint     = NkColor32::White();
        r2d.DrawSprite(charSprite, params);

        // ── UI HUD ────────────────────────────────────────────────────────────────
        // Barre de vie (NineSlice pour un rendu élastique)
        auto* uiTex = renderer->GetTextures();
        NkTexId panelTex = uiTex->Load("Assets/UI/panel_bg.png", false, false);

        // Bar de vie en bas à gauche
        r2d.SetLayer(10); // layer UI au-dessus de tout
        r2d.DrawRect({10.f, (float32)600.f-50.f}, {200.f, 20.f},
                      {60, 60, 60, 200});                       // fond
        float32 hp = 0.7f; // 70% HP
        r2d.DrawGradientRect({12.f, (float32)600.f-48.f},
                              {(200.f-4.f)*hp, 16.f},
                              {0,220,60,255}, {0,180,30,255},  // vert
                              {0,200,50,255}, {0,160,20,255});

        // Texte FPS
        r2d.DrawText("FPS: 60", 0, {10.f, 10.f}, 16.f, NkColor32::Yellow());

        // Cercle de sélection sous le personnage
        float32 pulse = 0.8f + 0.2f*sinf(time*4.f);
        r2d.DrawCircleOutline({0.f, 10.f}, 28.f*pulse,
                               {100,200,255,200}, 2.f, 32);

        r2d.End();
        renderer->EndFrame();
    }

    void Destroy() { NkRenderer::Destroy(renderer); }
};

// =============================================================================
// EXEMPLE 3 : Rendu offscreen (minimap, réflexion, portal)
// =============================================================================
struct OffscreenExample {
    NkRenderer*       renderer    = nullptr;
    NkOffscreenTarget* minimapRT  = nullptr;
    NkOffscreenTarget* portalRT   = nullptr;

    void Init(NkIDevice* dev, uint32 w, uint32 h) {
        NkRendererConfig cfg = NkRendererConfig::ForGame(dev->GetApi(), w, h);
        renderer = NkRenderer::Create(dev, cfg);

        // ── Minimap : vue de dessus 256x256 ───────────────────────────────────────
        NkOffscreenDesc minimapDesc;
        minimapDesc.width  = 256;
        minimapDesc.height = 256;
        minimapDesc.colorFormats[0] = NkGPUFormat::NK_RGBA8_SRGB;
        minimapDesc.hasDepth = true;
        minimapDesc.resizable= false;
        minimapDesc.debugName= "Minimap";
        minimapRT = renderer->CreateOffscreenTarget(minimapDesc);

        // ── Portal : caméra alternative 512x512 ───────────────────────────────────
        NkOffscreenDesc portalDesc;
        portalDesc.width   = 512;
        portalDesc.height  = 512;
        portalDesc.colorFormats[0] = NkGPUFormat::NK_RGBA16_FLOAT;
        portalDesc.hasDepth= true;
        portalDesc.debugName= "PortalView";
        portalRT = renderer->CreateOffscreenTarget(portalDesc);
    }

    void Render(NkICommandBuffer* cmd) {
        // ── 1. Rendre la minimap ──────────────────────────────────────────────────
        NkCamera3D minimapCam;
        minimapCam.position      = {0, 50.f, 0};
        minimapCam.target        = {0, 0, 0};
        minimapCam.up            = {0, 0, -1};
        minimapCam.orthographic  = true;
        minimapCam.orthoSize     = 30.f;

        minimapRT->Begin(cmd, NkLoadOp::NK_CLEAR, NkLoadOp::NK_CLEAR);
        cmd->SetViewport(minimapRT->GetFullViewport());
        cmd->SetScissor(minimapRT->GetFullRect());
        // ... draw scene with minimapCam ...
        minimapRT->End(cmd);

        // ── 2. Rendre la vue du portail ────────────────────────────────────────────
        NkCamera3D portalCam;
        portalCam.position = {5.f, 1.5f, 10.f};
        portalCam.target   = {5.f, 1.5f, 0.f};
        portalCam.fovY     = 80.f;

        portalRT->Begin(cmd, NkLoadOp::NK_CLEAR, NkLoadOp::NK_CLEAR);
        cmd->SetViewport(portalRT->GetFullViewport());
        cmd->SetScissor(portalRT->GetFullRect());
        // ... draw scene from portal POV ...
        portalRT->End(cmd);

        // ── 3. Rendu principal ────────────────────────────────────────────────────
        // La minimap et le portail sont maintenant utilisables comme textures.
        // On peut les binder sur une surface dans la scène principale
        // via NkMaterialSystem (texture.albedo = minimapRT->GetColorTexId()).

        NkTexId minimapTex = minimapRT->GetColorTexId();
        NkTexId portalTex  = portalRT->GetColorTexId();

        // Afficher la minimap dans le coin via NkRender2D
        auto& r2d = *renderer->GetRender2D();
        r2d.DrawTexture(minimapTex,
                         {(float32)(renderer->GetWidth()-270), 10.f},
                         {256.f, 256.f});

        // Afficher un bord autour
        r2d.DrawRectOutline({(float32)(renderer->GetWidth()-272), 8.f},
                             {260.f, 260.f},
                             {255,255,255,200}, 2.f);

        // Le portail : son matériau utilise portalTex comme albedo
        // (appliqué sur la géométrie du portail dans la scène 3D)
    }

    void Destroy() {
        if (minimapRT) renderer->DestroyOffscreenTarget(minimapRT);
        if (portalRT)  renderer->DestroyOffscreenTarget(portalRT);
        NkRenderer::Destroy(renderer);
    }
};

// =============================================================================
// EXEMPLE 4 : Archviz (rendu architectural photoréaliste)
// =============================================================================
struct ArchvizScene {
    NkRenderer* renderer = nullptr;

    void Init(NkIDevice* dev, uint32 w, uint32 h) {
        // Config Archviz : ultra qualité, SSR, HBAO, color grading
        NkRendererConfig cfg = NkRendererConfig::ForArchviz(dev->GetApi(), w, h);
        cfg.shadow.cascadeCount = 4;
        cfg.shadow.resolution   = 4096;
        cfg.shadow.softShadows  = true;
        cfg.sky.mode            = NkSkyMode::NK_HDRI;
        cfg.sky.hdriPath        = "Assets/HDR/interior_daylight.hdr";
        cfg.sky.hdriIntensity   = 2.f;

        // Post-process cinématique
        cfg.postProcess.ssao          = true;
        cfg.postProcess.hbao          = true;
        cfg.postProcess.ssr           = true;
        cfg.postProcess.ssrMaxDistance= 50.f;
        cfg.postProcess.bloom         = false;  // pas de bloom en archiviz
        cfg.postProcess.colorGrading  = true;
        cfg.postProcess.toneMapping   = true;
        cfg.postProcess.taa           = true;   // TAA pour qualité maximale

        renderer = NkRenderer::Create(dev, cfg);

        auto* tex  = renderer->GetTextures();
        auto* mats = renderer->GetMaterials();

        // Béton brut avec détail
        NkTexId concreteAlbedo = tex->Load("Assets/Archviz/concrete_albedo.png", true, true);
        NkTexId concreteNormal = tex->Load("Assets/Archviz/concrete_normal.png", false, false);
        NkTexId concreteAO     = tex->Load("Assets/Archviz/concrete_baked_ao.png",false,false);

        NkMatId matConcrete = mats->CreateArchviz(concreteAlbedo, concreteNormal, concreteAO);
        auto* mc = mats->Get(matConcrete);
        mc->pbr.roughness      = 0.85f;
        mc->pbr.metallic       = 0.0f;
        mc->pbr.normalStrength = 1.5f;
        mc->pbr.uvScale        = {4.f, 4.f};
        mats->MarkDirty(matConcrete);

        // Verre architectural
        NkMatId matGlass = mats->CreateGlass(1.52f, 0.01f);
        auto* mg = mats->Get(matGlass);
        mg->pbr.albedo = {0.85f, 0.9f, 0.95f, 0.15f};  // légèrement transparent
        mats->MarkDirty(matGlass);

        // Bois (parquet)
        NkTexId woodAlbedo = tex->Load("Assets/Archviz/parquet_albedo.png", true, true);
        NkTexId woodNormal = tex->Load("Assets/Archviz/parquet_normal.png", false, false);
        NkMatId matWood = mats->CreateArchviz(woodAlbedo, woodNormal);
        auto* mw = mats->Get(matWood);
        mw->pbr.roughness = 0.4f;
        mw->pbr.uvScale   = {8.f, 8.f};
        mats->MarkDirty(matWood);

        // Post-process : color grading chaud et léger désaturation
        auto* pp = renderer->GetPostProcess();
        NkColorGradingParams cg;
        cg.enabled     = true;
        cg.saturation  = 0.85f;   // légèrement désaturé
        cg.contrast    = 1.1f;
        cg.brightness  = 0.05f;
        cg.gain        = {1.05f, 1.02f, 0.98f}; // chaud
        pp->SetColorGrading(cg);
        pp->SetToneMapping(NkToneMapOp::NK_TONEMAP_AGX, 1.2f, 2.2f);

        // Sol du bureau avec SSR
        // (Les réflexions sol seront capturées par le SSR automatiquement)
    }

    void Destroy() { NkRenderer::Destroy(renderer); }
};

// =============================================================================
// EXEMPLE 5 : Effet aquarelle (watercolor painting)
// =============================================================================
struct WatercolorScene {
    NkRenderer*    renderer  = nullptr;
    NkOffscreenTarget* paintRT = nullptr;

    void Init(NkIDevice* dev, uint32 w, uint32 h) {
        NkRendererConfig cfg = NkRendererConfig::ForGame(dev->GetApi(), w, h);
        cfg.postProcess.bloom = false;
        cfg.postProcess.ssao  = false;
        renderer = NkRenderer::Create(dev, cfg);

        // Passe offscreen pour capturer la scène proprement
        NkOffscreenDesc d;
        d.width = w; d.height = h;
        d.colorFormats[0] = NkGPUFormat::NK_RGBA8_SRGB;
        d.hasDepth = false;
        paintRT = renderer->CreateOffscreenTarget(d);

        auto* mats = renderer->GetMaterials();

        // Matériau aquarelle pour chaque objet
        NkMatId matPainting = mats->Create(NkMaterialType::NK_WATERCOLOR);
        auto* mp = mats->Get(matPainting);
        mp->pbr.albedo = {0.8f, 0.7f, 0.5f, 1.f};
        mats->MarkDirty(matPainting);

        // Effet post-process custom pour simuler le papier et les bords baveux
        auto* pp = renderer->GetPostProcess();
        pp->AddCustomPass("WatercolorEdges", [](
            NkICommandBuffer* cmd,
            NkTextureHandle input, NkTextureHandle depth,
            NkTextureHandle output, uint32 w, uint32 h)
        {
            // Ici on aurait un pipeline compute custom qui :
            // 1. Détecte les contours (Sobel/Laplacian)
            // 2. Ajoute du papier texturé
            // 3. Bavures aux bords (edge bleed)
            // 4. Variation d'humidité procédurale
            (void)cmd; (void)input; (void)depth; (void)output; (void)w; (void)h;
        });
    }

    void Destroy() {
        if (paintRT) renderer->DestroyOffscreenTarget(paintRT);
        NkRenderer::Destroy(renderer);
    }
};

// =============================================================================
// EXEMPLE 6 : Skybox procédurale + rendu volumétrique (brouillard)
// =============================================================================
struct AtmosphericScene {
    NkRenderer* renderer = nullptr;

    void Init(NkIDevice* dev, uint32 w, uint32 h) {
        NkRendererConfig cfg = NkRendererConfig::ForFilm(dev->GetApi(), w, h);

        // Sky procédural avec brouillard
        cfg.sky.mode        = NkSkyMode::NK_PROCEDURAL;
        cfg.sky.sunIntensity= 15.f;
        cfg.sky.turbidity   = 4.f;   // plus de particules = ciel plus rouge/orange
        cfg.sky.enableFog   = true;
        cfg.sky.fogStart    = 30.f;
        cfg.sky.fogEnd      = 200.f;
        cfg.sky.fogColor    = {0.7f, 0.75f, 0.8f, 1.f};

        renderer = NkRenderer::Create(dev, cfg);

        // Matériau volume pour brouillard volumétrique
        auto* mats = renderer->GetMaterials();
        NkMatId matFog = mats->Create(NkMaterialType::NK_VOLUME);
        auto* mf = mats->Get(matFog);
        mf->pbr.albedo    = {0.8f, 0.85f, 0.9f, 0.3f};
        mf->pbr.subsurface= 1.f;    // scattering
        mats->MarkDirty(matFog);
    }

    void Destroy() { NkRenderer::Destroy(renderer); }
};