// =============================================================================
// Tutoriels3D / Étape 5 — VOS PROPRES MESHES + SÉLECTION À LA SOURIS
//
// On ajoute à l'étape 4 deux façons de créer sa géométrie :
//
//   1. UN MESH DÉFINI PAR SES SOMMETS DANS LE CODE : une pyramide écrite à la
//      main (tableau de NkVertex3D + indices) -> NkMeshDesc::Simple -> GPU.
//      Aucun fichier à charger : la géométrie EST le code.
//
//   2. UN MESH ÉDITABLE (NkEditMesh, structure demi-arête façon Blender) : on
//      part d'un simple quad, et les touches E (extruder) / C (subdiviser)
//      modifient sa TOPOLOGIE en direct ; R le réinitialise. Après chaque
//      édition on re-triangule vers le GPU.
//
// Et une interaction de plus : CLIC GAUCHE = sélectionner un objet (lancer de
// rayon caméra -> boîte englobante) ; l'objet sélectionné est surligné.
// La caméra vient de l'étape 4 (fichier partagé camera_input.h).
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkSafeArea.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h" // NkTan

#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"

#include "04-Camera/camera_input.h" // caméra orbite de l'étape 4, réutilisée telle quelle

// Win32 définit DrawText en macro (GDI) — collision avec NkOverlayRenderer::DrawText.
#ifdef DrawText
#undef DrawText
#endif

using namespace nkentseu;
using namespace nkentseu::renderer;

static void ConfigureAppData(NkAppData &d) {
    d.appName = "Tuto05Meshes";
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)

// ── 1er mesh custom : une PYRAMIDE écrite sommet par sommet ───────────────────
// 4 faces latérales à normales franches (sommets dupliqués par face) + base.
// Les tableaux CPU (outV/outIdx) sont GARDÉS par l'appelant : ils servent à la
// sélection précise par triangle (le GPU n'est qu'un cache de la géométrie).
static NkMeshHandle CreatePyramidMesh(NkMeshSystem *meshes, NkVector<NkVertex3D> &outV, NkVector<uint32> &outIdx) {
    const NkVec3f apex = {0.f, 1.f, 0.f};
    const NkVec3f base[4] = {{-0.6f, 0.f, -0.6f}, {0.6f, 0.f, -0.6f}, {0.6f, 0.f, 0.6f}, {-0.6f, 0.f, 0.6f}};

    NkVector<NkVertex3D> &v = outV;
    NkVector<uint32> &idx = outIdx;
    v.Clear();
    idx.Clear();
    auto pushTri = [&](NkVec3f a, NkVec3f b, NkVec3f c) {
        NkVec3f n = (b - a).Cross(c - a);
        const float32 l = n.Len();
        if (l > 1e-6f)
            n = n * (1.f / l);
        NkVertex3D vert{};
        vert.normal = n;
        vert.color = 0xFFFFFFFFu;
        vert.pos = a;
        vert.uv = {0.f, 0.f};
        idx.PushBack((uint32)v.Size());
        v.PushBack(vert);
        vert.pos = b;
        vert.uv = {1.f, 0.f};
        idx.PushBack((uint32)v.Size());
        v.PushBack(vert);
        vert.pos = c;
        vert.uv = {0.5f, 1.f};
        idx.PushBack((uint32)v.Size());
        v.PushBack(vert);
    };
    for (int32 i = 0; i < 4; i++)
        pushTri(base[i], base[(i + 1) % 4], apex); // 4 faces latérales
    pushTri(base[0], base[3], base[2]);               // base (2 triangles, normale -Y)
    pushTri(base[0], base[2], base[1]);

    // Un tableau de sommets + un tableau d'indices -> un mesh GPU. C'est tout.
    NkMeshDesc desc = NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(), v.Data(), (uint32)v.Size(), idx.Data(),
                                         (uint32)idx.Size());
    desc.debugName = "TutoPyramide";
    return meshes->Create(desc);
}

