# Étape 2 — Brancher le GPU : device et renderer

> **Cible jenga** : `Tuto02Renderer` · **Source** : [main.cpp](main.cpp) (~140 lignes)
> **Nouveaux modules** : NKRHI (device GPU) · NKRenderer (rendu haut niveau)
> **Prérequis** : [Étape 1](../01-Fenetre/README.md) — fenêtre + événements

Une fenêtre sans rendu, c'est un cadre vide. Dans cette étape, on branche la
chaîne graphique complète du moteur : le **device** (l'accès brut au GPU) puis
le **renderer** (l'étage haut niveau qui sait dessiner). À la fin, l'écran
affiche du texte en overlay : le nom du backend GPU choisi et les FPS.

## Ce que vous saurez faire à la fin

- créer un device GPU sans écrire une ligne de Vulkan/DX/GL ;
- initialiser `NkRenderer` et comprendre qui fait quoi (NKRHI vs NKRenderer) ;
- écrire la boucle de rendu `BeginFrame → dessiner → Present → EndFrame` ;
- afficher du texte et des rectangles 2D par-dessus la scène (overlay) ;
- gérer le redimensionnement et la fermeture SANS fuite ni crash.

---

## 1. Deux étages : NKRHI en bas, NKRenderer en haut

```
votre code ──▶ NKRenderer   (scènes, matériaux PBR, ombres, texte, post-process)
                   │
                   ▼
               NKRHI        (device, command buffers, textures, shaders NkSL)
                   │
                   ▼
     Vulkan · OpenGL · DX11 · DX12 · Metal · Software (rasterizer CPU)
```

**NKRHI** (Rendering Hardware Interface) parle à l'API graphique native.
**NKRenderer** s'appuie dessus et offre des concepts de moteur : scène,
caméra, lumière, mesh, overlay… Dans les tutoriels on utilise surtout
NKRenderer ; NKRHI n'apparaît qu'à la création du device.

## 2. Créer le device : l'auto-détection

```cpp
NkDeviceInitInfo devInfo{};
devInfo.surface = window.GetSurfaceDesc();          // la surface où dessiner
devInfo.width   = (uint32)window.GetSize().width;
devInfo.height  = (uint32)window.GetSize().height;

NkIDevice *device = NkDeviceFactory::CreateAutoDetect(devInfo);
if (!device || !device->IsValid()) { /* erreur + return */ }

logger.Info("[Tuto02] Backend : {0}", NkGraphicsApiName(device->GetApi()));
```

`GetSurfaceDesc()` fait le pont fenêtre → GPU : il contient le HWND Windows,
la Display/Window X11, l'`ANativeWindow` Android… selon la plateforme.

`CreateAutoDetect` essaie les backends **dans l'ordre de priorité de la
plateforme** (par exemple : Vulkan → DX12 → DX11 → OpenGL → Software sur
Windows ; Vulkan → OpenGL ES → Software sur Android) et retourne le premier
qui s'initialise. Votre code ne change jamais — c'est le device qui s'adapte.

> 💡 Le backend **Software** (rasterizer 100 % CPU du moteur) est le filet de
> sécurité : l'app tourne même sans GPU (VM, serveur, CI).

## 3. Créer le renderer

```cpp
NkRendererConfig cfg = NkRendererConfig::ForGame(devInfo.api, devInfo.width, devInfo.height);
NkRenderer *renderer = NkRenderer::Create(device, cfg);
if (!renderer || !renderer->Initialize()) { /* erreur + cleanup + return */ }

NkRender2D        *r2d     = renderer->GetRender2D();   // rectangles, formes 2D
NkOverlayRenderer *overlay = renderer->GetOverlay();    // texte par-dessus tout
```

`ForGame` est un préréglage raisonnable (il existe aussi des configs plus
légères). `Create` + `Initialize` compilent les shaders internes, allouent les
render targets… C'est l'appel le plus coûteux du démarrage.

Le renderer expose ensuite ses **sous-systèmes** par des getters : `GetRender2D()`,
`GetOverlay()`, et à partir de l'étape 3 `GetRender3D()`, `GetMeshSystem()`,
`GetShadow()`.

## 4. La boucle de rendu

```cpp
NkClock clock;
while (running && window.IsOpen()) {
    events.PollEvents();                       // (étape 1)
    const float32 dt = clock.Tick().delta;     // temps écoulé depuis le dernier tour

    if (!renderer->BeginFrame())
        continue;                              // fenêtre minimisée, device occupé…

    // … ici viendront les draw calls 3D (étape 3) …

    if (overlay) {
        const NkSafeAreaInsets sa = window.GetSafeAreaInsets();
        const float32 ox = 10.f + sa.left, oy = 10.f + sa.top;

        overlay->BeginOverlay(renderer->GetCmd(), W, H);
        if (r2d)
            r2d->FillRect({ox, oy, 330.f, 70.f}, {0.f, 0.f, 0.f, 0.6f}); // fond noir 60 %
        overlay->DrawText({ox + 10.f, oy + 20.f}, "== Tuto 02 : NKRenderer est initialise ==");
        overlay->DrawText({ox + 10.f, oy + 40.f}, "Backend : %s", NkGraphicsApiName(device->GetApi()));
        overlay->DrawText({ox + 10.f, oy + 60.f}, "FPS ~ %.1f", dt > 1e-4f ? 1.f / dt : 0.f);
        overlay->EndOverlay();
    }

    renderer->Present();    // pousse l'image à l'écran
    renderer->EndFrame();   // clôt la frame côté device
}
```

Points à bien comprendre :

- **`BeginFrame` peut échouer** (retour `false`) : fenêtre minimisée,
  swapchain en cours de recréation… On saute simplement le tour — sans ce
  `continue`, on dessinerait dans le vide.
- **`NkClock::Tick()`** retourne `{delta, total}` en secondes : `delta` sert
  aux animations (étape 3) et à l'affichage des FPS (`1/dt`).
- **L'overlay s'écrit entre `BeginOverlay` et `EndOverlay`**, coordonnées en
  pixels, origine en haut à gauche.
- **La safe area** (`window.GetSafeAreaInsets()`) vaut 0 partout… sauf sur
  mobile, où elle écarte le panneau de l'encoche et des barres système. La
  prendre en compte dès maintenant coûte deux lignes et rend le code
  réellement portable.

## 5. Le redimensionnement — un piège classique

```cpp
events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent *e) {
    const uint32 w = (uint32)e->GetWidth(), h = (uint32)e->GetHeight();
    if (w > 0 && h > 0 && (w != W || h != H)) {   // <- les 3 conditions comptent
        W = w; H = h;
        renderer->OnResize(w, h);                  // recrée la swapchain
    }
});
```

Pourquoi ces gardes ?
- `w/h == 0` : minimiser la fenêtre envoie une taille nulle — recréer une
  swapchain 0×0 crashe certains backends ;
- `w == W && h == H` : Windows émet un `WM_SIZE` **à la création** de la
  fenêtre ; déclencher `OnResize` avant la première frame corrompt l'état de
  certains backends (piège documenté du moteur). On ne réagit qu'aux
  changements réels.

## 6. Fermeture : l'ordre inverse, toujours

```cpp
device->WaitIdle();               // 1. attendre que le GPU ait fini
NkRenderer::Destroy(renderer);    // 2. détruire le haut niveau
NkDeviceFactory::Destroy(device); // 3. puis le device
window.Close();                   // 4. et enfin la fenêtre
```

`WaitIdle()` d'abord : détruire des ressources que le GPU utilise encore est
un crash garanti (ou pire, aléatoire). Notez aussi le pattern
`Create`/`Destroy` du moteur — jamais de `new`/`delete` bruts.

---

## Compiler et lancer

```bat
cd D:\Projets\2026\Nkentseu\Nkentseu
jenga build --target Tuto02Renderer --config Release
.\Build\Bin\Release-Windows\Tuto02Renderer\Tuto02Renderer.exe
```

Vous devez voir : un fond sombre, un panneau semi-transparent en haut à
gauche avec le nom du backend (probablement `Vulkan` ou `Direct3D 12` sur un
PC récent) et un compteur de FPS. Redimensionnez : pas de crash, le panneau
reste en place.

## Pour aller plus loin (exercices)

1. Affichez la taille courante de la fenêtre dans le panneau (`%ux%u`).
2. Le device choisit tout seul — mais forcez un backend précis avec
   `NkDeviceFactory::CreateForApi(NK_GFX_API_OPENGL, devInfo)` et comparez
   les FPS.
3. Dessinez un deuxième `FillRect` coloré au centre de l'écran qui suit la
   taille de la fenêtre (`W/2`, `H/2`).

## Dépannage

| Symptôme | Cause probable |
|---|---|
| « Creation device KO » | Pilotes GPU absents/anciens — réessayez avec le backend Software |
| « Init renderer KO » + logs `CreateShader fail` | Lancez l'exe **depuis la racine du repo** : les shaders internes se résolvent par rapport au workspace |
| Crash au redimensionnement | Une des 3 gardes du callback resize manque |
| Crash à la fermeture | `WaitIdle()` oublié, ou ordre de destruction inversé |

**Étape suivante → [03-Scene](../03-Scene/README.md)** : caméra, lumière,
ombres et premiers objets 3D.
