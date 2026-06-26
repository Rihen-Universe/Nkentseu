// =============================================================================
// Demo3D.cpp  — Demo 2
//
// Demo minimaliste 3D :
//   - Config ForGame (RENDER3D + RENDER2D + TEXT + SHADOW + POST_PROCESS + OVERLAY)
//   - Camera 3D orbite autour de l'origine
//   - Mesh primitives : sol (plane) + 4x4 sphere grid + cube central
//   - 1 lumiere directionnelle + 2 lumieres ponctuelles colorees
//
// Demontre le path complet : NkScene/Lights/DrawCalls -> Render3D::Submit
//                            -> RenderGraph -> Flush.
// =============================================================================
#include "DemoCommon.h"
#include "NKWindow/Core/NkWESystem.h"   // NkEvents()
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKRenderer/Tools/Shadow/NkShadowSystem.h"
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"
#include "NKImage/NKImage.h"            // Phase H : test ecriture PNG procedural
#include <cstdio>

namespace nkentseu { namespace demo {

    struct Demo3DState {
        NkMeshHandle meshSphere;
        NkMeshHandle meshPlane;
        NkMeshHandle meshCube;
        NkTexHandle  cookieWindow;    // E.6 : cookie 2D pour le spot
        NkTexHandle  cookieCube;      // E.6b : cookie cube pour le point red
        float32      angle = 0.f;     // orbite camera
        // Panel debug : index PCF mode courant pour cycle (P)
        int32        pcfIdx = (int32)NkPCFMode::PCF5x5;
        // Phase H : true si le PNG a ete charge avec succes (vs fallback procedural).
        bool         phaseHLoadOk = false;
    };

    // E.6b : cubemap procedurale 128x128x6 pour point light.
    // Chaque face = pattern "X" : 2 bandes diagonales lumineuses sur fond noir.
    // Tres contraste pour etre clairement visible meme avec autres lumieres.
    static NkTexHandle CreateLanternCubeCookie(NkTextureLibrary* texLib,
                                                NkIDevice* dev) {
        const uint32 S = 128;
        NkTextureCreateDesc d;
        d.width = S; d.height = S; d.depth = 1;
        d.format = NkGPUFormat::NK_RGBA8_UNORM;
        d.isCubemap = true;
        d.mipLevels = 1;
        d.debugName = "Demo3D_LanternCube";
        NkTexHandle tex = texLib->Create(d);
        if (!tex.IsValid()) return tex;

        std::vector<uint8_t> pixels(S * S * 4);
        for (uint32 face = 0; face < 6; face++) {
            for (uint32 y = 0; y < S; y++) {
                for (uint32 x = 0; x < S; x++) {
                    // X pattern : bright si proche d'une diagonale (epaisseur 8 px)
                    float dx = float(x) - S * 0.5f;
                    float dy = float(y) - S * 0.5f;
                    float diag1 = std::abs(dx - dy);    // diagonale slash
                    float diag2 = std::abs(dx + dy);    // diagonale antislash
                    bool onCross = diag1 < 8.f || diag2 < 8.f;
                    // Trou central toujours brillant (faisceau "principal")
                    float r  = std::sqrt(dx*dx + dy*dy);
                    bool centerHole = r < S * 0.10f;
                    uint8_t v = (onCross || centerHole) ? 255 : 0;   // contraste max
                    uint32 idx = (y * S + x) * 4;
                    pixels[idx + 0] = v;
                    pixels[idx + 1] = v;
                    pixels[idx + 2] = v;
                    pixels[idx + 3] = 255;
                }
            }
            dev->WriteTextureRegion(texLib->GetRHIHandle(tex), pixels.data(),
                0, 0, 0, S, S, 1, 0, face);
        }
        return tex;
    }

