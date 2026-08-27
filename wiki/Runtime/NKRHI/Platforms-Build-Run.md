# NKRHI — Build & exécution par plateforme (backends GL / Vulkan / Metal / DX)

Cette page couvre **comment compiler et lancer** un projet qui utilise le RHI sur chaque
plateforme, quels backends sont disponibles, et les **pré-requis / pièges** propres à
chacune. Le choix du backend se fait à l'exécution (ex. `renderdemo -bvk`) ou via la config
device (`NkContextDesc::api`).

## Vue d'ensemble

| Plateforme | Backends compilés | Backend recommandé | Pré-requis matériel/SDK |
|------------|-------------------|--------------------|--------------------------|
| **Windows** | OpenGL (WGL), DX11, DX12, Vulkan*, Software | DX12 / Vulkan | — (Vulkan : SDK LunarG ou `vulkan-1`) |
| **Linux (dont WSL2)** | OpenGL (GLX), Vulkan*, Software | OpenGL (GLX) | `libvulkan-dev` + `mesa-vulkan-drivers` pour VK |
| **macOS** | Metal, OpenGL (≤4.1, **inutilisable**), Vulkan* (MoltenVK) | **Metal** | Mac + Xcode ; Vulkan = SDK LunarG macOS (MoltenVK) |
| **iOS** | Metal, OpenGL ES, Vulkan* (MoltenVK) | **Metal** | Mac + Xcode + device/simulateur |
| **Android** | OpenGL ES, Vulkan* | Vulkan / GLES | NDK |
| **Web** | WebGL (OpenGL ES) | WebGL | Emscripten |

\* Vulkan est **opt-in / auto-détecté** : `WANT_VULKAN` (cf. [config/graphics.jenga](../../../config/graphics.jenga))
vaut `True` si `VULKAN_SDK` est défini **ou** si `pkg-config --exists vulkan` réussit. Forçable via
`NK_ENABLE_VULKAN=1|0`.

---

## Windows

Aucun pré-requis pour OpenGL/DX11/DX12/Software (toolchain `clang-mingw` ucrt64). Le contexte GL
est créé par le RHI en **WGL** (Core 4.6 + Debug), DX11/DX12 nativement.

- **Vulkan** : définir `VULKAN_SDK` (SDK LunarG) → `WANT_VULKAN=True`, link `vulkan-1`.
- **Piège DLL** : au lancement, un crash `0xC0000139` *avant* `main` = `libstdc++-6.dll` mingw64 du
  PATH masque ucrt64 → copier `libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll` (ucrt64)
  à côté de l'exe.

```
jenga build --target renderdemo
Build\Bin\Debug-Windows\renderdemo\renderdemo.exe --demo=2          # OpenGL
Build\Bin\Debug-Windows\renderdemo\renderdemo.exe --demo=2 -bdx12   # DX12
```

---

## Linux (et WSL2)

Le RHI crée son contexte OpenGL en **GLX** (fenêtrage XLIB par défaut). Fonctionne sur Linux natif
comme sous **WSL2/WSLg** (rendu matériel via le driver D3D12/Dozen adossé au GPU Windows).

### OpenGL (prêt, aucun paquet supplémentaire)

```
jenga build --target renderdemo
./Build/Bin/Debug-Linux/renderdemo/renderdemo --demo=2
```

**Piège WSLg — GL 4.2 vs 4.3 requis (géré automatiquement)** : WSLg annonce **GL 4.2 par défaut**
alors que son driver D3D12 supporte 4.6, et le moteur exige **4.3** (compute shaders). Le RHI pose
donc lui-même `MESA_GL_VERSION_OVERRIDE=4.6` (+ `MESA_GLSL_VERSION_OVERRIDE=460`) au démarrage, sans
écraser une valeur déjà exportée par l'utilisateur. Résultat au run :
`[NkRHI_GL] Initialized (GL 4.6, D3D12 (NVIDIA GeForce RTX 3070 Laptop GPU))`. Sur un Linux natif
avec de vrais drivers GPU, GL 4.3+ est natif et l'override est sans effet.

> Autres pièges GLX (déjà gérés dans le RHI) : il faut `gladLoaderLoadGLX(dpy, screen)` **avant**
> tout appel `glXxxx` (sinon segfault), et un **handler d'erreur X** temporaire autour de
> `glXCreateContextAttribsARB` (sinon `GLXBadFBConfig` tue le process avant le fallback de version).

