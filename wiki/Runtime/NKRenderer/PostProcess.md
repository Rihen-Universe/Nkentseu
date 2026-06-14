# Les outils de post-traitement et de rendu de la scène

> Couche **Runtime** · NKRenderer · Les briques de haut niveau qui prennent une image *brute*
> sortie du GPU et la transforment en image *finale* : pile de post-traitement HDR
> (`NkPostProcessStack`), débruiteur (`NkDenoiserSystem`), renderers 2D/3D (`NkRender2D`,
> `NkRender3D`), texte (`NkTextRenderer`), overlay HUD (`NkOverlayRenderer`) et cible
> hors-écran (`NkOffscreenTarget`).

Une fois que le moteur a *calculé* une image — un buffer HDR de couleurs linéaires, une carte de
profondeur, un champ de vélocité — il reste tout le travail de **finition** : assombrir les coins
(SSAO), faire baver les hautes lumières (bloom), ramener la dynamique infinie du HDR dans l'écran
(tonemap/ACES), lisser les bords crénelés (FXAA), nettoyer le bruit d'un rendu stochastique
(denoise), puis dessiner par-dessus les sprites de l'interface, le texte des menus et les stats du
debug. C'est exactement la famille d'outils de cette page. La question n'est jamais « lequel
calcule l'image » — ça, c'est le rôle du `NkRenderGraph` et des systèmes PBR — mais « lequel
**compose** la scène, **finit** l'image et **dessine par-dessus** ». Chacun de ces sept types est
un sous-renderer autonome avec son propre cycle de vie `Init` / `Shutdown`.

Le fil rouge de toute la famille : ce sont des **passes** qui s'enchaînent sur un même
`NkICommandBuffer`. Certaines ouvrent leur propre *render pass* (`NkOffscreenTarget`,
`NkPostProcessStack::Execute`), d'autres supposent qu'une passe est déjà ouverte par le
`NkRenderGraph` appelant (les sous-passes `DrawBloomDownPass`, `DrawSSAOPass`…). C'est la
distinction la plus importante à garder en tête en lisant cette page.

- **Namespace** : `nkentseu::renderer`
- **Headers** : `NKRenderer/Tools/PostProcess/NkPostProcessStack.h`,
  `Tools/Denoiser/NkDenoiserSystem.h`, `Tools/Overlay/NkOverlayRenderer.h`,
  `Tools/Render2D/NkRender2D.h`, `Tools/Render3D/NkRender3D.h`, `Tools/Text/NkTextRenderer.h`,
  `Tools/Offscreen/NkOffscreenTarget.h`

---

## La finition de l'image : `NkPostProcessStack`

C'est le **maillon final** du rendu HDR. Le moteur dessine d'abord la scène dans une texture
flottante (haute dynamique, couleurs linéaires) ; `NkPostProcessStack` prend cette texture et lui
applique, dans l'ordre, une **pile** d'effets : SSAO/GTAO (occlusion ambiante de proximité), bloom
*dual-Kawase* multi-mip (le halo des hautes lumières), tonemap/ACES (compression de dynamique),
FXAA (anti-crénelage) et color grading par LUT. Le résultat est une texture LDR prête à présenter.

L'idée centrale est qu'une pile de post-traitement n'est **pas** un effet unique : c'est une
**chaîne** où chaque maillon lit la sortie du précédent. On la pilote par un `NkPostConfig` qu'on
copie via `SetConfig`, qu'on modifie en place via `GetConfig()` (référence mutable), et qu'on
inspecte via `HasAnyEffect()` — ce dernier renvoie `true` dès qu'au moins un effet est armé
(`ssao || bloom || toneMapping || fxaa || aces`), ce qui sert à décider si l'on alloue le *path*
HDR transitoire coûteux ou si l'on présente directement.

```cpp
NkPostProcessStack post;
post.Init(device, &texLib, &meshSys, &shaderLib, &resources, width, height);
post.GetConfig().bloom = true;      // on modifie la config en place
post.GetConfig().aces  = true;

// chaque frame :
NkTexHandle ldr = post.Execute(cmd, hdrColor, depthTex);  // pile complète, rend la texture finale
```

Ce n'est **pas** un simple shader de blit : `Execute` orchestre plusieurs sous-passes
(downsample/upsample du bloom, SSAO depth-only, blur, tonemap, FXAA) et gère ses propres cibles
internes. `OnResize` recrée réellement ces cibles (contrairement aux autres renderers qui ne font
que mémoriser la taille). À côté du chemin « tout-en-un » `Execute`, le header expose un chemin
**bas niveau** où chaque sous-passe (`DrawBloomDownPass`, `DrawSSAOPass`…) est appelée
individuellement — mais ces sous-passes **supposent une render pass déjà ouverte** par le
`NkRenderGraph` : elles ne font que *bind pipeline + descriptor + draw quad*, jamais de
`BeginRenderPass`. Confondre les deux niveaux est le piège classique.

> **En résumé.** `NkPostProcessStack` = la chaîne de finition HDR→LDR (SSAO, bloom, ACES, FXAA,
> LUT). `Execute` fait tout et rend la texture finale ; les `Draw*Pass` sont des briques pour le
> RenderGraph et exigent une render pass déjà ouverte. `HasAnyEffect()` décide d'activer le path
> HDR ; `OnResize` recrée vraiment les cibles internes.

---

## Nettoyer le bruit : `NkDenoiserSystem`

Quand une image est calculée par échantillonnage stochastique — *ray tracing*, *path tracing*,
ambient occlusion ou ombres à peu d'échantillons — elle ressort **bruitée** (granuleuse). Le rôle
du débruiteur est de retrouver l'image lisse derrière le grain, en s'aidant de buffers auxiliaires
(albédo, normales, profondeur, mouvement) et de l'**accumulation temporelle** des frames passées.

`NkDenoiserSystem` n'est **pas** un effet unique mais un **multiplexeur de backends** : Intel OIDN,
NVIDIA NRD (ReLAX/ReBLUR), un filtre spatial bilatéral universel, ou la pure accumulation
temporelle (TAA). On choisit explicitement, ou on laisse `NK_DENOISE_AUTO` sélectionner selon le
device (le repli universel effectif étant le filtre spatial). On décrit chaque demande par un
`NkDenoiseRequest` qui porte la texture couleur d'entrée (requise) et les guides optionnels.

