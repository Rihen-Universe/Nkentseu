# NKRenderer — Guide d'utilisation complet

## Sommaire
1. [Architecture générale](#architecture)
2. [Moteur de jeux 2D](#moteur-2d)
3. [Moteur de jeux 3D](#moteur-3d)
4. [Application d'animation 2D/3D (type Cascadeur)](#animation)
5. [Application de modélisation 3D](#modelisation)
6. [Application de design 2D](#design-2d)
7. [Shaders et matériaux](#shaders)
8. [Post-traitement](#postprocess)

---

## 1. Architecture générale {#architecture}

```
NkRenderer (façade)
├── NkResourceManager   ← textures, meshes, shaders, polices
├── NkMaterialSystem    ← templates + instances PBR/Toon/Unlit/...
├── NkRenderGraph       ← passes + barrières GPU automatiques
├── NkRender2D          ← sprites, shapes, texte, UI (batching auto)
├── NkRender3D          ← PBR AAA, ombres CSM, instancing, culling
├── NkPostProcessStack  ← SSAO, Bloom, Tonemap ACES, FXAA, DOF
├── NkOverlayRenderer   ← debug HUD, gizmos, stats
└── NkOffscreenTarget   ← capture offscreen (minimap, portail, export)
```

### Initialisation minimale

```cpp
#include "NKRenderer/NkRenderer.h"
using namespace nkentseu::renderer;

NkRenderer renderer;

NkRendererConfig cfg = NkRendererConfig::HighFidelity();
cfg.width  = 1920;
cfg.height = 1080;

bool ok = renderer.Init(myNkIDevice, cfg);
```

### Boucle principale

```cpp
// Par frame :
NkICommandBuffer* cmd = device->BeginFrame();
renderer.BeginFrame(cmd);

// — votre logique ici —

renderer.RenderFrame(cmd, renderScene);
renderer.EndFrame();
device->EndFrame();
```

---

## 2. Moteur de jeux 2D {#moteur-2d}

### Chargement des ressources

```cpp
NkResourceManager* res = renderer.GetResources();
NkMaterialSystem*  mat = renderer.GetMaterials();

// Textures
NkTextureHandle playerTex    = res->LoadTexture("assets/player.png");
NkTextureHandle tilemapTex   = res->LoadTexture("assets/tilemap.png");
NkFontHandle    uiFont       = res->LoadFont(NkFontDesc::TTF("assets/font.ttf", 16.f));

// Police SDF pour du texte net à toute taille
NkFontHandle titleFont = res->LoadFont(NkFontDesc::SDF("assets/title.ttf", 32.f));
```

### Rendu de scène 2D par frame

```cpp
NkRender2D* r2d = renderer.Get2D();

// Dans la boucle de rendu :
r2d->Begin(cmd, screenW, screenH);   // ou Begin(cmd, w, h, &myCamera2D)

// Fond
r2d->FillRect({0,0,(float)screenW,(float)screenH}, {0.1f,0.1f,0.15f,1.f});

// Tilemap (9 tranches, pas de distorsion à la resize)
r2d->DrawNineSlice({0,0,screenW,screenH}, tilemapTex, 8,8,8,8);

// Sprite du joueur (avec rotation et pivot au centre)
r2d->DrawSpriteRotated({playerX-32,playerY-32,64,64},
                        playerTex, playerAngle, {0.5f,0.5f});

// Sprite animé — extraire un sous-rect dans l'atlas
NkRectF uvFrame = { frameCol/8.f, frameRow/4.f, 1/8.f, 1/4.f };
r2d->DrawSprite({posX,posY,64,64}, playerTex, NkColorF::White(), uvFrame);

// Barre de vie
r2d->FillRect({10,10,200,16}, {0.2f,0.2f,0.2f,1});
r2d->FillRectGradH({10,10,health*2,16}, {0.2f,0.9f,0.2f,1},{0.8f,0.9f,0.1f,1});

// Texte
r2d->DrawText({10,30}, "Score: 9999", uiFont, 16.f, NkColorF::White());

// Formes géométriques (physique debug, éditeur de niveaux)
r2d->DrawRect ({bboxX,bboxY,bboxW,bboxH}, {0,1,0,0.8f}, 1.5f);
r2d->DrawCircle({cx,cy}, radius, {1,0.5f,0,1}, 2.f);

// Clip region (culling par zone, scroll)
r2d->PushClip({camX, camY, viewW, viewH});
// ... dessiner le monde dans ce clip ...
r2d->PopClip();

r2d->End();
```

### Caméra 2D

```cpp
NkCamera2D cam;
cam.SetCenter({playerX, playerY});
cam.SetZoom(2.f);   // zoom avant
cam.SetViewport(screenW, screenH);

r2d->Begin(cmd, screenW, screenH, &cam);
```

### Architecture d'un moteur de jeux 2D complet

```
GameEngine2D
├── ResourceRegistry      ← gère NkResourceManager
├── World                 ← grille de chunks, entités ECS
│   ├── TilemapSystem     ← r2d->DrawNineSlice / DrawSprite (atlas)
│   ├── SpriteSystem      ← r2d->DrawSpriteRotated + animations
│   ├── PhysicsSystem     ← Box2D / maison → debug avec DrawRect/DrawCircle
│   ├── ParticleSystem    ← r2d->FillCircle en mode additif
│   └── LightSystem2D     ← render offscreen + blend mode Screen
├── UISystem              ← r2d->DrawText, FillRoundRect, DrawNineSlice
├── CameraController2D    ← NkCamera2D smooth follow
└── FrameLoop
    ├── cmd = device->BeginFrame()
    ├── renderer.BeginFrame(cmd)
    ├── world.Update(dt)
    ├── renderer.Render2D(cmd, [&](NkRender2D* r){ world.Draw(r); })
    ├── renderer.EndFrame()
    └── device->EndFrame()
```

---

## 3. Moteur de jeux 3D {#moteur-3d}

### Chargement et matériaux

```cpp
NkResourceManager* res = renderer.GetResources();
NkMaterialSystem*  mat = renderer.GetMaterials();

// Charger une texture PBR
NkTextureHandle albedo  = res->LoadTexture("assets/rock_albedo.png",   true,  true);
NkTextureHandle normal  = res->LoadTexture("assets/rock_normal.png",   false, false);
NkTextureHandle orm     = res->LoadTexture("assets/rock_orm.png",      false, true);

// Créer un matériau PBR
NkMaterialHandle pbrTmpl = mat->FindMaterial("Default_PBR");
NkMaterialInstance* rock = mat->CreateInstance(pbrTmpl);
rock->SetAlbedoMap(albedo)
    ->SetNormalMap(normal, 1.f)
    ->SetORMMap(orm)
    ->SetRoughness(0.7f)
    ->SetMetallic(0.0f);

// Charger un mesh
NkMeshHandle rockMesh = res->CreateMesh(
    NkMeshDesc::FromBuffers(vertices, vtxCount, indices, idxCount));
```

### Scène et rendu 3D

```cpp
NkRenderScene scene{};
scene.camera = myCamera3D;
scene.time   = totalTime;
scene.deltaTime = dt;

// Lumière directionnelle (soleil)
NkLightDesc sun{};
sun.type      = NkLightType::NK_DIRECTIONAL;
sun.direction = {-0.5f,-1.f,-0.5f};
sun.color     = NkColorF::White();
sun.intensity = 5.f;
scene.lights.PushBack(sun);

// Ajouter des objets
for (auto& obj : world.objects) {
    NkDrawCall3D dc{};
    dc.mesh      = obj.mesh;
    dc.material  = obj.materialInst;
    dc.transform = obj.worldMatrix;
    dc.castShadow= true;
    dc.aabb      = obj.worldAABB;
    scene.drawCalls.PushBack(dc);
}

// Render (shadows CSM + culling + tri + PBR + post-process)
renderer.RenderFrame(cmd, scene);
```

### Utiliser NkScene (haut niveau)

```cpp
NkScene scene;
scene.Init("Level1");

// Terrain
auto terrain = scene.CreateEntity("Terrain");
scene.AddMeshComponent(terrain, terrainMesh, terrainMat);

// Personnage
auto player = scene.CreateEntity("Player");
scene.SetTranslation(player, {0,1,0});
scene.AddMeshComponent(player, playerMesh, playerMat);
scene.SetCastShadow(player, true);

// Lumière
auto sun = scene.CreateLight("Sun", NkLightType::NK_DIRECTIONAL);
scene.SetRotation(sun, {-45.f, 30.f, 0.f});
scene.SetLightIntensity(sun, 5.f);

// Caméra
auto cam = scene.CreateCamera("MainCam");
scene.SetTranslation(cam, {0,5,10});
scene.SetCameraFOV(cam, 65.f);

// --- Par frame ---
scene.Update(dt);
scene.SetTranslation(player, playerPos);  // mise à jour position

NkRenderScene renderScene{};
scene.CollectRenderScene(renderScene);
renderer.RenderFrame(cmd, renderScene);
```

### Architecture d'un moteur de jeux 3D complet

```
GameEngine3D
├── NkScene                 ← hiérarchie entités + propagation transforms
├── PhysicsWorld            ← NKPHYSICS ou Bullet/PhysX
├── AudioSystem             ← NKAUDIO
├── AnimationSystem         ← skeletal anim → NkRender3D::SubmitSkinned
├── LODSystem               ← choisit mesh selon distance caméra
├── StreamingSystem         ← charge/décharge chunks monde ouvert
├── ParticleSystem          ← NkRender3D particles GPU
├── UISystem                ← NkRender2D pour HUD + NkOverlayRenderer debug
└── FrameLoop
    ├── physics.Step(dt)
    ├── animation.Update(dt)
    ├── scene.Update(dt)
    ├── NkRenderScene rs; scene.CollectRenderScene(rs)
    ├── renderer.RenderFrame(cmd, rs, &ppConfig)
    └── r2d HUD
```

---

## 4. Application d'animation 2D/3D (côté rendu) {#animation}

### Principe (type Cascadeur)

Une application d'animation a besoin de :
- Rendu de rig squelettique (skinning GPU)
- Rendu multi-viewport (perspective, top, front, caméra)
- Onion skinning (frames fantômes semi-transparentes)
- Gizmos de manipulation 3D
- Rendu offscreen pour aperçu / export d'images

### Skinning GPU

```cpp
// Créer un mesh skinné
NkMeshHandle skinnedMesh = res->CreateMesh(
    NkMeshDesc::FromBuffers(skinnedVerts, vtxCount, indices, idxCount));
    // NkVertexSkinned : position + normal + tangent + uv + boneIds + weights

// Par frame d'animation : upload des matrices de bones
NkVector<NkMat4f> boneMatrices = animator.GetCurrentBoneMatrices();
renderer.Get3D()->SubmitSkinned(skinnedMesh, material, worldTransform,
                                  boneMatrices.Data(), boneMatrices.Size());
```

### Multi-viewport

```cpp
// 4 viewports (perspective, front, top, right)
struct Viewport { NkRectF rect; NkCamera3D cam; NkRenderMode mode; };
Viewport viewports[4] = {
    {{0,   0,   halfW,halfH}, perspectiveCam, NkRenderMode::NK_SOLID},
    {{halfW,0,  halfW,halfH}, frontCam,       NkRenderMode::NK_WIREFRAME},
    {{0,   halfH,halfW,halfH}, topCam,        NkRenderMode::NK_SOLID},
    {{halfW,halfH,halfW,halfH},rightCam,       NkRenderMode::NK_NORMALS},
};

for (auto& vp : viewports) {
    // Créer un offscreen target par viewport (ou utiliser scissor)
    offscreen.BeginCapture(cmd, true, {0.1f,0.1f,0.1f,1}, true);
    cmd->SetViewport(vp.rect);
    cmd->SetScissor({vp.rect.x,vp.rect.y,vp.rect.w,vp.rect.h});

    renderer.Get3D()->SetRenderMode(vp.mode);
    NkRenderScene rs; rs.camera = vp.cam;
    for (auto& dc : scene.drawCalls) rs.drawCalls.PushBack(dc);
    renderer.Get3D()->BeginScene({vp.cam, scene.lights});
    // ... submit calls ...
    renderer.Get3D()->EndScene(cmd);

    offscreen.EndCapture();
    // Blit le résultat dans le viewport UI correspondant
    r2d->DrawImage(offscreen.GetResult(), vp.rect);
}
```

### Onion skinning (frames fantômes)

```cpp
// Dessiner les frames précédentes/suivantes en semi-transparent
for (int ghostOffset = -3; ghostOffset <= 3; ++ghostOffset) {
    if (ghostOffset == 0) continue;
    float alpha = 1.f - fabsf(ghostOffset) / 4.f;
    NkColorF tint = ghostOffset < 0
        ? NkColorF{1,0.3f,0.3f,alpha*0.5f}   // passé = rouge
        : NkColorF{0.3f,0.3f,1,alpha*0.5f};   // futur = bleu

    NkVector<NkMat4f> ghostBones = animator.GetBoneMatricesAtFrame(
        currentFrame + ghostOffset);
    renderer.Get3D()->SubmitSkinned(mesh, ghostMat, worldTrans,
                                      ghostBones.Data(), ghostBones.Size(),
                                      tint);
}
```

### Gizmos de manipulation (handles)

```cpp
NkRender3D* r3d = renderer.Get3D();
NkOverlayRenderer* overlay = renderer.GetOverlay();

// Axes de transformation
r3d->DrawDebugAxes(selectedObject.worldMatrix, 1.f);

// Manipulateur de rotation (arcs)
if (isRotating) {
    r3d->DrawDebugCircle(selectedPos, {1,0,0,1}, 1.f); // axe X
    r3d->DrawDebugCircle(selectedPos, {0,1,0,1}, 1.f); // axe Y
    r3d->DrawDebugCircle(selectedPos, {0,0,1,1}, 1.f); // axe Z
}

// Label du bone en overlay
overlay->AddWorldGizmo(boneWorldPos, viewProj,
                         NkOverlayRenderer::GizmoType::CROSS,
                         {1,0.8f,0,1}, 6.f, bone.name.CStr());
```

---

## 5. Application de modélisation 3D {#modelisation}

### Rendu en wireframe / face normale / UV

```cpp
// Changer le mode de rendu selon l'outil actif
switch (currentDisplayMode) {
    case DisplayMode::SOLID:      r3d->SetRenderMode(NkRenderMode::NK_SOLID);      break;
    case DisplayMode::WIREFRAME:  r3d->SetRenderMode(NkRenderMode::NK_WIREFRAME);  break;
    case DisplayMode::NORMALS:    r3d->SetRenderMode(NkRenderMode::NK_NORMALS);    break;
    case DisplayMode::UV:         r3d->SetRenderMode(NkRenderMode::NK_UV);         break;
    case DisplayMode::AO:         r3d->SetRenderMode(NkRenderMode::NK_AO);         break;
}
```

### Mesh dynamique (édition en temps réel)

```cpp
// Créer un mesh dynamique (VBO/IBO mis à jour à chaque frame)
NkDynamicMeshDesc dynDesc = NkDynamicMeshDesc::Create<NkVertex3D>(
    maxVerts, maxIndices, "EditMesh");
NkMeshHandle editMesh = res->CreateDynamicMesh(dynDesc);

// Après modification des vertices par l'utilisateur :
res->UpdateMeshVertices(editMesh, modifiedVerts, newVertexCount);
res->UpdateMeshIndices(editMesh, modifiedIndices, newIndexCount);
```

### Rendu des normales de debug

```cpp
// Visualiser les normales par vertex
for (uint32 i = 0; i < vtxCount; ++i) {
    NkVec3f p = {verts[i].px, verts[i].py, verts[i].pz};
    NkVec3f n = p + NkVec3f{verts[i].nx, verts[i].ny, verts[i].nz} * 0.1f;
    r3d->DrawDebugLine(p, n, {0,0.8f,1,1});
}
```

### Capture de rendu (export d'image)

```cpp
// Créer un target offscreen à la résolution souhaitée
NkOffscreenDesc ods{};
ods.width  = 4096;
ods.height = 4096;
ods.name   = "ExportRender";
ods.hasDepth = true;

NkOffscreenTarget exportTarget;
exportTarget.Init(device, res, ods);

// Rendre dans ce target
exportTarget.BeginCapture(cmd, true, {0,0,0,1}, true);
// ... submit draw calls avec exportCamera ...
exportTarget.EndCapture();

// Lire les pixels sur le CPU
std::vector<uint8_t> pixels(4096*4096*4);
exportTarget.ReadbackPixels(pixels.data(), 4096*4);
// → sauvegarder en PNG/EXR
```

### Minimap / aperçu caméra

```cpp
// Créer un target minimap 256×256
NkOffscreenDesc mmDesc{};
mmDesc.width=256; mmDesc.height=256; mmDesc.name="Minimap"; mmDesc.hasDepth=true;
NkOffscreenTarget minimapTarget;
minimapTarget.Init(device, res, mmDesc);

// Chaque frame : rendre depuis la vue du dessus
NkCamera3D topCam; topCam.SetOrtho(-50,50,-50,50,0.1f,500.f);
topCam.SetPositionTarget({0,100,0},{0,0,0});

minimapTarget.BeginCapture(cmd);
// ... rendre la scène avec topCam ...
minimapTarget.EndCapture();

// Afficher dans le HUD via r2d
r2d->DrawImage(minimapTarget.GetResult(), {screenW-260.f, 8.f, 252.f, 252.f});
```

---

## 6. Application de design 2D {#design-2d}

### Canvas infini avec zoom/pan

```cpp
NkCamera2D canvas;
canvas.SetCenter({panX, panY});
canvas.SetZoom(zoomLevel);   // e.g. 0.1 → 10.0
canvas.SetViewport(screenW, screenH);

r2d->Begin(cmd, screenW, screenH, &canvas);

// Grille du canvas
float gridStep = 50.f;
for (float x = -5000; x <= 5000; x += gridStep)
    r2d->DrawLine({x,-5000},{x,5000}, {0.3f,0.3f,0.3f,0.5f}, 1.f);
for (float y = -5000; y <= 5000; y += gridStep)
    r2d->DrawLine({-5000,y},{5000,y}, {0.3f,0.3f,0.3f,0.5f}, 1.f);

// Formes vectorielles
for (auto& shape : document.shapes) {
    switch (shape.type) {
        case ShapeType::RECT:
            if (shape.filled)
                r2d->FillRoundRect(shape.bounds, shape.fillColor, shape.cornerRadius);
            if (shape.stroked)
                r2d->DrawRect(shape.bounds, shape.strokeColor, shape.strokeWidth);
            break;
        case ShapeType::ELLIPSE:
            if (shape.filled)
                r2d->FillCircle(shape.center, shape.radius, shape.fillColor);
            if (shape.stroked)
                r2d->DrawCircle(shape.center, shape.radius, shape.strokeColor, shape.strokeWidth);
            break;
        case ShapeType::PATH:
            r2d->DrawPolyline(shape.points.Data(), shape.points.Size(),
                               shape.strokeColor, shape.strokeWidth, shape.closed);
            break;
        case ShapeType::BEZIER:
            r2d->DrawBezier(shape.p0, shape.p1, shape.p2, shape.p3,
                             shape.strokeColor, shape.strokeWidth);
            break;
        case ShapeType::IMAGE:
            r2d->DrawSprite(shape.bounds, shape.texture, shape.fillColor);
            break;
        case ShapeType::TEXT:
            r2d->DrawText(shape.origin, shape.text.CStr(), shape.font,
                           shape.fontSize, shape.fillColor);
            break;
    }
}

// Sélection en cours
if (hasSelection) {
    r2d->DrawRect(selectionBounds, {0.2f,0.6f,1.f,1.f}, 2.f);
    // Poignées aux coins
    float h = 4.f;
    r2d->FillRect({selectionBounds.x-h, selectionBounds.y-h, h*2,h*2}, {1,1,1,1});
    r2d->FillRect({selectionBounds.Right()-h, selectionBounds.y-h, h*2,h*2},{1,1,1,1});
    r2d->FillRect({selectionBounds.x-h, selectionBounds.Bottom()-h,h*2,h*2},{1,1,1,1});
    r2d->FillRect({selectionBounds.Right()-h,selectionBounds.Bottom()-h,h*2,h*2},{1,1,1,1});
}

r2d->End();
```

### Export PNG depuis le canvas

```cpp
// Rendre le document dans un offscreen à résolution d'export
NkOffscreenTarget exportRT;
exportRT.Init(device, res, {exportW, exportH, "Export", false});
exportRT.BeginCapture(cmd, true, documentBackground);
// ... rendre les shapes sur le canvas offscreen ...
exportRT.EndCapture();

// Readback
std::vector<uint8> pixels(exportW * exportH * 4);
exportRT.ReadbackPixels(pixels.data(), exportW*4);
// stb_image_write_png("output.png", exportW, exportH, 4, pixels.data(), exportW*4);
```

---

## 7. Shaders et matériaux {#shaders}

### Créer un matériau custom

```cpp
NkMaterialDesc myDesc = NkMaterialDesc::Custom("WaterMat");
myDesc.shaderDesc
    .SetGL  (NkShaderStageFlag::NK_VERTEX,   water_vert_gl)
    .SetGL  (NkShaderStageFlag::NK_FRAGMENT, water_frag_gl)
    .SetVK  (NkShaderStageFlag::NK_VERTEX,   water_vert_vk)
    .SetVK  (NkShaderStageFlag::NK_FRAGMENT, water_frag_vk)
    .SetDX11(NkShaderStageFlag::NK_VERTEX,   water_vert_hlsl)
    .SetDX11(NkShaderStageFlag::NK_PIXEL,    water_frag_hlsl);
myDesc.blendMode   = NkBlendMode::NK_ALPHA;
myDesc.cullMode    = NkCullMode::NK_NONE;
myDesc.depthWrite  = false;
myDesc.renderQueue = NkRenderQueue::NK_TRANSPARENT;

NkMaterialHandle waterTmpl = mat->RegisterMaterial("Water", myDesc);
NkMaterialInstance* waterInst = mat->CreateInstance(waterTmpl);
waterInst->SetTexture("uNormalMap1", waveNormal1Tex)
          ->SetTexture("uNormalMap2", waveNormal2Tex)
          ->SetFloat  ("uSpeed", 0.3f)
          ->SetColor  ("uDeepColor", {0.01f,0.05f,0.2f,0.9f});
```

### Utiliser un matériau Toon

```cpp
NkMaterialDesc toonDesc = NkMaterialDesc::Toon("Toon_Character");
NkMaterialHandle toon = mat->FindMaterial("Default_Toon");
NkMaterialInstance* charMat = mat->CreateInstance(toon);
charMat->SetAlbedoMap(charAlbedo)
       ->SetColor("uShadowColor", {0.2f,0.1f,0.3f,1.f})
       ->SetFloat("uShadowThreshold", 0.3f)
       ->SetFloat("uShadowSmoothness", 0.05f)
       ->SetVec4 ("uOutlineColor", 0,0,0,1)
       ->SetFloat("uOutlineWidth", 2.f);
```

---

## 8. Post-traitement {#postprocess}

```cpp
NkPostProcessConfig pp;

// SSAO
pp.ssao.enabled   = true;
pp.ssao.radius    = 0.5f;
pp.ssao.intensity = 1.2f;
pp.ssao.bias      = 0.025f;

// Bloom
pp.bloom.enabled      = true;
pp.bloom.threshold    = 1.2f;
pp.bloom.intensity    = 0.6f;
pp.bloom.filterRadius = 0.005f;

// DOF
pp.dof.enabled       = true;
pp.dof.focusDistance = 10.f;
pp.dof.focusRange    = 5.f;
pp.dof.bokehRadius   = 8.f;
pp.dof.samples       = 16;

// Tonemap
pp.tonemap.op           = NkTonemapOp::NK_ACES;
pp.tonemap.exposure     = 1.2f;
pp.tonemap.gamma        = 2.2f;
pp.tonemap.contrast     = 1.05f;
pp.tonemap.saturation   = 1.1f;

// Film grain + vignette
pp.filmGrain.enabled   = true;
pp.filmGrain.intensity = 0.4f;
pp.vignette.enabled    = true;
pp.vignette.intensity  = 0.5f;

// Anti-aliasing
pp.antiAliasing = NkAAMode::NK_FXAA;

renderer.RenderFrame(cmd, scene, &pp);
```

---

## Annexe — Hiérarchie des fichiers

```
NKRenderer/
├── NkRenderer.h                     ← façade principale
├── Core/
│   ├── NkRendererTypes.h            ← handles, enums, structs fondamentaux
│   ├── NkRendererConfig.h           ← presets de configuration
│   ├── NkResourceManager.h/.cpp     ← textures, meshes, polices, RTs
│   ├── NkRenderGraph.h/.cpp         ← FrameGraph (passes + barrières)
│   └── NkCamera.h/.cpp              ← NkCamera2D, NkCamera3D, UBOs
├── Resources/
│   ├── NkVertexFormats.h            ← NkVertex2D/3D/Skinned/Debug/Particle
│   └── NkResourceDescs.h            ← NkTextureDesc, NkMeshDesc, ...
├── Materials/
│   └── NkMaterialSystem.h/.cpp      ← templates + instances
├── Scene/
│   └── NkScene.h/.cpp               ← entités + composants + transform
├── Tools/
│   ├── Render2D/NkRender2D.h/.cpp   ← rendu 2D batché
│   ├── Render3D/NkRender3D.h/.cpp   ← PBR 3D + shadows + instancing
│   ├── PostProcess/                 ← SSAO/Bloom/DOF/Tonemap/FXAA
│   ├── Overlay/NkOverlayRenderer    ← debug HUD + gizmos
│   └── Offscreen/NkOffscreenTarget  ← capture offscreen
├── Shaders/
│   ├── PBR/{GL,VK,DX11,DX12,MSL,NkSL}/
│   ├── Toon/{GL,VK,DX11,DX12,MSL,NkSL}/
│   ├── Unlit/{GL,VK,DX11,DX12,MSL,NkSL}/
│   ├── PostProcess/{GL,VK,DX11}/    ← SSAO,Bloom,Tonemap,FXAA,DOF
│   ├── ShadowDepth/{GL,VK,DX11}/
│   ├── Debug/{GL,VK,DX11}/
│   └── Particles/{GL,VK,DX11}/
└── src/                             ← toutes les implémentations .cpp
    ├── Core/
    ├── Materials/
    ├── Scene/
    ├── Graph/
    └── Tools/
```