### Vulkan (paquets à installer)

Le backend Vulkan n'est compilé que si Vulkan est détecté. Sur Ubuntu/WSL2 (jammy) :

```bash
sudo apt update
sudo apt install -y libvulkan-dev mesa-vulkan-drivers vulkan-tools
```

| Paquet | Rôle |
|--------|------|
| `libvulkan-dev` | Loader `libvulkan.so` + headers + `vulkan.pc` → **active `WANT_VULKAN`** (`pkg-config --exists vulkan`) et fournit la lib à linker |
| `mesa-vulkan-drivers` | Drivers ICD : **Dozen (dzn)** = Vulkan→D3D12→GPU (WSLg), + **lavapipe** (software) |
| `vulkan-tools` | `vulkaninfo` / `vkcube` pour vérifier |

Vérifier puis builder :

```bash
vulkaninfo --summary       # liste les devices Vulkan (dzn/RTX si dispo, sinon lavapipe)
jenga build --target renderdemo
./Build/Bin/Debug-Linux/renderdemo/renderdemo -bvk --demo=1
```

Alternative : le **SDK LunarG Linux** (https://vulkan.lunarg.com/sdk/home#linux) définit `VULKAN_SDK`
(via `source setup-env.sh`) → `WANT_VULKAN=True` par ce chemin aussi, et fournit les validation layers.

**Corrections build/link Vulkan Linux (déjà en place)** :
- `VK_USE_PLATFORM_XLIB_KHR` / `_XCB_KHR` / `_WAYLAND_KHR` définis **avant** `<vulkan.h>` (jenga NKRHI
  + [NkDeviceInitInfo.h](../../../Kernel/Runtime/NKRHI/src/NKRHI/Core/NkDeviceInitInfo.h)) → sinon
  `VkXlibSurfaceCreateInfoKHR` / `vkCreateXlibSurfaceKHR` « undeclared ».
- `-lvulkan` ajouté sur les 3 filtres Linux (xlib/xcb/wayland) de [RendererSandbox.jenga](../../../Applications/Sandbox/RendererSandbox.jenga)
  → sinon `undefined reference to vkDestroyBuffer…` au link final.
- **glslang** : ne PAS linker le `glslang` **système** (`GLSLANG_LIBS_UNIX`) en plus de **NKGLSlang**
  (le projet build le sien) → link fail / doublons. Le jenga ne lie le système que si `USE_NKGLSLANG=False`.

### État actuel & deux options d'exécution

Le backend Vulkan **compile, linke, s'initialise ET rend** sur Linux (device, swapchain, extensions
`VK_KHR_{xlib,xcb,wayland}_surface`, pipelines, demos 2D+3D, exit propre). Sur **WSL2 sans GPU Vulkan
exposé**, le seul device est **lavapipe (software)** — lent mais fonctionnel, sans risque thermique GPU.

> ✅ **Bug SPIR-V corrigé (2026-07-02)** : `NkSLCompiler::CompileToSPIRV` testait `NKSL_HAS_GLSLANG` /
> `NKSL_HAS_SHADERC` — macros **jamais définies** → il renvoyait le **texte GLSL** comme "SPIR-V"
> (`Invalid SPIR-V magic` → `vkCreateGraphicsPipelines`=`VK_ERROR_UNKNOWN`). Corrigé en déléguant à
> `NkGLSLToSPIRV` (vraie impl glslang guardée par `NK_RHI_GLSLANG_ENABLED`). Les erreurs résiduelles
> `ParticlesBillboard`/`TrailMesh`/`Decal` (`shader id=0`) sont **pré-existantes** (shaders VFX non
> déployés, identiques sur Windows), non fatales.

**Option A — Vulkan software (lavapipe) + validation layers** *(pour debugger sans GPU)*

```bash
sudo apt install -y vulkan-validationlayers     # fournit VK_LAYER_KHRONOS_validation
# NK_VK_VALIDATION=1 : opt-in câblé dans le moteur (validation -> NkLog). Validation OFF par défaut
# (une couche buggée peut crasher au boot, cf. msvcp140 Huawei sur Windows).
NK_VK_VALIDATION=1 NK_MAXFRAMES=8 ./Build/Bin/Debug-Linux/renderdemo/renderdemo -bvk --demo=1
```
Rendu **software** (CPU, lent) mais **aucun risque thermique GPU**. Sert à valider le pipeline VK et
lire les erreurs de validation exactes.