    // Genere un cookie procedural 256x256 RGBA : motif "window bars".
    // Barres + bordure noires (~12% de transmittance), centre/cellules blanches.
    static NkTexHandle CreateWindowCookie(NkTextureLibrary* texLib) {
        const uint32 W = 256, H = 256;
        std::vector<uint8_t> pixels(W * H * 4);
        for (uint32 y = 0; y < H; y++) {
            for (uint32 x = 0; x < W; x++) {
                bool barV = (x % 64) < 8;
                bool barH = (y % 64) < 8;
                bool border = (x < 8 || x >= W - 8 || y < 8 || y >= H - 8);
                uint8_t v = (barV || barH || border) ? 30 : 255;
                uint32 idx = (y * W + x) * 4;
                pixels[idx + 0] = v;
                pixels[idx + 1] = v;
                pixels[idx + 2] = v;
                pixels[idx + 3] = 255;
            }
        }
        NkTextureCreateDesc d;
        d.pixels    = pixels.data();
        d.width     = W;
        d.height    = H;
        d.mipLevels = 1;
        d.format    = NkGPUFormat::NK_RGBA8_UNORM;
        d.debugName = "Demo3D_WindowCookie";
        return texLib->Create(d);
    }

    // Phase H : test de la pipeline texture file-based.
    //
    // Genere (si absent) un fichier PNG procedural "test_pattern.png" 256x256
    // contenant un motif damier color (4 quadrants RGB + bordures), puis le
    // charge via NkTextureLibrary::Load(). Demontre toute la chaine :
    //   1. NkImage::Alloc + SavePNG  -> ecriture disque
    //   2. NkTextureLibrary::Load    -> decode PNG + upload GPU + mip chain
    //   3. retourne un NkTexHandle utilisable comme tout autre texture.
    //
    // Le fichier est genere dans Resources/NKRenderer/Textures/Defaults/
    // (relativement au CWD ou au repo). On essaie d'abord ce chemin, sinon
    // fallback sur le CWD. La premiere execution genere le fichier ; les
    // suivantes le lisent. Si ecriture echoue (permissions / dir inexistant),
    // on retourne false et l'init utilise le cookie procedural.
    static const char* kPhaseHTestPathPrimary =
        "Resources/NKRenderer/Textures/Defaults/test_pattern.png";
    static const char* kPhaseHTestPathFallback = "test_pattern.png";

    static bool GenerateTestPatternPNG(const char* outPath) {
        // Verifie d'abord s'il existe deja (skip si present).
        if (FILE* f = ::fopen(outPath, "rb")) {
            ::fclose(f);
            return true;
        }

        const int32 W = 256, H = 256;
        NkImage* img = NkImage::Alloc(W, H, NkImagePixelFormat::NK_RGBA32);
        if (!img || !img->IsValid()) {
            if (img) img->Free();
            return false;
        }

        uint8_t* px = img->Pixels();
        const int32 stride = img->Stride();
        for (int32 y = 0; y < H; ++y) {
            uint8_t* row = px + (size_t)y * stride;
            for (int32 x = 0; x < W; ++x) {
                // 4 quadrants : rouge / vert / bleu / jaune.
                bool right = (x >= W / 2);
                bool bottom = (y >= H / 2);
                uint8_t r = (!right && !bottom) || (right && bottom) ? 255 : 0;
                uint8_t g = (right && !bottom) || (right && bottom) ? 255 : 0;
                uint8_t b = (!right && bottom) ? 255 : 0;
                // Damier 16x16 pour valider qu'on lit bien le PNG (et pas un
                // buffer noir/blanc).
                bool ck = ((x / 16) ^ (y / 16)) & 1;
                if (ck) { r = (uint8_t)(r * 0.6f); g = (uint8_t)(g * 0.6f); b = (uint8_t)(b * 0.6f); }
                // Bordure noire 4px pour visualiser les bords.
                bool border = (x < 4 || x >= W - 4 || y < 4 || y >= H - 4);
                if (border) { r = g = b = 0; }
                row[x*4 + 0] = r;
                row[x*4 + 1] = g;
                row[x*4 + 2] = b;
                row[x*4 + 3] = 255;
            }
        }
        bool ok = img->SavePNG(outPath);
        img->Free();
        return ok;
    }

    // Helper d'affichage : nom court de PCFMode
    static const char* PcfModeName(NkPCFMode m) {
        switch (m) {
            case NkPCFMode::NONE:    return "NONE";
            case NkPCFMode::PCF3x3:  return "PCF3x3";
            case NkPCFMode::PCF5x5:  return "PCF5x5";
            case NkPCFMode::POISSON: return "POISSON";
            case NkPCFMode::PCSS:    return "PCSS";
        }
        return "?";
    }