// ── 2e mesh custom : le MESH ÉDITABLE (demi-arête) ────────────────────────────
// Repart d'un quad 1x1 posé à plat. Les commandes d'édition mutent `em`.
static void ResetEditMesh(NkEditMesh &em) {
    NkVertex3D v[4] = {};
    const NkVec3f pos[4] = {{-0.5f, 0.f, -0.5f}, {0.5f, 0.f, -0.5f}, {0.5f, 0.f, 0.5f}, {-0.5f, 0.f, 0.5f}};
    for (int32 i = 0; i < 4; i++) {
        v[i].pos = pos[i];
        v[i].normal = {0.f, 1.f, 0.f};
        v[i].uv = {pos[i].x + 0.5f, pos[i].z + 0.5f};
        v[i].color = 0xFFFFFFFFu;
    }
    const uint32 faceStart[2] = {0, 4}; // 1 face (n-gon) : sommets [0..4[
    const uint32 faceVerts[4] = {0, 1, 2, 3};
    em.BuildFromPolygons(v, 4, faceStart, 1, faceVerts);
}

// Triangule le NkEditMesh vers un mesh GPU (recréé à chaque édition : la
// topologie change, donc le nombre de sommets aussi). Les triangles CPU sont
// aussi rendus à l'appelant (outV/outIdx) pour la sélection précise.
static NkMeshHandle UploadEditMesh(NkMeshSystem *meshes, NkEditMesh &em, NkMeshHandle old,
                                   NkVector<NkVertex3D> &outV, NkVector<uint32> &outIdx) {
    if (old.IsValid())
        meshes->Release(old);
    em.RecomputeNormals();
    NkVector<NkEmId> triFace;
    em.Triangulate(outV, outIdx, triFace);
    NkMeshDesc desc = NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(), outV.Data(), (uint32)outV.Size(),
                                         outIdx.Data(), (uint32)outIdx.Size());
    desc.debugName = "TutoEditMesh";
    return meshes->Create(desc);
}

