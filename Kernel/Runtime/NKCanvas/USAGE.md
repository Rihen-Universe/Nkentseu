# NKCanvas — Guide d'utilisation

Couche graphique 2D SFML-like de Nkentseu. Sit au-dessus de `NkWindow` (fenêtre native multi-OS) et d'un backend GPU choisi (OpenGL / Vulkan / DX11 / DX12 / Metal / Software).

Mis à jour : **2026-05-30** (livraison A.1 → A.8 — 8/10 étapes de la refonte SFML).

---

## 1. Vue d'ensemble de l'API

```
NkRenderWindow  (target)
   │
   ├── NkRenderer2D  (facade user-facing → backend GPU via NkIRenderer2D)
   │      └── delegated to OpenGL / Vulkan / DX11 / DX12 / Software backend
   │
   ├── Draw(NkDrawable, NkRenderStates)
   │      ↓ NkSprite, NkText, NkRectangleShape, NkCircleShape,
   │        NkConvexShape, NkLineShape, NkVertexArray, …
   │
   └── Draw(NkVertex* raw, count, NkPrimitiveType, NkRenderStates)
```

Conventions de nommage :
- `NkRenderer2D` (concrete, user-facing) > `NkIRenderer2D` (interface backend).
- `NkDrawable` (nouveau) > `NkIDrawable2D` (legacy, conservé pour compat).
- Tous les types vivent dans `nkentseu::renderer::`.

---

## 2. Mise en route minimale

### Hello, sprite

```cpp
#include "NKWindow/Core/NkWindow.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h"
#include "NKCanvas/Core/NkContextDesc.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

int main() {
    // 1. Fenêtre native.
    NkWindow window;
    NkWindowConfig cfg;
    cfg.title = "Hello NKCanvas";
    cfg.width  = 800;
    cfg.height = 600;
    window.Create(cfg);

    // 2. Choisir le backend graphique (NkContextDesc) et instancier le target.
    NkContextDesc desc;
    desc.api = NkGraphicsApi::NK_OPENGL;        // ou NK_VULKAN, NK_DX11, …
    NkRenderWindow target(window, desc);
    if (!target.IsValid()) return 1;

    // 3. Charger une texture (via NkIRenderer2D — backend-agnostique).
    NkTexture tex;
    tex.LoadFromFile(*target.GetRenderer(), "assets/sprite.png");

    NkSprite sprite(tex);
    sprite.SetPosition({100, 100});

    // 4. Frame loop.
    while (window.IsOpen()) {
        window.PollEvents();

        target.Clear({30, 30, 30, 255});
        target.Draw(sprite);                     // nouveau pattern NkDrawable
        target.Display();
    }
    return 0;
}
```

**Note importante** : `target.Draw(sprite)` utilise le pattern moderne `NkDrawable`. Pour le legacy NkIRenderer2D-direct, on peut faire `target.GetRenderer()->Draw(sprite)` (équivalent fonctionnel le temps de la migration A.8 → ultérieur).

---

## 3. Choisir l'API graphique

Le `NkContextDesc::api` détermine quel backend est instancié par `NkContextFactory::Create`. Les valeurs disponibles :

```cpp
enum class NkGraphicsApi {
    NK_GFX_API_AUTO       // détection auto plateforme
    NK_GFX_API_SOFTWARE,  // CPU rasterizer, fallback universel
    NK_OPENGL,            // GL 3.3+ / GLES 3.0
    NK_VULKAN,            // Vulkan 1.0+
    NK_DX11,              // DirectX 11 (Windows)
    NK_DX12,              // DirectX 12 (Windows / Xbox)
    NK_METAL,             // Metal (macOS / iOS) — context seulement, Renderer2D pas encore livré
};
```

Pour le fallback automatique (essaie Vulkan, puis GL, puis Software) :

```cpp
NkContextDesc desc;
desc.api = NkGraphicsApi::NK_GFX_API_AUTO;
NkIGraphicsContext* ctx = NkContextFactory::CreateWithFallback(window, desc);
```

---

## 4. Primitives rapides — `NkRenderer2D`

Pour dessiner ponctuellement (pas de drawable persistant) :