    bool Demo3D_Init(DemoCtx& ctx) {
        auto* st = new Demo3DState();
        ctx.userData = st;

        auto* meshSys = ctx.renderer->GetMeshSystem();
        st->meshSphere = meshSys->GetSphere();
        st->meshPlane  = meshSys->GetPlane();
        st->meshCube   = meshSys->GetCube();

        // ── Phase E.6 : creation procedurale des cookies + bind ──────────────
        auto* texLib = ctx.renderer->GetTextures();
        auto* r3d    = ctx.renderer->GetRender3D();
        auto* device = ctx.renderer->GetDevice();
        if (texLib && r3d) {
            // Phase H : test de la pipeline file-based.
            // On genere un PNG "test_pattern.png" puis on le charge via
            // NkTextureLibrary::Load(). Si la chaine fonctionne, on l'utilise
            // comme cookie spot (plus visible que le procedural en RAM car
            // sample par texture(...) GLSL avec mips). Fallback : cookie
            // procedural (CreateWindowCookie) si Load echoue.
            const char* pngPath = nullptr;
            if (GenerateTestPatternPNG(kPhaseHTestPathPrimary)) {
                pngPath = kPhaseHTestPathPrimary;
            } else if (GenerateTestPatternPNG(kPhaseHTestPathFallback)) {
                pngPath = kPhaseHTestPathFallback;
            }

            if (pngPath) {
                NkLoadOptions opts;
                // Cookie : on veut le PNG en valeur lineaire (pas de gamma)
                // pour que la modulation lumiere*cookie soit correcte. On
                // pourrait passer srgb=true pour un albedo PBR classique.
                opts.srgb       = false;
                opts.genMipmaps = true;     // mips utiles pour cookie
                opts.useClampEdge = true;   // cookie : pas de tiling
                opts.debugName  = "Demo3D_PhaseH_TestPattern";
                st->cookieWindow = texLib->Load(NkString(pngPath), opts);
                if (st->cookieWindow.IsValid() &&
                    st->cookieWindow.id != texLib->GetError().id) {
                    logger.Info("[Demo3D][Phase H] PNG charge OK depuis '{0}'\n", pngPath);
                    st->phaseHLoadOk = true;
                } else {
                    logger.Errorf("[Demo3D][Phase H] Echec Load PNG, fallback procedural\n");
                    st->cookieWindow = CreateWindowCookie(texLib);
                }
            } else {
                logger.Errorf("[Demo3D][Phase H] Impossible d'ecrire le PNG de test, fallback procedural\n");
                st->cookieWindow = CreateWindowCookie(texLib);
            }

            if (st->cookieWindow.IsValid()) {
                r3d->SetLightCookie3D(0, texLib->GetRHIHandle(st->cookieWindow));
                logger.Info("[Demo3D] Cookie 2D bind au slot 0\n");
            }
            // E.6b : cube cookie pour le point light rouge (procedural, OK).
            if (device) {
                st->cookieCube = CreateLanternCubeCookie(texLib, device);
                if (st->cookieCube.IsValid()) {
                    r3d->SetLightCookieCube3D(0, texLib->GetRHIHandle(st->cookieCube));
                    logger.Info("[Demo3D] Lantern cube cookie cree + bind au slot 0\n");
                }
            }
        }

        // ── Shortcuts clavier pour tweak des params shadow en live ──
        // [ / ] : shadowBias -+ 0.0005
        // , / . : sceneRadius -+ 1.0
        // P     : cycle PCF mode
        // R     : reset to defaults
        // V     : toggle VSync (utile pour mesurer le vrai FPS GPU)
        auto* shadowSys = ctx.renderer->GetShadow();
        auto* renderer = ctx.renderer;
        NkEvents().AddEventCallback<NkKeyPressEvent>([renderer](NkKeyPressEvent* e) {
            if (e->GetKey() == NkKey::NK_V) {
                static bool vsync = true;
                vsync = !vsync;
                renderer->SetVSync(vsync);
                logger.Info("[Demo3D] VSync = {0}\n", vsync);
            }
        });
        if (shadowSys) {
            NkEvents().AddEventCallback<NkKeyPressEvent>([shadowSys, st](NkKeyPressEvent* e) {
                auto& cfg = shadowSys->GetConfig();
                switch (e->GetKey()) {
                    case NkKey::NK_LBRACKET:
                        cfg.shadowBias = NkMax(0.0001f, cfg.shadowBias - 0.0005f); break;
                    case NkKey::NK_RBRACKET:
                        cfg.shadowBias += 0.0005f; break;
                    case NkKey::NK_P:
                        st->pcfIdx = (st->pcfIdx + 1) % 5;
                        cfg.quality = (NkVSMShadowQuality)st->pcfIdx; break;
                    case NkKey::NK_N:    // softness - (ombres plus dures)
                        cfg.softness = NkMax(0.0005f, cfg.softness - 0.001f); break;
                    case NkKey::NK_M:    // softness + (ombres plus douces)
                        cfg.softness = NkMin(0.020f,  cfg.softness + 0.001f); break;
                    case NkKey::NK_R:
                        cfg.shadowBias  = 0.001f;
                        cfg.softness    = 0.003f;
                        cfg.quality     = NkVSMShadowQuality::PCF5x5;
                        st->pcfIdx      = (int32)NkVSMShadowQuality::PCF5x5;
                        break;
                    default: break;
                }
            });
        }

        logger.Info("[Demo3D] Init OK — meshes : sphere={0} plane={1} cube={2}\n",
                    (uint64)st->meshSphere.id,
                    (uint64)st->meshPlane.id,
                    (uint64)st->meshCube.id);
        return true;
    }