// ── Sélection : rayon caméra -> boîte englobante (test des "slabs") ──────────
static bool RayHitsAABB(NkVec3f ro, NkVec3f rd, const NkAABB &box, float32 &outT) {
    float32 tmin = 0.f, tmax = 1e30f;
    const float32 o[3] = {ro.x, ro.y, ro.z}, d[3] = {rd.x, rd.y, rd.z};
    const float32 bmin[3] = {box.min.x, box.min.y, box.min.z}, bmax[3] = {box.max.x, box.max.y, box.max.z};
    for (int32 a = 0; a < 3; a++) {
        if (d[a] > -1e-8f && d[a] < 1e-8f) {
            if (o[a] < bmin[a] || o[a] > bmax[a])
                return false; // rayon parallèle hors de la tranche
            continue;
        }
        float32 t0 = (bmin[a] - o[a]) / d[a], t1 = (bmax[a] - o[a]) / d[a];
        if (t0 > t1) {
            const float32 tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        if (t0 > tmin)
            tmin = t0;
        if (t1 < tmax)
            tmax = t1;
        if (tmin > tmax)
            return false;
    }
    outT = tmin;
    return true;
}

// ── Sélection PRÉCISE : rayon -> triangles (Möller–Trumbore) ─────────────────
// Teste le rayon contre CHAQUE triangle du mesh (translaté de `offset`) et
// retourne l'intersection la plus proche. C'est ce qui évite le piège du test
// AABB seul : la boîte englobante déborde de l'objet (surtout une pyramide ou
// un mesh extrudé), donc cliquer au-dessus/en-dessous sélectionnait "dans le
// vide". L'AABB reste utile comme pré-filtre rapide avant ce test exact.
static bool RayHitsTriangles(NkVec3f ro, NkVec3f rd, const NkVector<NkVertex3D> &v, const NkVector<uint32> &idx,
                             NkVec3f offset, float32 &outT) {
    bool hit = false;
    float32 best = 1e30f;
    for (uint32 k = 0; k + 2 < (uint32)idx.Size(); k += 3) {
        const NkVec3f a = v[idx[k]].pos + offset;
        const NkVec3f b = v[idx[k + 1]].pos + offset;
        const NkVec3f c = v[idx[k + 2]].pos + offset;
        const NkVec3f e1 = b - a, e2 = c - a;
        const NkVec3f p = rd.Cross(e2);
        const float32 det = e1.Dot(p);
        if (det > -1e-8f && det < 1e-8f)
            continue; // rayon parallèle au plan du triangle
        const float32 inv = 1.f / det;
        const NkVec3f s = ro - a;
        const float32 u = s.Dot(p) * inv;
        if (u < 0.f || u > 1.f)
            continue;
        const NkVec3f q = s.Cross(e1);
        const float32 w = rd.Dot(q) * inv;
        if (w < 0.f || u + w > 1.f)
            continue;
        const float32 t = e2.Dot(q) * inv;
        if (t > 1e-4f && t < best) {
            best = t;
            hit = true;
        }
    }
    if (hit)
        outT = best;
    return hit;
}

int nkmain(const NkEntryState &state) {
    (void)state;

    // ── Fenêtre + device + renderer (étapes 1-2) ──────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title = "Tuto 05 — Meshes custom + selection (clic) + edition (E/C/R)";
    winCfg.width = 1280;
    winCfg.height = 720;
    winCfg.centered = true;
    winCfg.resizable = true;

    NkWindow window(winCfg);
    if (!window.IsValid()) {
        logger.Error("[Tuto05] Creation fenetre KO");
        return 1;
    }

    NkDeviceInitInfo devInfo{};
    devInfo.surface = window.GetSurfaceDesc();
    devInfo.width = (uint32)window.GetSize().width;
    devInfo.height = (uint32)window.GetSize().height;

    NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
    if (!device || !device->IsValid()) {
        logger.Error("[Tuto05] Creation device KO");
        window.Close();
        return 2;
    }

    NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, devInfo.width, devInfo.height);
    NkRenderer *renderer = NkRenderer::Create(device, cfg);
    if (!renderer || !renderer->Initialize()) {
        logger.Error("[Tuto05] Init renderer KO");
        NkRenderer::Destroy(renderer);
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 3;
    }

    NkRender3D *r3d = renderer->GetRender3D();
    NkRender2D *r2d = renderer->GetRender2D();
    NkOverlayRenderer *overlay = renderer->GetOverlay();

    // Ombres STABLES malgré la caméra mobile : cascade ancrée au monde (auto-fit
    // aux casters), sinon les bords d'ombre vibrent pendant l'orbite (swimming).
    if (auto *shadow = renderer->GetShadow())
        shadow->GetConfig().autoFitDirectional = true;

    NkMeshSystem *meshes = renderer->GetMeshSystem();
    NkMeshHandle meshPlane = meshes->GetPlane();

    // ── Nos deux meshes custom ────────────────────────────────────────────────
    // On garde les triangles CPU de chaque objet (objV/objI) : c'est la source
    // de vérité pour la sélection précise par triangle (le GPU n'est qu'un cache).
    NkVector<NkVertex3D> objV[2];
    NkVector<uint32> objI[2];

    NkMeshHandle meshPyramide = CreatePyramidMesh(meshes, objV[0], objI[0]);

    NkEditMesh editMesh;
    ResetEditMesh(editMesh);
    NkMeshHandle meshEdit = UploadEditMesh(meshes, editMesh, NkMeshHandle{}, objV[1], objI[1]);
    uint32 editOps = 0; // compteur d'éditions (affiché dans le panneau)

    // ── La scène : liste d'objets sélectionnables ─────────────────────────────
    struct TutoObjet {
            const char *name;
            NkVec3f pos;
            NkVec3f tint;
            NkAABB aabbLocal; // avant translation
    };
    TutoObjet objets[2] = {
        {"Pyramide (sommets dans le code)", {-1.5f, 0.f, 0.f}, {0.95f, 0.45f, 0.2f}, {{-0.6f, 0.f, -0.6f}, {0.6f, 1.f, 0.6f}}},
        {"Mesh editable (E/C/R)", {1.5f, 0.6f, 0.f}, {0.3f, 0.7f, 0.95f}, {{-1.f, -0.6f, -1.f}, {1.f, 1.f, 1.f}}},
    };
    int32 selection = -1; // index de l'objet sélectionné (-1 = aucun)

    // ── Événements + caméra (étape 4) + clic de sélection ─────────────────────
    bool running = true;
    uint32 W = devInfo.width;
    uint32 H = devInfo.height;
    NkEventSystem &events = NkEvents();

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent *) { running = false; });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
        const uint32 w = (uint32)e->GetWidth(), h = (uint32)e->GetHeight();
        if (w > 0 && h > 0 && (w != W || h != H)) {
            W = w;
            H = h;
            renderer->OnResize(w, h);
        }
    });

    tuto::TutoCameraInput camInput;
    camInput.orbit.SetCenter({0.f, 0.5f, 0.f}, 6.5f, 0.9f, -0.35f);
    camInput.Install();

    // Touches d'édition : elles mutent le NkEditMesh, puis on re-triangule.
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *e) {
        const NkKey k = e->GetKey();
        if (k == NkKey::NK_ESCAPE) {
            running = false;
        } else if (k == NkKey::NK_E) {
            editMesh.SelectAll(); // tout le mesh…
            NkExtrudeParams p;
            p.offset = 0.35f; // …extrudé de 35 cm le long des normales
            if (editMesh.ExtrudeSelectedFaces(p)) {
                meshEdit = UploadEditMesh(meshes, editMesh, meshEdit, objV[1], objI[1]);
                editOps++;
            }
        } else if (k == NkKey::NK_C) {
            editMesh.SelectAll();
            if (editMesh.SubdivideSelectedFaces()) {
                meshEdit = UploadEditMesh(meshes, editMesh, meshEdit, objV[1], objI[1]);
                editOps++;
            }
        } else if (k == NkKey::NK_R) {
            ResetEditMesh(editMesh);
            meshEdit = UploadEditMesh(meshes, editMesh, meshEdit, objV[1], objI[1]);
            editOps = 0;
        }
    });

    // Sélection au TAP (clic bref souris, ou toucher sans glisser sur mobile —
    // fourni par camera_input.h, donc AUCUN conflit avec l'orbite au drag) :
    // rayon depuis la caméra vers le pixel visé, pré-filtre AABB puis test
    // EXACT par triangle (sinon on sélectionne "dans le vide" autour de l'objet).
    auto pickAt = [&](float32 px, float32 py) {
        const NkVec3f ro = camInput.orbit.GetPosition();
        const NkVec3f tg = camInput.orbit.GetTarget();
        NkVec3f fwd = tg - ro;
        const float32 fl = fwd.Len();
        if (fl < 1e-6f)
            return;
        fwd = fwd * (1.f / fl);
        NkVec3f rgt = fwd.Cross({0.f, 1.f, 0.f});
        const float32 rl = rgt.Len();
        if (rl < 1e-6f)
            return;
        rgt = rgt * (1.f / rl);
        const NkVec3f upv = rgt.Cross(fwd);
        // Demi-tangentes du frustum : la direction du rayon interpole le champ de vision.
        const float32 thY = math::NkTan(60.f * 0.5f * 3.14159265f / 180.f);
        const float32 thX = thY * ((float32)W / (float32)H);
        const float32 nx = px / (float32)W * 2.f - 1.f;
        const float32 ny = 1.f - py / (float32)H * 2.f;
        NkVec3f rd = fwd + rgt * (nx * thX) + upv * (ny * thY);
        rd = rd * (1.f / rd.Len());

        selection = -1;
        float32 best = 1e30f;
        for (int32 i = 0; i < 2; i++) {
            // 1) Pré-filtre grossier : la boîte englobante (rapide, mais plus
            //    large que l'objet — insuffisante seule pour une sélection juste).
            NkAABB world = {objets[i].aabbLocal.min + objets[i].pos, objets[i].aabbLocal.max + objets[i].pos};
            float32 tBox;
            if (!RayHitsAABB(ro, rd, world, tBox))
                continue;
            // 2) Test exact : le rayon touche-t-il VRAIMENT un triangle du mesh ?
            float32 t;
            if (RayHitsTriangles(ro, rd, objV[i], objI[i], objets[i].pos, t) && t < best) {
                best = t;
                selection = i;
            }
        }
    };

    // ── Boucle principale ─────────────────────────────────────────────────────
    NkClock clock;
    float32 total = 0.f;

    while (running && window.IsOpen()) {
        events.PollEvents();
        if (!running)
            break;

        const float32 dt = clock.Tick().delta;
        total += dt;
        camInput.Update(dt);

        // Un tap (clic bref / toucher sans glisser) = tentative de sélection.
        {
            float32 tx, ty;
            if (camInput.ConsumeTap(tx, ty))
                pickAt(tx, ty);
        }

        if (!renderer->BeginFrame())
            continue;

        NkCamera3DData camData;
        camData.up = {0.f, 1.f, 0.f};
        camData.fovY = 60.f;
        camData.aspect = (float32)W / (float32)H;
        camData.nearPlane = 0.1f;
        camData.farPlane = 100.f;
        NkCamera3D cam(camData);
        camInput.orbit.Apply(cam);

        NkSceneContext sctx;
        sctx.camera = cam;
        sctx.time = total;
        sctx.ambientIntensity = 0.15f;

        NkLightDesc sun;
        sun.type = NkLightType::NK_DIRECTIONAL;
        sun.direction = {-0.4f, -1.f, -0.3f};
        sun.color = {1.f, 0.95f, 0.85f};
        sun.intensity = 3.f;
        sun.castShadow = true;
        sun.shadowStatic = false; // caméra mobile + mesh édité en direct -> pas de cache d'ombre
        sctx.lights.PushBack(sun);

        r3d->BeginScene(sctx);

        { // sol
            NkDrawCall3D dc;
            dc.mesh = meshPlane;
            dc.transform = NkMat4f::Scale({12.f, 1.f, 12.f});
            dc.aabb = {{-12.f, 0.f, -12.f}, {12.f, 0.f, 12.f}};
            dc.castShadow = false;
            dc.tint = {0.14f, 0.14f, 0.16f};
            dc.metallic = 0.f;
            dc.roughness = 0.9f;
            r3d->Submit(dc);
        }

        // Les deux objets custom ; l'objet sélectionné est surligné (tint éclaircie).
        for (int32 i = 0; i < 2; i++) {
            NkDrawCall3D dc;
            dc.mesh = (i == 0) ? meshPyramide : meshEdit;
            dc.transform = NkMat4f::Translate(objets[i].pos);
            dc.aabb = {objets[i].aabbLocal.min + objets[i].pos, objets[i].aabbLocal.max + objets[i].pos};
            dc.tint = objets[i].tint;
            if (i == selection)
                dc.tint = {1.f, 0.85f, 0.35f}; // surlignage sélection
            dc.metallic = 0.1f;
            dc.roughness = 0.55f;
            r3d->Submit(dc);
        }

        if (overlay) {
            // Safe area : sur mobile (encoche, barres système) on décale le
            // panneau pour rester dans la zone visible ; ailleurs insets = 0.
            const NkSafeAreaInsets sa = window.GetSafeAreaInsets();
            const float32 ox = 10.f + sa.left, oy = 10.f + sa.top;
            overlay->BeginOverlay(renderer->GetCmd(), W, H);
            if (r2d)
                r2d->FillRect({ox, oy, 460.f, 110.f}, {0.f, 0.f, 0.f, 0.6f});
            overlay->DrawText({ox + 10.f, oy + 20.f}, "== Tuto 05 : meshes custom + selection ==");
            overlay->DrawText({ox + 10.f, oy + 40.f}, "Tap/clic bref: selectionner | Selection: %s",
                              selection >= 0 ? objets[selection].name : "(aucune)");
            overlay->DrawText({ox + 10.f, oy + 60.f}, "E: extruder  C: subdiviser  R: reset (%u editions, %u faces)",
                              editOps, editMesh.FaceCount());
            overlay->DrawText({ox + 10.f, oy + 80.f}, "Camera: drag orbite | Shift+drag pan | molette/pincer zoom | WASD");
            overlay->DrawText({ox + 10.f, oy + 100.f}, "FPS ~ %.1f | Echap = quitter", dt > 1e-4f ? 1.f / dt : 0.f);
            overlay->EndOverlay();
        }

        renderer->Present();
        renderer->EndFrame();
    }

    // ── Fermeture propre ──────────────────────────────────────────────────────
    device->WaitIdle();
    meshes->Release(meshPyramide);
    meshes->Release(meshEdit);
    NkRenderer::Destroy(renderer);
    NkDeviceFactory::Destroy(device);
    window.Close();
    logger.Info("[Tuto05] Termine proprement.");
    return 0;
}
