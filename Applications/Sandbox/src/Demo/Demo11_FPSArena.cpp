// =============================================================================
// Demo11_FPSArena.cpp  — Arene FPS style ce_que_je_veux.jpg
//
// Reproduit l'esthetique de l'image de reference :
//   - Sol : cube aplati avec dalles jaune-orange (tiles)
//   - Murs : 4 cubes blancs avec petites dalles carrees claires (pas briques)
//   - Cubes rouges en barricades de differentes tailles
//   - Cylindres bleus en barils
//   - Sphere grise metal
//   - Eclairage doux : sun directionnel pur + ambient sky bleu clair
//
// Camera FPS :
//   - WASD     : se deplacer (forward/strafe)
//   - Souris   : viser (yaw + pitch) — bouge la souris dans la fenetre
//   - SPACE / LCTRL : monter / descendre (free-fly, pas de gravite)
//   - SHIFT    : sprint x2
//   - TAB      : toggle souris capturee (recentrage auto)
//   - ESC      : quitter (handled by main.cpp)
//
// IMPORTANT : NkInput.IsKeyDown / MouseDeltaX/Y ne fonctionnent pas sur Win32
// actuellement (cf. feedback_nkinput_polling_win32). On utilise donc le
// pattern callback de DemoCamera : maintien d'un state local mis a jour via
// NkEvents().AddEventCallback.
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"
#include "NKRenderer/Tools/Text/NkTextRenderer.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKWindow/Core/NkWindow.h"
#include <cmath>
#include <vector>

namespace nkentseu {
    namespace demo {

        // =====================================================================
        // FPS camera (etat + update poll-based via NkInput)
        // =====================================================================
        struct FPSCamera {
            NkVec3f pos    = {0.f, 1.7f, 6.f};   // hauteur yeux ~1.7m
            float32 yaw    = -1.5707963f;        // -pi/2 = regarde -Z
            float32 pitch  = 0.f;
            float32 fovY   = 75.f;
            float32 speed  = 5.f;                // m/s
            float32 sens   = 0.0030f;            // rad par pixel

            // Etat input (mis a jour via callbacks NkEvents)
            bool keyW=false, keyA=false, keyS=false, keyD=false;
            // Arrow keys : meme effet que WASD (ergonomic alternative).
            bool keyArrowU=false, keyArrowD=false, keyArrowL=false, keyArrowR=false;
            bool keyUp=false, keyDown=false;       // monter/descendre (SPACE/LCTRL)
            bool keySprint=false;
            bool mouseLook=true;                   // toggle TAB
            float32 mouseDx=0.f, mouseDy=0.f;      // delta accumule par les events

            NkVec3f Forward() const {
                const float32 cp = cosf(pitch);
                return { cp * cosf(yaw), sinf(pitch), cp * sinf(yaw) };
            }
            NkVec3f Right() const {
                // Right = cross(forward, up) horizontalise. La matrice de vue
                // (NkCamera3D::RebuildImpl) calcule right = cross(fwd, up) :
                // pour fwd=-Z, up=+Y -> right=+X. On DOIT matcher ce signe sinon
                // le strafe est inverse (gauche<->droite). Avec fwd={0,0,-1} :
                // r = {-f.z, 0, f.x} = {1,0,0} = +X (droite de l'ecran). OK.
                NkVec3f f = Forward();
                NkVec3f r = { -f.z, 0.f, f.x };
                const float32 l = sqrtf(r.x*r.x + r.z*r.z);
                if (l > 1e-6f) { r.x /= l; r.z /= l; }
                return r;
            }

            // Tick : applique mouse delta accumule + WASD.
            void Update(float32 dt) {
                if (mouseLook) {
                    yaw   += mouseDx * sens;
                    pitch -= mouseDy * sens;
                    const float32 kLim = 1.553f;
                    if (pitch >  kLim) pitch =  kLim;
                    if (pitch < -kLim) pitch = -kLim;
                }
                mouseDx = mouseDy = 0.f;           // consume

                float32 fwd = 0.f, str = 0.f, up = 0.f;
                if (keyW || keyArrowU) fwd += 1.f;
                if (keyS || keyArrowD) fwd -= 1.f;
                if (keyD || keyArrowR) str += 1.f;
                if (keyA || keyArrowL) str -= 1.f;
                if (keyUp)   up += 1.f;
                if (keyDown) up -= 1.f;

                float32 v = speed * (keySprint ? 2.f : 1.f);
                const NkVec3f f = Forward();
                const NkVec3f r = Right();
                pos.x += (f.x * fwd + r.x * str)      * v * dt;
                pos.y += (f.y * fwd + 1.f * up)       * v * dt;
                pos.z += (f.z * fwd + r.z * str)      * v * dt;
            }

            void Apply(NkCamera3D& cam) const {
                cam.SetPosition(pos);
                NkVec3f tgt = { pos.x + cosf(pitch)*cosf(yaw),
                                pos.y + sinf(pitch),
                                pos.z + cosf(pitch)*sinf(yaw) };
                cam.SetTarget(tgt);
            }
        };