```cpp
NkDenoiserSystem denoiser;
denoiser.Init(device, NkDenoiserBackend::NK_DENOISE_AUTO);

NkDenoiseRequest req;
req.colorIn  = rtColorNoisy;     // HDR RGBA16F bruité
req.normalIn = gbufferNormal;    // guide optionnel
req.colorOut = rtColorClean;     // doit être alloué
denoiser.Denoise(cmd, req);
```

Le point délicat est l'**historique temporel** : le débruiteur accumule les frames précédentes en
ping-pong (deux buffers). C'est ce qui donne sa stabilité — mais quand la caméra **coupe**
(téléportation, changement de plan), cet historique devient faux : il faut l'invalider avec
`ResetHistory()` (ou `resetHistory = true` dans la requête), sinon des fantômes traînent à l'écran.

> **En résumé.** `NkDenoiserSystem` choisit un backend de débruitage (OIDN / NRD / spatial / TAA,
> ou AUTO) et nettoie une couleur bruitée en s'aidant d'albédo/normales/profondeur/mouvement.
> `Denoise` (complet) ou `DenoiseFast` (color-only). Pensez à `ResetHistory()` après tout cut
> caméra, sinon l'accumulation temporelle laisse des traînées.

---

## Dessiner en 2D : `NkRender2D`

`NkRender2D` est le renderer **2D batché** complet : sprites, formes vectorielles (rectangles,
cercles, lignes, bézier, polylignes), 9-slice, pile de clip, *et* un éclairage 2D évolué (lumières
ponctuelles, cookies, normal mapping, ombres portées circulaires et AABB, glow). Tout ce qui
s'affiche « à plat » — interface, HUD, jeu 2D, debug — passe par lui.

Le mot-clé est **batché** : au lieu d'un *draw call* par sprite, `NkRender2D` accumule les
géométries dans un grand tampon (`maxVerts`, 65536 par défaut) et les émet en paquets, ce qui est
l'inverse d'un appel naïf un-par-un. On encadre une frame par `Begin` (qui pose la caméra : centre,
zoom, rotation) et `End`, et `FlushPending` émet ce qui reste. Sans `shaderLib` fourni à `Init`,
les pipelines n'ont pas de programme : **rien ne s'affiche** (mode dégradé).

```cpp
NkRender2D r2d;
r2d.Init(device, &texLib, &shaderLib);

r2d.Begin(cmd, w, h);
r2d.FillRoundRect({20,20,200,60}, {0.1f,0.1f,0.12f,1}, 8.f);   // panneau
r2d.DrawSprite({40,30, 32,32}, iconTex);                        // icône
r2d.DrawLine({0,h*0.5f}, {w,h*0.5f}, {1,1,1,0.2f});            // guide
r2d.End();
```

L'**éclairage 2D** suit un ordre strict : on pose les cookies (`SetLightCookie`), puis les lumières
(`SetLights2D`, jusqu'à `kMaxLights2D = 16` + ambient), puis on dessine les shapes lit
(`SetLit(true)`). Une lumière n'affecte une shape que si leurs masques de calque se croisent
(`SetLayerMask`, 8 bits) — c'est ainsi qu'on isole un éclairage à certains calques. Les ombres
portées se déclarent par caster circulaire (`SetShadowCasters2D`) ou AABB
(`SetShadowCastersAABB2D`), avec une `SetIsoTransform` 2×2 pour les vues isométriques. Tous ces
appels d'état (`SetLights2D`, `SetShadow*`, cookies) **doivent être entre `Begin` et `End`**, avant
les draws concernés. Enfin, `DrawSpriteGlow` (halo radial additif paramétré par `SetGlowParams`)
n'est **pas** batché — un quad par appel, coûteux : réservez-le aux objets clés.

> **En résumé.** `NkRender2D` = renderer 2D **batché** (sprites + formes + 9-slice + clip) avec un
> vrai éclairage 2D (lumières, cookies, normal map, ombres circulaires/AABB, glow). Frame entre
> `Begin`/`End` ; ordre lighting `cookie → SetLights2D → draws lit` ; tout l'état lumière reste
> dans la frame. Sans `shaderLib`, rien ne s'affiche ; `DrawSpriteGlow` n'est pas batché.

---

## Dessiner en 3D : `NkRender3D`

`NkRender3D` est le renderer **3D PBR forward** : il consomme une scène (caméra, lumières,
environnement) et des *draw calls* (statiques, instanciés, skinnés), gère la passe d'ombres, le
skybox HDR, les réflexions planaires et un riche jeu de gizmos de debug. C'est le cœur visuel du
rendu 3D au-dessus du `NkRenderGraph`.

Le piège fondateur tient en une règle : `ResetFrame()` doit être appelé **une fois par frame**,
avant toute passe, et il est **distinct** de `BeginScene`. `ResetFrame` remet à zéro le compteur
d'UBO objet *partagé entre toutes les passes* ; si on le faisait dans `BeginScene`, une seconde
passe (réflexion, ombre) écraserait les UBOs de la première. La séquence canonique est donc
`ResetFrame` → `BeginScene(ctx)` → `Submit*(...)` → `Flush(cmd)`.

```cpp
r3d.ResetFrame();                 // UNE FOIS, avant toute passe
r3d.BeginScene(sceneCtx);
r3d.Submit(drawCallCube);
r3d.SubmitSkinned(drawCallCharacter);
r3d.Flush(cmd);                   // émet la passe géométrie
```

Le *submit* se décline selon la géométrie : `Submit`/`SubmitMany` pour les objets simples,
`SubmitInstanced` pour des milliers de copies d'un même mesh (herbe, foule), `SubmitSkinned` (et sa
variante *tinted*) pour les personnages animés par squelette. Les `Flush` viennent en plusieurs
saveurs : le `Flush(cmd)` ordinaire, une surcharge *render-to-texture* qui prend un
`NkRenderPassHandle`, et `FlushIntoRT` qui injecte une **matrice miroir** pré-multipliée pour les
réflexions planaires (avec un *clip plane* optionnel : les draw calls derrière le plan sont
ignorés). Cette dernière ne consomme pas les files et ne ferme pas la scène — on peut donc flusher
le miroir *puis* la vue normale.