**Option B — Vulkan matériel (GPU réel) sur WSL2**

Le driver software ne touche pas le GPU. Pour du Vulkan **hardware** sur WSLg il faut un ICD GPU :
- **Dozen (dzn)** = Vulkan→D3D12→GPU : nécessite un **mesa récent** (le `mesa-vulkan-drivers` de jammy
  23.2 ne l'expose pas ou de façon incomplète). Via PPA : `sudo add-apt-repository ppa:kisak/kisak-mesa`
  puis `sudo apt upgrade`. Vérifier avec `vulkaninfo --summary` (device `dzn`/`Microsoft Direct3D12`).
- **ICD Vulkan NVIDIA WSL** : fourni par le driver Windows NVIDIA récent (expose un ICD sous
  `/usr/lib/wsl/lib/`). Si présent, `vulkaninfo` liste la carte NVIDIA directement.

Sur un **Linux natif** avec drivers GPU (Mesa radeonsi/anv, ou NVIDIA propriétaire), le Vulkan matériel
est natif — aucune de ces manips WSL n'est nécessaire.

> **Sécurité matérielle** : en Vulkan **hardware** sur scène très éclairée, le GPU peut monter à 100 % en
> continu (extinction possible sur certains portables). Deux garde-fous **configurables** sont actifs :
> cap FPS (`NK_FPS_CAP`, défaut 120, `0` = illimité) et clamp HDR (`NK_HDR_CLAMP`, défaut 64, `0` =
> désactivé). En Vulkan **software** (lavapipe) le risque est CPU, pas GPU.

---

## macOS

> ⚠️ **Nécessite un Mac + Xcode.** Impossible à compiler depuis Windows/Linux. Section documentée
> depuis le code + specs Apple (non testée dans cet environnement).

Backend Metal réel : `src/NKRHI/Metal/NkMetalDevice.mm` (compilé sur `system:macOS || system:iOS`).
Frameworks liés : `Cocoa`, `QuartzCore`, `OpenGL`, `Metal`. La surface porte `NSView*` + `CAMetalLayer`.

- **Metal (recommandé)** : chemin natif Apple, aucun SDK tiers. À privilégier.
- **OpenGL — INUTILISABLE pour le moteur** : Apple plafonne OpenGL à **4.1** (et l'a déprécié), or le
  RHI exige **GL 4.3** (compute). Le backend GL ne satisfait donc pas le moteur sur macOS → utiliser
  Metal ou Vulkan/MoltenVK.
- **Vulkan (MoltenVK)** : installer le **SDK LunarG macOS** (https://vulkan.lunarg.com/sdk/home#mac)
  → définit `VULKAN_SDK`, fournit **MoltenVK** (Vulkan-sur-Metal) + les libs glslang. `WANT_VULKAN`
  passe à `True`, link depuis `$VULKAN_SDK/lib`.

```
jenga build --target renderdemo --platform macos
./Build/Bin/Debug-macOS/renderdemo/renderdemo            # Metal
./Build/Bin/Debug-macOS/renderdemo/renderdemo -bvk       # Vulkan via MoltenVK (si SDK installé)
```

---

## iOS

> ⚠️ **Nécessite un Mac + Xcode + un device (ou simulateur).** Section documentée depuis le code +
> specs Apple (non testée dans cet environnement).

Même backend Metal que macOS (`NkMetalDevice.mm`). Frameworks : `UIKit`, `QuartzCore`, `OpenGLES`,
`Metal`. La surface porte `UIView*` + `CAMetalLayer`. Jeux verrouillés en **paysage** par convention.

- **Metal (recommandé)** : seul chemin GPU pleinement supporté sur iOS.
- **OpenGL ES** : présent (`OpenGLES`), mais Apple l'a déprécié ; le RHI accepte **ES 3.0+** sur ce
  chemin (`NK_OPENGL_ES`). Metal reste préférable.
- **Vulkan (MoltenVK)** : via le SDK LunarG iOS (MoltenVK ciblant iOS). Peu courant ; Metal direct
  est le choix par défaut.

Le build passe par la toolchain Xcode ; signature/déploiement device gérés côté Xcode
(cf. pipeline d'app iOS).

---

## Résumé « quel backend lancer »

- **Windows** : `-bdx12` ou `-bvk` (perf), OpenGL par défaut.
- **Linux / WSL2** : OpenGL par défaut (marche direct) ; `-bvk` après `apt install libvulkan-dev
  mesa-vulkan-drivers`.
- **macOS / iOS** : **Metal** (défaut) ; OpenGL inutilisable sur macOS (cap 4.1) ; Vulkan seulement
  si MoltenVK (SDK LunarG).

## ⚠️ Cinq pièges d'instrument — à lire AVANT de mesurer un backend

Ces cinq pièges ont été rencontrés le **27/08/2026** en tentant d'observer un défaut sous Vulkan.
Chacun produit une mesure **fausse mais crédible** — c'est ce qui les rend coûteux. Ils ne
concernent pas un module en particulier : ils concernent **quiconque lance un binaire du dépôt pour
en tirer une conclusion**.

> 📌 Les quatre ont réellement produit une conclusion fausse ce jour-là, dont une qui a failli faire
> réécrire dix-huit shaders sains. Aucun n'est théorique.

### 0. 🔴 « J'ai lancé `r2d01` » ne dit pas **quel exécutable** — et deux personnes peuvent en parler sans parler du même

Le 27/08, deux mesures de `r2d01` sous Vulkan se sont contredites frontalement :

| | Backend obtenu | Conclusion tirée |
|---|---|---|
| Chantier A | `Vulkan`, 12–27 lignes `NkRHI_VK`, 0 `NkRHI_GL` | « Vulkan démarre puis échoue au 2ᵉ lancement » |
| Chantier B | `OpenGL`, **0** occurrence de `Vulkan` / `vkCreate` | « `r2d01` ne PEUT PAS tourner en Vulkan ; ton Vulkan n'a jamais été Vulkan » |

**Les deux journaux disent vrai.** L'un des binaires portait un **correctif local d'une ligne** — le
patron sûr, l'API demandée mise en tête de la liste de repli — l'autre non. Sans ce correctif,
`r2d01` sur Windows part sur la liste en dur qui commence par OpenGL (§ 2) et **ne peut pas**
atteindre Vulkan. Avec, il l'atteint.

> **Le nom d'une cible identifie une source, pas un exécutable.** Un binaire porte l'état de
> l'arbre à l'instant du `build` : correctifs locaux, branche, patch d'expérience réverté depuis.
> Deux personnes qui disent « j'ai lancé `r2d01` » peuvent lancer deux programmes différents.

**Règle** : quand deux mesures se contredisent, la première question n'est pas « qui a raison » mais
**« as-tu un correctif local dans l'arbre au moment du build ? »**. Le second réflexe est de
comparer les **journaux du device**, pas les conclusions. Ici, `NkRHI_VK` **contre** `NkRHI_GL` a
tranché en une ligne — les deux camps avaient la preuve dans leur propre journal, sans la comparer.

⚠️ **Et une leçon plus dure** : le chantier A avait **écrit lui-même** l'avertissement du § 2 — que
`--backend=` est ignoré — et l'appliquait correctement. Ça ne l'a pas empêché d'être contredit sur
la base d'un binaire différent du sien. **Connaître un piège ne protège pas de lui ; seul un
instrument qui lit ce qu'il a obtenu le fait**, et il faut que *les deux* camps le lisent.

### 1. 🔴 Le **cache de shaders** rend la 1re exécution différente de toutes les suivantes

**Le piège le plus coûteux des quatre**, et celui qui m'a fait accuser dix-huit shaders innocents.

Le cache vit **à côté de l'exécutable** : `<dossier de l'exe>/cache/shaders/*.nksc`. Il survit aux
recompilations du binaire — **`jenga build` ne le purge pas**.

Deux façons symétriques de se tromper, et il faut connaître les deux :

| Protocole | Ce qu'on croit mesurer | Ce qu'on mesure |
|---|---|---|
| Purger le cache, lancer **une** fois | « le backend marche » | **le seul cas qui marche**, si le défaut est dans l'écriture du cache |
| Lancer sans purger, après un autre backend | « ce backend est cassé » | **les restes de l'exécution précédente** |

Mesuré le 27/08 sur `main`, banc `r2d01` sous Vulkan : **cache froid → 0 échec ; 2ᵉ lancement → 16
échecs.** Deux chantiers avaient chacun une moitié du tableau et des conclusions opposées, chacune
juste dans son protocole.

**Règle** : pour juger un backend, **purger le cache ET lancer deux fois**. La 1re exécution teste
la génération, la 2ᵉ teste le **cycle écriture → relecture** du cache — ce sont deux choses
différentes, et un défaut peut vivre dans la seconde seule.

**Test le moins cher du dépôt** — ce que le cache contient réellement :

```bash
head -c 20 <exe_dir>/cache/shaders/*.nksc | xxd
#   23766572 = "#ver"  -> du TEXTE GLSL
#   03022307           -> du SPIR-V
```

Sous un device Vulkan, une entrée qui commence par `#ver` est **une anomalie** : le backend attend
du SPIR-V. (Sous OpenGL, du texte est normal.)

### 2. 🔴 `--backend=` peut être **sans effet, en silence**

`NkDeviceFactory::CreateWithFallback(init, order)` (`Core/NkDeviceFactory.cpp:136-146`) **ignore
complètement `init.api`** : il parcourt `order` et écrase `effectiveInit.api = api` à chaque tour.

Conséquence, mesurée sur `Applications/Sandbox/.../Base05/NkRenderer2DDemo.cpp` : la démo analyse
consciencieusement `--backend=vulkan` (`ParseBackend`, `:34-48`), le pose dans `devInfo.api` (`:68`)
— **et le jette** (`:75-80`). Sur Windows la liste commence par OpenGL : **j'ai cru mesurer Vulkan
pendant deux exécutions alors que je mesurais OpenGL.**

**Comment ne pas se faire avoir** : le seul juge est la **ligne de journal du backend obtenu**, pas
l'option passée —

```
[2DDemo] Backend: OpenGL          <- l'option disait vulkan
```

**Toujours lire cette ligne avant d'interpréter un résultat.** `NkDeviceFactory` en émet une
équivalente : `[NkDeviceFactory] Device RHI cree: <API>`.

⚠️ **Quatre fonctions, quatre contrats différents — et deux portent le MÊME nom :**

| Fonction | Honore `init.api` ? | Appelants réels |
|---|---|---|
| `NkDeviceFactory::CreateForApi(api, init)` | **oui** | `renderdemo` (`Demo/main.cpp:569`) : `-bvk` **fonctionne** |
| `NkDeviceFactory::Create(init)` | **oui** — lit `NkDeviceInitApi(init)` (`NkDeviceFactory.cpp:41`), refuse si `NONE` | — |
| `NkDeviceFactory::CreateWithFallback(init, order)` | 🔴 **non** — `order` gagne toujours | **9** |
| `NkContextFactory::CreateWithFallback(...)` | *(autre fonction, NKCanvas)* | **5** |

`NkEditorRHIRenderer.h:50-56` documente déjà ce risque et s'en protège en filtrant *avant* l'appel
(`NkEditorGfxApiSupported`) — c'est le bon patron, il n'est simplement pas généralisé.

#### 🔴 Ce piège a lui-même été « corrigé » à tort — deux fois. Voici la mesure qui tranche.

Cet avertissement a été contesté le 27/08 : *« `NkDeviceFactory::CreateWithFallback` a **zéro
appelant** ; les occurrences sont celles de `NkContextFactory::CreateWithFallback`, une autre
fonction ; et `NkDeviceFactory::Create` **lit** bien `init.api` »*. **Les deux dernières affirmations
sont vraies, la première est fausse**, et l'ensemble mène à une conclusion inverse de la réalité.

Ce qui est mesuré sur `main` :

```
grep "NkDeviceFactory::CreateWithFallback"  (nom QUALIFIE, hors Build/ Externals/,
                                             hors sa propre definition, hors commentaires)
  -> 9 appelants, dont NkRenderer2DDemo.cpp:75 et :79, et NkEditorRHIRenderer.h:98

grep "NkContextFactory::CreateWithFallback"  -> 5 appelants  (fonction DIFFERENTE)

grep "::CreateWithFallback(const" dans Kernel/  -> DEUX fabriques distinctes :
  NkContextFactory  et  NkDeviceFactory
```

Et le corps, inchangé :

```cpp
for (auto api : order) {
    if (!IsApiSupported(api)) continue;
    NkDeviceInitInfo effectiveInit = init;
    effectiveInit.api = api;        // <- init.api ecrase, a chaque tour
```

**Preuve expérimentale, plus forte que la lecture** : à argument identique (`--backend=vulkan`),
**en ne changeant que l'ordre de la liste**, le backend obtenu passe de `OpenGL` à `Vulkan`. Si
`init.api` était honoré, réordonner la liste n'aurait rien pu changer.

> 📌 **La cause de l'erreur est le piège que cette page enseigne ailleurs** : `Create` et
> `CreateWithFallback` sont deux fonctions du **même** `NkDeviceFactory` aux contrats **opposés**,
> et `CreateWithFallback` est en plus **homonyme** d'une fonction de `NKCanvas`. Lire l'une et
> conclure sur l'autre est l'erreur naturelle. **Le nom qualifié n'est pas un luxe :
> `grep CreateWithFallback` non qualifié mélange deux fabriques.**

#### ✅ Le corollaire qui vaut plus que le piège : **un banc doit LIRE sa configuration, pas l'annoncer**

En allant vérifier cet avertissement, on a découvert que des bancs **annonçaient le backend sans le
lire** : ils affichaient `NkGraphicsApiName(NK_GFX_API_VULKAN)` — c'est-à-dire ce qu'ils avaient
**demandé**. Leur affichage était vrai **par accident**, et serait resté vrai en toutes
circonstances, y compris en mesurant autre chose.

> **Un banc qui annonce sa configuration au lieu de la lire est un banc qui témoigne de son
> intention.** Il ne peut pas, par construction, révéler l'écart entre ce qu'on a demandé et ce
> qu'on a obtenu — c'est-à-dire exactement le défaut que cette section décrit.

**Règle** : afficher `dev->GetApi()` (ce que le device **est**), jamais l'API demandée, et **refuser
de mesurer** si les deux divergent. C'est ce que ces bancs font désormais.

### 3. Lancer hors de la racine du dépôt fait manquer **tous** les shaders

Les shaders sont cherchés en **(1)** `Resources/NKRenderer/Shaders/…` *relatif au répertoire
courant*, puis **(2)** `<dossier de l'exécutable>/Resources/NKRenderer/Shaders/…`. Lancer un binaire
depuis `Build/Bin/…` sans déploiement fait échouer les deux :

```
[NkShaderLibrary] 'PP_FXAA' INTROUVABLE -- ce n'est PAS un shader invalide,
c'est un FICHIER ABSENT, et aucune source embarquee ne le remplace.
```

Le message est excellent — **encore faut-il le lire** : sans lui, l'échec ressort en aval comme un
shader cassé. **Lancez depuis la racine du dépôt** (`-WorkingDirectory`), ou déployez `Resources/`
à côté du binaire.

⚠️ **Et le cas mixte est silencieux.** Le journal choisit sa branche sur
`if (overrideVS || overrideFS)` → `Trace` : il suffit qu'**un seul** des deux fichiers existe pour
que l'avertissement « repli sur la source EMBARQUEE » **ne soit pas émis**, alors qu'un des deux
étages vient bien d'une source embarquée écrite pour un autre backend.

### 4. `PrintWindow` rend **noir** sur une fenêtre composée par le GPU

Pour photographier une fenêtre GL/Vulkan/DX sous Windows, `PrintWindow` (et tout ce qui bâtit sur
un DC de fenêtre) rend une image **noire ou vide** : le contenu vit dans une chaîne d'échange que
le compositeur ne restitue pas par ce chemin.

> 🔴 **C'est le pire des trois** : il fabrique un **faux positif** parfaitement crédible quand on
> cherche justement un écran noir. J'aurais « observé » le défaut que je cherchais — sur un
> instrument cassé.

**Utiliser une capture des pixels réels de l'écran** — `Graphics.CopyFromScreen` sur le
`GetWindowRect` de la fenêtre, après l'avoir mise au premier plan. Contrôle de bon sens : une
capture qui rend **100 % de pixels non-noirs à luminance ~765/765** est une fenêtre blanche qui en
recouvre une autre, pas un rendu — recommencez.

---

[← Récap NKRHI](../NKRHI.md) · [Device.md](Device.md) · [← Couche Runtime](../README.md)