    void Demo3D_Frame(DemoCtx& ctx, float32 dt) {
        auto* st = (Demo3DState*)ctx.userData;
        // DIAG (gated NK_FIX_CAM) : fige la caméra + le temps pour comparer DX12/VK
        // au MÊME angle/pose. Pose déterministe identique sur les 2 backends.
        static int fixcam = -1;
        if (fixcam == -1) { const char* v = getenv("NK_FIX_CAM"); fixcam = (v && v[0] && v[0] != '0') ? 1 : 0; }
        if (fixcam) { st->angle = 0.6f; ctx.totalTime = 1.0f; }
        else        { st->angle += dt * 0.45f; }

        if (!ctx.renderer->BeginFrame()) return;

        auto* r3d = ctx.renderer->GetRender3D();
        if (!r3d) {
            ctx.renderer->Present();
            ctx.renderer->EndFrame();
            return;
        }

        // ── Camera (orbite autour de l'origine) ─────────────────────────────
        NkCamera3DData camData;
        camData.position  = {cosf(st->angle) * 5.5f, 2.5f, sinf(st->angle) * 5.5f};
        camData.target    = {0.f, 0.5f, 0.f};
        camData.up        = {0.f, 1.f, 0.f};
        camData.fovY      = 60.f;
        camData.aspect    = (float32)ctx.width / (float32)ctx.height;
        camData.nearPlane = 0.1f;
        camData.farPlane  = 100.f;
        NkCamera3D cam(camData);

        // ── Lights ───────────────────────────────────────────────────────────
        NkSceneContext sctx;
        sctx.camera = cam;
        sctx.time   = ctx.totalTime;

        // Soleil directionnel
        NkLightDesc sun;
        sun.type      = NkLightType::NK_DIRECTIONAL;
        sun.direction = {-0.4f, -1.f, -0.3f};
        sun.color     = {1.f, 0.95f, 0.85f};
        sun.intensity = 3.f;
        sun.castShadow  = true;
        sun.shadowStatic= true;  // NkVSM v1 cache : sun ne bouge pas
        sctx.lights.PushBack(sun);

        // Lumiere ponctuelle rouge — avec cube cookie "lantern" (E.6b).
        // Boostee (intensity 12, range 10) pour que le pattern X soit clair
        // meme face au sun + spot. Position legerement haute pour eviter
        // d'etre dans le sol.
        NkLightDesc redLight;
        redLight.type      = NkLightType::NK_POINT;
        redLight.position  = {3.f, 2.5f, 0.f};
        redLight.color     = {1.f, 0.2f, 0.1f};
        redLight.intensity = 12.f;
        redLight.range     = 10.f;
        redLight.cookieIdx = 0;            // utilise le cube bind au slot 0
        redLight.castShadow  = true;       // NkVSM : cubemap 6 faces shadow
        redLight.shadowStatic= true;       // position fixe
        sctx.lights.PushBack(redLight);

        // Fill bleue
        NkLightDesc blue;
        blue.type      = NkLightType::NK_POINT;
        blue.position  = {-2.f, 1.f, 1.f};
        blue.color     = {0.2f, 0.5f, 1.f};
        blue.intensity = 2.5f;
        blue.range     = 8.f;
        blue.castShadow  = true;           // NkVSM : cubemap 6 faces shadow
        blue.shadowStatic= true;           // position fixe
        sctx.lights.PushBack(blue);

        // E.6 : Spot light avec cookie procedural "window bars" projete au sol.
        // Tournant lentement pour montrer la projection dynamique.
        NkLightDesc spot;
        spot.type       = NkLightType::NK_SPOT;
        spot.position   = {3.f * cosf(ctx.totalTime * 0.3f),
                            4.f,
                            3.f * sinf(ctx.totalTime * 0.3f)};
        spot.direction  = NkVec3f{0.f, 0.f, 0.f} - spot.position;   // pointe vers origine
        spot.direction  = spot.direction.Normalized();
        spot.color      = {1.f, 0.95f, 0.85f};
        spot.intensity  = 8.f;
        spot.range      = 10.f;
        spot.innerAngle = 18.f;
        spot.outerAngle = 28.f;
        spot.cookieIdx  = 0;            // utilise le slot bind par Init
        spot.castShadow = true;         // NkVSM : 1 tile shadow map per spot
        sctx.lights.PushBack(spot);

        sctx.ambientIntensity = 0.15f;

        r3d->BeginScene(sctx);

        // ── Sol ──────────────────────────────────────────────────────────────
        {
            NkDrawCall3D dc;
            dc.mesh      = st->meshPlane;
            dc.transform = NkMat4f::Scale({10.f, 1.f, 10.f});
            dc.aabb      = {{-5, 0, -5}, {5, 0, 5}};
            dc.castShadow= false;
            r3d->Submit(dc);
        }

        // ── Grille 4x4 de spheres : grille PBR canonique ─────────────────────
        // Colonnes -> metallic (0..1) : voir l'effet F0 changer
        // Lignes   -> roughness (~0..1) : voir le blur GGX changer
        // La sphere top-left (col=0, row=0) est dielectric mirror -> reflet net
        // du sky. Top-right (col=3, row=0) est metal poli -> reflet teinte par
        // l'albedo. Bottom-row : surfaces rugueuses, ambient diffus dominant.
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                float32 x = (col - 1.5f) * 1.2f;
                float32 z = (row - 1.5f) * 1.2f;

                NkDrawCall3D dc;
                dc.mesh      = st->meshSphere;
                dc.transform = NkMat4f::Translate({x, 0.5f, z}) *
                               NkMat4f::Scale({0.45f, 0.45f, 0.45f});
                dc.aabb      = {{x - 0.25f, 0.25f, z - 0.25f},
                                {x + 0.25f, 0.75f, z + 0.25f}};
                dc.tint      = {(float32)col / 3.f, (float32)row / 3.f, 0.7f};
                dc.metallic  = (float32)col / 3.f;             // 0, 0.33, 0.66, 1
                dc.roughness = 0.05f + (float32)row / 3.f * 0.95f; // 0.05 .. 1
                r3d->Submit(dc);
            }
        }