Côté état, on règle l'IBL (`SetIBLStrength`, 0..1), le skybox (`SetSkyboxEnabled`, qui exige un
`NkEnvironmentSystem` chargé), le wireframe, la Material Parameter Collection
(`SetMaterialCollection` ; `nullptr` rend le binding 25 invalide et casse les shaders qui en
dépendent), le VoxelAO (`SetVoxelAO`), et jusqu'à `kMaxCookies3D = 8` cookies 2D + `kMaxCookiesCube3D = 4`
cookies cubemap pour les lumières. Enfin, toute une **boîte à outils de debug** dessine lignes,
sphères, cercles, AABB, axes, grilles et flèches dans l'espace monde — indispensable pour
visualiser physique, IA et hiérarchies.

> **En résumé.** `NkRender3D` = renderer 3D PBR forward (ombres, skybox, skinning, instancing,
> réflexions planaires, gizmos). Règle d'or : `ResetFrame()` une fois par frame, **avant** et
> **distinct** de `BeginScene`. `Submit*` selon la géométrie ; `FlushIntoRT` pour les miroirs ;
> `SetMaterialCollection(nullptr)` casse le binding 25.

---

## Afficher du texte : `NkTextRenderer`

`NkTextRenderer` est le **pont texte** du moteur : il charge des polices (backend NKFont par
défaut — TTF/OTF/WOFF, atlas bitmap ou SDF), les dessine en 2D batché *en déléguant à `NkRender2D`*,
mesure des chaînes, projette du texte billboard dans le monde 3D et peut même **extruder** une
chaîne en mesh 3D. Ce n'est donc pas un système de glyphes bas niveau — c'est le NKFont *câblé au
GPU*, prêt à l'emploi.

Le détail à connaître d'emblée : ce header fait `#undef DrawText`. Sur Windows, `DrawText` est une
**macro GDI** qui se substituerait au nom de la méthode et casserait la compilation ; le header la
neutralise pour toute l'unité de compilation qui l'inclut. C'est volontaire et nécessaire.

```cpp
NkTextRenderer text;
text.Init(device, &texLib, &r2d);                   // s'appuie sur NkRender2D
NkFontHandle font = text.LoadFont("fonts/Inter.ttf", 24.f);

text.DrawText({16, 16}, "Score: 42", font, 24.f, 0xFFFFFFFF);
NkVec2f size = text.CalcTextSize("Score: 42", font, 24.f);  // pour centrer/aligner
```

Au-delà du `DrawText` simple, on a `DrawTextCentered` (dans un rectangle), `DrawTextWorld` (texte
projeté depuis une position monde via une `viewProj` — étiquettes flottantes, dégâts en jeu), la
mesure (`CalcTextSize`/`CalcTextWidth` pour le *layout*) et `ExtrudeText3D` qui transforme une
chaîne en `NkMeshHandle` solide utilisable par `NkRender3D` (titres 3D, logos). Le système est
**extensible** : au lieu de NKFont, on peut brancher un backend de police custom via un
`NkTextFontLoaderDesc` (quatre callbacks Load/GetGlyph/Metrics/Destroy) et `LoadFontCustom`.
Attention enfin à `UnloadFont`, qui prend le handle **par référence** et le réinitialise.

> **En résumé.** `NkTextRenderer` = pont texte (NKFont par défaut, ou backend custom à 4
> callbacks), rendu 2D batché via `NkRender2D`, plus texte monde (`DrawTextWorld`), mesure et
> extrusion 3D (`ExtrudeText3D`). Le header neutralise la macro GDI `DrawText` ; `UnloadFont`
> invalide le handle qu'on lui passe.

---

## Le HUD de debug : `NkOverlayRenderer`

`NkOverlayRenderer` est la couche **overlay** la plus légère : elle dessine les statistiques du
renderer, des textures de debug et du texte formaté **par-dessus** tout le reste. Elle ne possède
**pas** de cible de rendu propre — elle se branche simplement sur un `NkRender2D` et un
`NkTextRenderer` existants et émet ses draws dans le contexte courant.

C'est l'outil du **diagnostic en jeu** : afficher les FPS et le nombre de draw calls
(`DrawStats`), inspecter une texture intermédiaire (shadow map, G-buffer, AO) sans quitter
l'application (`ShowTexture` dans un rectangle), ou logger une valeur à l'écran avec une syntaxe
*printf* (`DrawText(pos, "frame %d", n)`). On encadre les émissions par `BeginOverlay`/`EndOverlay`
et `FlushPending` pousse ce qui reste.

```cpp
overlay.Init(device, &r2d, &text);
overlay.BeginOverlay(cmd, w, h);
overlay.DrawStats(renderer.GetStats());            // FPS, draw calls…
overlay.ShowTexture(shadowMap, {w-260, 20, 240, 240});
overlay.DrawText({10, h-24}, "build %s", versionStr);
overlay.EndOverlay();
```

Comme `NkTextRenderer`, ce header fait `#undef DrawText` (macro GDI) — l'inclure désactive
`DrawText`/`DrawTextW`/`DrawTextA` de GDI dans l'unité de compilation. Et comme il **réutilise** un
`NkRender2D` et un `NkTextRenderer`, c'est une couche fine : aucune ressource lourde, juste de
l'orchestration au-dessus des deux sous-renderers.

> **En résumé.** `NkOverlayRenderer` = HUD de debug léger (stats, textures debug, texte *printf*)
> qui réutilise `NkRender2D` + `NkTextRenderer`, sans cible propre. Émissions entre
> `BeginOverlay`/`EndOverlay`. Le header neutralise aussi la macro GDI `DrawText`.

---

## Rendre hors de l'écran : `NkOffscreenTarget`

Tout ce qui précède dessine, in fine, vers l'écran (le swapchain). `NkOffscreenTarget` répond au
besoin inverse : dessiner vers une **texture** plutôt que vers l'écran. C'est un FBO + render pass
encapsulés, avec attachements couleur (et profondeur/stencil optionnels) et un **readback CPU**
facultatif.