```cpp
NkRenderer2D& r = target.GetRenderer2D();

r.DrawLine({10, 10}, {200, 50}, NkColor2D::Red, 2.f);
r.DrawFilledRect({50, 50, 100, 80}, NkColor2D::Blue);
r.DrawRect({200, 50, 60, 60}, NkColor2D::White, 2.f, NkColor2D::Yellow);     // contour
r.DrawCircle({400, 300}, 50.f, NkColor2D::Green, 64);                         // 64 segments
r.DrawFilledTriangle({500, 100}, {550, 200}, {450, 200}, NkColor2D::Magenta);
r.DrawPoint({600, 400}, NkColor2D::White, 4.f);
```

---

## 5. Shapes persistantes — pattern SFML

Plus efficace que les primitives ponctuelles quand on dessine la même forme de nombreuses fois (les vertices sont régénérés seulement si tu changes les paramètres).

### Rectangle

```cpp
#include "NKCanvas/Renderer/Shapes/NkRectangleShape.h"

NkRectangleShape rect({100.f, 50.f});      // largeur × hauteur
rect.SetPosition({200, 200});
rect.SetFillColor({255, 128, 0, 255});      // orange opaque
rect.SetOutlineColor(NkColor2D::White);
rect.SetOutlineThickness(2.f);

target.Draw(rect);
```

### Cercle

```cpp
#include "NKCanvas/Renderer/Shapes/NkCircleShape.h"

NkCircleShape circle(50.f);                 // rayon
circle.SetPointCount(64);                    // segments — défaut 32
circle.SetPosition({400, 300});
circle.SetOrigin({50.f, 50.f});             // centre (rotation/scale autour)
circle.SetFillColor(NkColor2D::Cyan);

target.Draw(circle);
```

### Polygone convexe

```cpp
#include "NKCanvas/Renderer/Shapes/NkConvexShape.h"

NkConvexShape pent(5);
pent.SetPoint(0, { 50.f,   0.f});
pent.SetPoint(1, {100.f,  35.f});
pent.SetPoint(2, { 80.f,  90.f});
pent.SetPoint(3, { 20.f,  90.f});
pent.SetPoint(4, {  0.f,  35.f});
pent.SetPosition({600, 300});
pent.SetFillColor(NkColor2D::Magenta);

target.Draw(pent);
```

### Segment épais

```cpp
#include "NKCanvas/Renderer/Shapes/NkLineShape.h"

NkLineShape line({100, 500}, {700, 550}, 5.f);
line.SetFillColor(NkColor2D::Yellow);
target.Draw(line);
```

---

## 6. Transform — animation & hiérarchie

Toute classe drawable héritant de `NkTransformable` expose les API SFML usuelles :

```cpp
NkRectangleShape r({60.f, 60.f});
r.SetPosition({400, 300});
r.SetOrigin({30.f, 30.f});         // pivot au centre du rectangle
r.SetRotation(0.5f);                 // radians

// Animation en boucle
r.Rotate(0.01f);
r.Move({1.f, 0.f});
r.Scale({1.01f, 1.f});

target.Draw(r);
```

Composition manuelle de transforms :

```cpp
NkTransform parent;
parent.Translate({100.f, 100.f}).Rotate(0.5f);

NkTransform child;
child.Translate({50.f, 0.f}).Scale({2.f, 2.f});

NkTransform world = parent * child;      // child appliqué d'abord, puis parent
const float32* gpuMatrix = world.GetMatrix();    // 16 floats column-major pour upload
```

---

## 7. Texte

```cpp
#include "NKCanvas/Renderer/Resources/NkFont.h"

NkFont font;
font.LoadFromFile(*target.GetRenderer(), "assets/Roboto-Regular.ttf");

NkText title(font, "Bonjour NKCanvas !", /*sizePx=*/32);
title.SetFillColor(NkColor2D::White);
title.SetPosition({50, 50});

target.Draw(title);
```

---

## 8. Vertex array personnalisé

Pour générer du custom geometry batché (utile pour les particules, tilemaps, etc.).

