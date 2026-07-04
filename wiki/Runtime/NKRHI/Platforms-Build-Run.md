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

[← Récap NKRHI](../NKRHI.md) · [Device.md](Device.md) · [← Couche Runtime](../README.md)