Ce n'est **pas** la pile de post-traitement : c'est la *cible* dans laquelle on rend. Les usages
sont partout où une image sert d'entrée à autre chose : un **miroir** ou un portail (rendre la
scène vue d'un autre point), une **caméra de sécurité** dans le jeu, une **miniature** d'asset pour
l'éditeur, un **screenshot** lu côté CPU, une texture procédurale. On décrit la cible par un
`NkOffscreenDesc` (taille, formats — par défaut couleur sRGB 8 bits + profondeur D32, 1024×1024,
flags `hdr`/`readable`/`readback`/`layers`).

```cpp
NkOffscreenDesc desc;
desc.width = 512; desc.height = 512;
desc.readback = true;                       // on veut relire côté CPU

NkOffscreenTarget rt;
rt.Init(device, &texLib, desc);
rt.BeginCapture(cmd);                        // ouvre la passe (clear par défaut)
r3d.Flush(cmd, rt.GetRP());                  // on rend dedans
rt.EndCapture(cmd);

NkTexHandle result = rt.GetColorResult();    // utilisable comme texture
uint8 pixels[512*512*4];
rt.ReadbackPixels(pixels);                   // exige desc.readback = true
```

La séquence est toujours `BeginCapture` → draws → `EndCapture` (le `BeginCapture` ouvre la passe
et clear couleur/profondeur par défaut). On récupère ensuite le résultat comme texture
(`GetColorResult`, `GetDepthResult`) ou côté CPU (`ReadbackPixels`, qui suppose `readback = true`
dans le desc, `rowPitch = 0` donnant un pitch serré). `Resize` recrée les attachements. Par défaut
`readable = true` : la couleur est échantillonnable comme texture ordinaire.

> **En résumé.** `NkOffscreenTarget` = cible de rendu **hors-écran** (FBO + RP, couleur + depth
> optionnel) avec readback CPU facultatif. On rend entre `BeginCapture`/`EndCapture`, on relit par
> `GetColorResult` (GPU) ou `ReadbackPixels` (CPU, exige `readback = true`). L'outil des miroirs,
> miniatures, screenshots et textures procédurales.

---

## Aperçu de l'API

Tous ces types suivent le cycle `Init(...)` / `Shutdown()` (pas de `Create`/`Destroy` ici), prennent
un `NkICommandBuffer* cmd` sur leurs méthodes de dessin, et — sauf `NkPostProcessStack` qui recrée
réellement ses cibles internes — `OnResize` ne fait que mémoriser la taille.

### `NkPostProcessStack` — pile de finition HDR→LDR

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `Init`, `Shutdown`, `OnResize` | Init device+dépendances+taille / libère / recrée vraiment les RT internes |
| Config | `SetConfig`, `GetConfig` | Copie la config / référence mutable (modif en place) |
| Pile complète | `Execute(cmd, hdrIn, depth, velocity=Null)` | Exécute toute la pile, renvoie la texture finale |
| Pile RHI | `ExecuteRHI(cmd, hdrIn, bloomTex, ssaoTex)` | Variante handles RHI (RenderGraph), bind direct |
| Sous-passes bloom | `DrawBloomDownPass`, `DrawBloomUpPass` | Downsample 13-tap / upsample tent — render pass déjà ouverte |
| Sous-passes SSAO | `DrawSSAOPass`, `DrawSSAOBlurPass` | SSAO depth-only / blur de denoise |
| Effets unitaires | `RunSSAO`, `RunBloom`, `RunTonemap`, `RunFXAA` | Chaque effet isolé, renvoie son RT |
| FXAA dédié | `IsFXAAEnabled`, `ExecuteFXAA` | Active ? / passe FXAA sur le RT courant |
| Inspection | `HasAnyEffect` `[noexcept]`, `GetToneTexHandle` | Au moins un effet armé ? / RT LDR intermédiaire |

### `NkDenoiserSystem` — débruitage

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Backends | `NkDenoiserBackend` (OIDN/NRD/SPATIAL/TAA/AUTO) | Choix du moteur de débruitage |
| Modes NRD | `NkNRDMode` (REBLUR/RELAX/SIGMA) | Sous-mode du backend NVIDIA NRD |
| Config | `NkDenoiserConfig` | temporal, historyFrames, spatialRadius/Passes, blendFactor, guides… |
| Requête | `NkDenoiseRequest` | colorIn (requis) + albedo/normal/depth/motion (opt.) + colorOut + resetHistory |
| Cycle de vie | `Init(device, preferred, cfg)`, `Shutdown` | Sélectionne le backend / libère |
| Débruitage | `Denoise(cmd, req)`, `DenoiseFast(cmd, in, out, reset)` | Complet (avec guides) / rapide color-only |
| Historique | `ResetHistory` | Invalide l'accumulation temporelle (cut caméra) |
| Config | `SetConfig`, `GetConfig` | Régler / lire la config |
| Inspection | `IsSupported`, `GetActiveBackend`, `GetBackendName`, `SupportsBackend` | État et capacités |

### `NkRender2D` — renderer 2D batché

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `Init(device, texLib, shaderLib, maxVerts=65536)`, `Shutdown`, `OnResize` | Init (sans shaderLib = rien dessiné) / libère / cache taille |
| Frame | `Begin(cmd, w, h, cX, cY, zoom, rotDeg)`, `End`, `FlushPending` | Ouvre avec caméra / ferme / émet le reste |
| Sprites | `DrawSprite`, `DrawSpriteRotated`, `DrawNineSlice`, `DrawSpriteGlow` | Sprite / pivoté / 9-slice / halo additif (non batché) |
| Formes pleines | `FillRect`, `FillRectGradH/V`, `FillRoundRect`, `FillCircle`, `FillTriangle` | Remplissages (uni, dégradés, arrondi, cercle, triangle) |
| Contours | `DrawRect`, `DrawRoundRect`, `DrawCircle`, `DrawLine`, `DrawArc`, `DrawPolyline`, `DrawBezier` | Traits épaisseur réglable |
| Images | `DrawImage(tex,dst,tint)`, `DrawImage(tex,dst,src,tint)` | Image plein quad / sous-région |
| Éclairage 2D | `SetLit`/`IsLit`, `SetLights2D`, `SetLightCookie`, `SetNormalMap`/`SetNormalMode` | Mode lit, ≤16 lumières + ambient, cookies, normal map |
| Calques | `SetLayerMask`/`GetLayerMask` | Masque 8 bits lumière↔shape |
| Ombres 2D | `SetShadowCasters2D`, `SetShadowCastersAABB2D`, `SetIsoTransform`/`SetIsoTransformIdentity` | Casters cercle/AABB + transform iso |
| Glow | `SetGlowParams` | Couleur/intensité/power du halo |
| Clip/Blend/Layer | `PushClip`/`PopClip`, `SetBlendMode`, `SetLayer` | Pile de clip rect / mélange / ordre |
| Stats | `GetBatchCount`, `GetVertexCount` | Compteurs de la frame |

