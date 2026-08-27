# Le contrat de frame

> Couche **Runtime** · NKRenderer · Ce que `BeginFrame`, `Present` et `EndFrame` **font
> réellement**, combien de flux de frame existent dans le dépôt, et lesquels sont corrects.

Cette page est le résultat d'un **relevé mesuré** (24 août 2026) : chaque affirmation vient de la
lecture d'une implémentation ou d'un décompte de sites d'appel, pas d'un nom de méthode ni d'un
commentaire. Les chiffres sont donnés parce qu'un contrat de frame sans décompte n'est pas
vérifiable.

---

## 1. Ce que fait chaque point d'entrée

Les trois méthodes sont déclarées dans `NkRenderer.h:66-68` et implémentées dans
`Core/NkRendererImpl.cpp`. **L'ordre correct est `BeginFrame` → (soumission) → `Present` →
`EndFrame`.** Les noms suggèrent l'inverse ; le code dit ceci :

### `BeginFrame()` → `bool` — `NkRendererImpl.cpp:1421`

| Ce qu'elle fait | Détail |
|---|---|
| Redimensionne **avant** d'ouvrir la frame | Compare la swapchain à `mCfg` et appelle `OnResize` (sauf en mode `SetRenderSizeOverride`). Fait après l'ouverture, cela détruisait des cibles encore liées → mort à la restauration d'une fenêtre réduite. |
| Ouvre la frame device | `mDevice->BeginFrame(mFrameCtx)` — **si elle renvoie `false`, la frame est à sauter** |
| Reconstruit le graphe si nécessaire | `ConsumeSelOutlineGraphDirty()` puis `FlushGraphRebuilds()` — à l'aplomb de la frame, jamais en son milieu |
| Recharge les shaders à chaud | `mShaders->PollHotReload()`, throttlé 1 frame sur 60 |
| Ouvre le command buffer | `mCmd->Reset()`, `mCmd->Begin()`, `mCmd->ResetStats()` |
| Réarme les sous-systèmes | `mRender3D->ResetFrame()` (pool d'UBO objets), `mMaterialCollection->Upload()` |

- **Exige avant :** `Initialize()` réussi (`mInitialized`).
- **Laisse après :** une frame device ouverte **et** un command buffer ouvert. Les deux doivent
  être fermés — c'est `Present` qui ferme le command buffer, `EndFrame` qui ferme la frame device.
- ⚠️ **Ne remet pas `mStats` à zéro** — volontaire : l'overlay la lit *pendant* la frame ; elle est
  recopiée en fin de frame.

### `Present()` → `void` — `NkRendererImpl.cpp:1520`

C'est **elle qui fait tout le travail GPU**, malgré son nom.

| Ce qu'elle fait | Détail |
|---|---|
| Réflexions planaires | `mPlanarReflection->RenderReflections(...)` — **avant** toute `BeginRenderPass` du graphe (Vulkan interdit les render passes imbriquées) |
| **Exécute le render graph** | `mRenderGraph->Execute(mCmd)` — toutes les passes : ombres, géométrie, post-process, overlay |
| **Ferme le command buffer** | `mCmd->End()` |
| **Soumet et présente** | `mDevice->SubmitAndPresent(mCmd)` |
| Cadence la frame | Pacing sleep+spin ancré sur l'horaire idéal si `mFrameCapFps > 0` |

- **Exige avant :** `BeginFrame()` ayant renvoyé `true` (garde : `if (!mCmd) return;`), et la
  soumission de la scène.
- **Laisse après :** command buffer fermé, travail soumis, image présentée. Le graphe **n'est pas**
  reset (il persiste entre frames ; seul `RebuildRenderGraph()` le reconstruit).

### `EndFrame()` → `void` — `NkRendererImpl.cpp:1496`

| Ce qu'elle fait | Détail |
|---|---|
| Clôt la frame device | `mDevice->EndFrame(mFrameCtx)` |
| Fige les statistiques | Recopie `mCmd->Stats()` (draw calls, triangles, vertices) dans `mStats` |
| Mesure le temps CPU | `mStats.cpuTimeMs` |

- **Exige avant :** que la frame ait été **soumise**, c'est-à-dire que `Present()` ait déjà tourné.
- **Ne soumet rien. Ne ferme pas le command buffer. Ne présente rien.**
- ⚠️ `mStats.gpuTimeMs` reste à zéro : les requêtes de timestamp ne sont pas câblées. Le champ est
  laissé nul plutôt que rempli d'un chiffre inventé.

### `SubmitDrawList` / `SubmitRenderGraph` — **cherché, absents de cette façade**

`NkRenderer.h` **ne déclare ni `SubmitDrawList` ni `SubmitRenderGraph`**. Les 211 lignes du header
ont été lues intégralement. Le `SubmitDrawList` que l'on trouve dans NK3DModeler appartient à une
**autre classe** — voir le flux B ci-dessous. La soumission au render graph se fait via
`GetRenderGraph()->Execute(cmd)` ou, en flux normal, à l'intérieur de `Present()`.

---

## 2. Combien de flux existent réellement : **trois**

Ils sont distincts non par style, mais par **qui possède la frame device**.

### Flux A — la façade possède la frame (`NkRenderer`)

```cpp
if (!renderer->BeginFrame()) continue;   // frame à sauter
renderer->GetRender3D()->BeginScene(cam);
renderer->GetRender3D()->Submit(drawCall);
renderer->GetRender3D()->EndScene();
renderer->Present();    // exécute le graphe, ferme le CB, soumet + présente
renderer->EndFrame();   // clôt la frame device
```

C'est le flux canonique. **`Present` avant `EndFrame`.**

### Flux B — la coquille éditeur possède la frame (`editorkit::NkIEditorRenderer`)

⚠️ **Ce n'est pas `NkRenderer`.** C'est une interface **entièrement différente**, implémentée par
`nkgui::NkEditorRHIRenderer` (`Integrations/NKGui/NkEditorRHIRenderer.h`, header-only).

```cpp
renderer.BeginFrame();                          // ouvre device + CB + passe backbuffer
renderer.SubmitDrawList(ui.dl, w, h);           // accumule (fusion dans une liste unique)
renderer.SubmitDrawList(ui.dlOverlay, w, h);
renderer.EndFrame();                            // Submit + EndRenderPass + End + SubmitAndPresent + EndFrame device
```

**Il n'y a pas de `Present()` à appeler parce que `EndFrame()` fait elle-même le
`SubmitAndPresent`** (`NkEditorRHIRenderer.h`, corps de `EndFrame`). Ce flux a **deux** points
d'entrée, pas trois — c'est un contrat différent, sur une classe différente.

`SubmitDrawList` n'envoie rien : elle **accumule** dans `mMerged` (`Append` décale les index) et
l'envoi se fait une seule fois dans `EndFrame`. Appeler `mBackend.Submit` deux fois écrasait le
début du tampon de sommets partagé — c'est la cause du défaut « la bande du haut n'est pas peinte ».

### Flux C — un hôte possède la frame, `NkRenderer` est embarqué (mode partagé)

L'éditeur ouvre la frame device (flux B) et fait rendre une vue 3D **dans la même frame**, sur le
**même command buffer**. Ici, `BeginFrame`, `Present` et `EndFrame` de `NkRenderer` **ne sont jamais
appelés**. L'hôte rejoue à la main ce que `BeginFrame` ferait, puis exécute le graphe :

```cpp
// « Ce que NkRenderer::BeginFrame ferait si nous possedions la frame »
r3d->ResetFrame();  materialCollection->Upload();  renderer->FlushGraphRebuilds();
// ... soumission de la scène ...
if (auto *graph = r3->GetRenderGraph()) graph->Execute(cmd);
// PAS de BeginFrame/EndFrame/Present : l'éditeur possède la frame device.
```

> ⚠️ **CORRECTION DATÉE — 27/08/2026.** La première version de cette page (24/08) annonçait
> **2 sites** pour ce flux. Un relevé exhaustif en compte **5**. Je laisse la correction visible
> plutôt que de réécrire le chiffre en douce : *qu'une page écrite il y a trois jours soit déjà
> fausse est exactement l'argument de ce chantier* — une documentation qui date ses mesures
> vieillit visiblement au lieu de se périmer en silence.

**5 sites mesurés** (`grep` sur `GetRenderGraph()` hors `Build/`, `Externals/`, `Kernel/` → 5) :

| Site | Fichier | Bloc de rejeu | `graph->Execute(cmd)` |
|---|---|---|---|
| A | `NK3DModeler/.../Viewport/NkDemo3D.cpp` | 5697-5711 | 10940-10941 |
| B | `NK3DModeler/.../Viewport/NkMatPreview3D.h` | 372-377 | 561-562 |
| C | `NK3DModeler/.../Viewport/NkViewport3D.cpp` | 2106-2109 | 2622-2623 |
| D | `NKXRDemo/src/NKXRDemo/main.cpp` (les 2 rendus **par œil**) | 726-733 | 796-797 |
| E | `NkAnimaEditor/.../AnimBridge.cpp` | 883-886 | 1038-1039 |

⚠️ Le site D est **hybride** : `rMain->BeginFrame()` est bien appelé pour le compositeur
(`main.cpp:717`), mais les deux renderers **par œil** ne le sont jamais — ce sont eux qui rejouent.

#### 🔴 Le rejeu a DÉRIVÉ : aucun site ne fait plus de 3 des 9 étapes applicables

`BeginFrame` fait **12 étapes**. Trois (`resize`, `mDevice->BeginFrame`, `mCmd Reset/Begin`) sont
structurellement le travail de l'hôte en mode partagé — restent **9 étapes applicables**.

| Site | Score | `FlushGraphRebuilds` (S6) ? |
|---|---|---|
| A `NkDemo3D.cpp` | 3/9 | ✔ `:5708` |
| B `NkMatPreview3D.h` | 3/9 | ✔ `:374` |
| **C `NkViewport3D.cpp`** | **2/9** | 🔴 **✘ — 0 occurrence dans tout le fichier** |
| D `NKXRDemo/main.cpp` | 3/9 | ✔ `:729` |
| **E `AnimBridge.cpp`** | **2/9** | 🔴 **✘ — 0 occurrence dans tout le fichier** |

La « recette » pratiquée est un trio figé — `FlushGraphRebuilds` + `ResetFrame` + `Upload` — et
**six étapes sont omises par les cinq sites uniformément** :

| Étape omise | Par | Conséquence concrète |
|---|---|---|
| 🔴 **S5** `ConsumeSelOutlineGraphDirty` → `RebuildRenderGraph` | **5/5** | Le basculement « contour de sélection » **ne peut pas prendre effet** — dans le modeleur, la seule application où il compte. C'est le prochain incident `FlushGraphRebuilds`, déjà armé |
| **S7** `PollHotReload` | **5/5** | Le rechargement à chaud des shaders est **mort en silence** dans tous les éditeurs |
| **S8** `mFrameCounter++` | 5/5 | Figé à 0. Piège amorcé : qui ajoutera S7 sans S8 aura un throttle 1/60 dégradé en « chaque frame » |
| **S10** `mCmd->ResetStats()` | 5/5 | `GetStats()` d'un renderer en mode partagé renvoie **zéro en permanence** |
| **S1/S2** `mFrameCtx` + horodatage CPU | 5/5 | `cpuTimeMs` sans signification sur ces renderers |

#### Comment la dérive s'est produite — et le commit qui la nomme sans la corriger

`FlushGraphRebuilds` est né le **2026-08-09** (`b08027c85`, « le bouton d'occlusion ambiante agit
enfin en mode partage »). À cette date, trois sites existaient : E (`3240b1ae`, **2026-06-29**),
C (`9b9e12fe`, 2026-07-31), A (2026-08-01). **Le commit n'a corrigé que A.**

🔴 Et pourtant, le commentaire que ce même commit écrit dans `NkDemo3D.cpp:5703-5704` dit :
*« (Meme contrainte et meme modele que NkViewport3D.cpp.) »* — il **nomme** le site C comme
partageant « la même contrainte et le même modèle », et le laisse non corrigé. `NkRenderer.h:170`,
écrit par le même commit, nomme **les deux** : *« cf. NkDemo3D/NkViewport3D »*. Le site E, le plus
ancien — et que `NkViewport3D.cpp:10-11` cite comme **son propre modèle** — n'est mentionné nulle
part, et a été modifié **quatre fois depuis** sans jamais recevoir l'appel.

Les sites écrits *après* le 09/08 (D le 10/08, B le 16/08) ont copié la bonne recette : la dérive
n'est pas une négligence, c'est **une recette qui se recopie et qu'un correctif n'a rattrapée qu'à
un seul endroit**.

#### ⚠️ La suppression annoncée de `NkViewport3D.cpp` ne règle pas le problème

`NkViewport3D.cpp` doit être supprimé et `NkDemo3D` renommé en `NkViewport3D`. Mesuré :
`NK3DModeler.jenga:43` compile `src/**.cpp` — **`NkDemo3D.cpp` et `NkViewport3D.cpp` coexistent
aujourd'hui dans le même binaire**, ce sont deux sites indépendants, pas un fichier et sa copie.

Si `NkViewport3D.cpp` disparaît : **4 sites subsistent** (A, B, D, E). Les sites sans S6 passent de
2 à **1 — `AnimBridge.cpp`**, qui est justement l'**ancêtre** du motif (29/06, un mois avant que
`NkViewport3D.cpp` existe) et que le fichier supprimé citait comme modèle. Les 6 étapes omises par
tous restent omises par 4 sur 4.

