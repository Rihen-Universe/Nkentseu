# La surface publique de `NkRenderer` — ce qui est branché, ce qui ne l'est pas

> Couche **Runtime** · NKRenderer · L'API publique de la façade par domaine, avec pour chacun le
> **nombre de sites d'appel réels** et la liste de ce qui est **déclaré sans être honoré**.

Cette page complète [Core.md](Core.md) (qui décrit l'API) et
[Frame-Contract.md](Frame-Contract.md) (qui décrit la frame). Elle répond à une seule question,
celle qui coûte du temps en maintenance : **est-ce que ça fait quelque chose ?**

⚠️ **Pourquoi cette page existe.** Plusieurs éléments de cette façade sont des **cadrans sans
aiguille** : un setter qui écrit un champ que personne ne lit, un drapeau que six applications
activent et que l'implémentation ne teste jamais, un compteur affiché à l'écran qui n'est jamais
alimenté. Rien ne les distingue à l'usage d'une API qui marche. **Les recenser est la seule
protection.**

Relevé du 24 août 2026 sur `Nkentseu-merge` @ `main`. Corpus : 2 431 fichiers C/C++ hors `Build/`,
`Externals/` et hors `NKRenderer/` lui-même. Les décomptes sont des `grep … | wc -l` avec
désambiguïsation **par variable réceptrice** (`GetWidth`, `GetStats`, `OnResize`… existent sur
d'autres classes).

📌 **Périmètre.** Le cycle de frame est traité dans [Frame-Contract.md](Frame-Contract.md). Les
setters de `NkMaterial` ne sont **pas** repris ici : un relevé existant a établi qu'une trentaine de
setters de surface **n'ont aucun effet en PBR** (le shader lit le chemin du draw call, pas le
`MaterialUBO`). Ce constat fait référence — `NkMaterial.h` déclare 49 `Set*` distincts.

---

## 1. Accès aux sous-systèmes — 18 accesseurs, 3 morts

Tous sont des `return mXxx.Get();` en ligne, sans allocation paresseuse : ils renvoient `nullptr` si
le sous-système n'a pas été alloué.

| Accesseur | Sites | Fichiers | État |
|---|---:|---:|---|
| `GetMeshSystem` | 70 | 26 | cœur |
| `GetTextures` | 59 | 22 | cœur |
| `GetRender3D` | 57 | 29 | cœur |
| `GetCmd` *(cf. §6)* | 31 | — | cœur |
| `GetMaterials` | 27 | 18 | cœur |
| `GetOverlay` | 26 | 26 | cœur |
| `GetRender2D` | 18 | 15 | cœur |
| `GetShadow` | 15 | 7 | courant |
| `GetMaterialCollection` | 7 | 6 | courant |
| `GetRenderGraph` | 5 | 5 | ⚠️ usage **mode partagé** (flux C) |
| `GetVoxelAO` | 5 | 5 | courant |
| `GetPostProcess` | 4 | 2 | rare |
| `GetTextRenderer` | 3 | 3 | rare — les apps passent par `GetRender2D`/`GetOverlay` |
| `GetEnvironment` | 3 | **1** | rare |
| `GetPlanarReflection` | 2 | 2 | rare |
| `GetVFX` | 1 | **1** | rare — `Noge/ECS/Systems/NkParticleSystem.cpp:17` |
| 🔴 `GetShaders` | **0** | 0 | **jamais appelé** |
| 🔴 `GetAnimation` | **0** | 0 | **jamais appelé** |
| 🔴 `GetSimulation` | **0** | 0 | **jamais appelé** |

⚠️ `GetShaders()` (`NkRenderer.h:84`) : le nom n'apparaît **nulle part** hors de sa déclaration et
de son implémentation. `NkShaderLibrary` n'étant accessible que par le renderer, c'est une **API
publique inatteignable**.
`GetAnimation()` et `GetSimulation()` : les sous-systèmes **sont alloués** (`NkRendererImpl.cpp:451`
et `:467`) et pilotés en interne, mais **aucun appelant ne les récupère jamais**.

---

## 2. Cibles offscreen — le groupe le mieux branché

| Méthode | Ce que fait l'implémentation | Sites |
|---|---|---:|
| `CreateOffscreen` (`:103` → `Impl:1695`) | alloue, `Init(device, textures, desc)`, empile ; détruit et renvoie `nullptr` si l'init échoue | 7 |
| `DestroyOffscreen` (`:104` → `:1705`) | scan linéaire, `Shutdown`, `Delete`, annule le pointeur de l'appelant | 7 |
| `SetFinalColorTarget` (`:110` → `:1719`) | stocke `mFinalColorOverride`, **force `mMirrorToScreen = false`**, reconstruit le graphe ; consommé à `:796-798` (`ImportTexture("Swapchain", …)`) | 14 |
| `SetRenderSizeOverride` (`:125` → `:1576`) | pose `mRenderOverrideW/H`, `ApplyRenderSize(touchDevice=false)` ; relu dans `BeginFrame` `:1443` | 12 |
| `SetBackgroundColor` (`:135` → `:488`) | stocke `mClearColor` + reconstruit le graphe **sans garde `mInitialized`** (contrairement aux deux ci-dessus) ; consommé en valeur `NK_CLEAR` à `:859`/`:875` | 8 |
| `SetFinalColorTargetMirror` (`:117` → `:1728`) | comme ci-dessus + `mMirrorToScreen` | **1** |

⚠️ **Piège mesuré sur `SetFinalColorTargetMirror`** : le blit miroir (`Impl:1406-1413`) est **gardé
par `mPostProcess` non nul**. Sans `NK_SS_POST_PROCESS`, demander le miroir **ne fait rien, en
silence**. Son unique appelant est `Sandbox/Demo/main.cpp:1040` ; tous les éditeurs utilisent
`SetFinalColorTarget` nu.

---

## 3. Overlay UI applicatif — branché correctement

`SetUIOverlayCallback` (`NkRenderer.h:145` → `Impl:482`) stocke le callback et reconstruit le graphe.
Consommé à `Impl:1386` (la passe Overlay2D est créée si `has2D || hasOverlay || mUIOverlayCb`) et
invoqué à `:1395-1396`. **4 sites, 2 fichiers** — paires enregistrement/désenregistrement :
`Nogee/Layers/UILayer.cpp:69` et `:97`, `PV3DE/Layers/MedicalUILayer.cpp:55` et `:83`.

---

## 4. Configuration dynamique — dont **deux setters inertes**

| Méthode | Ce qu'elle fait vraiment | Sites |
|---|---|---:|
| `OnResize` (`:79` → `:1572`) | `ApplyRenderSize(touchDevice=true)` : refuse < 32 px, `mDevice->OnResize`, propage à Render2D/3D/Overlay/PostProcess, `WaitIdle`, reconstruit le graphe | 15 |
| `SetPostConfig` (`:165` → `:1646`) | compare l'ancien/nouveau jeu de passes, arme `mPostGraphDirty`, assigne `mCfg.postProcess`, forwarde à `mPostProcess->SetConfig` | 4 |
| `FlushGraphRebuilds` (`:176` → `:1687`) | si `mPostGraphDirty` : reconstruit le graphe | 3 |
| `SetFrameRateCap`/`GetFrameRateCap` (`:75`/`:76`) | pose `mFrameCapFps`, consommé dans `Present()` `:1547-1569` | 3 / 2 |
| 🔴 `SetVSync` (`:164` → `:1642`) | **`mCfg.vsync = e;` — et rien d'autre** | 2 |
| 🔴 `SetWireframe` (`:166` → `:1681`) | pose `mCfg.wireframe` (jamais relu) puis `mRender3D->SetWireframe(e)` | **0** |

### 🔴 `SetVSync` n'a aucun effet

`mCfg.vsync` est écrit ici et par deux presets, et **relu nulle part** — vérifié par un `grep vsync`
exhaustif sur tout le module : 7 occurrences, dont 3 commentaires, 1 déclaration, 2 presets et
l'écriture du setter. **Aucun appel à un `SetVSync` device ni à un intervalle de swap.** Les deux
interrupteurs d'interface (`NkDemo3D.cpp:4560`, `Sandbox/Demo/Demo3D.cpp:2394`) ne font que basculer
un booléen stocké.
*Non vérifié :* si le vsync est appliqué à la **création du device**, depuis un chemin de
configuration hors NKRenderer.

### `SetWireframe` : 0 appel sur la façade

Les 10 appels du dépôt visent **directement** `NkRender3D` — y compris
`Noge/ECS/Systems/NkRenderSystem.h:78` (`mRenderer->GetRender3D()->SetWireframe(v)`), qui contourne
délibérément la façade. Rien n'est perdu : `mCfg.wireframe` n'est de toute façon jamais relu.

> ⚠️ **CORRECTION DATÉE — 27/08/2026.** Cette page affirmait le 24/08 que `FlushGraphRebuilds` était
> *« **exactement** consommé par les trois hôtes en mode partagé pour lesquels il a été écrit »*.
> **C'était faux, et de la pire façon : le mot « exactement » était le problème.** Il y a **5** hôtes
> en mode partagé, pas 3 — et les **2** qui manquent sont précisément ceux qui n'appellent **pas**
> `FlushGraphRebuilds`. J'avais compté les appelants de la fonction et conclu sur les sites qui en
> auraient besoin : c'est l'erreur que ce chantier documente ailleurs — *compter les sites d'un nom
> ne dit pas où ce nom **manque***.
>
> Je laisse la correction visible et datée plutôt que de réécrire la ligne : une page écrite il y a
> trois jours qui vieillit à vue est plus fiable qu'une page qui se corrige en silence.

`FlushGraphRebuilds` est consommé par **3 des 5** hôtes en mode partagé (flux C) :
`NkDemo3D.cpp:5708`, `NkMatPreview3D.h:374`, `NKXRDemo/main.cpp:729`.

🔴 **Les 2 autres ne l'appellent pas** — 0 occurrence dans tout le fichier :
`NK3DModeler/.../Viewport/NkViewport3D.cpp` et `NkAnimaEditor/.../AnimBridge.cpp`. Dans ces deux
éditeurs, activer SSAO / bloom / FXAA / TAA depuis un panneau **ne fait rien** : c'est le défaut du
9 août, corrigé à un seul endroit. Décompte complet, chronologie et scores de conformité :
[Frame-Contract.md § Flux C](Frame-Contract.md).

---

## 5. Sous-systèmes à chaud — API réelle, **zéro consommateur de production**

`EnableSubsystem` (`:184`), `DisableSubsystem` (`:189`), `IsSubsystemActive` (`:192`),
`GetActiveSubsystems` (`:195`) sont pleinement implémentés (`Impl:524-647`) — et **consommés par un
seul fichier** : `Sandbox/Demo/DemoSubsystems.cpp`, la démo écrite pour les exercer.

Défauts relevés dans l'implémentation (non corrigés) :

- `EnableSubsystem` appelle `mCfg.Enable(flags)` **inconditionnellement**, y compris pour un flag dont
  l'init a échoué ou qui n'a aucun sous-système derrière — l'état rapporté ment alors.
- `InitShadow` est testé **deux fois** (`:535` et `:546`).
- `DisableSubsystem` ne remet jamais à zéro `mPlanarReflection` ni `mVoxelAO`, alors que les deux
  sont créés sous `needsR3D`.

### `NkSubsystemFlags` (`NkRendererConfig.h:30-51`) — 6 drapeaux sur 18 ne servent à rien

| Drapeau | Lu par l'implémentation ? |
|---|---|
| `RENDER2D`, `RENDER3D`, `TEXT`, `SHADOW`, `POST_PROCESS`, `VFX`, `ANIMATION`, `OVERLAY`, `SIMULATION` | **oui** |
| ⚠️ `NK_SS_UI` | **partiellement** — seulement comme terme d'un `||` dans `needsR2D` (`:185`) et dans la cascade de désactivation. **Aucun sous-système UI n'est jamais alloué**, et `GetActiveSubsystems` ne le rapporte jamais → `IsSubsystemActive(NK_SS_UI)` est **toujours faux** |
| 🔴 `NK_SS_OFFSCREEN` | **NON** — et **6 sites applicatifs l'activent** (`NkDemo3D.cpp:10992`, `NkMatPreview3D.h:187`, `NkViewport3D.cpp:727`, `AnimBridge.cpp:683`, `NKXRDemo/main.cpp:336` et `:463`). `CreateOffscreen` fonctionne indépendamment du drapeau : **c'est une ligne placebo recopiée dans 5 applications** |
| 🔴 `NK_SS_RAYTRACING`, `NK_SS_GPU_CULLING`, `NK_SS_2D_ESSENTIALS`, `NK_SS_3D_BASE`, `NK_SS_DEBUG` | **NON** — déclarés, utilisés par personne, nulle part |

---

## 6. Statistiques — **le cas d'école du cadran sans aiguille**

`GetStats` (`:198`) a **16 sites externes dans 15 fichiers**, et **tous sont la même ligne** :
`overlay->DrawStats(renderer->GetStats())`. **Aucun appelant n'inspecte un champ.**
🔴 `ResetStats` (`:199`) : **0 site externe** (les 9 occurrences du nom appartiennent à `NKMemory`,
`NKCanvas` et `NKRHI`).

### `NkRendererStats` (`NkRendererTypes.h:658-679`) — 16 champs

| Champ | Écrit ? | Lu ? |
|---|---|---|
| `drawCalls`, `triangles`, `batchCount` | **oui** (`Impl:1505-1511`) | seulement `NkOverlayRenderer.cpp:49-50` |
| `cpuTimeMs` | **oui** (`:1516`) | seulement l'overlay |
| `vertices` | **oui** (`:1507`) | 🔴 **personne** — écrit chaque frame, jamais affiché |
| 🔴 **`gpuTimeMs`** | **NON — jamais écrit** | **AFFICHÉ** — `NkOverlayRenderer.cpp:50` imprime `"GPU:%.2fms"` |
| `textureBinds`, `shaderSwitches`, `pipelineSwitches`, `culled`, `lightsActive`, `shadowCasters`, `cullTimeMs`, `shadowTimeMs`, `geomTimeMs`, `postTimeMs` | **NON** | **NON** |

> 🔴 **`gpuTimeMs` affiche `0.00 ms` en permanence, dans les 15 applications qui appellent
> `DrawStats`.** Ce n'est pas un oubli local : **aucun backend NKRHI ne sait produire un timestamp
> GPU** (cf. [NKRHI/Backend-Divergence.md](../NKRHI/Backend-Divergence.md) §6). Le commentaire de
> `Impl:1513-1515` l'assume explicitement — mais l'overlay, lui, affiche le chiffre comme s'il était
> mesuré.

**Bilan : 11 champs sur 16 ne sont jamais écrits ; 12 sur 16 ne sont jamais lus.** Seuls
`drawCalls`, `triangles`, `cpuTimeMs` et `batchCount` bouclent écriture → lecture, et uniquement dans
l'overlay du module lui-même.

---

## 7. Accès bas niveau

| Méthode | Corps | Sites |
|---|---|---:|
| `GetCmd` (`:203`) | `return mCmd;` | 31 |
| `GetWidth` / `GetHeight` (`:205`/`:206`) | `return mCfg.width/height;` | 12 / 6 |
| `GetConfig` (`:207`) | `return mCfg;` | 9 (8 dans `NkDemo3D.cpp`, pour relire `postProcess` avant `SetPostConfig`) |
| `GetDevice` (`:202`) | `return mDevice;` | 3 |
| 🔴 `GetFrameIndex` (`:204`) | `return mFrameIndex;` | **0** |

⚠️ `GetFrameIndex` n'est pas seulement inutilisé : il expose le **compteur privé du renderer**, qui
n'est *pas* celui qui fait autorité. Tout le code à tampon circulaire utilise
`mDevice->GetFrameIndex()` (`Impl:78-90`). Un futur appelant qui ferait confiance à cet accesseur
indexerait sur le mauvais compteur.

---

## 8. `NkRendererConfig` — **11 champs sur 22 ne sont jamais lus**

Méthode : pour chaque champ, `grep` sur `(mCfg|cfg|c)\.<champ>` dans tout `NKRenderer/src`, puis
**inspection manuelle de chaque occurrence** pour écarter les champs homonymes d'autres structures
(`NkOffscreenDesc.hdr/width/height`, `NkVFXEmitterDesc.maxParticles`,
`NkDeferredPass::Config.maxLights`, `NkVirtualShadowMaps::Config.quality` — chacune a son **propre**
`mCfg`).

| Champ vivant | Où il est lu |
|---|---|
| `api` | `Impl:72, 134, 158` |
| `width` / `height` | `GetWidth/GetHeight`, `ApplyRenderSize`, dimensionnement des transients |
| `subsystems` | `Impl:185-260`, `576`, `619` |
| `framesInFlight` | `Impl:91, 95, 96` — ⚠️ **écrasé** par `mDevice->GetMaxFramesInFlight()` : la valeur de config n'est qu'une suggestion journalisée |
| `shadow`, `postProcess`, `ibl`, `deferred` | lus abondamment |

| 🔴 Champ mort | Ligne | Constat |
|---|---|---|
| `pipeline` (`NkPipelineMode`) | :366 | 0 occurrence. Les 4 presets le posent ; **rien ne branche dessus**. Le vrai commutateur avant/différé est le booléen `deferred` |
| `quality` (`NkRenderQuality`) | :367 | 0 occurrence (les hits `mCfg.quality` appartiennent à `NkVirtualShadowMaps`, autre structure) |
| `hdr` | :368 | 0 occurrence (les `hdr` lus sont `desc.hdr` sur `NkOffscreenDesc`) |
| `msaaSamples` | :370 | le jeton n'apparaît nulle part ailleurs |
| `maxLights` | :373 | `NkDeferredPass` a son propre `Config::maxLights = 1024` ; celui-ci (256) ne lui est jamais transmis |
| `maxParticles` | :374 | tous les hits sont le `desc.maxParticles` par émetteur de `NkVFXSystem` |
| `maxMeshes` | :375 | jeton absent ailleurs |
| `cluster` (`NkClusterConfig`) | :397 | 0 occurrence — et `NkRendererTypes.h:499` porte le commentaire « doit matcher cluster.maxLightsPerCluster » à côté d'un `uint32 indices[256]` **codé en dur** : la structure n'existe que pour être recopiée à la main |
| `debugOverlay` | :413 | 0 occurrence. `ForEditor` le met à `true` (`:534`) sans effet ; l'overlay dépend de `NK_SS_OVERLAY` |
| `validation` | :416 | 0 occurrence. Les couches de validation Vulkan **ne sont pas pilotées d'ici** |
| 🔴 `voxelAOEnabled` | :404 | voir l'encadré ci-dessous |
| *(en écriture seule)* `vsync`, `wireframe` | :369, :414 | écrits par leurs setters, jamais relus |

> 🔴 **`voxelAOEnabled` : le commentaire contredit le code.** Le commentaire
> (`NkRendererConfig.h:399-403`) promet noir sur blanc : *« false = sous-systeme NON alloue (gratuit)
> et GetVoxelAO() renvoie nullptr »*. Or le jeton `voxelAOEnabled` n'apparaît **qu'à sa
> déclaration** : `NkRendererImpl.cpp:226` alloue `NkVoxelAOSystem` **inconditionnellement** dès que
> `needsR3D`. Avec la valeur par défaut `false`, `GetVoxelAO()` renvoie donc un système **vivant**, et
> le coût annoncé comme évité (texture 3D + compute) **est payé**. C'est le pire cas de cette page :
> une documentation qui promet une optimisation que le code n'applique pas.

Les six presets `ForGame/ForFilm/ForArchviz/ForMobile/ForEditor/ForOffscreen` passent l'essentiel de
leur corps à régler des champs que personne ne lit.

---

## 9. Défauts par défaut : 5 méthodes non pures dans une interface pure

`NkRenderer.h` déclare cinq méthodes avec un **corps par défaut inerte** au lieu de `= 0` :

| Méthode | Ligne | Corps par défaut |
|---|---|---|
| `SetFinalColorTargetMirror` | :117 | `(void)mirrorToScreen; SetFinalColorTarget(target);` — **jette silencieusement la demande de miroir** |
| `SetRenderSizeOverride` | :125 | vide |
| `SetBackgroundColor` | :135 | vide |
| `SetUIOverlayCallback` | :145 | vide |
| `FlushGraphRebuilds` | :176 | vide |

`NkRendererImpl` est la **seule** sous-classe de `NkRenderer` du dépôt (un unique
`public NkRenderer`, `NkRendererImpl.h:34` ; les 48 sites externes passent par la fabrique). Ces
corps par défaut **ne s'exécutent donc jamais** aujourd'hui. Ils existent pour épargner à un second
backend hypothétique de les implémenter — au prix de transformer tout futur override manquant en
**no-op silencieux plutôt qu'en erreur de compilation**.

---

## 10. Récapitulatif — déclaré mais non honoré

1. `GetShaders()`, `GetAnimation()`, `GetSimulation()` — **0 site d'appel**
2. `SetWireframe()` sur la façade — 0 site (10 appels la contournent)
3. `ResetStats()`, `GetFrameIndex()` — 0 site
4. `SetVSync()` — écrit un champ que rien ne lit
5. `NkRendererStats::gpuTimeMs` — **affiché, jamais écrit** (0.00 ms partout)
6. `NkRendererStats` — 11 champs sur 16 jamais écrits, 12 sur 16 jamais lus
7. `NK_SS_OFFSCREEN` — activé par 6 sites applicatifs, lu par personne
8. `NK_SS_RAYTRACING`, `NK_SS_GPU_CULLING`, `NK_SS_2D_ESSENTIALS`, `NK_SS_3D_BASE`, `NK_SS_DEBUG` — 0 usage
9. `NK_SS_UI` — `IsSubsystemActive(NK_SS_UI)` toujours faux
10. `NkRendererConfig` — 11 champs morts, 2 en écriture seule
11. `voxelAOEnabled` — **contrat documenté contredit par l'implémentation**
12. `SetFinalColorTargetMirror` — le miroir est inerte sans `NK_SS_POST_PROCESS`

[← Le cœur du renderer](Core.md) · [← Contrat de frame](Frame-Contract.md) · [← Doc NKRenderer](README.md)