### `NkRender3D` — renderer 3D PBR

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `Init(...)`, `Shutdown`, `OnResize` | Init (mesh/mat/graph/shadow/env/shaders/res) / libère / cache taille |
| Frame | `ResetFrame`, `BeginScene`, `Flush`, `Flush(rp)`, `FlushIntoRT`, `IsInScene` | UBO reset / ouvre la scène / émet / render-to-texture / miroir |
| Submit | `Submit`, `SubmitMany`, `SubmitInstanced`, `SubmitSkinned`, `SubmitSkinnedTinted` | Simple / lot / instancié / skinné / skinné teinté |
| État rendu | `SetWireframe`, `SetIBLStrength`/`GetIBLStrength`, `SetSkyboxEnabled`/`IsSkyboxEnabled` | Fil de fer / force IBL / skybox HDR |
| Réflexions | `SetMirrorViewProj` | Override viewProj miroir (CameraUBO) |
| Systèmes | `SetMaterialCollection`, `SetVoxelAO` | MPC (binding 25) / VoxelAO (binding 27) |
| Cookies 3D | `SetLightCookie3D` (≤8), `SetLightCookieCube3D` (≤4) | Cookie 2D spot/dir / cubemap point |
| Ombres | `RenderShadowPass(cmd, lightVP)` | Rend les opaques castShadow depuis la lumière |
| Contexte | `GetSceneContext` `[noexcept]`, `GetFrameSlot` `[noexcept]`, `GetFramesInFlight` `[noexcept]` | Scène / slot ring UBO / frames in-flight |
| Debug | `DebugDrawDirectSwapchain`, `DrawDebugLine/Sphere/Circle/AABB/Axes/Grid/Arrow` | Gizmos monde + triangle d'isolation de bug |

### `NkTextRenderer` — pont texte

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Callbacks custom | `NkTextLoadFn`, `NkTextGlyphFn`, `NkTextMetricsFn`, `NkTextDestroyFn` | Backend de police personnalisé |
| Descripteurs | `NkTextFontLoaderDesc` (`IsValid`), `NkFontEntry` | Loader custom (4 callbacks) / entrée police chargée |
| Cycle de vie | `Init(device, texLib, r2d)`, `Shutdown` | S'appuie sur NkRender2D / libère |
| Polices | `LoadFont`, `LoadFontFromMemory`, `GetDefaultFont`, `LoadFontCustom`, `UnloadFont` | Charge (fichier/mémoire/embarquée/custom) / décharge (handle par réf.) |
| Rendu 2D | `DrawText`, `DrawTextCentered`, `DrawTextWorld` | Texte position / centré dans rect / billboard monde |
| Mesure | `CalcTextSize`, `CalcTextWidth` | Dimensions pour le layout |
| 3D | `ExtrudeText3D` | Chaîne → mesh 3D solide |
| Flush | `FlushPending` | Émet en fin de frame |

### `NkOverlayRenderer` — HUD de debug

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `Init(device, r2d, txt)`, `Shutdown`, `OnResize` | Branche les sous-renderers / libère / cache taille |
| Frame | `BeginOverlay`, `EndOverlay`, `FlushPending` | Ouvre / ferme / émet le reste |
| Dessin | `DrawStats`, `ShowTexture`, `DrawText` | Stats renderer / texture debug / texte printf |

### `NkOffscreenTarget` — cible hors-écran

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Descripteur | `NkOffscreenDesc` | taille, formats, hasDepth/Stencil, hdr/readable/readback, layers, name |
| Cycle de vie | `Init(device, texLib, desc)`, `Shutdown`, `IsValid` | Crée FBO+RP / libère / valide ? |
| Capture | `BeginCapture(cmd, clearColor, cc, clearDepth)`, `EndCapture` | Encadre les draws (clear opt.) |
| Readback | `ReadbackPixels(dst, rowPitch=0)` | Lit les pixels CPU (exige readback=true) |
| Redimensionnement | `Resize(w, h)` | Recrée les attachements |
| Résultats | `GetColorResult`, `GetDepthResult`, `GetRP`, `GetFBO` | Texture couleur/depth, render pass, framebuffer |
| Dimensions | `GetWidth`, `GetHeight`, `GetDesc` | Taille et descripteur |

---

## Référence complète

Chaque type est repris ici à fond, avec ses usages dans les différents domaines du temps réel.

### `NkPostProcessStack` à fond

**La pile, pas l'effet.** Le concept central est qu'une chaîne de post-traitement est une
**séquence ordonnée** : SSAO assombrit les recoins → bloom diffuse les hautes lumières → tonemap
ramène le HDR dans [0,1] → FXAA lisse les bords → la LUT corrige les couleurs. `Execute` orchestre
toute cette chaîne et renvoie la texture finale ; `ExecuteRHI` est la variante consommée par le
`NkRenderGraph`, qui bind directement des handles RHI (`bloomTex`, `ssaoTex`) sans les wrapper dans
le `NkTextureLibrary` — un `bloomTex` invalide y vaut « bloom ignoré » (texture noire).

