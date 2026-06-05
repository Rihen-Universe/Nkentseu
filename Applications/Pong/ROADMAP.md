# Pong Ultra Arena — Roadmap

> Jeu vitrine du moteur **Nkentseu** (catégorie **Noge** — id paquet
> `com.rihen.nkentseu.noge.pong`). GDD de référence :
> [docs/GDD_PONG_ULTRA_ARENA_v1.1.docx](docs/GDD_PONG_ULTRA_ARENA_v1.1.docx)
> (+ maquettes [docs/01_splash_et_menus.html](docs/01_splash_et_menus.html),
> [docs/02_terrain_de_jeu_1v1.html](docs/02_terrain_de_jeu_1v1.html),
> [docs/03_assets_library.html](docs/03_assets_library.html)).
> Dernière mise à jour : 2026-05-31.

## Où situer Pong dans le moteur

- **Pong** est un **jeu 2D** : il se rend via **NKCanvas** (couche graphique
  SFML-like) + **NKUI** (l'ImGui maison, immediate-mode) pour les menus/HUD.
- À ne pas confondre avec le chemin **moteur/éditeur** : **Noge** (moteur de jeu
  2D/3D) et **Nogee** (l'éditeur de Noge, qui *utilise* Noge) se rendent eux via
  **NKRenderer** (→ NKRHI), **pas** NKCanvas. NKUI est aussi consommé par Nogee,
  mais **à travers NKRenderer**. Bref : deux chemins de rendu (2D NKCanvas / 3D
  NKRenderer) ; Pong est sur le chemin 2D.

## Contexte : régression assumée — et où vit le « vrai » jeu

**Pong Ultra Arena fonctionnait sur Windows ET Android** (jeu complet : splash,
menus, 1v1, IA, power-ups, obstacles, particules, multijoueur LAN, audio) **avant**
le renommage NKContext→NKCanvas (2026-05-28) et la refonte SFML-like (2026-05-30).
Il s'appuyait sur son **propre stack rendu maison** en OpenGL brut via glad
(`Render/` : `GLContext` + `GLRenderer2D` + `Texture2D` + `FontAtlas` + `SafeArea`),
parce que `NkTexture` de NKCanvas était cassé (`NkTextureSetBackend` jamais câblé —
corrigé depuis, A.7).

> ⚠️ **Le vrai `Apps.cpp` (jeu complet, baseline qui marchait Win+Android) est dans
> [`Applications/Pong2/`](../Pong2/)**, pas ici. **`Pong2` est la référence de
> travail** à conserver (jeu fonctionnel sur l'ancien stack OpenGL maison).
>
> Le présent dossier **`Pong/`** est le **chantier de migration** vers NKCanvas.
> Son [src/Pong/Apps.cpp](src/Pong/Apps.cpp) a été **volontairement simplifié** en
> petite démo (paddles/balle/scores en formes) **dans le seul but de tester le
> rendu cross-backend de NKCanvas de façon minimale** — d'où le strip de l'IA, des
> scènes, des fontes, du réseau. Les autres fichiers de `Pong/` (`Game/`, `Net/`,
> `Audio/`, `UI/Scenes/`, `Render/`) sont des copies gardées **hors-build**
> ([Pong.jenga:61](Pong.jenga#L61) : `files(["src/Pong/Apps.cpp"])`).

La migration consiste donc à **réamener le jeu complet de `Pong2` sur NKCanvas**
dans `Pong/`, progressivement. C'est le cœur de cette roadmap.

---

## Synthèse

| Élément | Statut | Effort | Priorité |
|---|---|---|---|
| Démo NKCanvas SFML-like (paddles/balle/scores, formes) | ✅ | — | — |
| Sélection backend dynamique (`pong.config`) + fallback chain | ✅ | — | — |
| Event pump + input clavier (W/S, ↑/↓, ESC) | ✅ | — | — |
| Resize + DPI change (`OnResize`/`OnDpiChange`) | ✅ | — | — |
| Rendu **OpenGL** runtime | ✅ | — | — |
| Rendu **Software** runtime | ❌ | M | P1 |
| Rendu **DX11** runtime | ❌ | M | P1 |
| Rendu **DX12** runtime | ❌ | M | P1 |
| Rendu **Vulkan** runtime | ❌ | M | P1 |
| Pong jouable OpenGL **Windows + Android** (jalon « remarche ») | ⏳ | M | P1 |
| Intégration **NKUI** (menus/HUD via NkRenderTarget) | ⏳ | L | P1 |
| Textures/fontes via NKCanvas (`NkTexture`/`NkFont`/`NkSprite`/`NkText`) | ⏳ | M | P1 |
| Re-migration Ultra Arena (27 `.cpp` : Game/Net/Audio/Scenes) | ⏳ | XL | P2 |
| Multijoueur LAN (NetworkDiscovery/Session, UDP 7778) | 🔶 | L | P2 |
| Audio (AudioManager → NKAudio) | 🔶 | M | P2 |
| Parité multi-plateforme (macOS/iOS/HarmonyOS/Web) + packaging | ⏳ | L | P3 |
| Nettoyage (`Apps copy.cpp` parasite, ancien `Render/` legacy) | ⏳ | S | P3 |

Légende : ✅ livré · 🔶 en cours · ⏳ à venir · ❌ cassé/non fonctionnel · 🚫 abandonné.
Effort : S (≤1j) · M (2-5j) · L (1-2 sem) · XL (>2 sem).

---

## Livré

### Démo NKCanvas SFML-like — **banc d'essai backend** ([src/Pong/Apps.cpp](src/Pong/Apps.cpp))
> Volontairement minimal : sert à **valider le rendu cross-backend** de NKCanvas,
> pas à jouer. Le jeu complet de référence est dans [`Pong2/`](../Pong2/).
- `nkmain(NkEntryState)` ouvre `NkWindow` 1280×720, crée un `NkRenderWindow`
  (NKCanvas) selon le backend choisi.
- Scène animée : 2 `NkRectangleShape` (paddles) + `NkCircleShape` (balle) +
  ligne médiane pointillée + scores en **7-segments** dessinés via
  `NkRenderer2D::DrawFilledRect`. Auto-démo (IA « suit la balle ») quand aucune
  touche n'est pressée.
- **Input clavier** : W/S (paddle gauche), ↑/↓ (paddle droit), ESC (quitter),
  via `NkEvents().PollEvent()` + `ev->Is<T>()`/`ev->As<T>()`.
- **Event pump** correct → plus de freeze « Not Responding ».
- **Resize/DPI** : `target->OnResize()` / `OnDpiChange()` sur events, avec garde
  anti-resize-redondant au démarrage (tracking `currentSize`).
- Logger : sink fichier `logs/app.log` (Pong est SUBSYSTEM:WINDOWS, pas de stdout
  attaché depuis un terminal non-console).

### Sélection de backend dynamique ([src/Pong/PongConfig.h](src/Pong/PongConfig.h))
- Fichier `pong.config` texte (`backend=opengl|vulkan|dx11|dx12|software|auto`),
  lu/créé à l'ouverture via `NkFile` (portable Android AAsset / Web FS / Harmony).
- `PickBestForPlatform()` + `PlatformFallbackChain()` : Windows DX12→DX11→Vulkan→
  OpenGL→Software ; Linux/Android/Harmony Vulkan→OpenGL→Software ; macOS/iOS
  Metal→…→Software ; Web OpenGL(WebGL)→Software.
- En mode explicite, on n'itère PAS le fallback (choix conscient de l'utilisateur).

### Build multi-plateforme ([Pong.jenga](Pong.jenga))
- Cibles configurées : **Windows** (clang-mingw : opengl32 + d3d11/d3d12/dxgi +
  Media Foundation + WASAPI + Winsock), **Linux** XLib & Wayland, **macOS**
  (Cocoa/OpenGL), **Android** (EGL/GLESv3, minSdk 24, ABIs armeabi-v7a/arm64/x86/
  x86_64, `androidisgame`, permissions LAN), **HarmonyOS**, **iOS**, **Web**
  (Emscripten WebGL2 ASYNCIFY).
- Métadonnées installeur (publisher Rihen Universe, licence GPL3, raccourci bureau),
  icône `Resources/Pong/Textures/iconexe/ponicon2.png`, assets `Resources/Pong/`.
- Dépendances : NKCanvas, NKGlad, NKUI, NKFont, NKImage, NKNetwork, NKAudio,
  NKWindow, NKEvent + couches de base.

---

## En cours

### Multijoueur LAN ([src/Pong/Net/](src/Pong/Net/))
- `NetworkDiscovery` (broadcast UDP, port **7778**), `NetworkSession`,
  `AfricaPlaces`, `NetProtocol.h`. Code présent mais **hors-build** (à recompiler
  lors de la re-migration). Permissions Android/Harmony déjà déclarées dans le jenga.

### Audio ([src/Pong/Audio/AudioManager.cpp](src/Pong/Audio/AudioManager.cpp))
- Wrapper NKAudio (SFX/musique). Présent mais hors-build.

---

## À venir

### Phase 1 — Pong jouable, **OpenGL d'abord**, Windows + Android (P1)
Restaurer un Pong **jouable** sur les 2 plateformes déjà supportées, en OpenGL
(seul backend NKCanvas qui marche au runtime). Ne PAS bloquer sur Vulkan/DX/SW.
- Étendre la démo : intégrer textures/fontes NKCanvas (`NkTexture`/`NkFont`/
  `NkSprite`/`NkText`, désormais que `NkTextureSetBackend` est câblé A.7).
- Mettre `pong.config` par défaut sur `backend=opengl` (actuellement `vulkan`,
  cassé) tant que les autres backends ne sont pas validés.

### Phase 2 — Intégration NKUI (P1)
- Faire rendre les widgets **NKUI** (ImGui maison) à travers
  `NkRenderTarget`/`NkRenderer2D` de NKCanvas (item « Intégration NKUI » de la
  ROADMAP NKCanvas). Câbler menus/HUD de Pong dessus.

### Phase 3 — Re-migration Ultra Arena (P2)
- **Source de vérité = [`Pong2/`](../Pong2/)** (jeu complet fonctionnel). Réamener
  son contenu (Game + Net + Audio + 12 scènes + SceneManager, ~27 `.cpp`) sur
  NKCanvas dans `Pong/`, puis réactiver le glob [Pong.jenga:61](Pong.jenga#L61).
- Approche **adaptateur** : `GLRenderer2DAdapter` shadow l'API de
  [Render/GLRenderer2D](src/Pong/Render/GLRenderer2D.h) et map chaque méthode vers
  `NkRenderer2D` → les 33 fichiers de scènes compilent sans réécriture, migration
  progressive ensuite (~506 appels GLRenderer2D recensés).
- Vérifier l'include obsolète `Render/SafeArea.h` → `NKEvent/NkSafeArea.h`.

### Phase 4 — Débloquer Vulkan/DX11/DX12/Software (P1, sessions hardware)
Voir section Bugs. Ordre conseillé : OpenGL (✅) → Software → DX12 → DX11 → Vulkan.
**Question d'architecture à trancher ici** : plutôt que debugger les backends
*propres* à NKCanvas, évaluer la convergence du Renderer2D sur **NKRHI** (dont les
5 backends sont validés depuis 2026-05-31) — Pong en hériterait gratuitement.

### Phase 5 — Parité multi-plateforme + packaging (P3)
- Valider macOS/iOS (Metal — Renderer2D Metal manquant), HarmonyOS, Web.
- `jenga package` (MSI/EXE/DEB/PKG/APK) testé end-to-end.

---

## Bugs / quirks connus

- **`pong.config` est actuellement sur `backend=vulkan`** → Pong démarre sur un
  backend qui **crashe**. Mettre `opengl` pour un run qui marche.
- **Vulkan : crash** dans `NkVulkanRenderer2D::CreatePipelines` (juste après
  « Using precompiled SPIR-V shaders »). De plus, les **validation layers** sont
  désactivées ([Apps.cpp:99](src/Pong/Apps.cpp#L99)) car le layer VK SDK 1.4.350
  crashe `vkCreateInstance` sur le pilote NVIDIA actuel.
- **DX11 : n'affiche rien** (probable sampler/viewport non initialisé).
- **DX12 : crash** à l'init PSO/descriptor heap.
- **Software : écran noir** (framebuffer non blité ou format pixel mismatch).
- **Tous backends : shutdown lent** (destructeur `NkRenderWindow` attend
  probablement une opération GPU bloquante).
- Ces bugs **nécessitent une validation runtime sur hardware** (logs validation
  Vulkan, RenderDoc/PIX pour DX) — à traiter en sessions dédiées avec GPU.
- **Hygiène** : `Apps copy.cpp` (parasite, non suivi) à supprimer ; l'ancien
  `Render/`/`Game/` legacy en OpenGL brut sera retiré une fois la migration finie.

---

## Dépendances

- **Modules moteur** : NKCanvas (rendu 2D), NKUI (menus/HUD), NKFont, NKImage,
  NKWindow, NKEvent, NKAudio, NKNetwork, NKFileSystem, NKGlad + base (NKCore,
  NKMath, NKMemory, NKContainers, NKThreading, NKTime, NKLogger, NKStream,
  NKPlatform).
- **Externes** (conditionnels) : Vulkan SDK (`NK_ENABLE_VULKAN=auto`), D3D11/12 +
  DXGI (Windows), EGL/GLESv3 (Android/Harmony), Media Foundation + WASAPI (Windows),
  Winsock (NKNetwork).
- **État amont** : OpenGL NKCanvas ✅ ; autres backends NKCanvas ❌ runtime
  (cf. [Kernel/Runtime/NKCanvas/ROADMAP.md](../../Kernel/Runtime/NKCanvas/ROADMAP.md)).