        // =====================================================================
        // Demo11 state
        // =====================================================================
        struct Demo11State {
            NkMeshHandle meshCube;
            NkMeshHandle meshSphere;
            NkMeshHandle meshCylinder;

            // Materials (PBR avec textures procedural ou couleurs simples)
            NkMaterial*  matFloor    = nullptr;   // sol jaune-orange tiles
            NkMaterial*  matWall     = nullptr;   // murs carres blancs
            NkMaterial*  matBarricade= nullptr;   // cube rouge
            NkMaterial*  matBarrel   = nullptr;   // cylindre bleu
            NkMaterial*  matSphere   = nullptr;   // sphere grise

            // Textures procedural (REPEAT sampler par defaut sur Create())
            NkTexHandle  texFloor;
            NkTexHandle  texWall;

            FPSCamera    cam;

            // Mouse mode toggles (F1/F2/F3)
            bool mouseHidden  = false;   // F1 : ShowMouse(false)
            bool mouseClipped = false;   // F2 : ClipMouseToClient(true)

            // Textes 3D extrudes : 3 sur murs + 1 couche sur sol
            NkMeshHandle textRihen;       // mur -Z (entree)
            NkMeshHandle textNkentseu;    // mur +X (droite)
            NkMeshHandle textNoge;        // mur +Z (face)
            NkMeshHandle textNKRenderer;  // couche sur le sol
            NkMaterial*  matText = nullptr;
        };

        // =====================================================================
        // Procedural textures
        // =====================================================================

        // Texture = 1 SEULE tile remplissant 256x256 pixels. Le triplanar
        // controle la repetition mondiale (tileSize en metres). Comme ca la
        // grille du sol et celle du mur ont EXACTEMENT le meme pas visuel
        // si on leur passe le meme tileSize.
        //
        // Layout : carre plein avec joint sombre sur 2 bords (top + left)
        // pour que la repetition forme une vraie grille continue (REPEAT
        // sampler aligne le joint top du tile suivant avec le joint bottom
        // du courant -> grille parfaite sans seam).
        static NkTexHandle CreateSingleTileTexture(NkTextureLibrary* texLib,
                                                    NkVec3f tileColor,
                                                    NkVec3f groutColor,
                                                    const char* dbgName)
        {
            const uint32 W = 256, H = 256;
            const uint32 grout = 6;            // 6/256 = 2.3% du tile en grout
            std::vector<uint8_t> px(W * H * 4);
            for (uint32 y = 0; y < H; ++y) {
                for (uint32 x = 0; x < W; ++x) {
                    // Joint sur top et left uniquement (sera complete par le
                    // top du tile suivant grace au REPEAT sampler).
                    bool isGrout = (x < grout) || (y < grout);
                    // Variation tres legere par pixel pour eviter look plastique.
                    uint32 h = (x * 73856093u) ^ (y * 19349663u);
                    int8 dv = (int8)((h & 0x07) - 4);   // -4..+3
                    NkVec3f c = isGrout ? groutColor : tileColor;
                    uint8 r = (uint8)NkMax(0, NkMin(255, (int)(c.x * 255.f) + dv));
                    uint8 g = (uint8)NkMax(0, NkMin(255, (int)(c.y * 255.f) + dv));
                    uint8 b = (uint8)NkMax(0, NkMin(255, (int)(c.z * 255.f) + dv));
                    uint32 idx = (y * W + x) * 4;
                    px[idx + 0] = r;
                    px[idx + 1] = g;
                    px[idx + 2] = b;
                    px[idx + 3] = 255;
                }
            }
            NkTextureCreateDesc d;
            d.pixels    = px.data();
            d.width     = W;
            d.height    = H;
            d.mipLevels = 1;
            d.format    = NkGPUFormat::NK_RGBA8_UNORM;
            d.srgb      = true;
            d.debugName = dbgName;
            return texLib->Create(d);
        }

        // Sol : jaune-orange + joint marron fonce
        static NkTexHandle CreateFloorTileTexture(NkTextureLibrary* texLib) {
            return CreateSingleTileTexture(texLib,
                {0.92f, 0.62f, 0.18f},          // jaune-orange tile
                {0.22f, 0.18f, 0.10f},          // joint marron
                "Demo11_FloorTile");
        }
        // Mur : blanc casse + joint gris clair
        static NkTexHandle CreateWallTileTexture(NkTextureLibrary* texLib) {
            return CreateSingleTileTexture(texLib,
                {0.92f, 0.92f, 0.90f},          // tile blanc casse
                {0.68f, 0.68f, 0.66f},          // joint gris
                "Demo11_WallTile");
        }