**Deux niveaux d'API.** Le niveau haut (`Execute`) gère ses propres render passes et ses cibles
internes. Le niveau bas (`DrawBloomDownPass`, `DrawBloomUpPass`, `DrawSSAOPass`,
`DrawSSAOBlurPass`) **suppose une render pass déjà ouverte** par l'appelant : ces sous-passes ne
font que bind pipeline + descriptor + draw quad. Le `DrawBloomDownPass` est un downsample 2×
*13-tap* (filtre Call of Duty), le `DrawBloomUpPass` un upsample *tent 3×3* avec blend additif, le
`DrawSSAOBlurPass` un denoise croisé après GTAO. C'est ainsi que le RenderGraph compose lui-même la
chaîne, frame par frame.

**Détails de comportement.** En interne, le bloom travaille sur `kBloomMips = 6` niveaux et réserve
`kBloomDescSets = 33` descriptor sets (11 sous-passes × 3 frames in-flight) — une contrainte
Vulkan : on ne ré-update pas un descriptor set en cours d'usage GPU. La LUT de color grading par
défaut est l'identité (taille 16). `HasAnyEffect()` (`noexcept`) renvoie `true` dès qu'un effet est
armé, ce qui pilote l'activation du *path* HDR transitoire — inutile d'allouer une chaîne lourde
pour une scène sans aucun effet. `GetToneTexHandle()` expose le RT LDR intermédiaire, que
`ExecuteFXAA` relit pour appliquer le FXAA sur le RT courant.

Cas d'usage, par domaine :
- **Rendu** — l'étape de finition standard de toute scène 3D HDR : ACES pour le rendu cinéma, bloom
  pour les sources lumineuses, SSAO pour l'ancrage au sol des objets.
- **2D / UI** — un jeu 2D stylisé peut activer bloom + grading pour un rendu néon, sans toucher au
  pipeline 3D.
- **Outils / éditeur** — toggler chaque effet en live via `GetConfig()` pour prévisualiser leur
  impact, ou tout désactiver (`HasAnyEffect()` → `false`) pour un mode « rendu brut » de debug.
- **GPU** — `ExecuteRHI` est le point d'entrée bas niveau quand on tisse soi-même les passes dans un
  RenderGraph custom.

### `NkDenoiserSystem` à fond

**Le multiplexeur de backends.** `NkDenoiserBackend` énumère les moteurs : `NK_DENOISE_OIDN` (Intel
Open Image Denoise, qualité offline), `NK_DENOISE_NRD` (NVIDIA Real-time Denoisers ReLAX/ReBLUR),
`NK_DENOISE_SPATIAL` (filtre bilatéral universel, le repli effectif), `NK_DENOISE_TAA`
(accumulation temporelle seule) et `NK_DENOISE_AUTO` (sélection selon le device). Quand on choisit
NRD, `NkNRDMode` raffine : `NK_NRD_REBLUR` (diffus + spéculaire, path tracing), `NK_NRD_RELAX`
(lumières directes/indirectes), `NK_NRD_SIGMA` (ombres).

**Config et requête.** `NkDenoiserConfig` règle le compromis stabilité/réactivité : `temporal`
active l'accumulation, `historyFrames` (8) sa profondeur, `blendFactor` (0.1) le mélange entre
historique et frame courante (0 = full history, 1 = no history), `spatialRadius`/`spatialPasses` le
filtre spatial, et les drapeaux `useAlbedo`/`useNormal`/`useMotion` quels guides exploiter.
`NkDenoiseRequest` porte la demande concrète : `colorIn` (HDR RGBA16F, requis), les guides
optionnels (`albedoIn`, `normalIn`, `depthIn` pour la reprojection, `motionIn` pour la TAA),
`colorOut` (à allouer), une `exposure` et un `resetHistory`.

**L'historique est le point sensible.** Le système accumule en ping-pong (deux buffers) — d'où sa
stabilité temporelle. Mais à chaque **discontinuité de caméra** (cut, téléportation, premier frame
après un load), l'historique devient faux et produit du *ghosting* : il faut `ResetHistory()` (ou
`resetHistory = true`). `DenoiseFast` est la version color-only quand on n'a pas de guides.

Cas d'usage, par domaine :
- **Rendu** — nettoyer un buffer de ray/path tracing, des ombres traçées (`NK_NRD_SIGMA`), une AO
  stochastique avant de la passer au post-traitement.
- **GPU / threading** — s'exécute sur le command buffer fourni (compute ou transfer), donc
  s'intègre dans la timeline GPU comme n'importe quelle passe.
- **Gameplay / caméra** — toute cinématique avec coupures doit déclencher `ResetHistory()` au
  changement de plan pour éviter les traînées.
- **Outils** — `GetBackendName()`/`SupportsBackend()` exposent à l'éditeur les options réellement
  disponibles sur la machine.

### `NkRender2D` à fond

**Batching et frame.** Le batcher accumule la géométrie 2D dans un tampon de `maxVerts` sommets
(65536 par défaut) et l'émet en paquets : c'est l'opposé d'un draw call par sprite. La frame
s'ouvre par `Begin` avec une **caméra 2D** (centre, zoom, rotation) — ce qui permet de faire défiler
et zoomer une scène 2D sans recalculer chaque position. `FlushPending` vide le reste, `End` ferme.
Sans `shaderLib`, les pipelines sont sans programme et rien ne s'affiche.

**Le répertoire de dessin.** Sprites (`DrawSprite`, `DrawSpriteRotated` autour d'un pivot,
`DrawNineSlice` pour les panneaux étirables, `DrawSpriteGlow` halo additif). Formes pleines
(`FillRect`, dégradés horizontal/vertical, `FillRoundRect`, `FillCircle`, `FillTriangle`).
Contours d'épaisseur réglable (`DrawRect`, `DrawRoundRect`, `DrawCircle`, `DrawLine`, `DrawArc`,
`DrawPolyline` ouverte ou fermée, `DrawBezier` cubique). Et l'image générale en deux surcharges
(plein quad, ou sous-région `src`).