> **Donc : la suppression retire une instance de la dérive, pas la dérive.** Et le motif se répand
> encore — deux des cinq sites datent d'après le correctif, dont un dans une **seconde application**
> (`NKXRDemo`), hors du modeleur.

⚠️ C'est ce flux qui rend `FlushGraphRebuilds()` nécessaire dans la façade : `BeginFrame` ne tournant
jamais, le drapeau de reconstruction restait armé pour rien et activer l'occlusion ambiante depuis un
panneau ne faisait **rien**.

---

## 3. Décompte des appelants

Périmètre : tout le dépôt hors `Build/` et `Externals/`. Candidats = fichiers incluant
`NKRenderer/NkRenderer.h` ou `DemoCommon.h` (qui porte `NkRenderer *renderer`, `DemoCommon.h:33`).
Paires comptées **adjacentes** (≤ 3 lignes d'écart), pour ne pas apparier un appel d'un bloc de
sortie anticipée avec un appel du corps principal.

| Flux | Fichiers | Sites |
|---|---|---|
| **A** — façade, ordre **correct** (`Present` → `EndFrame`) | **25** | **44** |
| **A** — façade, ordre **inversé** (`EndFrame` → `Present`) | **3** | **3** |
| **B** — coquille éditeur (`NkIEditorRenderer`, 2 points d'entrée) | 1 (`NK3DModeler/main.cpp`) | 1 boucle |
| **C** — mode partagé (aucun des trois appelé) | **5** *(corrigé le 27/08 ; 2 annoncés le 24/08)* | 5 |

### Les trois sites réellement fautifs

| Fichier | Lignes |
|---|---|
| `Applications/DemoRW/src/DemoRW/main.cpp` | 199-200 |
| `Applications/Sandbox/src/DemoNkentseu/Base05/NkRenderer2DDemo.cpp` | 223-224 |
| `Engine/Noge/src/Noge/Core/NkApplication.cpp` | 122-123 |

> **Verdict.** Le chiffre de « 15 appelants fautifs » est une **surestimation**. Sur la façade
> `NkRenderer`, il y a **3 sites inversés**, pas 15. Les 44 autres sites sont corrects, et les flux
> B et C sont **légitimes** : les corriger casserait ce qui marche.

Le sur-comptage vient probablement d'un appariement **non adjacent** : les démos Sandbox contiennent
un bloc de sortie anticipée (`Present(); EndFrame(); return;`) *puis*, plusieurs centaines de lignes
plus bas, la fin du corps principal (`Present(); EndFrame();`). Un appariement naïf lit le `EndFrame`
du premier bloc avec le `Present` du second et déclare une inversion qui n'existe pas. Le même
relevé sans contrainte d'adjacence produit **22** faux positifs sur ce dépôt.

---

## 4. Pourquoi l'inversion est un vrai défaut — et pourquoi elle passe inaperçue

L'effet dépend du backend, et c'est ce qui la rend traître.

### Vulkan — échec **total et silencieux**

`NkVulkanDevice::EndFrame` (`NkVulkanDevice.cpp:2397`) teste `mFrameAcquired && !mFrameSubmitted`.
Avec l'ordre inversé, `Present()` n'a pas encore tourné : `mFrameSubmitted` est **faux**. `EndFrame`
conclut donc que la frame a été **abandonnée** et :

1. soumet un batch vide pour consommer le sémaphore d'acquisition,
2. `vkQueueWaitIdle`,
3. **`RecreateSwapchain(...)` — à chaque frame**,
4. pose `mFrameAcquired = false` (ligne 2420).

Ensuite `Present()` tourne : il exécute tout le render graph, ferme le command buffer… et
`SubmitAndPresent` **retourne immédiatement** sur sa toute première ligne
(`NkVulkanDevice.cpp:2273` : `if (!mFrameAcquired || ...) return;`).

**Rien n'est jamais soumis. Rien n'est jamais présenté. Aucune erreur pilote n'est levée.** C'est
exactement le motif « image noire sans une seule erreur pilote ».

### OpenGL / DirectX 11 — marche par accident

`EndFrame` ne fait qu'avancer `mFrameIndex`/`mFrameNumber` (`NkOpenglDevice.cpp:2957`,
`NkDirectX11Device.cpp:1702`). `SubmitAndPresent` n'a **pas** de garde d'acquisition et ne dépend pas
de `mFrameIndex`. L'inversion est donc invisible sur ces backends — ce qui explique que les trois
applications fautives semblent fonctionner : sur Windows, elles retombent par défaut sur DX11.

### DirectX 12 — désynchronisation silencieuse

`NkDirectX12Device::SubmitAndPresent` (`NkDirectX12Device.cpp:2855`) ouvre sur
`auto &fd = mFrameData[mFrameIndex];`. Or `EndFrame` a **déjà avancé `mFrameIndex`**
(`NkDirectX12Device.cpp:2974`). La fence signalée et l'allocateur attendu appartiennent donc à un
**autre slot de frame** que celui qui a enregistré les commandes. Cela ne casse pas visiblement à
`framesInFlight` faible, mais c'est une désynchronisation réelle du recyclage d'allocateurs.

> **Conclusion.** L'ordre `Present` → `EndFrame` n'est pas une convention de style : sur Vulkan, il
> décide si quoi que ce soit s'affiche.

---

## 4bis. ⚠️ Ce que la MESURE a montré — et ce qu'elle a **démenti** (27/08/2026)

Le §4 ci-dessus est établi par **lecture** des backends. J'ai ensuite tenté de l'**observer**. Le
résultat corrige une affirmation trop forte de la première version de cette page.

### Protocole

`Applications/Sandbox/.../NkRenderer2DDemo.cpp` (l'un des 3 sites inversés), cible jenga `r2d01`,
compilée en Debug, lancée depuis la racine du dépôt (indispensable : sinon
`Resources/NKRenderer/Shaders` est invisible et **tous** les shaders manquent). Capture des pixels
réels de l'écran (`CopyFromScreen`, pas `PrintWindow` — qui rend noir sur une fenêtre composée par le
GPU et aurait fabriqué un faux positif).

### Résultat 1 — 🔴 Le banc ne peut pas tourner sous Vulkan **ni** sous DX12 sur `main`

| Backend | `NkRenderer::Initialize()` | Cause |
|---|---|---|
| **OpenGL** | ✅ réussit, 0 shader en échec | — |
| **Vulkan** | ❌ **échoue** | **18 programmes** ne compilent pas : `'non-opaque uniforms outside a block' : not allowed when using GLSL for Vulkan`. Pas seulement le post-process : `Render2D`, `Shadow`, `Skybox`, `Instanced`, `Blit`, `Glow2D`, `InfiniteGrid`… Les fichiers sont **trouvés** (0 « INTROUVABLE ») : ce sont de vraies erreurs de compilation |
| **DX12** | ❌ **échoue** | **11 programmes** en échec, même famille |

Cause **préexistante sur `main`**, sans rapport avec l'ordre de frame : la seule chose que j'avais
modifiée pour ce test était l'ordre de préférence d'API, réverté depuis.

> 🔴 **Conséquence sur la lecture du §4 : le défaut d'ordre est LATENT, pas actif.** Écrire que les
> trois sites « donnent un écran noir sous Vulkan aujourd'hui » serait faux : **personne ne peut
> lancer NKRenderer sous Vulkan sur `main`**, l'initialisation échoue avant. Le mécanisme du §4
> reste exact — il se déclenchera **le jour où les shaders Vulkan seront réparés**, et ce jour-là il
> se manifestera par un écran noir sans erreur pilote, c'est-à-dire au pire moment.

### Résultat 2 — le correctif ne régresse pas le seul backend qui tourne

| | Backend | pixels non-noirs | luminance moyenne | rendu |
|---|---|---|---|---|
| **Avant** (`EndFrame` → `Present`) | OpenGL | 18099 / 18483 (**97,9 %**) | 74,9 | correct |
| **Après** (`Present` → `EndFrame`) | OpenGL | 18099 / 18483 (**97,9 %**) | 76,1 | correct |

Identique — conforme au §4 : sous OpenGL, `EndFrame` ne fait qu'avancer un compteur, l'inversion est
invisible. (Le delta de luminance est la balle qui rebondit, à une position différente.)

> ⚠️ **À dire clairement : le passage écran noir → image n'a PAS été observé.** Il est *déduit* de la
> lecture de `NkVulkanDevice.cpp:2273/2397/2420`, chaîne courte et sans branche cachée, mais
> **déduit**. Ce qui est **observé**, c'est (a) que Vulkan et DX12 ne démarrent pas sur `main`, et
> (b) que le correctif ne casse rien sous OpenGL. L'observation complète demande d'abord la
> réparation des shaders Vulkan — **hors périmètre de ce chantier** (un autre chantier tient les
> shaders).

### Défaut annexe relevé au passage (non corrigé)

`NkDeviceFactory::CreateWithFallback` (`Core/NkDeviceFactory.cpp:136-146`) **ignore complètement
`init.api`** : il itère la liste `order` et écrase `effectiveInit.api = api` à chaque tour. Donc
`NkRenderer2DDemo` analyse consciencieusement `--backend=vulkan` (`ParseBackend`, `:34-48`), le pose
dans `devInfo.api` (`:68`) — et le jette. Sur Windows la liste commence par OpenGL : **l'option
`--backend=` de cette démo n'a aucun effet**, en silence. C'est ce qui m'a d'abord fait croire que je
mesurais Vulkan alors que je mesurais OpenGL.

---

## 5. Défauts relevés (non corrigés)

| Défaut | Emplacement | Nature |
|---|---|---|
| Ordre `EndFrame`/`Present` inversé | `NkApplication.cpp:122-123`, `DemoRW/main.cpp:199-200`, `NkRenderer2DDemo.cpp:223-224` | Rendu nul sous Vulkan |
| Commentaires inversés | `NkApplication.cpp:122-123` — « `EndFrame(); // soumet le render graph` » et « `Present(); // présente la swapchain` » | C'est `Present` qui soumet le graphe ; le commentaire enseigne l'erreur |
| `USAGE.md` décrit une **API morte** | `Kernel/Runtime/NKRenderer/src/USAGE.md:47-55, 143-148` | `renderer.Init(device, cfg)`, `renderer.BeginFrame(cmd)`, `renderer.RenderFrame(cmd, ...)`, `renderer.Render2D(cmd, lambda)`, `device->BeginFrame()` renvoyant un command buffer : **aucun n'existe**. `NkIDevice::BeginFrame(NkFrameContext&)` renvoie un `bool`. Document à réécrire ou à retirer. |
| `mStats.gpuTimeMs` jamais alimenté | `NkRendererImpl.cpp:1496` | Champ exposé, laissé à zéro (assumé dans le code) |

---

## 6. Comment se relit ce contrat

Si vous devez re-vérifier ce que fait un point d'entrée, **ne cherchez pas le nom** : dans ce dépôt,
`BeginFrame` est porté par au moins six classes sans lien (`NkRenderer`, `NkIDevice`,
`NkIEditorRenderer`, `NkUIContext`/`NkUIInput`, `NkIGraphicsContext`, `NkXrSession`). Un `grep
BeginFrame` remonte **88 sites** dont une minorité concerne la façade. Partez de l'**include**
(`NKRenderer/NkRenderer.h`) ou du **type déclaré**, puis lisez le corps.

[← Le cœur du renderer](Core.md) · [← Doc NKRenderer](README.md) · [← Couche Runtime](../README.md)