        // =====================================================================
        // Init / Frame / Shutdown
        // =====================================================================
        bool Demo11_FPSArena_Init(DemoCtx& ctx) {
            auto* st = new Demo11State();
            ctx.userData = st;

            auto* meshSys = ctx.renderer->GetMeshSystem();
            auto* matSys  = ctx.renderer->GetMaterials();
            auto* texLib  = ctx.renderer->GetTextures();
            if (!meshSys || !matSys || !texLib) {
                logger.Errorf("[Demo11] Sous-systemes manquants\n");
                delete st; ctx.userData = nullptr; return false;
            }
            st->meshCube     = meshSys->GetCube();
            st->meshSphere   = meshSys->GetSphere();
            st->meshCylinder = meshSys->GetCylinder();

            st->texFloor = CreateFloorTileTexture(texLib);
            st->texWall  = CreateWallTileTexture (texLib);

            // Materials PBR — triplanar pour sol + murs avec MEME tileSize :
            // grille identique entre sol et murs. 0.5m / tile = carrelage de
            // 50 cm visible peu importe le scale du cube (cube de 24m ou 1m,
            // pareil).
            const float32 kTileMeters = 0.5f;
            st->matFloor = NkMaterial::Create(matSys, NkMaterialType::NK_PBR_METALLIC);
            if (st->matFloor && st->matFloor->IsValid()) {
                st->matFloor->SetAlbedoMap(st->texFloor)
                            ->SetMetallic(0.0f)
                            ->SetRoughness(0.7f)
                            ->SetTriplanarTileSize(kTileMeters);
            }
            st->matWall = NkMaterial::Create(matSys, NkMaterialType::NK_PBR_METALLIC);
            if (st->matWall && st->matWall->IsValid()) {
                st->matWall->SetAlbedoMap(st->texWall)
                           ->SetMetallic(0.0f)
                           ->SetRoughness(0.85f)
                           ->SetTriplanarTileSize(kTileMeters);
            }
            st->matBarricade = NkMaterial::Create(matSys, NkMaterialType::NK_PBR_METALLIC);
            if (st->matBarricade && st->matBarricade->IsValid()) {
                st->matBarricade->SetAlbedo({0.82f, 0.32f, 0.28f})   // rouge-saumon
                                ->SetMetallic(0.0f)
                                ->SetRoughness(0.75f);
            }
            st->matBarrel = NkMaterial::Create(matSys, NkMaterialType::NK_PBR_METALLIC);
            if (st->matBarrel && st->matBarrel->IsValid()) {
                st->matBarrel->SetAlbedo({0.28f, 0.55f, 0.85f})      // bleu plastique
                             ->SetMetallic(0.0f)
                             ->SetRoughness(0.55f);
            }
            st->matSphere = NkMaterial::Create(matSys, NkMaterialType::NK_PBR_METALLIC);
            if (st->matSphere && st->matSphere->IsValid()) {
                st->matSphere->SetAlbedo({0.75f, 0.75f, 0.75f})
                             ->SetMetallic(0.2f)
                             ->SetRoughness(0.45f);
            }

            // ── Material pour les textes 3D (couleur doree, peu rugueux) ─────
            st->matText = NkMaterial::Create(matSys, NkMaterialType::NK_PBR_METALLIC);
            if (st->matText && st->matText->IsValid()) {
                st->matText->SetAlbedo({0.85f, 0.65f, 0.20f})   // dore
                           ->SetMetallic(0.7f)
                           ->SetRoughness(0.35f);
            }

            // ── Generation des 4 textes 3D extrudes via NKFont ───────────────
            // NkTextRenderer::ExtrudeText3D retourne un NkMeshHandle directe-
            // ment soumissible a NkRender3D. Le scale=1 + extrusionDepth=ED
            // donnent un texte ~ "size" pixels haut dans le repere local.
            // Le texte local est sur le plan XY, extrude le long de Z.
            //   X = direction lecture (gauche -> droite)
            //   Y = vertical (baseline a Y=0, ascender vers +Y)
            //   Z = profondeur d'extrusion (avant face vers -Z, arriere +Z)
            // Pour l'instant on genere a l'identite ; la position/rotation est
            // appliquee per-drawcall via dc.transform dans Frame().
            if (auto* txt = ctx.renderer->GetTextRenderer()) {
                NkFontHandle font = txt->GetDefaultFont();
                // scale * fontSize pixels -> echelle world. Avec font 13px et
                // scale=0.04 -> chaque glyphe ~ 0.5m de haut. Bon pour mur.
                const float32 sc = 0.04f;
                const float32 ed = 0.15f;   // 15 cm d'extrusion
                st->textRihen      = txt->ExtrudeText3D("Rihen",      font, sc, ed, meshSys);
                st->textNkentseu   = txt->ExtrudeText3D("Nkentseu",   font, sc, ed, meshSys);
                st->textNoge       = txt->ExtrudeText3D("Noge",       font, sc, ed, meshSys);
                // Texte du sol : un peu plus gros (~80cm de haut)
                st->textNKRenderer = txt->ExtrudeText3D("NKRenderer", font, sc * 1.6f, ed, meshSys);
            }

            // ── Input : callbacks (NkInput poll cassé sur Win32) ─────────────
            auto* state = st;
            auto SetKey = [state](NkKey k, bool down) {
                switch (k) {
                    case NkKey::NK_W:      state->cam.keyW      = down; break;
                    case NkKey::NK_A:      state->cam.keyA      = down; break;
                    case NkKey::NK_S:      state->cam.keyS      = down; break;
                    case NkKey::NK_D:      state->cam.keyD      = down; break;
                    case NkKey::NK_UP:     state->cam.keyArrowU = down; break;
                    case NkKey::NK_DOWN:   state->cam.keyArrowD = down; break;
                    case NkKey::NK_LEFT:   state->cam.keyArrowL = down; break;
                    case NkKey::NK_RIGHT:  state->cam.keyArrowR = down; break;
                    case NkKey::NK_SPACE:  state->cam.keyUp     = down; break;
                    case NkKey::NK_LCTRL:  state->cam.keyDown   = down; break;
                    case NkKey::NK_LSHIFT: state->cam.keySprint = down; break;
                    default: break;
                }
            };
            // Mouse mode helpers (F1/F2/F3) :
            //   F1 = toggle hide mouse (ShowMouse(false/true))
            //   F2 = toggle clip mouse in window (ClipMouseToClient(true/false))
            //   F3 = toggle both simultanement
            auto* win = ctx.window;
            auto ApplyMouseModes = [state, win]() {
                if (!win) return;
                win->ShowMouse(!state->mouseHidden);
                win->ClipMouseToClient(state->mouseClipped);
            };
            NkEvents().AddEventCallback<NkKeyPressEvent>([state, SetKey, ApplyMouseModes](NkKeyPressEvent* e) {
                if (!e) return;
                SetKey(e->GetKey(), true);
                switch (e->GetKey()) {
                    case NkKey::NK_TAB:
                        state->cam.mouseLook = !state->cam.mouseLook;
                        logger.Info("[Demo11] mouseLook = {0}\n", state->cam.mouseLook);
                        break;
                    case NkKey::NK_F1:
                        state->mouseHidden = !state->mouseHidden;
                        ApplyMouseModes();
                        logger.Info("[Demo11] F1 : mouseHidden = {0}\n", state->mouseHidden);
                        break;
                    case NkKey::NK_F2:
                        state->mouseClipped = !state->mouseClipped;
                        ApplyMouseModes();
                        logger.Info("[Demo11] F2 : mouseClipped = {0}\n", state->mouseClipped);
                        break;
                    case NkKey::NK_F3: {
                        // Toggle both : si l'un des deux est off, on les active
                        // tous les deux ; sinon on les coupe tous les deux.
                        bool both = state->mouseHidden && state->mouseClipped;
                        state->mouseHidden  = !both;
                        state->mouseClipped = !both;
                        ApplyMouseModes();
                        logger.Info("[Demo11] F3 : hide={0} clip={1}\n",
                                    state->mouseHidden, state->mouseClipped);
                        break;
                    }
                    default: break;
                }
            });
            NkEvents().AddEventCallback<NkKeyReleaseEvent>([SetKey](NkKeyReleaseEvent* e) {
                if (!e) return;
                SetKey(e->GetKey(), false);
            });
            // Camera rotation via NkMouseRawEvent : delta brut du capteur,
            // pas affecte par les bords de la fenetre. Permet rotation Y
            // illimitee (360+ deg) meme sans clip de souris. NkMouseMoveEvent
            // serait clamp aux bords client.
            NkEvents().AddEventCallback<NkMouseRawEvent>([state](NkMouseRawEvent* e) {
                if (!e || !state->cam.mouseLook) return;
                state->cam.mouseDx += (float32)e->GetDeltaX();
                state->cam.mouseDy += (float32)e->GetDeltaY();
            });

            // Shadow : cascade 0 fixe a radius 25m (couvre l'arene 30x30,
            // diagonale ~42m -> 25 suffit centre origine). NkVirtualShadowMaps
            // utilise useFixedCascadeRadius=true par defaut -> sphere fixe
            // par cascade, snap-to-texel sur grille world stable. Texels
            // d'ombre ancres sur le monde, pas sur le frustum camera ->
            // pas de shimmering quand la camera bouge.
            if (auto* shadowSys = ctx.renderer->GetShadow()) {
                auto& sc = shadowSys->GetConfig();
                sc.useFixedCascadeRadius = true;
                sc.cascadeFixedRadius[0] = 25.f;   // cascade 0 = toute l'arene
                // Center ancre au monde (origine) : l'arene 30x30 est close et
                // entierement couverte par cette unique cascade de radius 25.
                // Ancrer le centre au monde (au lieu de suivre la camera) ancre
                // les texels d'ombre au sol -> plus de glissement d'ombre quand
                // le joueur se deplace (shadow swimming elimine).
                sc.useFixedCascadeCenter = true;
                sc.cascadeWorldCenter    = {0.f, 0.f, 0.f};
            }

            logger.Info("[Demo11] === FPS Arena ===\n");
            logger.Info("[Demo11] WASD/Arrows : deplacer | Souris : viser | SPACE/CTRL : monter/descendre\n");
            logger.Info("[Demo11] SHIFT : sprint | TAB : toggle souris look\n");
            logger.Info("[Demo11] F1 : cacher souris | F2 : confiner a fenetre | F3 : les deux | ESC : quitter\n");
            return true;
        }