**L'éclairage 2D, dans l'ordre.** C'est ce qui distingue `NkRender2D` d'un simple sprite batcher.
On pose d'abord les cookies (`SetLightCookie` aux slots [0..7], référencés par `cookieIdx` d'un
`NkLight2DDesc`), puis les lumières (`SetLights2D`, ≤ `kMaxLights2D = 16` + ambient), puis on active
le mode lit (`SetLit(true)`) sur les draws concernés. Une lumière n'éclaire une shape que si leurs
masques de calque se croisent (`SetLayerMask`, 8 bits, défaut `0xFF`) — d'où la possibilité
d'isoler des éclairages par couche (décor / personnages / FX). `SetNormalMap` + `SetNormalMode`
ajoutent un faux N·L 3D ; `SetShadowCasters2D` (cercles, ≤32) et `SetShadowCastersAABB2D` (boîtes,
≤32, slab test) projettent des ombres ; `SetIsoTransform` (2×2) adapte le ray test aux vues
isométriques. **Tout cet état doit vivre entre `Begin` et `End`**, avant les draws lit.

Cas d'usage, par domaine :
- **UI / 2D** — interfaces, panneaux 9-slice, jauges (`DrawArc`), graphes (`DrawPolyline`),
  curseurs.
- **Gameplay 2D** — un jeu 2D complet éclairé dynamiquement (torches, projecteurs avec cookies,
  ombres portées), y compris isométrique.
- **Outils / éditeur** — gizmos 2D, grilles, règles, sélection en surbrillance.
- **Debug** — overlays de courbes et de zones, via la pile de clip (`PushClip`/`PopClip`) pour
  cantonner un tracé à une fenêtre.

### `NkRender3D` à fond

**La règle `ResetFrame`.** C'est l'invariant à ne jamais violer : `ResetFrame()` une fois par
frame, **avant** la première passe, et **distinct** de `BeginScene`. Il remet à zéro le compteur
d'UBO objet *partagé entre toutes les passes* (pool `kMaxObjectsPerFrame = 1024`) ; le confier à
`BeginScene` ferait écraser, par une seconde passe (ombre, réflexion), les UBOs de la première. Le
ring UBO est multi-frame (`framesInFlight` clampé [1,3], lu via `GetFrameSlot`/`GetFramesInFlight`).

**Soumettre la géométrie.** `Submit`/`SubmitMany` pour les meshes statiques, `SubmitInstanced` pour
des milliers d'instances d'un même mesh (herbe, foule, particules), `SubmitSkinned` (+
`SubmitSkinnedTinted` pour appliquer une teinte/alpha) pour les personnages animés par squelette.
La scène elle-même (caméra, lumières) arrive via `BeginScene(NkSceneContext)`.

**Flusher, et les réflexions.** `Flush(cmd)` émet la passe géométrie standard ; `Flush(cmd, rp)`
rend dans une render pass fournie (render-to-texture) ; `FlushIntoRT(cmd, rp, mirrorMat,
mirrorViewProj, clipPlane)` rend une vue **miroir** pré-multipliée pour la réflexion planaire — avec
un `clipPlane = (Nx,Ny,Nz,d)` qui, si `||N|| > 0`, ignore les draw calls tels que
`dot(N, center) + d <= 0` (ne pas refléter ce qui est sous le miroir). `FlushIntoRT` ne consomme
pas les files et ne ferme pas la scène : on peut flusher le miroir puis la vue normale.
`SetMirrorViewProj` injecte directement la viewProj miroir dans le CameraUBO.

**État et systèmes.** `SetIBLStrength` (0..1, ambient image-based), `SetSkyboxEnabled` (exige un
`NkEnvironmentSystem` chargé), `SetWireframe`. `SetMaterialCollection` branche une Material
Parameter Collection (set=0 binding=25) — `nullptr` rend ce binding invalide et casse les shaders
qui en dépendent (les autres restent OK). `SetVoxelAO` injecte une texture 3D d'AO (binding=27).
Les cookies : ≤ `kMaxCookies3D = 8` cookies 2D (spot/directional, référencés par `cookieIdx` d'un
`NkLightDesc`) et ≤ `kMaxCookiesCube3D = 4` cubemaps (point lights). Les pipelines PBR/skybox/shadow
sont créés *lazy* au premier flush (compatibilité render pass Vulkan/DX12).

**Debug.** Une boîte complète de gizmos monde : `DrawDebugLine` (avec durée de vie), `DrawDebugSphere`,
`DrawDebugCircle` (orienté par une normale), `DrawDebugAABB`, `DrawDebugAxes`, `DrawDebugGrid`,
`DrawDebugArrow` — et `DebugDrawDirectSwapchain`, un triangle NDC tracé directement dans le
swapchain (bypass de la passe géométrie) pour isoler un bug d'affichage. `RenderShadowPass` est
appelé par `NkShadowSystem` pour rendre les opaques `castShadow` depuis le point de vue d'une
lumière.

Cas d'usage, par domaine :
- **Rendu** — toute la scène 3D PBR : statiques, foules instanciées, personnages skinnés, skybox,
  miroirs/portails (`FlushIntoRT`).
- **Animation** — `SubmitSkinned` pour les squelettes ; `SubmitSkinnedTinted` pour un flash de
  dégâts ou une équipe colorée.
- **Physique / IA** — visualiser colliders (`DrawDebugAABB`/`DrawDebugSphere`), rayons et chemins
  (`DrawDebugLine`/`DrawDebugArrow`), grilles de navigation (`DrawDebugGrid`).
- **Outils / éditeur** — `DrawDebugAxes` sur l'objet sélectionné, gizmos de transformation,
  rendu-vers-texture pour les miniatures de scène.

### `NkTextRenderer` à fond

**Un pont, pas un moteur de glyphes.** `NkTextRenderer` enveloppe le module NKFont (TTF/OTF/WOFF,
atlas bitmap ou SDF) et le câble au GPU en déléguant le rendu à `NkRender2D`. Une `NkFontEntry`
décrit chaque police chargée (handle, texture d'atlas, SDF ou non, taille, métriques ascent/descent/lineH,
et soit un `NkFont*`/`NkFontAtlas*` NKFont, soit un loader custom). `#undef DrawText` neutralise la
macro GDI homonyme.

**Charger et dessiner.** `LoadFont` (fichier), `LoadFontFromMemory` (octets embarqués),
`GetDefaultFont` (police NKFont intégrée, toujours dispo). Le rendu : `DrawText` (position de base),
`DrawTextCentered` (centré dans un rectangle), `DrawTextWorld` (texte **billboard** projeté depuis
une position monde via une `viewProj` et la taille de viewport). `colorRGBA` vaut `0xFFFFFFFF`
(blanc opaque) par défaut. `UnloadFont` prend le handle **par référence** et le réinitialise.