        // ── Cube central rotatif : metal or poli (gold metallic, low rough) ──
        {
            NkDrawCall3D dc;
            dc.mesh = st->meshCube;
            float32 y = 0.5f + sinf(ctx.totalTime * 1.5f) * 0.2f;
            dc.transform = NkMat4f::Translate({0, y, 0}) *
                           NkMat4f::RotationY(NkAngle::FromRad(ctx.totalTime * 0.8f)) *
                           NkMat4f::Scale({0.6f, 0.6f, 0.6f});
            dc.aabb = {{-0.35f, 0.1f, -0.35f}, {0.35f, 0.9f, 0.35f}};
            dc.tint      = {1.f, 0.8f, 0.3f};   // gold albedo
            dc.metallic  = 1.f;
            dc.roughness = 0.15f;
            r3d->Submit(dc);
        }

        // ── Colonnes bloquantes pour visualiser les ombres point/spot ────────
        // NkVSM v0 : ces colonnes castent des ombres pour TOUTES les lights
        // (sun + red + blue + spot) -> on doit voir 4 ombres differentes
        // projetees sur le sol pour chaque colonne.
        // Position colonnes :
        //   - col0 a (-4, 1, -2) : devant la red light pour ombre rouge
        //   - col1 a (1, 1, 4)   : derriere les spheres pour ombre spot
        for (int c = 0; c < 2; c++) {
            float32 cx = (c == 0) ? -4.f : 1.f;
            float32 cz = (c == 0) ? -2.f :  4.f;
            NkDrawCall3D dc;
            dc.mesh      = st->meshCube;
            dc.transform = NkMat4f::Translate({cx, 1.f, cz}) *
                           NkMat4f::Scale({0.3f, 2.f, 0.3f});  // colonne 2m haute
            dc.aabb      = {{cx - 0.2f, 0.f, cz - 0.2f},
                            {cx + 0.2f, 2.f, cz + 0.2f}};
            dc.tint      = {0.7f, 0.7f, 0.7f};
            dc.metallic  = 0.f;
            dc.roughness = 0.6f;
            dc.castShadow= true;
            r3d->Submit(dc);
        }