        void Demo11_FPSArena_Frame(DemoCtx& ctx, float32 dt) {
            auto* st = (Demo11State*)ctx.userData;
            st->cam.Update(dt);

            // DIAG (gated NK_DEMO11_AUTOPAN) : translation laterale automatique
            // de la camera (oscillation X) pour reproduire/observer le swimming
            // d'ombre quand la camera se deplace, sans input clavier. 0 effet
            // sans la variable d'env.
            static int autopan = -1;
            if (autopan == -1) {
                const char* v = getenv("NK_DEMO11_AUTOPAN");
                autopan = (v && v[0] && v[0] != '0') ? 1 : 0;
            }
            if (autopan) {
                // Oscille X entre -8 et +8 metres, regard fixe vers -Z.
                st->cam.pos.x   = 8.f * sinf(ctx.totalTime * 0.6f);
                st->cam.pos.y   = 1.7f;
                st->cam.pos.z   = 6.f;
                st->cam.yaw     = -1.5707963f;
                st->cam.pitch   = 0.f;
            }

            if (!ctx.renderer->BeginFrame()) return;
            auto* r3d = ctx.renderer->GetRender3D();
            if (!r3d) { ctx.renderer->Present(); ctx.renderer->EndFrame(); return; }

            // ── Camera ───────────────────────────────────────────────────────
            NkCamera3DData camData;
            camData.up        = {0.f, 1.f, 0.f};
            camData.fovY      = st->cam.fovY;
            camData.aspect    = (float32)ctx.width / (float32)ctx.height;
            camData.nearPlane = 0.05f;
            camData.farPlane  = 200.f;
            NkCamera3D cam(camData);
            st->cam.Apply(cam);

            // ── Lights : sun doux + ambient (look outdoor sky comme ref) ─────
            // Pas de point lights (responsable de l'overexposure precedent).
            NkSceneContext sctx;
            sctx.camera = cam;
            sctx.time   = ctx.totalTime;

            NkLightDesc sun;
            sun.type        = NkLightType::NK_DIRECTIONAL;
            sun.direction   = {-0.30f, -1.f, -0.25f};
            sun.color       = {1.f, 0.97f, 0.92f};
            sun.intensity   = 2.2f;           // doux (avant : 3.2 trop fort)
            sun.castShadow  = true;
            sun.shadowStatic= true;
            sctx.lights.PushBack(sun);

            sctx.ambientIntensity = 0.35f;    // sky ambient (avant : 0.18 trop sombre)

            r3d->BeginScene(sctx);

            // =================================================================
            // Helpers (toutes les dimensions en METRES — 1 unit = 1m)
            // =================================================================
            // SubmitBoxM : pos = centre, size = dimensions en metres.
            auto SubmitBoxM = [&](NkVec3f pos, NkVec3f sizeM, NkMaterial* m,
                                   bool castShadow = true) {
                NkDrawCall3D dc;
                dc.mesh      = st->meshCube;
                dc.transform = NkMat4f::Translate(pos) * NkMat4f::Scale(sizeM);
                NkVec3f half = { sizeM.x * 0.5f, sizeM.y * 0.5f, sizeM.z * 0.5f };
                dc.aabb      = {{pos.x - half.x, pos.y - half.y, pos.z - half.z},
                                {pos.x + half.x, pos.y + half.y, pos.z + half.z}};
                dc.castShadow= castShadow;
                dc.tint      = {1.f, 1.f, 1.f};
                if (m && m->IsValid()) dc.material = m->GetInstHandle();
                r3d->Submit(dc);
            };
            // Cube carre (1m, 2m...). pos = centre, sideM = arete en metres.
            // pos.y devrait etre sideM*0.5 si le cube est pose au sol.
            auto SubmitCubeM = [&](NkVec3f pos, float32 sideM, NkMaterial* m) {
                SubmitBoxM(pos, {sideM, sideM, sideM}, m);
            };
            // Sphere : pos = centre, diamM = diametre en metres. La primitive
            // sphere a diametre natif 1m (rayon 0.5) -> scale = diamM.
            auto SubmitSphereM = [&](NkVec3f pos, float32 diamM, NkMaterial* m) {
                const float32 s = diamM;
                const float32 r = diamM * 0.5f;
                NkDrawCall3D dc;
                dc.mesh      = st->meshSphere;
                dc.transform = NkMat4f::Translate(pos) * NkMat4f::Scale({s,s,s});
                dc.aabb      = {{pos.x - r, pos.y - r, pos.z - r},
                                {pos.x + r, pos.y + r, pos.z + r}};
                dc.castShadow= true;
                if (m && m->IsValid()) dc.material = m->GetInstHandle();
                r3d->Submit(dc);
            };
            // Cylindre vertical (baril) : pos = base, diamM = diametre,
            // heightM = hauteur. Primitive cylindre a diametre 1m, hauteur 1m.
            auto SubmitCylinderM = [&](NkVec3f basePos, float32 diamM,
                                        float32 heightM, NkMaterial* m) {
                const float32 r = diamM * 0.5f;
                NkVec3f c = { basePos.x, basePos.y + heightM * 0.5f, basePos.z };
                NkDrawCall3D dc;
                dc.mesh      = st->meshCylinder;
                dc.transform = NkMat4f::Translate(c) * NkMat4f::Scale({diamM, heightM, diamM});
                dc.aabb      = {{basePos.x - r, basePos.y, basePos.z - r},
                                {basePos.x + r, basePos.y + heightM, basePos.z + r}};
                dc.castShadow= true;
                if (m && m->IsValid()) dc.material = m->GetInstHandle();
                r3d->Submit(dc);
            };
            // Escalier : nSteps marches montant dans direction +dir (X ou Z).
            // basePos = bord bas-milieu de la 1re marche. widthM = largeur
            // perpendiculaire a dir. Chaque marche = stepDepthM x stepHeightM.
            // Total hauteur top = nSteps * stepHeightM.
            auto SubmitStaircase = [&](NkVec3f basePos, NkVec3f dir,
                                        int nSteps, float32 stepDepthM,
                                        float32 stepHeightM, float32 widthM,
                                        NkMaterial* m) {
                for (int i = 0; i < nSteps; i++) {
                    float32 sY = (i + 1) * stepHeightM * 0.5f;
                    float32 sH = (i + 1) * stepHeightM;       // marche pleine du sol -> visible quand on regarde de cote
                    NkVec3f stepPos = {
                        basePos.x + dir.x * (i * stepDepthM + stepDepthM * 0.5f),
                        basePos.y + sY,
                        basePos.z + dir.z * (i * stepDepthM + stepDepthM * 0.5f),
                    };
                    NkVec3f stepSize = {
                        (dir.x != 0.f) ? stepDepthM : widthM,
                        sH,
                        (dir.z != 0.f) ? stepDepthM : widthM,
                    };
                    SubmitBoxM(stepPos, stepSize, m);
                }
            };

            // =================================================================
            // SOL : cube aplati 30m x 0.5m x 30m centre en Y=-0.25
            // =================================================================
            SubmitBoxM({0.f, -0.25f, 0.f}, {30.f, 0.5f, 30.f}, st->matFloor, false);

            // =================================================================
            // ENCEINTE EXTERIEURE : 4 murs 30m x 4m x 0.4m
            // =================================================================
            const float32 outH = 4.f, outT = 0.4f, outL = 30.f;
            SubmitBoxM({0.f,    outH*0.5f,  15.f}, {outL, outH, outT}, st->matWall);  // +Z
            SubmitBoxM({0.f,    outH*0.5f, -15.f}, {outL, outH, outT}, st->matWall);  // -Z
            SubmitBoxM({ 15.f,  outH*0.5f,  0.f }, {outT, outH, outL}, st->matWall);  // +X
            SubmitBoxM({-15.f,  outH*0.5f,  0.f }, {outT, outH, outL}, st->matWall);  // -X

            // =================================================================
            // MURS INTERNES : 3 pieces (gauche, centre-bas, droite).
            // Chaque mur interne fait 3m de haut (plus bas que l'enceinte) avec
            // une ouverture libre (on construit 2 segments laissant un gap).
            // =================================================================
            const float32 iH = 3.f, iT = 0.3f;
            // Mur vertical separant gauche/centre, a X=-6, allant Z=-15 a +15
            // avec une ouverture de 4m centree en Z=0.
            {
                // segment 1 : Z=-15 a Z=-2 (longueur 13)
                SubmitBoxM({-6.f, iH*0.5f, -8.5f}, {iT, iH, 13.f}, st->matWall);
                // segment 2 : Z=+2 a Z=+15 (longueur 13)
                SubmitBoxM({-6.f, iH*0.5f,  8.5f}, {iT, iH, 13.f}, st->matWall);
            }
            // Mur vertical separant centre/droite, a X=+6, avec ouverture
            // centree en Z=0 de 4m.
            {
                SubmitBoxM({ 6.f, iH*0.5f, -8.5f}, {iT, iH, 13.f}, st->matWall);
                SubmitBoxM({ 6.f, iH*0.5f,  8.5f}, {iT, iH, 13.f}, st->matWall);
            }
            // Mur horizontal interne dans la piece centrale (Z=+5), avec
            // ouverture de 4m centree en X=0. Cree une sub-piece dans le centre.
            {
                SubmitBoxM({-4.f, iH*0.5f,  5.f}, {4.f, iH, iT}, st->matWall);
                SubmitBoxM({ 4.f, iH*0.5f,  5.f}, {4.f, iH, iT}, st->matWall);
            }

            // =================================================================
            // PLATEFORME SURELEVEE + ESCALIER (piece de droite)
            // Plateforme 6m x 6m a hauteur 2m, accessible par escalier coté -Z.
            // =================================================================
            const float32 plY = 2.f, plThick = 0.3f;
            SubmitBoxM({10.f, plY + plThick*0.5f, 8.f}, {6.f, plThick, 6.f}, st->matWall);

            // Escalier : 10 marches de 0.2m de haut, 0.4m de profondeur,
            // 2m de large, montant de Z=4 a Z=8 (longueur 4m). Direction +Z.
            SubmitStaircase({10.f, 0.f, 3.6f}, {0.f, 0.f, 1.f},
                             /*nSteps=*/10, /*stepDepth=*/0.4f, /*stepH=*/0.2f,
                             /*width=*/2.f, st->matWall);

            // Petite plateforme intermediaire dans la piece centrale a hauteur 1m
            SubmitBoxM({0.f, 0.5f + 0.15f, 9.f}, {3.f, 0.3f, 3.f}, st->matWall);
            // Escalier court y menant (3 marches de 0.4m profondeur, 0.2m haut)
            SubmitStaircase({0.f, 0.f, 6.6f}, {0.f, 0.f, 1.f},
                             3, 0.4f, 0.2f, 2.f, st->matWall);

            // =================================================================
            // CUBES ROUGES (barricades) — UNIQUEMENT 1m ou 2m de cote
            // =================================================================
            // Piece de gauche
            SubmitCubeM({-11.f, 1.0f, -8.f}, 2.0f, st->matBarricade);   // 2m
            SubmitCubeM({-10.f, 0.5f, -4.f}, 1.0f, st->matBarricade);   // 1m
            SubmitCubeM({-12.f, 0.5f,  4.f}, 1.0f, st->matBarricade);   // 1m
            SubmitCubeM({ -9.f, 0.5f,  9.f}, 1.0f, st->matBarricade);   // 1m
            // Piece centrale
            SubmitCubeM({-2.f,  1.0f, -6.f}, 2.0f, st->matBarricade);   // 2m
            SubmitCubeM({ 3.f,  0.5f, -8.f}, 1.0f, st->matBarricade);   // 1m
            SubmitCubeM({ 0.f,  1.0f, -2.f}, 2.0f, st->matBarricade);   // 2m central
            // Sur la petite plateforme centrale (Y=1m)
            SubmitCubeM({-0.5f, 1.5f, 9.f}, 1.0f, st->matBarricade);
            // Piece de droite (autour de l'escalier/plateforme)
            SubmitCubeM({ 8.f,  0.5f, -8.f}, 1.0f, st->matBarricade);   // 1m
            SubmitCubeM({12.f,  1.0f, -4.f}, 2.0f, st->matBarricade);   // 2m
            // Sur la grande plateforme (Y=2m + 0.5m pour cube de 1m)
            SubmitCubeM({ 9.f,  2.8f,  9.f}, 1.0f, st->matBarricade);

            // =================================================================
            // SPHERES METAL — UNIQUEMENT 1m ou 2m de diametre
            // =================================================================
            SubmitSphereM({-4.f, 0.5f,  2.f}, 1.0f, st->matSphere);     // 1m, piece gauche
            SubmitSphereM({ 2.f, 1.0f,  2.f}, 2.0f, st->matSphere);     // 2m, centre
            SubmitSphereM({11.f, 0.5f,  4.f}, 1.0f, st->matSphere);     // 1m, piece droite
            SubmitSphereM({ 9.f, 2.8f,  6.f}, 1.0f, st->matSphere);     // sur plateforme

            // =================================================================
            // BARILS (cylindres bleus) — diametre 0.6m, hauteur 1.2m
            // =================================================================
            // Cluster piece gauche
            SubmitCylinderM({-11.f, 0.f, -2.f}, 0.6f, 1.2f, st->matBarrel);
            SubmitCylinderM({-10.f, 0.f, -1.f}, 0.6f, 1.2f, st->matBarrel);
            // Cluster centre
            SubmitCylinderM({ 1.5f, 0.f, -10.f}, 0.6f, 1.2f, st->matBarrel);
            SubmitCylinderM({ 2.5f, 0.f, -10.f}, 0.6f, 1.2f, st->matBarrel);
            SubmitCylinderM({ 2.0f, 0.f,  -9.f}, 0.6f, 1.2f, st->matBarrel);
            // Cluster piece droite + sous escalier
            SubmitCylinderM({12.f, 0.f, -2.f}, 0.6f, 1.2f, st->matBarrel);
            SubmitCylinderM({ 8.f, 0.f,  3.f}, 0.6f, 1.2f, st->matBarrel);

            // =================================================================
            // TEXTES 3D EXTRUDES
            // Le mesh texte est sur le plan local XY (X=lecture, Y=up,
            // Z=extrusion). On le pose contre un mur ou couche sur le sol
            // via une matrice de transformation.
            // =================================================================
            auto SubmitText3D = [&](NkMeshHandle mesh, const NkMat4f& xform) {
                if (!mesh.IsValid()) return;
                NkDrawCall3D dc;
                dc.mesh       = mesh;
                dc.transform  = xform;
                // AABB large (le mesh extruder fait son propre culling — on
                // donne une AABB englobante grossiere pour Que le culling
                // n'elimine pas le texte).
                dc.aabb       = {{-30.f, -30.f, -30.f}, {30.f, 30.f, 30.f}};
                dc.castShadow = true;
                dc.tint       = {1.f, 1.f, 1.f};
                if (st->matText && st->matText->IsValid())
                    dc.material = st->matText->GetInstHandle();
                r3d->Submit(dc);
            };

            // ── Rihen : mur -Z (entree), face +Z (vers l'interieur de l'arene)
            // Texte sur plan XY local, face -Z native (front face). Pour qu'il
            // soit lisible quand on regarde le mur -Z depuis l'interieur :
            //   - Position au mur -Z (z = -14.7, juste devant le mur a 0.3m
            //     pour qu'il "soit pose" sur sa surface)
            //   - Y = 2.5m (hauteur lisible)
            //   - X centre (-4 pour qu'il commence a gauche, le mesh va vers +X)
            SubmitText3D(st->textRihen,
                NkMat4f::Translate({-4.f, 2.5f, -14.7f}));

            // ── Nkentseu : mur +X (droite). Rotation -90 deg autour de Y pour
            // que le plan XY texte devienne plan YZ world (texte parallele au
            // mur +X). La face front (-Z local) finit pointant +X local rotated
            // -> on positionne juste devant le mur a x=14.7.
            {
                NkMat4f rotY = NkMat4f::RotationY(NkAngle::FromRad(-1.5707963f));
                NkMat4f trans = NkMat4f::Translate({14.7f, 2.5f, 4.f});
                SubmitText3D(st->textNkentseu, trans * rotY);
            }

            // ── Noge : mur +Z (face arriere de l'arene depuis spawn). Rotation
            // 180 deg autour de Y pour que la face front (-Z local) pointe -Z
            // world (= visible depuis l'interieur en regardant +Z).
            {
                NkMat4f rotY = NkMat4f::RotationY(NkAngle::FromRad(3.1415927f));
                NkMat4f trans = NkMat4f::Translate({2.f, 2.5f, 14.7f});
                SubmitText3D(st->textNoge, trans * rotY);
            }

            // ── NKRenderer : couche sur le sol (texte horizontal). Rotation
            // -90 deg autour de X pour rabattre le plan XY (vertical) sur le
            // plan XZ (horizontal). Le texte sera lisible si on le regarde
            // depuis +Y (vue de dessus) OU depuis -Z (depuis l'entree).
            {
                NkMat4f rotX = NkMat4f::RotationX(NkAngle::FromRad(-1.5707963f));
                // Y = 0.05 pour eviter z-fight avec le sol (sol a y=0)
                NkMat4f trans = NkMat4f::Translate({-4.f, 0.05f, 0.f});
                SubmitText3D(st->textNKRenderer, trans * rotX);
            }

            // ── Overlay HUD ──────────────────────────────────────────────────
            if (auto* overlay = ctx.renderer->GetOverlay()) {
                overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
                overlay->DrawStats(ctx.renderer->GetStats());
                overlay->DrawText({20.f, 35.f},
                    "Demo11 FPS Arena  |  API : %s  |  FPS : %.0f",
                    NkGraphicsApiName(ctx.api), dt > 1e-5f ? 1.f / dt : 0.f);
                overlay->DrawText({20.f, 55.f},
                    "Pos : %.1f %.1f %.1f  yaw=%.2f pitch=%.2f",
                    st->cam.pos.x, st->cam.pos.y, st->cam.pos.z,
                    st->cam.yaw, st->cam.pitch);
                overlay->DrawText({20.f, 75.f},
                    "WASD/Arrows : move  Mouse : look  SPACE/CTRL : up/dn  SHIFT : sprint  TAB : toggle mouseLook");
                overlay->DrawText({20.f, 95.f},
                    "MouseLook=%s   F1 hide=[%s]   F2 clip=[%s]   F3 both=[%s]",
                    st->cam.mouseLook ? "ON" : "OFF",
                    st->mouseHidden  ? "X" : " ",
                    st->mouseClipped ? "X" : " ",
                    (st->mouseHidden && st->mouseClipped) ? "X" : " ");
                overlay->EndOverlay();
            }

            ctx.renderer->Present();
            ctx.renderer->EndFrame();
        }