```cpp
#include "NKCanvas/Renderer/Core/NkVertexArray.h"

NkVertexArray va(NkPrimitiveType::NK_TRIANGLES, 3);
va[0] = NkVertex{ 100.f, 100.f,  0.f, 0.f,  255,   0,   0, 255};
va[1] = NkVertex{ 200.f, 100.f,  1.f, 0.f,    0, 255,   0, 255};
va[2] = NkVertex{ 150.f, 200.f, 0.5f, 1.f,    0,   0, 255, 255};

// Submit directe.
target.Draw(va);
// Ou avec états perso (texture, blend, transform parent).
NkRenderStates st;
st.transform.Translate({300.f, 0.f});
st.blendMode = NkBlendMode::NK_ADD;
target.Draw(va, st);
```

`NkPrimitiveType` disponibles : `NK_POINTS`, `NK_LINES`, `NK_LINE_STRIP`, `NK_TRIANGLES`, `NK_TRIANGLE_STRIP`, `NK_TRIANGLE_FAN`.

---

## 9. Resize & DPI change — recreate swapchain

L'application est responsable d'écouter les events de la fenêtre et de notifier le target. Pattern recommandé :

```cpp
EventSystem::Subscribe<NkWindowResizeEvent>([&](const NkWindowResizeEvent& e){
    target.OnResize(e.width, e.height);    // recreate swapchain via NkIGraphicsContext
});

EventSystem::Subscribe<NkWindowDpiEvent>([&](const NkWindowDpiEvent&){
    target.OnDpiChange();                   // recalcule la taille physique puis OnResize
});
```

Sur Win32 c'est automatique : `WM_DPICHANGED` envoie un suggested rect, Windows redimensionne la fenêtre, et `WM_SIZE` est émis → `NkWindowResizeEvent` → `target.OnResize`. Sur macOS, Wayland, iOS il faut intercepter le DPI change séparément (cf. `project_session_20260529_d_harmony.md` pour la doctrine).

---

## 10. Camera 2D (view)

```cpp
NkView2D view;
view.center = {400.f, 300.f};
view.size   = {800.f, 600.f};       // taille visible en coords monde
view.rotation = 0.f;
target.SetView(view);

// View par défaut (couvre toute la fenêtre).
target.SetView(target.GetDefaultView());

// Convertir position souris pixel → coords monde.
NkVec2f worldPos = target.MapPixelToCoords({mouseX, mouseY});
```

---

## 11. État de rendu (`NkRenderStates`)

Permet la composition (parent → enfants) et la sélection de texture/blend par draw :

```cpp
NkRenderStates st = NkRenderStates::Default();
st.blendMode = NkBlendMode::NK_ADD;
st.texture   = &particleTex;
st.transform.Translate({100, 100}).Rotate(0.5f);

target.Draw(particleVertexArray, st);
```

Drawables qui héritent de `NkTransformable` (NkSprite/NkShape) composent automatiquement leur transform local au transform de `states`.

---

## 12. Shaders custom — `NkShader`

NKCanvas est **autonome de NKRHI** : son système shader vit dans `Renderer/Resources/NkShader.h` et utilise une dispatch table backend. L'utilisateur fournit le source dans le langage du backend cible (GLSL pour OpenGL, HLSL pour DX, MSL pour Metal, SPIR-V pour Vulkan). Le backend choisit la variante qui correspond.

```cpp
#include "NKCanvas/Renderer/Resources/NkShader.h"

const char* glsl_vert = R"(
    #version 330 core
    layout(location=0) in vec2 aPos;
    layout(location=1) in vec2 aUV;
    layout(location=2) in vec4 aCol;
    uniform mat4 u_Projection;
    out vec2 vUV;
    out vec4 vCol;
    void main() {
        gl_Position = u_Projection * vec4(aPos, 0.0, 1.0);
        vUV = aUV; vCol = aCol;
    }
)";

const char* glsl_frag = R"(
    #version 330 core
    in  vec2 vUV;
    in  vec4 vCol;
    uniform sampler2D u_Texture;
    uniform float uTime;
    out vec4 oColor;
    void main() {
        // Modification UV : scroll horizontal + ondulation verticale
        vec2 modUV = vUV;
        modUV.x = fract(vUV.x + uTime * 0.3);
        modUV.y = vUV.y + sin(vUV.x * 20.0 + uTime * 2.0) * 0.02;
        oColor = texture(u_Texture, modUV) * vCol;
    }
)";

NkShader scrollShader;
scrollShader.SetSourceGLSL(glsl_vert, glsl_frag);
scrollShader.Compile(*target.GetRenderer());

// Frame loop
scrollShader.SetFloat("uTime", currentTime);
NkRenderStates st = NkRenderStates::Default();
st.shader  = &scrollShader;
st.texture = &myTexture;
target.Draw(sprite, st);
```