        // Debug visualizations
        r3d->DrawDebugAxes(NkMat4f::Identity(), 1.f);
        r3d->DrawDebugGrid({0, 0, 0}, 1.f, 20, {0.3f, 0.3f, 0.3f, 1.f});

        // ── Overlay ──────────────────────────────────────────────────────────
        if (auto* overlay = ctx.renderer->GetOverlay()) {
            overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
            overlay->DrawStats(ctx.renderer->GetStats());
            overlay->DrawText({20.f, 35.f}, "Demo 3D - PBR primitives  |  API : %s",
                              NkGraphicsApiName(ctx.api));
            overlay->DrawText({20.f, 55.f}, "FPS approx: %.1f  |  dt: %.2f ms",
                              dt > 1e-4f ? 1.f / dt : 0.f, dt * 1000.f);
            // Phase H : indication visuelle du chargement texture file-based.
            overlay->DrawText({20.f, 75.f},
                "[Phase H] Texture file-based : %s",
                st->phaseHLoadOk ? "test_pattern.png LOAD OK"
                                 : "fallback procedural");

            // ── Debug panel : params shadow live-tunable ───────────────────────
            // Background semi-transparent en haut a droite
            if (auto* r2d = ctx.renderer->GetRender2D()) {
                NkRectF panel = {(float32)ctx.width - 320.f, 10.f, 310.f, 180.f};
                r2d->FillRect(panel, {0.f, 0.f, 0.f, 0.6f});
            }
            const float32 px = (float32)ctx.width - 310.f;
            overlay->DrawText({px, 30.f},  "== Shadow tweak (panel debug) ==");
            if (auto* sh = ctx.renderer->GetShadow()) {
                const auto& cfg = sh->GetConfig();
                overlay->DrawText({px, 50.f},  "[ / ]      bias     : %.4f", cfg.shadowBias);
                overlay->DrawText({px, 70.f},  " VSM atlas : %u px",         sh->GetAtlasSize());
                overlay->DrawText({px, 90.f},  " P         quality  : %d",   (int)cfg.quality);
                overlay->DrawText({px, 110.f}, " N / M     softness : %.3f", cfg.softness);
                overlay->DrawText({px, 130.f}, " slots: %u (rend %u | cache %u)",
                                   sh->GetActiveSlotCount(),
                                   sh->GetRenderedSlotsCount(),
                                   sh->GetCachedSlotsCount());
            } else {
                overlay->DrawText({px, 50.f}, "(no shadow system)");
            }
            overlay->DrawText({px, 160.f}, "framesInFlight : %u",
                              (uint32)ctx.renderer->GetConfig().framesInFlight);

            overlay->EndOverlay();
        }

        ctx.renderer->Present();
        ctx.renderer->EndFrame();
    }

    void Demo3D_Shutdown(DemoCtx& ctx) {
        delete (Demo3DState*)ctx.userData;
        ctx.userData = nullptr;
        logger.Info("[Demo3D] Shutdown\n");
    }

}} // namespace nkentseu::demo