        void Demo11_FPSArena_Shutdown(DemoCtx& ctx) {
            auto* st = (Demo11State*)ctx.userData;
            if (!st) return;
            // Restaurer la souris dans son etat normal avant exit (sinon le
            // ClipCursor reste actif dans Windows pour les autres apps).
            if (ctx.window) {
                ctx.window->ShowMouse(true);
                ctx.window->ClipMouseToClient(false);
            }
            NkMaterial::Destroy(st->matFloor);
            NkMaterial::Destroy(st->matWall);
            NkMaterial::Destroy(st->matBarricade);
            NkMaterial::Destroy(st->matBarrel);
            NkMaterial::Destroy(st->matSphere);
            NkMaterial::Destroy(st->matText);
            // Release des 4 mesh handles textes 3D
            if (auto* meshSys = ctx.renderer->GetMeshSystem()) {
                meshSys->Release(st->textRihen);
                meshSys->Release(st->textNkentseu);
                meshSys->Release(st->textNoge);
                meshSys->Release(st->textNKRenderer);
            }
            auto* texLib = ctx.renderer->GetTextures();
            if (texLib) {
                texLib->Release(st->texFloor);
                texLib->Release(st->texWall);
            }
            delete st;
            ctx.userData = nullptr;
            logger.Info("[Demo11] Shutdown\n");
        }

    } // namespace demo
} // namespace nkentseu

/*la version de nvidia app est 11.0.7.247 de 2026.
alt+Z ouvre overlay mais pas de gpu turning ni performance dans le panneau qui apparait.*/