**Backends supportés à 2026-05-30** :
- **OpenGL** ✅ compile + link + uniforms (Windows/Linux/macOS/Android/Web).
- **Software / Vulkan / DX11 / DX12 / Metal** : stubs câblés — `NkShader::Compile()` retourne `false` (architecture en place, intégration pipeline backend = chantier dédié par backend, cf. ROADMAP).

---

## 13. Matériaux — `NkMaterial`

Un **`NkMaterial`** encapsule un `NkShader` + ses paramètres préconfigurés + des states GPU (blend mode, texture). Permet de créer des bibliothèques d'effets réutilisables :

- **Toon 2D** (quantization couleur + outline)
- **Lumière 2D** (spot, attenuation)
- **Feu** (noise UV + gradient + additif)
- **Eau** (ripple sin + refraction)
- **Distortion**, **Color grading**, **Glow**, etc.

```cpp
#include "NKCanvas/Renderer/Resources/NkMaterial.h"

// Exemple : matériau « feu » réutilisable
NkShader fireShader;
fireShader.SetSourceGLSL(kFire_VS, kFire_FS);
fireShader.Compile(*target.GetRenderer());

NkMaterial fire;
fire.SetShader(&fireShader);
fire.SetBlendMode(NkBlendMode::NK_ADD);            // additif
fire.SetTexture("uNoise", &noiseTex);
fire.SetVec3("uColorHot",  1.0f, 0.7f, 0.2f);     // jaune-orangé
fire.SetVec3("uColorCool", 1.0f, 0.0f, 0.0f);     // rouge sombre
fire.SetFloat("uIntensity", 1.0f);

// Frame loop : juste mettre à jour ce qui change
fire.SetFloat("uTime", time);
target.Draw(flameSprite, fire.States());            // <— États du material appliqués
```

Autre exemple — **toon shader** :

```cpp
NkMaterial toon;
toon.SetShader(&toonShader);
toon.SetFloat("uLevels", 4.0f);              // 4 paliers de quantization
toon.SetVec3("uLightDir", 0.6f, -0.8f, 0.0f);
toon.SetVec4("uOutlineColor", 0.0f, 0.0f, 0.0f, 1.0f);
toon.SetFloat("uOutlineThickness", 1.5f);

target.Draw(characterSprite, toon.States());
```

Avantages :
- Une **instance par effet**, partagée entre tous les drawables qui l'utilisent.
- Plusieurs `NkMaterial` peuvent **partager le même `NkShader`** (variants — feu rouge vs feu bleu, même shader, params différents).
- L'**`Apply()`** est implicite via `States()` : tu mets à jour les uniforms et le draw les uploade automatiquement.

---

## 14. Render to texture — `NkRenderTexture`

Permet le rendu offscreen vers une texture, pour post-process, mini-map, blit UI, capture, etc.

```cpp
#include "NKCanvas/Renderer/Targets/NkRenderTexture.h"

NkRenderTexture rt;
if (rt.Create(*target.GetRenderer(), 512, 512)) {
    rt.Clear({0, 0, 0, 255});
    rt.Draw(scene);          // rendu dans le FBO offscreen
    rt.Display();             // unbind, restore framebuffer principal

    // La texture résultat est utilisable comme NkTexture
    NkTexture rtTex;
    rtTex.SetGPUId(rt.GetColorTextureGPUId());
    rtTex.SetHandle(nullptr);   // GL pure : pas de handle natif au-delà du GPU id

    NkSprite postSprite(rtTex);
    postSprite.SetPosition({100, 100});
    target.Draw(postSprite);    // affiche le contenu offscreen sur le target principal
}
```

**Backends supportés à 2026-05-30** :
- **OpenGL** ✅ FBO + color attachment GL_RGBA8 (Windows/Linux/macOS/Android/Web).
- **Software / Vulkan / DX11 / DX12 / Metal** : stubs — `Create()` retourne `false`. Implémentations différées (VkFramebuffer+VkRenderPass / ID3D11RenderTargetView / ID3D12Resource RTV / MTLTexture).