**Mesurer et extruder.** `CalcTextSize`/`CalcTextWidth` donnent les dimensions avant rendu —
indispensables pour aligner, centrer, ajuster un fond de bouton. `ExtrudeText3D` transforme une
chaîne en `NkMeshHandle` solide (via `NkFont::GenerateTextMesh3D`), utilisable tel quel dans
`NkRender3D` : titres 3D, logos, texte gravé.

**Backend custom.** Pour brancher un autre moteur de police, on remplit un `NkTextFontLoaderDesc`
avec quatre callbacks — `NkTextLoadFn` (charge), `NkTextGlyphFn` (rasterise un glyphe dans
l'atlas), `NkTextMetricsFn` (ascent/descent/line height), `NkTextDestroyFn` (libère) — et on appelle
`LoadFontCustom`. `IsValid()` vérifie que les quatre sont non-null.

Cas d'usage, par domaine :
- **UI** — tout le texte d'interface (menus, scores, dialogues), centré ou aligné via la mesure.
- **Gameplay** — nombres de dégâts flottants, noms au-dessus des personnages (`DrawTextWorld`).
- **Outils / éditeur** — labels de propriétés, valeurs de champs.
- **Rendu 3D** — titres et logos extrudés (`ExtrudeText3D`) intégrés à la scène.

### `NkOverlayRenderer` à fond

**Une couche d'orchestration.** `NkOverlayRenderer` ne possède aucune ressource lourde ni cible de
rendu : il se branche sur un `NkRender2D` et un `NkTextRenderer` existants (`Init(device, r2d,
txt)`) et émet ses dessins par-dessus la scène. Comme `NkTextRenderer`, il fait `#undef DrawText`
(macro GDI).

**Le diagnostic en jeu.** `DrawStats(NkRendererStats, pos)` affiche les statistiques du renderer
(FPS, draw calls, sommets…) à une position donnée. `ShowTexture(tex, dst)` peinte une texture dans
un rectangle — idéal pour inspecter une shadow map, un G-buffer, une AO ou n'importe quelle cible
intermédiaire sans quitter l'application. `DrawText(pos, fmt, ...)` est variadique style *printf*
pour logger une valeur à l'écran. On encadre par `BeginOverlay`/`EndOverlay`, `FlushPending` émet
le reste, `OnResize` mémorise la taille pour les `Begin` ultérieurs.

Cas d'usage, par domaine :
- **Debug / profilage** — HUD permanent de FPS et stats GPU pendant le développement.
- **Outils** — inspecteur de textures intermédiaires du pipeline (`ShowTexture`).
- **Gameplay** — affichage rapide de variables d'état (`DrawText` printf) en phase de mise au point.

### `NkOffscreenTarget` à fond

**Rendre vers une texture.** `NkOffscreenTarget` encapsule un framebuffer + une render pass avec un
attachement couleur et, optionnellement, profondeur/stencil. Le `NkOffscreenDesc` paramètre tout :
taille (1024×1024 par défaut), formats (`NK_RGBA8_SRGB` couleur, `NK_D32_FLOAT` profondeur par
défaut), `hasDepth`/`hasStencil`, `hdr`, `readable` (couleur samplée comme texture, `true` par
défaut), `readback` (lecture CPU), `layers` et un `name`.

**La séquence de capture.** `BeginCapture(cmd, clearColor, cc, clearDepth)` ouvre la passe (clear
couleur/profondeur par défaut), on y rend (par ex. `r3d.Flush(cmd, rt.GetRP())`), puis
`EndCapture`. On récupère ensuite le résultat soit comme **texture GPU** (`GetColorResult`,
`GetDepthResult`, plus `GetRP`/`GetFBO` pour câbler d'autres passes), soit côté **CPU** via
`ReadbackPixels(dst, rowPitch)` — qui suppose `readback = true` dans le desc (`rowPitch = 0` donne
un pitch serré). `Resize` recrée les attachements, `IsValid` indique si la cible est prête.

Cas d'usage, par domaine :
- **Rendu** — miroirs et portails (rendre la scène depuis un autre point de vue), reflets planaires
  combinés à `NkRender3D::FlushIntoRT`.
- **Gameplay** — caméras de sécurité, écrans intra-diégétiques, minimap rendue.
- **Outils / éditeur** — miniatures d'assets, prévisualisations de matériaux, captures d'écran.
- **IO** — `ReadbackPixels` pour exporter une image sur disque (screenshot) ou la traiter côté CPU.
- **GPU** — texture procédurale générée par un shader puis réutilisée comme entrée d'une autre passe.

---

### Exemple

```cpp
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/PostProcess/NkPostProcessStack.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Text/NkTextRenderer.h"
#include "NKRenderer/Tools/Overlay/NkOverlayRenderer.h"
using namespace nkentseu::renderer;

// 1) Rendre la scène 3D vers un buffer HDR (via le RenderGraph), puis finir l'image.
r3d.ResetFrame();                              // une fois, avant toute passe
r3d.BeginScene(sceneCtx);
r3d.Submit(cubeDrawCall);
r3d.SubmitSkinned(characterDrawCall);
r3d.Flush(cmd);

NkTexHandle finalLDR = post.Execute(cmd, hdrColor, depthTex);  // SSAO + bloom + ACES + FXAA

// 2) Dessiner l'UI et le texte par-dessus (2D batché).
r2d.Begin(cmd, w, h);
r2d.FillRoundRect({20,20,220,64}, {0.08f,0.08f,0.10f,0.8f}, 8.f);
text.DrawText({36,40}, "PV: 100", uiFont, 20.f);
r2d.End();

// 3) HUD de debug : stats + inspection d'une shadow map.
overlay.BeginOverlay(cmd, w, h);
overlay.DrawStats(renderer.GetStats());
overlay.ShowTexture(shadowMap, {w-260.f, 20.f, 240.f, 240.f});
overlay.EndOverlay();
```

---

[← Index NKRenderer](README.md) · [Récap NKRenderer](../NKRenderer.md) · [Couche Runtime](../README.md)