---

## 15. Backend pour drawables personnalisés

Pour créer ton propre drawable, hérite de `NkDrawable` (+ `NkTransformable` si tu veux position/rotation/scale) :

```cpp
#include "NKCanvas/Renderer/Core/NkDrawable.h"
#include "NKCanvas/Renderer/Core/NkTransformable.h"
#include "NKCanvas/Renderer/Targets/NkRenderTarget.h"

class MonGizmo : public NkTransformable, public NkDrawable {
public:
    void Draw(NkRenderTarget& target, const NkRenderStates& parentStates) const override {
        NkRenderStates s = parentStates;
        s.transform *= GetTransform();   // applique notre transform local

        NkVertex verts[3] = {
            { 0,  0,  0, 0, 255, 0, 0, 255},
            {50,  0,  0, 0,   0, 255, 0, 255},
            {25, 50,  0, 0,   0, 0, 255, 255},
        };
        target.Draw(verts, 3, NkPrimitiveType::NK_TRIANGLES, s);
    }
};

MonGizmo g;
g.SetPosition({400, 300});
target.Draw(g);
```

---

## 13. Pièges connus

1. **Mélange de drawables anciens/nouveaux** : NkSprite/NkText héritent à la fois de `NkIDrawable2D` (legacy) et `NkDrawable` (nouveau). Les deux fonctionnent ; préférer le nouveau pattern `target.Draw(sprite)` pour le nouveau code.
2. **`NkRenderTexture`** : encore en STUB pour tous backends à 2026-05-30. L'API est posée mais `Create()` retourne `false`. Roadmap A.10+.
3. **Sampler par-texture** : OpenGL et DX11 le supportent. DX12 et Vulkan utilisent un sampler global (root signature / immutable sampler) — `SetFilter/SetWrap` sont des no-ops sur ces backends en attendant un descriptor heap dédié.
4. **`mOutlineThickness > 1`** sur NkShape : actuellement, l'outline est rendu via `NK_LINE_STRIP` qui appelle `DrawLine(thickness=1)`. Les outlines épais demandent une expansion en quads (TODO).
5. **`Pong copy`** (dossier) : stale, ignorer. Le vrai Pong est dans `Applications/Pong/`.

---

## 14. Référence rapide — includes

```cpp
// Cœur (toujours nécessaires)
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"   // enums et alias
#include "NKCanvas/Core/NkContextDesc.h"

// Drawables intégrés
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h"        // contient NkText aussi
#include "NKCanvas/Renderer/Resources/NkFont.h"

// Shapes SFML-like
#include "NKCanvas/Renderer/Shapes/NkRectangleShape.h"
#include "NKCanvas/Renderer/Shapes/NkCircleShape.h"
#include "NKCanvas/Renderer/Shapes/NkConvexShape.h"
#include "NKCanvas/Renderer/Shapes/NkLineShape.h"

// Shaders / materials custom
#include "NKCanvas/Renderer/Resources/NkShader.h"
#include "NKCanvas/Renderer/Resources/NkMaterial.h"

// Render to texture (offscreen)
#include "NKCanvas/Renderer/Targets/NkRenderTexture.h"

// Custom drawable
#include "NKCanvas/Renderer/Core/NkDrawable.h"
#include "NKCanvas/Renderer/Core/NkTransformable.h"
#include "NKCanvas/Renderer/Core/NkVertexArray.h"
#include "NKCanvas/Renderer/Core/NkRenderStates.h"
#include "NKCanvas/Renderer/Core/NkTransform.h"
```

---

## 15. Roadmap immédiate

- **A.9** : refonte de **Pong** sur cette nouvelle API (vitrine SFML).
- **A.10** : ROADMAP NKCanvas formelle + NKUI integration future.
- **`NkRenderTexture`** : impl réelle cross-API (FBO/VkFramebuffer/RTV/MTLTexture).
- **DX11/DX12 binding** sur Windows desktop (actuellement compilé mais pas linké).
- **Metal Renderer2D** : pas encore implémenté (context placeholder seulement).
