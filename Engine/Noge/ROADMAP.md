# Noge — Roadmap (Engine Framework)

État actuel (2026-07-23, audit complet — 105 fichiers, 21 sous-systèmes) :
Noge est le **framework Application** au-dessus du moteur Nkentseu (NKECS,
NKRenderer, NKRHI…). Son ambition : fournir une API de type Unity/Unreal
(Application, LayerStack, EventBus, NkGameObject, NkActor, NkPrefab,
NkSceneGraph, NkComponentHandle) pour bâtir Noge (éditeur, codename Nogee) et
PV3DE (Patient Virtuel 3D Emotif).

**Contexte de production actuel (important pour l'ordre des phases) :** un
entraînement NKAI (Palier 5) occupe la carte graphique (contention Vulkan à
éviter). Tout travail sur Noge doit donc privilégier le **CPU-only** pendant
cette période — logique gameplay, ECS, sérialisation, algorithmes
géométriques/topologie qui ne nécessitent aucun contexte GPU actif. Le rendu/
shaders/viewport temps réel est reporté à la fin (Phase C). C'est le principe
organisateur de ce document (sections **Phase A / B / C** ci-dessous).

## 🚀 PRIORITÉ — Mondes volumineux : les optimisations prouvées en NKAI

> **Décision de Rihen, 6 août 2026.** « On doit implémenter ces optimisations
> dès que possible » — pour le **film d'animation**, le **jeu vidéo** et la
> **simulation**, les trois usages visés par Noge/Nogee.

Les techniques écrites pour faire tenir un modèle de 7 milliards de paramètres
dans 8 Go de VRAM (`Kernel/AI/NKInfer`, jalons QLoRA 4 et 5, **mesurées**) sont
**exactement** celles qu'exigent les scènes volumineuses. Pas des techniques
voisines : les mêmes, appliquées à d'autres octets.

| Prouvé en NKAI | Ce que ça donne pour une scène de production |
|---|---|
| Poids **quantifiés résidents**, décompressés dans le shader — **7×** moins de VRAM (36 Mo au lieu de 259) | Une scène de film ou de monde ouvert tient dans la carte au lieu de la saturer |
| **La table d'embedding jamais montée** : lue au fichier, une ligne par token | **Streaming** : seule la géométrie visible réside ; le reste attend sur disque |
| **Tuilage + mémoire partagée** : **×5** sur le calcul (61 → 282 GFLOPS) | Culling et rendu par tuiles — la même accélération sur le nombre d'objets |
| **KV-cache** : ne jamais recalculer le passé | Caches de géométrie et d'ombres entre deux images (le principe existe déjà : VSM, *dirty box* voxel) |

**Ce que Noge doit en tirer, par usage :**

- **Film d'animation** — les plans dépassent toujours la mémoire disponible : le
  streaming et la géométrie virtualisée décident du plafond de complexité d'un
  plan. Sans eux, on rogne la scène ; avec eux, on rogne le temps de rendu, ce
  qui est négociable.
- **Jeu vidéo** — budget VRAM **fixe** et 16 ms par image : la quantification
  des attributs et le culling tuilé sont les deux leviers qui décident du nombre
  d'objets à l'écran.
- **Simulation** — beaucoup d'entités, peu de variété : l'instanciation et les
  caches priment ; c'est le pilier le plus proche de ce qui existe déjà.

**Où ça s'implémente** : le gros du travail est dans **NKRenderer**, section
« Phase V — MONDES VOLUMINEUX » de `Kernel/Runtime/NKRenderer/ROADMAP.md` (liste
détaillée : géométrie virtualisée, atlas de textures virtuel, hiérarchie
spatiale, quantification des sommets). **Noge en est le consommateur** : c'est
lui qui décide *quoi* charger selon la scène, la caméra et le plan — le rendu ne
fait qu'exécuter.

**Nuance à retenir avant d'optimiser** : en IA le goulot est la bande passante
mémoire ; en 3D c'est plutôt le nombre d'appels de dessin et la latence disque.
Les remèdes se ressemblent, les priorités diffèrent — **mesurer d'abord**, comme
le jalon 4 l'a fait (avant/après à quatre tailles, sans quoi on ne sait pas si
le gain existe).

---

## 🏗️ ANALYSE DE SCOPE — Noge comme moteur de production (piliers)

Question différente des deux sections voisines (celle-ci répond à *« qu'est-ce
qu'un moteur 2D/3D niveau production a besoin d'avoir, et où en est le dépôt
entier — pas seulement Noge — sur chaque pilier »*, la section d'intégration
ci-dessous répond à *« Noge est-il câblé sur les 8 sous-systèmes déjà
identifiés »*). Établie par `Glob`/`Grep` massif sur tout le dépôt
(`Kernel/Runtime/*`, `Kernel/AI/*`, `Kernel/System/*`, `Engine/*`,
`Applications/*`), pas par relecture de la doc existante seule.

**Triangle honnête (même règle que `Kernel/AI/ROADMAP.md`/`ARCHITECTURE.md`
sur NKAI — « pédagogique/fonctionnel » vs « concurrencer PyTorch/frontière »,
jamais promis) :**
- **(A) Cible réelle** — les PATTERNS et l'ARCHITECTURE d'un moteur pro : ECS
  piloté, pipeline de rendu moderne (forward+deferred, PBR/IBL, ombres
  virtuelles), physique intégrée, pipeline d'assets, éditeur intégré,
  scripting, réseau. **Atteignable**, et une bonne partie existe déjà dans ce
  dépôt (cf. tableau ci-dessous) — le travail restant est majoritairement de
  l'**intégration**, pas de la construction from-scratch.
- **(B) Ce qui n'est jamais promis MAINTENANT** — rivaliser en
  **échelle/budget/équipe** avec Epic Games *à court terme*. Correction de
  Rihen du 2026-07-23 : Lumen/Nanite/MetaHuman-équivalents **ne sont plus
  classés hors scope** — ce sont des **ambitions long terme assumées** (cf.
  section dédiée « 🔭 Ambitions long terme assumées » ci-dessous), avec la
  **même règle d'honnêteté** que celle déjà posée pour NKAI
  (`Kernel/AI/ROADMAP.md`, section « 🎯 Ambition & cap » : *« VISER GRAND…
  l'ambition est illimitée ; seul le discours public reste honnête (jamais
  niveau frontière avant de l'avoir prouvé) »*) : jamais dire « on égale
  Lumen/Nanite/MetaHuman » tant que ce n'est pas prouvé à petite échelle, mais
  jamais non plus renoncer par principe.

### Tableau de synthèse — 15 piliers (pilier 15 Navigation IA ajouté le 2026-07-24)

Légende verdict : ✅ **INTÉGRER MAINTENANT** (existe et fonctionne, juste à
brancher) · 🔨 **CONSTRUIRE** (n'existe pas, nécessaire, priorité justifiée) ·
⏸️ **PLUS TARD** (utile, pas bloquant pour un premier moteur qui tourne) ·
❌ **HORS SCOPE ASSUMÉ** (raison explicite).

| # | Pilier | État réel (fichiers réels cités) | Verdict |
|---|---|---|---|
| 1 | **Rendu** | `Kernel/Runtime/NKRenderer` : forward PBR+IBL(CPU+GPU compute)+VSM multi-lights+post-process(bloom/ACES/FXAA/SSAO/LUT) **mature** (son propre ROADMAP : « ~80 % du minimum viable UE5-like »). Deferred v1+v2 **livré et validé sur 4 backends** (`Passes/Deferred/NkDeferredPass`) mais **opt-in, jamais exposé par `NkRenderSystem`** côté Noge. LOD auto = **n'existe nulle part** dans tout le dépôt (Phase P du ROADMAP NKRenderer, jamais commencée). Côté Noge : meshes branchés (`NkRenderSystem.cpp` réel), matériaux cassés (`materialHandle` toujours 0, déjà dans le tableau d'intégration ci-dessous). | ✅ cœur (Phase I4 déjà planifiée) · 🔨 LOD auto (absent partout) mais ⏸️ (aucun jeu du portfolio n'en a besoin) · ⏸️ activer le deferred côté Noge |
| 2 | **Matériaux/Shaders (NkSL)** | `Kernel/Runtime/NKSL/src/NKSL` : **22 `.cpp` + 17 `.h`** (CodeGen/Compiler/Core/Frontend/Reflection/ShaderConvert/VM) — compilateur **réel et prouvé en production** : compile TOUS les shaders `.nksl` de NKRenderer (PBR, deferred, skinning, IBL compute) vers GLSL/HLSL/MSL/SPIR-V, et les kernels compute de NKAI (`NKTensorGpu`). **Zéro lien avec Noge** (0 `#include NKSL` dans `Engine/Noge/src`, dépendance jenga morte). | ✅ **INTÉGRER MAINTENANT** — ce n'est pas « construire un compilateur », c'est brancher un compilateur qui tourne déjà (Phase I4 déjà planifiée) |
| 3 | Physique/Collision | Déjà cartographié **branché** sur Noge (`NkPhysicsSystem.cpp` réel) — RAS, non re-détaillé (cf. mission). | ✅ RAS |
| 4 | **Animation** | `Applications/NkAnima` (app séparée) : M0 IK ✅ terminé, M1 pose/timeline très avancé, M2 blend/state-machine partiel, **M3 physique d'animation façon Cascadeur ✅ boucle complète** (COM/équilibre/contacts/auto-posing/pont non-destructif, 9/9 tests headless `NkAnimPhysTest`). Côté Noge : `Anim/NkLocomotion.h`, `Anim2D/NkTween.h`+`NkAtlas2D.h`, `Rigging/NkIKSolver.h` = **specs seules (0 `.cpp`)**, **zéro lien avec NkAnima** ni avec `Tools/IK/NkIKSystem` de NKRenderer (déjà l'IK réel utilisé par NkAnima M0). | 🔨 **CONSTRUIRE le pont adaptateur** (même schéma que `NkPhysicsSystem` — pas de réimplémentation FABRIK/blend), déjà listé Phase A items 3/4/6 ci-dessous (non commencés) |
| 5 | **Audio** | `Kernel/Runtime/NKAudio` : moteur **AAA quasi complet** — 256 voix, HRTF, buses hiérarchiques + sidechain, DSP (reverb/compressor/EQ/distortion/chorus), codecs WAV/MP3/OGG/FLAC/**Opus**/AIFF, **capture micro 5 plateformes + denoiser**, 8 backends natifs. **2026-07-23 (G1.1) : NKAudio branché sur NKECS/Noge** — `NKAudio`+`NKMedia` ajoutés à `Noge.jenga`, `NkAudioSystem` (`Engine/Noge/src/Noge/ECS/Systems/`) traduit `NkAudioSourceComponent`/`NkAudioListenerComponent` (inchangés) en appels réels `AudioEngine::Play`/`Stop`/`SetSourcePosition`/`SetListenerPosition`, prouvé par `Applications/NkAudioECSDemo` (compilé + exécuté, **13/13 assertions OK**, deux clips WAV joués via des entités ECS jusqu'à la fin réelle de lecture). Reste : streaming ECS (clips longs via `IAudioStream` plutôt que chargement complet), bus/effets DSP non exposés côté composant ECS (accessibles via `NkAudioSystem::Engine()` en usage avancé seulement). | ✅ **pont ECS fait** (G1.1) · ⏸️ reste : streaming ECS, exposition bus/DSP au niveau composant |
| 6 | **UI/Éditeur intégré** | Deux moteurs UI distincts coexistent : **NKUI** (`Kernel/Runtime/NKUI`, legacy immediate-mode, 17 `.cpp`/23 `.h`, mature — docking/animation/thèmes/gizmo/file-browser) **réellement utilisé par Nogee** (`Applications/Nogee/src/Nogee/Layers/UILayer.cpp` → `nkui::NkUIContext`) ; **NKGui** (`Kernel/Runtime/NKGui`, réécriture « next-gen » en cours, 6 `.cpp`/7 `.h`) utilisé par **NKCode** (IDE standalone, LSP+PTY+syntax, réel) et **NKEditorKit** (widgets partagés, 1 seul `.cpp` réel sur 18 fichiers) — **pas** par Nogee/Noge. `Applications/Nogee` : **15 `.cpp` réels** (AssetManager/CommandHistory/EditorCamera/GizmoSystem/SelectionManager/ProjectManager/EditorLayer/UILayer/ViewportLayer/Panels) — le plus avancé du lot. **UI in-game (HUD ECS)** : **2026-07-24 (G2.1) : premier système livré et prouvé** — `NkUISystem` (`Engine/Noge/src/Noge/ECS/Systems/`) consomme enfin `ECS/Components/UI/NkUIComponent.h` (composants inchangés) : layout anchor/pivot → liste de primitives abstraite (`NkUIDrawCmd`) → backend de dessin interchangeable (`NkUIDrawBackend`) ; backend actuel = **NKCanvas Software headless** (`NkSoftwareFramebuffer` CPU + texte NKFont embarqué), cible de production = **`renderer::NkRender2D`** (NKRenderer, même device NKRHI que la scène 3D — NKCanvas a ses propres devices, inutilisable dans une fenêtre de jeu NKRHI). Prouvé par `Applications/NkUIHudDemo` : **29 OK / 0 FAIL** (assertions de pixels + PNG). Détail : Phase G2 item 1. | ✅ rien de bloquant (Nogee tourne déjà) · ✅ **système ECS UI in-game (HUD) fait** (G2.1, backend Software headless ; reste : backend `NkRender2D` en fenêtre de jeu, textures/sprites UI, entrées souris/clavier, hiérarchie de rects) · ⏸️ convergence NKUI/NKGui (dette déjà notée par le projet lui-même dans `Applications/NKCode/ROADMAP.md`) · ✅ **renderer d'éditeur RHI GÉNÉRALISÉ (2026-07-24)** : `nkentseu::nkgui::NkEditorRHIRenderer` (`Integrations/NKGui/NkEditorRHIRenderer.h`, header-only, lib `NKGuiIntegration`) — impl NKRHI de `editorkit::NkIEditorRenderer` extraite de NkAnimaEditor (frame device-level : BeginFrame + passe swapchain + `NkGuiRHIBackend::Submit` + SubmitAndPresent ; `GetDevice`/`GetBackend`/`SetPreUI` pour viewport 3D offscreen à device partagé) ; NkAnimaEditor recompile via un alias local (`nkanima::NkEditorRHIRenderer`), NKEditorKitDemo/NKCode (défaut NKCanvas) recompilent inchangés — **compilation seule, validation visuelle en attente (contrainte matérielle : extinctions sur pics GPU)** ; côté Nogee : flag `--ui=rhi\|nkui` posé (`UkConfig.h::NogeeUiBackend`) ; **2026-07-24 : cible Nogee réhabilitée — 68 erreurs → build vert (0 erreur, link OK)**, câblage RHIShell restant (voir note migration Nogee, Phase G5) |
| 7 | **Scripting/Visual scripting** | Scripting C++ natif statique (`NkScriptComponent`/`NkScriptSystem`) = ✅ livré, cycle de vie complet, compilé en dur dans le binaire (**mise à jour 2026-07-24** : `NkScriptComponent.h` désSTLisé — `NkSharedPtr`/`NkMakeScript` NKMemory au lieu de `std::shared_ptr`/`std::function`, la violation zéro-STL de l'audit est corrigée). **Hot-reload C++ natif par DLL : ✅ LIVRÉ ET PROUVÉ (2026-07-24, jalon G2.3)** — `NkScriptBridge.h` n'est plus une spec : `NkScriptBridge.cpp` réel (LoadLibrary/dlopen, shadow copy Windows, hooks NKMemory injectés) + ABI C autonome `NkScriptABI.h`, prouvé par `Applications/NkHotReloadDemo` (16 assertions OK : compteur préservé à travers un reload v1→v2 réel + nouveau comportement actif). Détail : section « Quintette de scripting » pilier 1. Bridges C#(`Scripting/CSharp/NkScriptCSharp.h`, cible Mono/CoreCLR via P/Invoke, `#ifdef NKECS_MONO_AVAILABLE`)/Python(`Scripting/Python/NkScriptPython.h`) = **specs seules**. `ECS/VisualScript/NkBlueprint.h`+`NkBlueprintHotReload.h` = spec seule (696 lignes, STL aussi). Substrat commun visé = **NKGraph** (`Kernel/Runtime/NKGraph/`) : **aucun code**, seulement `ROADMAP.md` — décision d'architecture du 2026-07-09, les 6 briques P1-P6 sont toutes ❌, « aucun code encore, ce document est la spec de référence AVANT la première implémentation », et **conçu dès le départ multi-consommateur** (matériaux/VFX/Blueprint/procédural/anim graphs, pas un Blueprint gameplay isolé). | ✅ scripting C++ natif statique RAS · ✅ **C++ hot-reload DLL FAIT** (2026-07-24, `NkHotReloadDemo` 16 OK/0 FAIL — 1er des 5 piliers livré) · 🗣️ **AMBITION ASSUMÉE** (correction Rihen 2026-07-23, n'est plus hors scope) — cf. section dédiée « 🗣️ Quintette de scripting + script visuel » ci-dessous pour C#/Python/langage from-scratch/visuel |
| 8 | **Pipeline d'assets** | **MIS À JOUR (2026-07-23)** : `Engine/Noge/src/Noge/IO/{NkOBJIO,NkGLTFIO,NkFBXImporter}.h/.cpp` **réécrits en fins adaptateurs et prouvés par exécution réelle** (`Applications/NkAssetIODemo`, 53 OK/0 FAIL — voir « Incrément Phase G1 » plus bas) sur `Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/` : `renderer::LoadOBJ/LoadGLTF/LoadFBX` (glTF2.0+GLB avec skinning/morph targets/PBR, tous avec un vrai `.cpp`), dispatchés par extension dans `NkMeshSystem::Import` (`NkMeshSystem.cpp:163-176`), **déjà utilisé par Noge** (`NkRenderSystem.cpp`). `NkSVGIO` : **SVG import minimal ajouté a posteriori (2026-07-24)** — l'audit initial avait conclu "tout ou rien" (construire tout Design/Doc d'abord), mais une investigation plus poussée a montré que le codec réel expose de VRAIES données vectorielles (`NkSVGShapeView` : contours polylignes X/Y, fill/stroke, opacité, fill-rule) et que seul un SOUS-ENSEMBLE de construction manquait. Implémenté : `NkVectorPath::MoveTo/LineTo/Close`, `NkVectorDocument::AddArtboard`/`NkArtboard::AddLayer`/`NkVectorLayer::AddPath`, `NkSVGIO::Import` (+ correctifs d'includes cassés et ambiguïtés `NkColor`/`NkSpan` dans les 4 headers Design/Doc/Color, + `NkColorManager.cpp` minimal pour `FromSRGB`). Prouvé par exécution réelle : `Applications/NkSVGImportDemo`, **24 OK/0 FAIL** (SVG synthétique 3 formes couleurs/opacité vérifiées + `github.svg` réel comparé à la baseline `NkSVGImage::ShapeCount()` + chemin d'erreur). Limites honnêtes (documentées dans `NkSVGIO.h/.cpp`) : contours déjà aplatis en polylignes (pas de Béziers préservées), pas de dégradés/texte/groupes/clip-paths/fill-opacity (non exposés par la vue), pas d'export ; tessellation/booléens/sérialisation/undo réels restent des specs sans corps. Textures : `Kernel/Runtime/NKImage/src/NKImage/Codecs/` = BMP/EXR/GIF/HDR/ICO/JPEG/PNG/PPM/QOI/**SVG**(raster uniquement, pas de doc vectoriel)/TGA/WEBP, tous réels. Polices : `Kernel/Runtime/NKFont/src/NKFont/Core` = parser+rasterizer+detect réel, from-scratch (pas FreeType). | ✅ **FAIT** pour OBJ/glTF/FBX (4ᵉ application du pattern NkEditableMesh/NkPhysicsSystem/NKNetwork, prouvé par exécution) · ✅ **FAIT (minimal, 2026-07-24)** import SVG basique `NkSVGIO::Import` prouvé par exécution (`NkSVGImportDemo`, 24 OK/0 FAIL) · 🔨 le reste de Design/Doc (tessellation, booléens, export SVG, undo réel, blend modes) reste un chantier séparé |
| 9 | Réseau/Multijoueur | **2026-07-24 (Phase I1) : NKNetwork branché sur NKECS/Noge** — `NKNetwork` ajouté à `Noge.jenga`, `ECS/Replication/NkNetWorld.h/.cpp` réécrit en adaptateur mince (`NkNetSystem` + composant `ecs::NkNetEntity`) sur `net::NkNetWorld` réel, prouvé par `Applications/NkNetWorldDemo` (**18/18 assertions OK** : loopback 127.0.0.1, 2 entités ECS, convergence client==serveur après snapshot/delta, despawn répliqué). | ✅ **pont ECS fait** (Phase I1) |
| 10 | **Particules/VFX** | `ECS/Systems/NkParticleSystem.cpp` = ✅ livré, pont réel vers `NKRenderer::NkVFXSystem` (déjà dans le tableau d'audit du jour). Graphe de nodes VFX (édition visuelle) dépend de NKGraph (pilier 7, zéro code). | ✅ runtime RAS · ⏸️ graphe d'édition (même sort que Blueprint) |
| 11 | **IA de gameplay / comportement** | `Kernel/AI/NKRL` (Q-learning tabulaire, 100 % réussite prouvé `NKRLTest`), `Kernel/AI/NKAgent` (mémoire+perception+policy, **200/200 = 100 %** sur grid-world, livré 2026-07-23), `Kernel/AI/NKEvolve` (algo génétique réel, fitness 0,013→0,91/200 générations, livré 2026-07-23), `Kernel/AI/NKCivilization` (multi-agent grille partagée — **« PAS encore sur NKECS »**, dixit son propre audit du jour ; prototype `Applications/NKCivilizationTest` séparé). **2026-07-23 (G1.2) : NKAgent/NKRL branchés sur NKECS/Noge** — `NkAgentComponent` (`Engine/Noge/src/Noge/ECS/Components/AI/`) + `NkAgentSystem` (`Engine/Noge/src/Noge/ECS/Systems/`), prouvé par `Applications/NkAgentEcsDemo` (compilé + exécuté, **200/200 = 100 %** en évaluation gloutonne, piloté par une entité ECS). `NkBehaviourComponent` générique (au-delà de RL tabulaire) et le pont `NKCivilization`/`NKEvolve` restent à faire. | ✅ **pont RL/agent -> ECS fait** (G1.2) · 🔨 reste : `NKEvolve`/`NKCivilization` -> ECS, `NkBehaviourComponent` générique (arbres de comportement / au-delà du Q-learning tabulaire) |
| 12 | **Terrain/Monde ouvert** | Recherche exhaustive (`grep -r "Terrain\|Heightmap"` sur `Kernel/`+`Engine/`) : **zéro classe de terrain**. Seuls résultats pertinents : `NkRender3D::SetInfiniteGridEnabled` (grille de référence visuelle éditeur, pas un terrain jouable) et `Resources/NKRenderer/Shaders/Terrain/NkSL/terrain.nksl` (un shader isolé, aucun système CPU heightmap/streaming/LOD derrière). | ❌ **HORS SCOPE ASSUMÉ** — aucun jeu du portfolio (Pong=2D paddle, Mou=éducatif 2D, Songo'o=jeu de plateau 2D, NKPA=scène 3D bornée) n'a besoin de terrain ; chantier de mois sans consommateur réel aujourd'hui |
| 13 | **Sequencer/Cinématiques** | `Engine/Noge/src/Noge/Sequencer/NkSequencer.h` = spec seule (0 `.cpp`), déjà dans le tableau d'audit du jour, déjà positionné Phase A item 11 (structures/évaluation timeline CPU) puis Phase C (rendu tracks caméra/lumière). | ⏸️ **PLUS TARD** — utile pour PV3DE, pas bloquant pour un premier moteur qui tourne ; positionnement déjà correct, pas de changement |
| 14 | **Build/Export multiplateforme** | `Noge.jenga` a déjà les filtres `system:Android/HarmonyOS/iOS/Web/UWP/XboxSeries/XboxOne` (lignes 118-204) — **pattern générique hérité**, identique à `NKAudio.jenga` où Android/HarmonyOS/Web **ont réellement été cross-compilés et validés** (capture micro AAudio/getUserMedia/OHAudio, testé). Pour **Noge spécifiquement** : seule preuve d'exécution documentée = `jenga build --target Noge --config Debug --platform Windows` (ligne 289 de ce fichier) — **aucune preuve de cross-compilation Android/HarmonyOS/Web pour Noge/Nogee** à ce jour. | ✅ **INTÉGRER À COÛT FAIBLE** — la plomberie jenga existe déjà (copier le pattern NKAudio), valider un build Android est une tâche de config + un jalon testable, pas une construction ; ⏸️ export packagé complet (APK signé/HAP/wasm) plus tard |
| 15 | **Navigation IA (NavMesh/Pathfinding)** | **AUCUN système n'existait nulle part dans le dépôt avant le 2026-07-24** (grep exhaustif `NavMesh\|Pathfind\|A\*` sur `Kernel/`+`Engine/` : zéro résultat pertinent). Seul indice pré-existant : `Engine/Noge/src/Noge/Crowd/NkCrowdSim.h` (`NkCrowdGrid`/`NkCrowdManager`, spec seule 0 `.cpp`, **include cassé + `std::pair` STL** déjà noté dans l'audit ci-dessus) — c'est une grille spatiale de VOISINAGE pour du **boids/évitement de foule**, pas un NavMesh ; futur consommateur potentiel d'un chemin calculé, pas le système de navigation lui-même. **2026-07-24 : premier système livré et prouvé** — nouveau module `Kernel/Runtime/NKNavigation` (`NkNavMesh.h/.cpp`, zéro-STL, posé sur `NKCollision`) : génère un NavMesh triangulé depuis une géométrie de sol (grille filtrée par pente + obstacles AABB), sonde la géométrie par un **raycast RÉEL** (`collision::NkRayTriangle3D`, zéro réimplémentation), construit le graphe d'adjacence des triangles (table de hachage d'arêtes, généraliste), et calcule un chemin par **A*** (coût = distance centroïde-centroïde, waypoints = milieux de portails). Pont ECS côté Noge : `NkNavAgentComponent` (`Engine/Noge/src/Noge/ECS/Components/AI/`, tampon borné de 64 waypoints, zéro allocation dynamique par tick) + `NkNavigationSystem` (`Engine/Noge/src/Noge/ECS/Systems/`, groupe `Update`, possède le `nav::NkNavMesh` PARTAGÉ — même schéma que `NkPhysicsSystem`/`physics::NkPhysicsWorld`). **Distinct de** `Kernel/AI/NKAgent`/`NKRL` (pilier 11 ci-dessus, IA de gameplay PAR APPRENTISSAGE) : ici navigation CLASSIQUE/déterministe, zéro apprentissage. **Jalon validé par DEUX exécutions réelles (2026-07-24)** : (1) `Applications/NkNavCoreDemo` (cœur seul, zéro dépendance à `Noge`, même contournement `jenga test` bloqué que les autres démos) → **11 OK / 0 FAIL**, exit 0 : NavMesh réel de 2832 triangles/1681 sommets sur sol 20×20 avec 3 obstacles sur la diagonale départ→arrivée, A* trouve un chemin de 82 waypoints (longueur 25,43 contre 22,63 en ligne droite = détour réel prouvé), simulation manuelle atteint EXACTEMENT le but en 131 frames sans jamais entrer dans un obstacle ; scénario négatif (mur solide sans ouverture) → `NkPathStatus::Unreachable` correctement détecté. (2) `Applications/NkNavDemo` (intégration ECS complète : entité `NkTransform`+`NkNavAgentComponent` pilotée par `NkNavigationSystem::Execute()` tick par tick) → **9 OK / 0 FAIL**, exit 0, arrivée EXACTE en (8, 0, 8) en 135 frames (4,5 s), chemin PLANIFIÉ et déplacement RÉEL vérifiés hors obstacles à chaque tick. L'exécution de la démo ECS a d'abord été bloquée par un incident externe (`Noge.lib` ne linkait pas — ambiguïté `NkColor`/`NkSpan` dans `Noge/Color`+`Noge/Design`, chantier Design/Doc d'un autre agent), levé par ce même chantier en cours de route. **Bug réel débusqué et corrigé grâce à la démo ECS** : le tampon borné de 64 waypoints TRONQUAIT silencieusement le chemin de 82 waypoints → l'agent déclarait `Arrived` à ~5 unités du but (première exécution : 8 OK / 1 FAIL) ; corrigé dans `NkNavigationSystem.cpp` — fin de tampon = REPLANIFICATION depuis la position courante si la destination n'est pas réellement à portée (le chemin restant raccourcit à chaque replan → convergence garantie ; c'est aussi le mécanisme « replanifie si besoin » du jalon). Voir la sous-section dédiée ci-dessous (« Phase G1 item 5 — Navigation IA ») pour le détail complet et les limitations honnêtes (pas de funnel/string-pulling, obstacles supposés posés au sol, liste ouverte A* linéaire). | 🔨 **CONSTRUIT ET PROUVÉ bout-en-bout** (cœur NavMesh+A* 11/11 + pont ECS 9/9, 2026-07-24) · ⏸️ reste : lissage funnel des chemins, NavMesh multi-étage/3D, évitement dynamique d'agents mobiles (`NkCrowdSim`), regénération incrémentale (aujourd'hui `Build()` recalcule tout) |

### ❌ Hors scope assumé — liste explicite et raisons

- **Terrain streaming / monde ouvert immense** (type UE5 World Partition) —
  voir pilier 12 : aucun consommateur réel dans le portfolio actuel.
- **LLM entraîné from-scratch à l'échelle frontière** — hors sujet moteur
  mais même règle que `Kernel/AI/ROADMAP.md` : NKAI *supporte* l'inférence/
  fine-tuning de petits modèles, n'entraîne jamais un concurrent de
  GPT/Claude — rappelé ici pour cohérence transverse du discours du projet.

**Retirés de cette liste le 2026-07-23** (correction de Rihen — ce ne sont
**plus** des renoncements permanents, mais des ambitions long terme assumées
avec un premier jalon honnête défini) : **Lumen/Nanite/MetaHuman-équivalents**
(pilier 1, cf. « 🔭 Ambitions long terme assumées » ci-dessous) et les
**bridges de scripting C#/Python/DLL + langage from-scratch** (pilier 7, cf.
« 🗣️ Quintette de scripting + script visuel » ci-dessous).

---

## 🔭 Ambitions long terme assumées (R&D, pas maintenant)

Correction de Rihen du 2026-07-23 : Lumen (GI temps réel), Nanite (géométrie
virtualisée) et MetaHuman (humains haute-fidélité) ne sont **plus** listés
« hors scope » — ce sont de **vraies ambitions long terme**, pas un
renoncement. Même règle d'honnêteté que `Kernel/AI/ROADMAP.md` (section
« 🎯 Ambition & cap ») : l'ambition est illimitée, seul le discours public
reste honnête tant que rien n'est prouvé. Ci-dessous, pour chacun : la
fondation réelle déjà posée dans ce dépôt, le premier jalon honnête et
testable (petite échelle), et le positionnement dans le séquencement — **au-
delà de G1-G5**, dans une Phase G6 R&D (cf. section séquencée en fin de
document).

### GI temps réel (équivalent Lumen, from-scratch, sans RTX propriétaire)

- **Fondation réelle déjà là** :
  `Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/VoxelAO/NkVoxelAOSystem.{h,cpp}`
  (Phase H.6 du ROADMAP NKRenderer, « UE5 Lumen-light approximé » dixit son
  propre commentaire d'en-tête) — voxelise en CPU des occluders AABB
  world-space dans une texture 3D `R8_UNORM` (64×32×64 par défaut), le shader
  PBR fait du **cone-tracing** dans l'hémisphère normale pour calculer une
  **AO longue portée** qui assombrit l'IBL. Livré et fonctionnel, mais
  **occlusion uniquement** (pas de radiance/couleur transportée), **statique**
  (voxelisé une fois au boot, jamais mis à jour par frame), et **AABB-based**
  (pas mesh-précis) — limitations documentées explicitement dans le fichier
  lui-même (« Limitation v0 »). L'**IBL compute** (pilier 2/pilier 1 du
  tableau ci-dessus, PBR+IBL CPU+GPU compute, noté « ~80 % du minimum viable
  UE5-like » par le propre ROADMAP de NKRenderer) est la seconde brique déjà
  mature dont une GI voxel s'appuierait. Un second système,
  `Tools/Voxel/NkVoxelSystem.h` (raymarch d'un volume voxel éditable façon
  ZBrush, explicitement marqué « ⚠️ SQUELETTE — à implémenter/tester plus
  tard » dans son propre en-tête), **partage potentiellement le volume** mais
  sert la **sculpture**, pas l'éclairage — à ne pas confondre comme même
  chantier.
- **Premier jalon réaliste** (pas « Lumen ») : faire évoluer
  `NkVoxelAOSystem` d'un système d'**occlusion pure** vers un système qui
  **transporte de la radiance** (voxel cone tracing avec injection de couleur
  depuis des surfaces émissives/éclairées, pas juste un facteur d'assombrissement)
  — sur **une seule scène simple, statique**, et qui **bat en qualité** l'AO
  actuel (ex. color bleeding visible d'un mur rouge sur un mur blanc voisin,
  ce que le voxel AO actuel, purement occlusif, ne peut pas produire). Zéro
  ray-tracing matériel Nvidia RTX — cohérent avec la posture « possédé,
  portable » déjà affirmée pour NKAI/compute. La mise à jour **dynamique**
  (per-frame, aujourd'hui absente même du voxel AO) est un jalon séparé et
  plus tard.
- **Séquencement** : Phase R&D (G6), après un premier jeu Noge complet
  (G1-G5). Le raytracing matériel (Phase R du ROADMAP NKRenderer, priorité 4,
  jamais commencé) reste hors de cette ambition — cohérent avec le choix
  « from-scratch/portable » déjà pris.

### Géométrie virtualisée (équivalent Nanite, clustering/meshlet from-scratch)

- **Fondation réelle déjà là** : **beaucoup plus mince** que pour la GI.
  `grep -ri "cluster|meshlet|LOD"` sur tout `NKRenderer` ne remonte **aucun**
  système de clustering/meshlet — juste un **bucket de distance** :
  `Tools/Culling/NkCullingSystem.h`, champ `lodDistances[4]` +
  `ComputeLOD(dist)` qui renvoie un **index 0-3** utilisé pour le culling par
  distance, **pas** une sélection/génération de maillage simplifié — aucune
  géométrie alternative n'existe derrière cet index aujourd'hui. Par
  ailleurs, `NkSWRasterizer` (mentionné dans l'historique) existe bien —
  **dans `Kernel/Runtime/NKRHI/src/NKRHI/Software/NkSoftwareDevice.cpp`** —
  mais c'est le **rasterizer logiciel du backend RHI "Software"** (rendu CPU
  sans carte graphique, scanline mono-thread, cf.
  `NKRHI/src/NKRHI/Software/ROADMAP.md`), **sans rapport conceptuel avec
  Nanite** (ce n'est pas un pipeline de culling GPU-driven par clusters, c'est
  un chemin de rendu alternatif pour machines sans GPU) — à ne pas
  sur-vendre comme un embryon de Nanite. **Honnêtement : la géométrie
  virtualisée n'a quasiment aucun point de départ réel dans ce dépôt
  aujourd'hui**, contrairement à la GI.
- **Premier jalon réaliste** (pas « Nanite ») : un **LOD discret classique**
  — 2-3 maillages pré-simplifiés par asset, sélectionnés au *draw call* par
  le bucket de distance déjà réel de `NkCullingSystem::ComputeLOD` (aucune
  réimplémentation de la distance-classification, juste brancher une
  sélection de mesh dessus). C'est explicitement l'item déjà noté 🔨 « à
  construire un jour » du pilier 1 du tableau ci-dessus — pas un nouveau
  chantier inventé ici. Le vrai **clustering/meshlet + culling GPU-driven +
  streaming de géométrie** (la vraie définition de Nanite) reste **entièrement
  à construire from-scratch**, aucune fondation dessus.
- **Séquencement** : le LOD discret (sélection de mesh pré-simplifié) est
  **assez simple pour rentrer dans G1-G5** (réutilise `NkCullingSystem`
  existant) — voir Phase G2/G3 ci-dessous. Le vrai clustering/meshlet
  virtualisé (Nanite-like) est Phase R&D (G6), horizon long, aucune fondation
  aujourd'hui.

### Humains haute-fidélité (équivalent MetaHuman, stylisé d'abord)

- **Fondation réelle déjà là** : `Applications/PV3DE/src/PV3DE/Face/
  NkFaceController.{h,cpp}` + `NkFaceControllerV2.{h,cpp}` — **code réel, pas
  une spec** : pilotage des **46 Action Units FACS**, mapping AU→blendshape
  du mesh tête (`NkBlendshapeBinding`), presets FACS, clignement automatique
  et réflexe (`ForceBlink`), direction du regard (`SetGazeDirection`,
  consommée par `NkPatientRenderer` pour l'offset du regard dans le shader
  des yeux). C'est une fondation **stylisée/fonctionnelle** (rig blendshape
  piloté par FACS pour un patient virtuel émotif), **pas** un MetaHuman
  inachevé — ambition et échelle différentes par conception, comme déjà noté
  dans ce document. Côté Noge (pas PV3DE), `Facial/NkFacialRig.h` +
  `NkSkinMaterial.h` (SSS peau/yeux/dents) restent des **specs seules**
  (0 `.cpp`), pas encore reliées au vrai `NkFaceController`.
- **Premier jalon réaliste** (pas « MetaHuman ») : pousser le **rendu peau**
  au-delà du blendshape pur — un premier shader SSS (subsurface scattering)
  simple sur `NkSkinMaterial.h` (aujourd'hui spec seule) branché sur le
  visage déjà piloté par `NkFaceController`, sur **un seul visage stylisé**
  déjà existant dans PV3DE, jugé sur un critère visuel simple (translucence
  crédible aux oreilles/nez sous contre-jour) plutôt qu'un scan
  photoréaliste. Le scan 3D humain / capture volumétrique reste hors
  d'atteinte d'un dev solo et n'est pas visé.
- **Séquencement** : Phase R&D (G6) pour le SSS avancé + tout ce qui
  approcherait un niveau « scanné photoréaliste ». Le rig FACS actuel
  continue d'évoluer indépendamment côté PV3DE (hors de cette roadmap Noge).

---

## 🗣️ Quintette de scripting + script visuel — ambition assumée

Remplace l'ancien verdict « ❌ HORS SCOPE ASSUMÉ bridges C#/Python » du
pilier 7. Correction de Rihen du 2026-07-23 : Rihen veut **5 options de
scripting**, pas seulement le C++ natif statique déjà livré — C#, Python, un
**langage from-scratch généraliste** (pas un DSL shader comme NkSL), du
**C++ nativement rechargé à chaud (hot-reload DLL)**, et du **script
visuel**. Pour chacun : état réel, dépendances, premier jalon.

### 1. C++ natif hot-reload (DLL) — ✅ **LIVRÉ ET PROUVÉ (2026-07-24, jalon G2.3)**

- **État réel** : **implémenté et prouvé par exécution réelle** (le premier
  des 5 piliers à passer de la spec au code qui tourne). Livraison :
  - `Engine/Noge/src/Noge/ECS/Scripting/NkScriptABI.h` (**nouveau**) : ABI C
    stable AUTONOME (zéro include moteur — un script se compile en `.dll`
    par un simple `clang++ -shared -I Engine/Noge/src`, sans linker le
    moteur) : `NkScriptContext`/`NkScriptDLLInfo`/`NkScriptVTable`/
    `NkScriptDLLFactory`, base `NkScriptDLLBase`, macros
    `NK_EXPORT_DLL_SCRIPT`/`NK_EXPORT_DLL_SCRIPT_VERSIONED`.
    `nkecs_get_factory` remplit un **paramètre de sortie** (pas de retour de
    struct C++ par valeur à travers `extern "C"`) et la DLL exporte
    `nkecs_set_allocator` : le loader **injecte les hooks NKMemory** →
    toutes les instances de scripts sont allouées/libérées via NKMemory
    (placement new + destructeur explicite, plus aucun `new`/`delete` brut
    dans la macro — violation corrigée comme prévu).
  - `NkScriptBridge.h` réécrit (déclarations + `NkDLLScriptAdapter`) +
    **`NkScriptBridge.cpp` (nouveau, l'implémentation réelle)** :
    `LoadLibraryA`/`GetProcAddress` (Windows) et `dlopen`/`dlsym` (POSIX),
    **shadow copy Windows** (`Foo.dll` → `Foo.dll.hotN` : LoadLibrary
    verrouille le fichier chargé, la copie permet de recompiler l'original
    pendant l'exécution), `LoadDirectory` réel (`NkDirectory::GetFiles`),
    `HotReload(world)` : détection mtime (`NkFileSystem::GetLastWriteTime`)
    → `CaptureState` (Serialize) → destruction des instances PENDANT que
    l'ancienne DLL est chargée → déchargement → rechargement →
    `RestoreState` (Deserialize) + `pendingStart` (OnStart de la nouvelle
    instance sans redémarrer les autres). **Si le rechargement échoue,
    l'ancienne version reste active** (la nouvelle DLL est chargée AVANT de
    décharger l'ancienne). Zéro STL (NkVector/NkString/NkSharedPtr maison).
  - `NkScriptComponent.h` **désSTLisé au passage** comme demandé :
    `std::shared_ptr`/`std::make_shared` → `memory::NkSharedPtr` +
    `NkMakeScript<T>` (allocation `NkGetDefaultAllocator`),
    `std::function` du registre → pointeur de fonction pur ;
    `<memory>`/`<vector>` supprimés. `NkScriptSystem.h` rendu
    auto-suffisant (include `NkTag.h` pour `NkInactive`).
- **Preuve (jalon testable de la roadmap, atteint tel quel)** :
  `Applications/NkHotReloadDemo` (cible jenga `NkHotReloadDemo`, enregistrée
  dans `Nkentseu.jenga`) — **16 assertions OK / 0 échec** : compile à
  RUNTIME `scripts/HotCounterScript_v1.cpp` (+1/tick, état JSON
  `{"counter":N}`) en `.dll` via le clang-mingw du toolchain (appel
  compilateur direct — voie retenue : la démo reste un seul exe, aucune
  cible DLL à orchestrer), la charge, l'attache à une entité ECS
  (`NkScriptHost` + `NkScriptSystem`), 5 ticks → compteur=5 ; recompile la
  V2 (+2/tick, version 2.0.0) **par-dessus la même .dll**, `HotReload(world)`
  → version 2.0.0 active, **compteur TOUJOURS à 5** (état préservé), 3
  ticks → **11 = 5+3×2** (nouveau comportement, pas 8 = ancien +1). Sortie
  réelle : `[script v1] tick -> counter=1..5`, `[script v2] OnStart
  (counter=5 — etat restaure)`, `counter=7/9/11`. Exécution :
  `.\Build\Bin\Release-Windows\NkHotReloadDemo\NkHotReloadDemo.exe` depuis
  la racine (trace détaillée dans `logs/app.log`).
- **Dépendances** : NKFileSystem (mtime + copy/delete, réel), NKMemory
  (hooks d'allocation injectés — fait), NKReflection (optionnel, pour
  `GetFieldsJSON`/`SetFieldFromJSON` prévus dans la vtable mais **toujours
  non implémentés** — reste à faire si l'éditeur en a besoin). Zéro
  dépendance à NKGraph.
- **Reste à faire (honnête)** : introspection `GetFieldsJSON`/
  `SetFieldFromJSON` (nullptr aujourd'hui) ; hooks collision/trigger de la
  vtable non branchés sur les événements physiques ECS ; accès composants
  typé depuis la DLL (le monde reste opaque côté script — voulu pour l'ABI,
  mais un jeu réel voudra des accesseurs C stables genre
  `nkecs_get_transform`) ; validé Windows/clang-mingw uniquement (chemin
  POSIX `dlopen` écrit mais non exécuté).

### 2. Bridge C#

- **État réel** : `Scripting/CSharp/NkScriptCSharp.h` — spec seule, cible
  **Mono/CoreCLR via P/Invoke** (`#ifdef NKECS_MONO_AVAILABLE`, sinon stubs de
  compilation), workflow déjà esquissé (`NkCSharpBridge::Init`/
  `LoadAssembly`/`ReloadAssembly`, classe de script C# `NkScript` avec
  `OnStart`/`OnUpdate`/`OnCollisionEnter`). Choix technique déjà posé dans le
  header : embarquer un runtime existant (Mono ou CoreCLR), **pas**
  réinventer un CLR.
- **Dépendances** : runtime Mono/CoreCLR externe (dépendance tierce lourde,
  pas encore dans le build). Zéro dépendance à NKGraph.
- **Premier jalon réaliste** : `NkCSharpBridge::Init` charge Mono en
  process, exécute un script C# `.cs` compilé en `.dll` qui modifie une
  `NkTransform` via `ctx.GetTransform()`, vérifié par assertions.

### 3. Bridge Python

- **État réel** : `Scripting/Python/NkScriptPython.h` — spec seule (non
  détaillée dans cette passe, même statut que CSharp : header présent,
  0 `.cpp`). Deux options possibles pour l'implémentation, à trancher :
  CPython embarqué (interop C API standard, dépendance lourde mais mature)
  ou un mini-interpréteur Python-subset from-scratch (plus de contrôle/moins
  de dépendance externe, mais un vrai chantier de parser+VM en soi).
- **Dépendances** : selon le choix ci-dessus — soit CPython (tierce, lourde)
  soit rien d'externe mais un vrai sous-projet interne. Zéro dépendance à
  NKGraph.
- **Premier jalon réaliste** : si CPython embarqué (recommandé pour aller
  vite) — charger l'interpréteur en process, exécuter un script `.py` qui
  modifie une entité ECS via un binding minimal, vérifié par assertions.

### 4. Langage from-scratch généraliste (proposition de nom : **NkPlay**)

- **État réel** : **aucun code et aucune spec dédiée** à ce jour côté
  scripting gameplay généraliste. Le seul langage from-scratch existant dans
  tout le dépôt est **`Spark/` (NkSpark)** — vérifié via
  `Spark/ROADMAP.md` : **« scaffold documentaire, aucune ligne de code »**,
  toutes les briques (`NkSparkLex/Parse/Sema/IR/Gen/Link/HAL/RT/CLI`) sont
  ❌. **Ce n'est pas le bon point de départ à réutiliser tel quel** : NkSpark
  cible explicitement l'**embarqué bare-metal** (ISA RISC-V RV32I, sortie
  Intel HEX, runtime bare-metal `crt0`+vecteurs, GPIO/UART/IRQ) — un chantier
  **différent** d'un langage de scripting gameplay haut-niveau (pas de
  bare-metal, pas de cross-compilation microcontrôleur, besoin d'un GC ou
  d'ownership simple, d'un binding ECS riche). Le **savoir-faire** front-end
  (lexer/parser/AST/sema, cf. note dans `Spark/ROADMAP.md` : « réutiliser
  l'expérience NkSL ») et l'expérience `NKSL` (compilateur shader réel et
  prouvé en production, `Kernel/Runtime/NKSL/`, 22 `.cpp`+17 `.h`) sont les
  vraies références techniques transférables — pas le code de NkSpark
  lui-même, qui doit **rester un chantier séparé** (bare-metal ≠ scripting
  gameplay). Nom proposé pour rester dans la convention Nk*/NKSL/NkSpark :
  **NkPlay** (à confirmer par Rihen — aucun nom existant ne collisionne).
- **Dépendances** : aucune obligatoire pour un langage interprété simple
  (front-end + VM bytecode maison, sur le modèle de ce que `NKSL` a déjà
  prouvé côté `CodeGen/Bytecode/`+`VM/`) ; un binding ECS (types/host
  functions exposées à la VM) serait nécessaire pour être utile en gameplay.
  Zéro dépendance à NKGraph (le langage textuel et le graphe visuel sont deux
  fronts différents sur un même besoin, pas le même substrat).
- **Premier jalon réaliste** : **le plus long des 5** — un lexer/parser/AST
  minimal (sous-ensemble du langage : variables, fonctions, boucles,
  appels), un backend bytecode + VM interprétée (réutilisant l'expérience
  `NKSL::VM`, pas le code lui-même qui est GPU-shader-oriented), exécutant un
  script `.nkp` trivial (`print`, boucle, appel d'une fonction hôte C++)
  vérifié par assertions.

### 5. Script visuel — **le même NKGraph, décliné par consommateur**

- **Précision de Rihen (2026-07-23)** : ce n'est **pas** un Blueprint
  gameplay isolé. `Kernel/Runtime/NKGraph/ROADMAP.md` (décision
  d'architecture du 2026-07-09, déjà citée pilier 7 ci-dessus) est
  explicitement conçu comme un **substrat de graphe unique et agnostique**,
  réutilisé par plusieurs consommateurs : nodes **Blueprint** (logique
  gameplay/actions ECS/NkAgent, via `ECS/VisualScript/NkBlueprint.h` côté
  Noge — spec seule, 696 lignes), nodes **matériau** (inputs texture/couleur
  → sorties BSDF, compilés vers **NkSL**, consommateur « NKRenderer Phase
  T.2 » déjà listé dans `NKGraph/ROADMAP.md`), nodes **VFX** (émetteurs/
  forces/comportements, consommateur « Noge VFX » déjà listé, dépendance déjà
  notée pilier 10 ci-dessus), et nodes **procéduraux** (génération
  paramétrique d'assets, consommateur explicitement listé dans
  `Kernel/AI/ROADMAP.md:118-120` : « graphe de nodes géométriques ⚠️ substrat
  = NKGraph »). Architecture en 3 couches déjà spécifiée : Cœur
  (`Kernel/Runtime/NKGraph`, modèle de données pur, sockets typés) → Édition
  (`Engine/NKEditorKit`, widget canvas partagé) → Sémantique métier (chaque
  consommateur fournit ses nodes + son backend d'exécution).
- **État réel du canvas d'édition** : **zéro embryon**, vérifié —
  `Engine/NKEditorKit/src/NKEditorKit/NkEditorCanvasRenderer.h` (seul fichier
  contenant « canvas » dans NKEditorKit) est le **backend de rendu 2D
  générique de la fenêtre IDE** (`NKCanvas`+`NkGuiCanvasBackend`, utilisé par
  NKCode), **pas** un widget de graphe de nodes (pas de pan/zoom de nodes, de
  fils, de sockets). La brique P4 « Widget canvas node-based » de
  `NKGraph/ROADMAP.md` est donc, comme les 5 autres briques P1-P6, **à
  0 % réel** — tout part de zéro, aucun consommateur n'a de longueur d'avance
  cachée.
- **Dépendances** : **NKGraph P1-P3** (modèle de données + évaluation +
  sérialisation, tous ❌ aujourd'hui) sont un préalable dur à **tout**
  consommateur visuel — rien de visuel ne peut démarrer avant. Le premier
  consommateur (« règle des deux consommateurs » déjà posée dans
  `NKGraph/ROADMAP.md`) doit construire le cœur **avec** lui.
- **Avis technique honnête sur l'ordre des 4 consommateurs** : le **graphe
  matériaux** est probablement le plus simple à livrer en premier — **backend
  d'exécution déjà résolu** (compiler vers `NkSL`, compilateur réel et prouvé
  en production, contrairement au Blueprint qui doit encore choisir
  interprétation VM vs génération C++, ou au VFX qui doit s'aplatir en plan
  d'exécution par frame). C'est cohérent avec la note déjà présente dans
  `NKGraph/ROADMAP.md` (« premier client = celui qui démarre en premier :
  Phase 4 NKCode marquée PROCHAIN, sinon le graphe de matériaux — périmètre
  fermé, backend NkSL existant »), mais NKCode est tenu par un autre agent —
  **côté Noge spécifiquement**, le graphe matériaux (périmètre fermé,
  backend NkSL déjà prouvé, déjà dans le pipeline Phase I4 de ce document)
  est le pari le plus sûr. Le Blueprint gameplay vient ensuite (deuxième
  consommateur, force la généralisation de l'API par la « règle des deux
  consommateurs »), puis VFX et procédural en réutilisation.
- **Premier jalon réaliste** : NKGraph P1 (modèle de données : nodes/sockets
  typés/connexions/validation, zéro-STL) + P4 minimal (canvas pan/zoom/fils,
  sans fioritures) construits **avec** un premier graphe matériau à 3-4
  nodes (texture → tint → sortie albédo) qui **compile réellement vers un
  programme NkSL** exécutable — pas une démo qui ne fait qu'afficher des
  boîtes reliées par des fils.

### Avis d'ensemble sur l'ordre des 5

Le plus simple/rapide en premier est probablement le **bridge C# ou Python**
(embarquer un runtime mature — Mono/CoreCLR ou CPython — est un travail
d'intégration, pas de recherche), **suivi de près par le C++ hot-reload**
(l'infra de détection de fichier modifié est déjà réelle et le mécanisme DLL
est un pattern Windows/POSIX standard, mais il reste plus de plomberie
projet — `NkScriptHost`, `LoadDirectory`, respect zéro-STL — que pour un
simple appel à un runtime existant). Le **langage from-scratch** est de
loin le plus long (compilateur + VM entiers à écrire, aucune fondation
directement réutilisable). Le **script visuel** dépend d'un préalable dur
(NKGraph P1-P3, 0 % fait) commun aux quatre autres consommateurs du dépôt —
il ne peut pas être « rapide » même si son premier consommateur (matériaux)
est bien choisi.

### Roadmap de développement globale séquencée

Étend (ne remplace pas) les phases I1-I4 ci-dessous — mêmes contrainte CPU-
first (GPU occupé par l'entraînement NKAI) et même format (jalon testable par
étape). Les items déjà couverts par I1-I4 ne sont pas dupliqués, juste
référencés.

**Phase G1 — CPU-only, immédiat (en parallèle de I1)**
1. ✅ **FAIT (2026-07-23) — NKAudio, le 9ᵉ sous-système manquant** : `NKAudio`
   (+ sa dépendance transitive `NKMedia`, codec Opus) ajoutés à `Noge.jenga`
   `_DEPS`/`includedirs`. `NkAudioSystem` (nouveau,
   `Engine/Noge/src/Noge/ECS/Systems/NkAudioSystem.h/.cpp`, groupe
   `PostUpdate`) traduit `NkAudioSourceComponent`/`NkAudioListenerComponent`
   (`ECS/Components/Audio/NkAudioComponents.h`, données ECS pures,
   inchangées) en appels réels à `audio::AudioEngine` (singleton NKAudio) :
   cache de clips chargés (`AudioLoader::Load`, entrées allouées
   individuellement via NKMemory New/Delete pour une adresse stable — le
   pointeur `AudioSample` est référencé NON-OWNING par les voix actives,
   cf. `Voice::sample` dans `NkAudioEngineCore.cpp` — un simple `NkVector`
   par valeur aurait invalidé les voix en cours à la moindre réallocation),
   `playOnStart`/`loop`/`spatialize`/`volume`/`pitch` → `VoiceParams` +
   `Play()`, position de la `NkTransform` de l'entité → `SetSourcePosition`,
   listener → `SetListenerPosition`/`SetMasterVolume`. API de contrôle
   explicite `PlaySource()`/`StopSource()`/`VoiceOf()` (équivalent concret du
   "NkAudioSource.Play()" du jalon). Même schéma d'adaptateur mince que
   `NkPhysicsSystem` : zéro réimplémentation, délégation pure à NKAudio.
   **Note d'honnêteté** : l'API NKAudio réelle (`NkAudio.h`) n'expose PAS de
   `AudioEngine::GetVoiceState(handle)` (mentionné dans une formulation
   informelle du jalon) — `IsPlaying(handle)`/`IsPaused(handle)`/
   `GetActiveVoices()` sont les méthodes réellement présentes, utilisées ici
   pour l'assertion "voix active".
   **Jalon validé par exécution réelle** : nouvelle appli console
   `Applications/NkAudioECSDemo` (même contournement `jenga test` bloqué que
   `NkEditableMeshDemo`/`NkAgentEcsDemo`) crée deux entités `ecs::NkWorld`
   Noge avec `NkAudioSourceComponent` (`Resources/Audio/powerup.wav` via
   `playOnStart` consommé par `NkAudioSystem::Execute()`, et
   `Resources/Audio/bleep.wav` via `NkAudioSystem::PlaySource()` explicite),
   backend `NULL_OUTPUT` (déterministe, thread de mixage réel — pas de
   simulation), et vérifie par 13 assertions réelles
   (`AudioEngine::IsPlaying`/`GetActiveVoices`/`IsInitialized` + resynchro du
   composant ECS) que les voix sont bien actives puis se terminent. Bug réel
   trouvé et corrigé en cours de route : `playOnStart` n'était pas consommé
   après le premier déclenchement → auto-replay infini dès que la voix se
   libérait ; fixé en un déclencheur one-shot (`src.playOnStart = false`
   immédiatement consommé, sémantique standard façon Unity).
   Compilé et EXÉCUTÉ pour de vrai
   (`Build/Bin/Debug-Windows/NkAudioECSDemo/NkAudioECSDemo.exe`, build
   `jenga build --target NkAudioECSDemo --config Debug --platform Windows` →
   `34/34` projets, succès, 2 warnings bénins `[[nodiscard]]`) → sortie
   réelle : `=== Resultat : 13 OK / 0 FAIL ===`, exit code 0.
   Correctif incident non lié effectué au passage pour débloquer la
   compilation partagée de `Noge` : `Engine/Noge/src/Noge/Anim/NkLocomotion.h`
   incluait `"Nkentseu/Rigging/NkIKSolver.h"` (chemin inexistant, typo dans un
   fichier en cours d'écriture par un autre chantier G1.4 au même moment) —
   corrigé en `"Noge/Rigging/NkIKSolver.h"` (chemin réel), un seul caractère
   de portée, aucune logique touchée.
   **Robustesse du jalon (2026-07-23, suite à une re-vérification indépendante
   du coordinateur)** : une relance indépendante a obtenu `7 OK / 6 FAIL` au
   lieu de `13/0`. Hypothèse initiale du coordinateur : course liée à la
   charge machine. **Diagnostic réel, reproduit à la demande** : pas une
   course — `srcComp.clipPath`/`src2Comp.clipPath` utilisaient un chemin
   relatif (`"Resources/Audio/*.wav"`) qui ne résout QUE si le CWD au
   lancement est la racine du dépôt (même limitation déjà présente et
   documentée dans `Applications/NkAudioDemo/src/main.cpp` : « lance depuis
   la racine du projet Nkentseu ») ; `dependfiles(...)` dans
   `NkAudioECSDemo.jenga` déclare la dépendance pour jenga mais **ne copie
   pas** `Resources/Audio/` à côté de l'exe. Lancer l'exe directement depuis
   `Build/Bin/Debug-Windows/NkAudioECSDemo/` (au lieu de la racine du dépôt)
   reproduit EXACTEMENT `7 OK / 6 FAIL` avec la même liste d'échecs (chargement
   du clip échoué → cascade de handles invalides → les vérifications
   « actif juste après » échouent, celles « inactif à la fin » réussissent
   trivialement puisqu'un handle invalide n'est jamais « playing »).
   Corrigé dans `Applications/NkAudioECSDemo/src/main.cpp` par
   `ResolveAudioPath()` (essaie `Resources/Audio/<f>` puis
   `../../../../Resources/Audio/<f>` avant d'abandonner, + log d'erreur
   explicite si aucun des deux ne résout). Revérifié **stable** : 3 lancements
   depuis la racine du dépôt, 3 lancements depuis
   `Build/Bin/Debug-Windows/NkAudioECSDemo/`, et 3 lancements depuis la racine
   sous charge CPU artificielle (4 boucles busy-wait en parallèle) →
   **9/9 runs à `13 OK / 0 FAIL`**, exit code 0 à chaque fois.
2. ✅ **FAIT (2026-07-23) — Pont IA de gameplay** : `NkAgentComponent`
   (`Engine/Noge/src/Noge/ECS/Components/AI/NkAgentComponent.h`, nouveau
   dossier) référence par pointeur NON POSSÉDÉ un `agent::NkAgent` +
   son `rl::NkGridWorld` (même philosophie que `NkCollider3D::physicsBodyId`
   — la ressource IA reste possédée par l'appelant, pas par l'archétype ECS,
   car `agent::NkAgent`/`rl::NkGridWorld` ne sont pas trivialement copiables).
   `NkAgentSystem` (`Engine/Noge/src/Noge/ECS/Systems/NkAgentSystem.h/.cpp`,
   groupe `Update`) appelle `agent::NkAgent::Step()` une fois par tick pour
   chaque entité porteuse du composant, gère `Reset()`/fin d'épisode et
   cumule `episodesCompleted`/`episodesReachedGoal`. `NKAgent`+`NKRL` ajoutés
   à `Noge.jenga` (`_DEPS` + `includedirs`).
   **Jalon validé par exécution réelle** : nouvelle appli console
   `Applications/NkAgentEcsDemo` (même contournement `jenga test` bloqué que
   `NkEditableMeshDemo`) rejoue EXACTEMENT le monde-grille de
   `NKAgentTest`/`NKRLTest` (grille 5×5, 3 trous, 4000 épisodes d'entraînement,
   epsilon décroissant 1.0→0.05, seed=42) mais piloté par
   `NkAgentSystem::Execute()` sur une entité d'un `ecs::NkWorld` Noge au lieu
   d'une boucle console isolée. Compilé et EXÉCUTÉ pour de vrai
   (`Build/Bin/Debug-Windows/NkAgentEcsDemo/NkAgentEcsDemo.exe`, build
   `jenga build --target NkAgentEcsDemo --config Debug --platform Windows` →
   `34/34 projets, SUCCESS`) : sortie réelle = évaluation gloutonne
   **200/200 épisodes (100 %)**, identique au résultat déjà prouvé par
   `NKAgentTest` en boucle console — même politique apprise (grille de
   flèches identique), mémoire bornée à 64/64 transitions retenues. Seuil de
   validation (`>=95%`) → `[ OK ]`.
   **Effet de bord assumé** : ce build a débusqué et corrigé deux erreurs de
   compilation préexistantes et sans lien avec ce chantier (includes
   manquants dans `Noge/Rigging/NkIKSolver.h` — `NKECS/System/NkSystem.h` et
   `NKECS/World/NkWorld.h`), qui bloquaient toute compilation de la lib
   `Noge` ; correction minimale (2 lignes `#include`) pour débloquer la
   preuve d'exécution de CE jalon, sans toucher à la logique IK/animation
   (chantier d'un autre agent, item G1.4).
3. ✅ **FAIT PARTIEL (2026-07-23) — Adaptateurs pipeline d'assets** :
   `Noge/IO/{NkOBJIO,NkGLTFIO,NkFBXImporter}.h/.cpp` réécrits en fins
   adaptateurs sur les loaders CPU réels de NKRenderer
   (`renderer::LoadOBJ/LoadGLTF/LoadFBX`, zéro nouvelle logique de parsing).
   `Noge/IO/NkSVGIO.h` initialement **non adapté** — dépendait de
   `NkVectorDocument` (0 % de code, 4 fichiers Design/Doc sans aucun `.cpp`),
   jugé hors scope d'un adaptateur "fin" (voir détail + décision honnête dans
   la section « Incrément Phase G1 — Adaptateurs pipeline d'assets »
   ci-dessous). **SVG import minimal ajouté a posteriori (2026-07-24)** :
   seul le sous-ensemble de construction nécessaire a été implémenté
   (`MoveTo/LineTo/Close` + `AddArtboard/AddLayer/AddPath` +
   `NkSVGIO::Import` sur le codec réel `NkSVGCodec`), prouvé par exécution
   réelle `Applications/NkSVGImportDemo` → **24 OK / 0 FAIL**, exit code 0
   (`jenga build --target NkSVGImportDemo --config Debug --platform Windows`
   → `36/36 projets, SUCCESS`) ; le reste de Design/Doc reste en specs sans
   corps (limites documentées dans `NkSVGIO.h/.cpp`). **Jalon validé par
   exécution réelle** : `Applications/NkAssetIODemo` (même contournement
   `jenga test` bloqué que `NkEditableMeshDemo`/`NkAgentEcsDemo`) importe
   `.obj`/`.gltf`/`.glb`/`.fbx` réels du repo (`Resources/Models/...`) via
   l'API Noge et compare par assertions au chemin de chargement CPU direct
   (`renderer::LoadOBJ/LoadGLTF/LoadFBX` — celui que `NkMeshSystem::Import`
   appelle en interne avant l'upload GPU passthrough, voir section dédiée) :
   **53 OK / 0 FAIL**, code de sortie 0. Compilé et exécuté pour de vrai
   (`jenga build --target NkAssetIODemo --config Debug --platform Windows` →
   `34/34 projets, SUCCESS` ; `Build/Bin/Debug-Windows/NkAssetIODemo/
   NkAssetIODemo.exe`).
4. ✅ **FAIT (2026-07-23/24) — Pont animation (IK + locomotion)** :
   `Engine/Noge/src/Noge/Rigging/NkIKSolver.h/.cpp` réécrit en adaptateur fin
   sur `renderer::NkIKSystem` (`Kernel/Runtime/NKRenderer/.../Tools/IK/
   NkIKSystem.h/.cpp`, réel, déjà utilisé par `DemoIK.cpp`/`DemoIKChar.cpp`/
   `DemoAnimIK.cpp`) : `Chain`/`TwoBoneChain`/`SplineChain` (API publique
   conservée) sont convertis en `NkIKChainDesc` + `renderer::NkIKSolver`
   enum et résolus via `NkIKSystem::SolveRig` — **zéro réimplémentation**
   locale de FABRIK/CCD/TwoBone. `BuildWorldPose`/`WriteBackChain` ne font
   QUE du marshalling FK local↔monde entre `ecs::NkSkeleton` et les matrices
   monde attendues par `NkIKRig::SetWorldPose` (même esprit que le round-trip
   `ToPolygons`/`BuildFromPolygons` de `NkEditableMesh`). `SolveSpline`
   délègue réellement à `NK_SPLINE`, qui reste un **stub côté NKRenderer**
   (`SolveChain_Spline` ne fait rien, `NkIKSystem.cpp:438`) — pas une
   régression introduite ici, juste une délégation honnête vers un chemin pas
   encore implémenté en amont. `TwoBoneChain::stretchable/maxStretch` et les
   contraintes par-os (`Chain::constraints`) ne sont PAS portées (aucun
   équivalent consommé côté backend, `ApplyConstraint()` est lui aussi un
   stub amont) — documenté, pas réimplémenté localement.
   `Engine/Noge/src/Noge/Anim/NkLocomotion.h/.cpp` : `NkFootIKSystem::Execute`
   délègue la jambe (hanche→genou→pied) à `NkIKSolver::SolveTwoBone` ;
   `NkLocomotionSystem` possède un `renderer::NkBlendTree1D` RÉEL
   (`ConfigureBlend(walk, run)` + `SetParameter`/`Update`) qui mélange deux
   clips — zéro blend réimplémenté. Limitation assumée et documentée : la
   pose écrite dans `ecs::NkSkeleton` (blend ET FK d'entrée de l'IK) suppose
   un squelette **PLAT** (`bones[i].parent == -1`) — pas de re-FK
   hiérarchique des descendants après l'IK/le blend. Le raycast sol de
   `NkFootIKSystem` délègue à `physics::NkPhysicsWorld::Raycast` (réel) si
   `SetPhysicsWorld()` est appelé, sinon repli plan-plat honnête (pas de
   raycast contre un mesh de terrain réimplémenté). `NkMotionMatchSystem` /
   `NkCrowdSystem` (Motion Matching, foule) **restent hors-scope** : specs
   déclarées, toujours 0 `.cpp`, non instanciées ailleurs dans le moteur
   (confirmé par grep) — aucune régression, chantiers à part entière non
   demandés par ce jalon.
   `Anim2D/NkTween.h` / `Anim2D/NkAtlas2D.h` : **vérifiés, laissés en spec**
   après recherche d'une fondation réelle équivalente. `NkAtlas2D` : aucune
   fondation nulle part dans le moteur (NKCanvas `NkSprite` = un seul
   rectangle de découpe, pas un atlas à frames nommées + clips ; aucun
   parseur TexturePacker/LibGDX) — hors-scope assumé, seul un include cassé
   corrigé (`"NKRenderer/src/Core/..."` → `"NKRenderer/Core/..."`, le
   fichier `src/Core/...` n'existe pas). `NkTween` : fondation **partielle**
   trouvée — `nkui::NkUIAnimator`/`NkUIEasing` (`Kernel/Runtime/NKUI/.../
   NkUIAnimation.h/.cpp`, réel, ~24 easings + pool de tweens float par id,
   utilisé par des démos NKUI réelles) — mais ne couvre ni Vec3/Vec4, ni
   séquences, ni callbacks, ni Kill/Complete : adosser `NkTween` dessus
   demande de fusionner deux modèles d'API différents (OOP par objet vs pool
   par id), documenté comme piste pour une passe dédiée plutôt que réécrit à
   la hâte.
   **Jalon validé par exécution réelle** : nouvelle appli console
   `Applications/NkLocomotionDemo` (même contournement `jenga test` bloqué
   que `NkEditableMeshDemo`/`NkAudioECSDemo`/`NkAgentEcsDemo`) — squelette
   plat procédural 9 os (hip/thighs/calfs/feet/toes), clips Walk/Run
   procéduraux (`NkAnimationClip::MakeProceduralWalk`, réel, zéro asset
   externe), `NkLocomotionSystem` + `NkFootIKSystem` exécutés sur une entité
   `ecs::NkWorld` réelle pendant 240 frames (balayage vitesse lente→rapide).
   Compilé et EXÉCUTÉ pour de vrai (`jenga build --target NkLocomotionDemo
   --config Debug` → `34/34` projets, succès, 8 warnings bénins déjà
   présents ailleurs dans `Noge` — 0 nouveau warning introduit par ce
   chantier) → sortie réelle (`Build/Bin/Debug-Windows/NkLocomotionDemo/
   NkLocomotionDemo.exe`) : `=== Resultat : 9 OK / 0 FAIL ===`, exit code 0.
   Preuves concrètes dans la sortie : le paramètre de blend suit la vitesse
   (≈0 en phase lente, >0.5 en phase rapide) ET la pose (bone[1]) DIFFÈRE
   entre Walk et Run (pas un stub qui renverrait toujours la même pose) ;
   l'IK a abaissé le pied jusqu'à Y=0.0200 = groundY(0)+footHeight(0.02)
   EXACTEMENT — convergence réelle du solveur TwoBone, pas une valeur figée.
   **Bug pré-existant débusqué et contourné (hors-scope, non corrigé à la
   racine)** : `ecs::NkEntityBuilder::With<T>(T&&)` (`NKECS/World/
   NkWorld.h:372`) a une résolution de surcharge cassée pour les lvalues
   nommées (la forwarding reference déduit `T` comme référence, ce qui casse
   l'appel explicite `Add<T>` en aval) — contourné dans la démo via
   `world.CreateEntity()` + `world.Add<T>(id, value)` avec argument de
   template explicite (fixe `T`, écarte la déduction ambiguë) ; **pas corrigé
   dans `NkWorld.h` lui-même** (code NKECS core, hors-scope de ce chantier
   Noge/Anim, cf. le même genre de note laissée par G1.1/G1.2 sur ce fichier).
   Deuxième correctif de config de build (pas de logique) : le `.jenga` de la
   démo ajoute `NKGLSlang`/`NKSPIRVCross` en dépendances directes — `NKSL`
   (`NkShaderConvert.cpp`) référence des symboles `spirv_cross::`/`glslang::`
   que le linker ne tire pas automatiquement d'un simple `dependson("NKSL")`
   transitif (fuite de propagation du système de build jenga, pas un défaut
   du code Noge/Anim ; `NkEditableMeshDemo` n'y était jamais confronté car
   son code ne force jamais le linker à inclure `NkShaderConvert.obj`).
5. ✅ **FAIT (2026-07-24) — Navigation IA (NavMesh + A*), nouveau pilier 15** :
   nouveau module `Kernel/Runtime/NKNavigation` (`NkNavTypes.h`,
   `NkNavMesh.h/.cpp`, umbrella `NKNavigation.h`, zéro-STL, posé sur
   `NKCollision` — même schéma que `NKPhysics`) : `NkNavMesh::Build()` génère
   un maillage de navigation triangulé depuis une géométrie de sol (grille
   régulière filtrée par pente `maxSlopeDeg` + empreinte d'obstacles AABB
   dilatée du rayon de l'agent), sonde la géométrie par un **raycast RÉEL**
   (`collision::NkRayTriangle3D`, zéro réimplémentation locale
   d'intersection rayon/triangle), construit le graphe d'adjacence des
   triangles via une table de hachage d'arêtes (généraliste, valable pour
   n'importe quelle triangulation) ; `NkNavMesh::FindPath()` calcule un
   chemin par **A*** (liste ouverte linéaire, coût = distance
   centroïde-centroïde, heuristique = distance euclidienne au centroïde
   d'arrivée), waypoints = milieux des portails traversés (PAS de lissage
   funnel/string-pulling — limitation v1 documentée dans l'en-tête de
   `NkNavMesh.h`). Ajouté au registre `config/modules.jenga`
   (`NKNavigation`, deps `NKCollision`) et à `Nkentseu.jenga`.
   Pont ECS côté Noge (même schéma que `NkPhysicsSystem`/`NkAgentSystem`) :
   `NkNavAgentComponent` (`Engine/Noge/src/Noge/ECS/Components/AI/`, tampon
   borné de 64 waypoints, zéro allocation dynamique par tick, état
   `Idle/Pathing/Moving/Arrived/Failed`) + `NkNavigationSystem`
   (`Engine/Noge/src/Noge/ECS/Systems/`, groupe `Update`, **POSSÈDE** le
   `nav::NkNavMesh` PARTAGÉ — le composant ne porte que l'état par-agent).
   `NKNavigation` ajouté à `Noge.jenga` (`_INCLUDE_DIRS`/`_DEPS`).
   **Distinct de** `NkAgentComponent`/`NkAgentSystem` (pilier 11, item G1.2 —
   IA de gameplay PAR APPRENTISSAGE via `NKAgent`/`NKRL`) : ici navigation
   CLASSIQUE/déterministe, zéro apprentissage — les deux systèmes coexistent
   sur des entités différentes.
   **Jalon validé par exécution réelle** : `Applications/NkNavCoreDemo`
   (compilé + exécuté, `jenga build --target NkNavCoreDemo --config Debug
   --platform Windows` → `SUCCESS`, exécutable lancé directement, **11 OK /
   0 FAIL**, exit code 0) — teste le CŒUR du système (`NKNavigation` seul,
   zéro dépendance à `Noge`) sur 2 scénarios réels :
     - **Scénario 1** (positif) : sol 20×20 (`cellSize=0.5`), 3 obstacles
       disposés SUR la diagonale départ→arrivée (une ligne droite les
       traverserait tous) → NavMesh réel de **2832 triangles / 1681
       sommets** ; A* trouve un chemin de **82 waypoints**, longueur
       **25,426** contre **22,627** en ligne droite (**detour réel prouvé,
       pas une ligne qui traverse les obstacles**) ; chaque segment du
       chemin vérifié (échantillonnage) hors des 3 obstacles ; une
       simulation manuelle (sans ECS, boucle `pos += dir * speed * dt`)
       suit les waypoints et atteint EXACTEMENT `(8, 0, 8)` en **131 frames
       (4,37 s à 30 fps)** sans jamais entrer dans un obstacle (position
       échantillonnée à chaque frame).
     - **Scénario 2** (négatif, robustesse) : mur solide sans ouverture
       (bande continue sur toute la largeur de la zone) séparant départ et
       arrivée en deux îlots déconnectés du graphe d'adjacence → `FindPath`
       renvoie correctement `NkPathStatus::Unreachable` (pas un chemin
       erroné ni un crash) — preuve que l'échec est DÉTECTÉ, pas juste
       absent de test.
   **Pont ECS validé lui aussi par exécution réelle (2026-07-24, après
   levée d'un blocage externe)** : `Applications/NkNavDemo` (NavMesh
   3-obstacles + entité ECS `NkTransform`+`NkNavAgentComponent` pilotée par
   `NkNavigationSystem::Execute()` tick par tick, 30 fps simulés) — compilé
   (`jenga build --target NkNavDemo --config Debug --platform Windows` →
   `36/36 projets, SUCCESS`) et exécuté : **9 OK / 0 FAIL**, exit code 0.
   Sortie réelle : NavMesh 2832 triangles/1681 sommets (identique à
   `NkNavCoreDemo`), chemin A* trouvé, agent arrivé EXACTEMENT en
   `(8.000, 0.000, 8.000)` (distance au but 0.0000) en **135 frames
   (4,5 s)**, chemin PLANIFIÉ et déplacement RÉEL (position ECS
   échantillonnée à chaque tick) vérifiés hors des 3 obstacles.
   Chronologie honnête du déblocage :
     - L'exécution a d'abord été impossible : `Noge.lib` ne linkait pas à
       cause de 2 fichiers sans rapport avec la navigation
       (`Noge/Doc/NkVectorDocument.cpp`, `Noge/IO/NkSVGIO.cpp` — ambiguïté
       `NkColor`/`NkSpan` dans `Noge/Color/NkColorManager.h` +
       `Noge/Design/Raster/NkRasterCanvas.h`, chantier Design/Doc d'un
       autre agent). D'où la démo découplée `NkNavCoreDemo`, créée pour
       prouver le cœur indépendamment. Le chantier Design/Doc a corrigé
       ses fichiers en parallèle (y compris l'include manquant
       `NKContainers/Views/NkSpan.h` dans `NkRasterCanvas.h` — le même fix
       que celui que ce chantier s'apprêtait à appliquer), après quoi
       `NkNavDemo` a compilé du premier coup.
     - **Bug réel débusqué par la première exécution de `NkNavDemo`
       (8 OK / 1 FAIL)** : le chemin A* de 82 waypoints était TRONQUÉ
       silencieusement par le tampon borné `kMaxWaypoints = 64` du
       composant → l'agent suivait les 64 premiers waypoints puis déclarait
       `Arrived` à ~5 unités du but (`(6.5, 0, 3.25)`, distance 4,98).
       Corrigé dans `NkNavigationSystem.cpp` (`StepEntity`) : la fin du
       tampon ne vaut `Arrived` que si la destination est RÉELLEMENT à
       portée (`arriveRadius`) ; sinon `needsRepath = true` → replan
       depuis la position courante (le chemin restant raccourcit à chaque
       replan, convergence garantie). C'est aussi l'implémentation concrète
       du « replanifie si besoin » demandé par le jalon — prouvée par la
       seconde exécution : le log montre un chemin final restocké de 20
       waypoints (le dernier segment replanifié) et l'arrivée exacte.
       `NkNavCoreDemo` re-exécuté après le fix : toujours 11 OK / 0 FAIL.

**Phase G2 — CPU-only**
1. ✅ **FAIT (2026-07-24) — Système ECS UI in-game (HUD)** : premier système
   réel derrière `ECS/Components/UI/NkUIComponent.h` (composants inchangés,
   jusqu'ici données sans aucun consommateur). `NkUISystem`
   (`Engine/Noge/src/Noge/ECS/Systems/NkUISystem.h/.cpp`, groupe `Render`)
   structuré en **3 étages découplés** (décision Rihen du jour, vérifiée dans
   le code : NKCanvas a ses propres backends/devices, PAS de dépendance NKRHI
   — cf. `NKCanvas/Renderer/Resources/NkShader.h` — donc dans une fenêtre de
   jeu possédée par NKRHI/NKRenderer, deux devices sur la même fenêtre =
   conflit de swapchain ; le HUD de production passera par le rendu 2D DÉJÀ
   RÉEL de NKRenderer, `Tools/Render2D/NkRender2D` — même device que la scène
   3D) :
     (a) le SYSTÈME fait le layout (anchor/pivot/anchoredPos/sizeDelta →
     `rectX/Y/W/H` écrits dans `NkRectTransform`, rôle du "NkUILayoutSystem"
     documenté par le composant mais inexistant) et produit une **liste de
     primitives abstraite** `NkUIDrawCmd` (rects colorés + lignes de texte à
     la baseline) — zéro dépendance à un moteur de dessin ;
     (b) une interface `NkUIDrawBackend` (Begin/DrawRect/DrawTextLine +
     métriques texte) consommée par un replay backend-agnostique ;
     (c) `NkUISoftwareCanvasBackend`, l'implémentation d'aujourd'hui, 100 %
     HEADLESS : primitives réelles du backend **Software** de NKCanvas
     (`NkSoftwareFramebuffer`, header-inline, rendu en mémoire CPU —
     `NkSoftwareContext` complet exige une NkWindow native, donc PAS
     headless) + texte réel via l'atlas CPU NKFont (police embarquée
     ProggyClean 13 px, `NkFontEmbedded::AddDefaultFont` + blit alpha8,
     même convention baseline que `NkGuiDrawList::AddText`). AUCUNE fenêtre,
     AUCUN device GPU (contrainte matérielle : extinctions thermiques sur
     pics GPU). Le futur backend `NkRender2D` implémentera (b) sans toucher
     au système. Widgets rendus : `NkUIPanel` (fond+bordure), `NkUIImage`
     (rect de couleur, type `Filled` horizontal/vertical), `NkUIProgressBar`
     (fond+remplissage, animation `animSpeed*dt`), `NkUIText` (alignements
     H/V, troncature) ; référentiel = premier `NkCanvas` ScreenSpace visible
     (referenceWidth/Height → échelle framebuffer, `visible=false` masque
     tout le HUD). **Jalon validé par exécution réelle** :
     `Applications/NkUIHudDemo` (même contournement `jenga test` bloqué que
     les autres démos, enregistrée dans `Nkentseu.jenga`), build
     `jenga build --target NkUIHudDemo --config Debug --platform Windows` →
     36/36 projets SUCCESS, exécution → **29 OK / 0 FAIL, exit 0** (stable
     depuis la racine du dépôt ET depuis `Build/Bin/.../NkUIHudDemo/` —
     police embarquée, zéro ressource externe) : rectangle de score ROUGE
     vérifié pixel par pixel aux coordonnées calculées par le layout
     (`GetPixel(40,20)==(255,0,0)`, coins inclus, extérieur = fond), jauge de
     vie 50 % (vert jusqu'à x<60, gris après), panel fond+bordure blanche
     2 px, ancrage MiddleCenter (carré 40×40 centré exactement en (160,90)),
     `visible=false` (widget et canvas) ne dessine rien, texte "SCORE 42" =
     119 pixels blancs purs / 219 encrés dans le rect (glyphes ProggyClean
     réellement blittés), liste de primitives vérifiée (9 Rect + 1 Text), et
     capture PNG du framebuffer via NKImage (`NkUIHudDemo_hud.png`, inspectée
     visuellement : conforme). **Limites honnêtes (documentées dans
     `NkUISystem.h`)** : pas de hiérarchie de rects ni localScale/rotation,
     un seul canvas, `NkUIImage.textureId`/9-slice/Tiled/Radial ignorés
     (rect de couleur unie), texte à la taille native de l'atlas
     (`fontSize`/bold/italic/outline/shadow ignorés, une seule ligne),
     `showLabel` des jauges non rendu, aucune entrée souris/clavier
     (boutons/sliders/toggles = données seulement), pas de z-order par
     widget. L'ancien jalon "sprite HUD" est couvert en version rect coloré +
     texte — le blit de vraie texture arrive avec le pipeline d'assets.
2. **Cross-compilation Android validée pour Noge** : plomberie jenga déjà
   présente (`Noge.jenga` filtres Android déjà là), suivre le pattern déjà
   prouvé par `NKAudio.jenga`. **Jalon** : `jenga build --target Noge
   --platform Android` réussit (`.a`/`.so` généré), même sans device réel
   pour valider l'exécution (ça viendra avec un jeu complet à déployer).
3. ✅ **Scripting — C++ hot-reload DLL** — **FAIT (2026-07-24)** :
   `NkScriptBridge.cpp` implémenté (LoadLibrary/dlopen réels, shadow copy
   Windows, hooks NKMemory injectés dans la DLL via `nkecs_set_allocator`,
   violations zéro-STL corrigées dans la macro ET dans
   `NkScriptComponent.h`), ABI C autonome `NkScriptABI.h`. **Jalon atteint**
   par `Applications/NkHotReloadDemo` (16 assertions OK / 0 échec) :
   compteur à 5 préservé à travers un reload réel v1(+1)→v2(+2), puis 11
   après 3 ticks — état conservé ET nouveau comportement actif. Détails +
   reste à faire : section « Quintette de scripting » pilier 1 ci-dessus.
4. **Scripting — bridge C# ou Python (le plus rapide des deux à choisir en
   premier)** : embarquer Mono/CoreCLR (C#) ou CPython (Python) — travail
   d'intégration d'un runtime mature, pas de recherche. CPU-only pour
   l'exécution du script lui-même (l'éventuel JIT du runtime tiers peut avoir
   ses propres contraintes, à vérifier au moment de l'intégration).
   **Jalon** : cf. section dédiée ci-dessus.

**Phase G3 — GPU léger (converge avec I3 existant)**
- Rien de nouveau par rapport à I3 (NKImage/NKFont CPU decode). Complément :
  une fois I3 fait, le pont assets de G1.3 peut aussi streamer des textures
  réelles via `NKImage`, pas seulement des meshes.

**Phase G4 — GPU lourd (converge avec I4 existant)**
- Rien de nouveau côté matériaux (déjà I4 : réparer `NkMaterialComponent` +
  premier pont NkSL). Complément : une fois le pipeline matériaux Noge→NkSL→
  NKRenderer fonctionnel, activer le **deferred v2** existant côté Noge
  (déjà validé 4 backends au niveau NKRenderer, jamais exposé par
  `NkRenderSystem`) — un flag `cfg.deferred` à câbler, pas un nouveau moteur.

**Phase G5 — Plus tard (post premier jeu Noge qui tourne bout-en-bout)**
- **Blueprint visuel + graphe matériaux (T.2) + graphe VFX** — les trois
  partagent le même substrat NKGraph (aujourd'hui zéro code, P1-P6 tous ❌) :
  à construire ensemble une fois un premier consommateur démarre (règle des
  « deux consommateurs » déjà posée dans `NKGraph/ROADMAP.md`).
- **Sequencer** : Phase A item 11 (timeline CPU pure) déjà listée ci-dessous,
  puis rendu des tracks (Phase C).
- **LOD auto / scene graph avancé** : Phase P du ROADMAP NKRenderer, déjà
  classée priorité 4 dans son propre document — cohérent, pas de changement.
- **Convergence NKUI/NKGui** : dette UI déjà identifiée par le projet
  lui-même (`Applications/NKCode/ROADMAP.md`, section « Widgets réutilisables »).
- **Migration UI de Nogee vers NkEditorShell + renderer RHI généralisé
  (note 2026-07-24)** : la brique est prête et compile —
  `nkentseu::nkgui::NkEditorRHIRenderer` (`Integrations/NKGui/
  NkEditorRHIRenderer.h`, généralisée depuis NkAnimaEditor qui recompile via
  alias). Côté Nogee, seul le **chemin de config** est posé (`--ui=rhi|nkui`,
  `NogeeUiBackend` dans `Applications/Nogee/src/Nogee/UkConfig.h` + repli
  loggé dans `Nogee.cpp` ; NKUI legacy reste le défaut, rien n'est supprimé).
  **Écart constaté honnêtement** : Nogee n'utilise pas NkEditorShell (shell
  maison NkApplication + LayerStack + UILayer NKUI) — le câblage réel (modèle
  `NkAnimaEditor/main.cpp` : `cfg.renderer = &rhi` + `SetPreUI`) reste à faire.
  **Réhabilitation Nogee (2026-07-24, faite)** : la cible `Nogee` (Windows
  Debug) est passée de **68 erreurs / 15 fichiers → build VERT (0 erreur,
  link OK, `Nogee.exe` produit)**. Corrections : alignement sur le framework
  réel d'Engine/Noge (`Noge/Core/NkApplication.h`, `NkLayer`/`NkOverlay`,
  entrée `nkmain` + `NkAppData` — l'ancien couple `Application`/
  `CreateApplication` n'existait pas) ; namespaces unifiés (`noge` au lieu du
  mélange `Noge`/`noge`) ; APIs kernel réelles (NkVector::Erase,
  NkHashMap::Find→pointeur, NkString::Empty/SubStr, NkImage instance,
  NkDirectory statique, NkAttachmentDesc/NkFramebufferDesc, NkQuery ECS,
  composants `ecs::NkName`/`NkTransform`/`NkSceneNode`) ; NKUI réel
  (`nkui::NkRect`, `fontManager.Default()`, signatures widgets) ; événements
  réels (`HasCtrl`, `NkMouseButtonPress/ReleaseEvent`, `NkTextInputEvent`,
  `NK_ENTER`/`NK_LCTRL`/`NK_BACK`) ; sélection notifiée par callback (le bus
  `NkEventBus` reste réservé aux NkEvent) ; `Nogee.jenga` complété (dép.
  `Noge`+fermeture transitive NKECS/NKRenderer/NKSerialization/…, link
  `winmm`/`avrt`/`ws2_32`). Ajouts génériques côté framework : ctor
  `NkApplicationConfig(const NkEntryState&)` (le struct était inconstructible
  sous Windows, `NkEntryState` n'y ayant pas de ctor par défaut) et helper
  `NkStrNCpy` dans `Noge/ECS/NkEcsUtil.h` (déjà référencé par
  `NkSequencer.h`). Le rendu scène du `ViewportLayer` est un clear FBO
  (brancher `GetRenderer()`/NkRenderSystem à la reprise) et le flag `--ui=rhi`
  logge toujours son repli NKUI. Validation visuelle du rendu RHI dans Nogee
  reportée pour contrainte matérielle (extinctions sur pics GPU —
  aucune app fenêtrée exécutée dans cette passe ; jalon = compilation).
  **Validation exécution Nogee (2026-07-25, faite)** : le crash au démarrage
  constaté par Rihen le 24 au soir (log muet après « [EditorLayer] Systemes
  initialises », access violation 0xc0000005, fault offset 0xd1a3) est
  **corrigé et prouvé au débogueur**. Cause racine (gdb : SIGSEGV à
  `ViewportLayer::OnAttach`, `mDevice = 0xbaadf000`) : **violation ODR de
  layout** — `NkWindowData::mDmScreen` était déclaré `DEVMODE`, macro Win32
  qui vaut `DEVMODEW` (220 o) avec `UNICODE` (Nogee, NKWindow) et `DEVMODEA`
  (156 o) sans (Engine/Noge) → les membres de `NkApplication` après `mWindow`
  étaient décalés de 64 octets selon la TU ; le `GetDevice()` inline gardé
  par le linker lisait `mDevice` à `this+0x880` alors que `InitDevice()`
  l'écrivait à `this+0x840` → pointeur poubelle (crash aléatoire : null les
  jours de chance → device silencieusement absent des layers, FBO jamais
  créé). Fix : `DEVMODEW` explicite dans `NkWin32Window.h/.cpp` (layout
  indépendant des macros de la TU incluante). Corollaires corrigés :
  suppression du **double OnAttach** de tous les layers
  (`NkApplication::Init` re-parcourait la stack alors que `PushLayer`
  attache déjà — double init NkUIContext + double FBO) et garde `mUIReady`
  dans `UILayer` (si `NkUIContext::Init` échoue, `OnUIRender` ne déréférence
  plus `fontManager.Default()` nul). **Démarrage mesuré (Debug, runs bornés,
  cache shaders en place)** : lancement → boucle principale **0,41 s**
  (device 192 ms, NKRenderer init + 51 shaders 173 ms, layers 17 ms) ;
  cache shaders froid : **0,52 s** (le cache disque `.nksc` de NkSL marche
  déjà, la compilation n'a jamais été le poste dominant). Le vrai poste
  dominant après le fix du device était l'**AssetBrowser** : chargement
  synchrone de TOUTES les textures du dossier courant au boot (≈ 3,8 s sur
  un dossier riche en images) → **thumbnails paresseux** (génération au
  premier rendu visible, budget 2/frame, `AssetBrowser.h/.cpp`). Bilan :
  boot ≈ 4,2 s → **0,41 s (~10×)**. L'exe tourne 25 s sans crash (fenêtre,
  render graph 19 passes, 1600×900), y compris sous gdb (debug heap, où le
  crash était 100 % reproductible avant le fix).
- **Terrain** : uniquement si un jeu du portfolio en exprime un jour le besoin
  explicite (cf. « Hors scope assumé »).
- **Export packagé complet** (APK signé, bundle HAP, wasm+html) au-delà du
  simple « ça compile » de G2.2.

**Phase G6 — Ambitions long terme (R&D, après un premier jeu Noge complet)**

Nouvelle phase (2026-07-23, correction de Rihen). Regroupe les items des
sections « 🔭 Ambitions long terme assumées » et « 🗣️ Quintette de scripting »
ci-dessus qui ne sont **pas** déjà couverts par G1-G5 — c'est-à-dire tout ce
qui est un vrai chantier R&D pluriannuel, pas une intégration de fondation
existante :

1. **GI voxel from-scratch avec radiance** (au-delà de l'occlusion pure de
   `NkVoxelAOSystem`) — premier jalon : une scène simple avec color-bleeding
   voxel, cf. section dédiée ci-dessus. Zéro ray-tracing matériel propriétaire.
2. **Géométrie virtualisée type Nanite** (clustering/meshlet + culling
   GPU-driven + streaming) — au-delà du simple LOD discret (celui-ci est déjà
   en G2/G3 via `NkCullingSystem`, cf. ci-dessus). Aucune fondation réelle
   aujourd'hui, chantier le plus long des trois ambitions rendu.
3. **Rendu peau SSS avancé** pour les humains haute-fidélité (au-delà du rig
   FACS/blendshape déjà réel de `NkFaceController`) — premier jalon : un
   visage stylisé déjà existant avec un premier shader SSS crédible.
4. **Langage de scripting from-scratch généraliste** (proposition de nom
   NkPlay, cf. section dédiée) — le plus long des 5 options de scripting,
   nécessite un compilateur + VM entiers. Ne réutilise pas `Spark/` (NkSpark,
   embarqué bare-metal, chantier séparé, 0 code aujourd'hui) mais peut
   s'inspirer de l'expérience front-end de `NKSL` (compilateur shader réel et
   prouvé en production).
5. **Script visuel multi-consommateur (NKGraph)** — cœur du graphe (P1-P3) +
   canvas d'édition (P4, 0 % réel aujourd'hui, vérifié) + premier consommateur
   (graphe matériaux recommandé, backend NkSL déjà prouvé — cf. avis
   technique détaillé ci-dessus), puis Blueprint gameplay/VFX/procédural en
   réutilisation. Recoupe partiellement le Blueprint visuel déjà noté G5
   ci-dessus (même préalable NKGraph) — les deux items ne sont pas dupliqués,
   G5 couvrait déjà « Blueprint visuel + graphe matériaux + graphe VFX »
   comme un seul chantier partageant NKGraph ; G6 ne fait qu'ajouter la
   précision multi-consommateur explicite et le procédural (Kernel/AI) à la
   liste des consommateurs.

Aucun de ces 5 items n'a de date cible — le premier jalon honnête de chacun
(défini dans les sections dédiées ci-dessus) est la seule chose engagée
aujourd'hui, pas un calendrier.

---

## 🎮 ROADMAP D'INTÉGRATION — Noge vers moteur complet

Cette section répond à une question différente de l'audit interne ci-dessous
(« quels fichiers Noge sont réels vs stubs ») : **pour chacun des 8 gros
sous-systèmes du moteur (`Kernel/Runtime/*`, `Kernel/System/NKNetwork`), Noge
est-il réellement branché dessus, ou déconnecté/dupliqué ?** Établie par
lecture de `Noge.jenga` (dépendances de build) + `grep` massif des `#include`
et usages réels de types dans `Engine/Noge/src/Noge/` (pas de nouvelle lecture
fichier par fichier de l'audit déjà fait le même jour).

**Correction d'une prémisse** : la mission de départ supposait que « NKECS
lui-même n'est pas câblé dans le workspace racine ». Vérifié faux à ce jour :
`git diff HEAD -- Nkentseu.jenga` (état non commité de la session) ne touche
**pas** la section NKECS — `with include("Kernel/Runtime/NKECS/NKECS.jenga")`
(ligne ~1200) est déjà présent dans `HEAD`, avec le commentaire *« DOIT etre
enregistre AVANT Noge »* déjà en place, à la bonne position (avant `Engine/
Noge/Noge.jenga` ligne ~1240). NKECS est un projet workspace normal, buildable
indépendamment. Si ce point a été vrai un jour, il a été corrigé **avant**
cette session — il n'y a rien à faire ici. Toute la suite de cette section
part de l'état réel constaté aujourd'hui, pas de la prémisse.

### Tableau de synthèse — sous-système × branchement réel

| Sous-système | Dans `Noge.jenga` (dependson/includedirs) ? | Utilisé réellement dans `Engine/Noge/src` ? | État | Dépend de |
|---|---|---|---|---|
| **NKECS** | ✅ oui | ✅ oui — `NkGameObject` = handle `NkEntityId`+`NkWorld*` (16 o), `NkEngineLayer` pilote `ecs::NkScheduler`/`NkWorld` réels | ✅ **branché** | rien (fondation) |
| **NKCollision/NKPhysics** | ✅ oui | ✅ oui — `NkPhysicsSystem.cpp` réel : `NKPhysics::NkPhysicsWorld`/`NkRigidBody` + `NKCollision::NkColShapes`, convertit `NkCollider3D`→`collision::NkShape`, crée les corps | ✅ **branché** (ECS gameplay) · 🔌 partiel (module design `Physics/NkPhysicsMesh.h` = cloth/hair/ragdoll/jiggle, spec seule, sans rapport avec ce qui précède) | NKECS |
| **NKRenderer** | ✅ oui | 🔌 partiel — `NkRenderSystem.cpp` réel pour les **meshes** (`NkMeshSystem::Import` lazy-load, `NkRender3D::Submit/Flush` réels) mais **cassé pour les matériaux** : `NkMaterialComponent::SetMaterial()` met toujours `materialHandle = 0` et **rien nulle part ne le résout** (pas d'équivalent au `meshSys->Import()` du mesh) → toute draw call part avec un `NkMatInstHandle` invalide | 🔌 **partiel** (meshes oui, matériaux non) | NKECS, NKRHI (device créé dès `NkApplication::InitDevice()`, donc 🎮 dès le boot) |
| **NkSL** | ✅ oui (`includedirs`/`dependson`, jamais utilisé — dépendance morte) | ❌ **zéro** `#include` NkSL/NKSL dans tout `Engine/Noge/src` (`grep` vide) | ❌ **non branché** | NKRenderer (matériaux) |
| **NKImage** | ✅ oui (dépendance morte, idem NkSL) | ❌ **zéro** `#include` NKImage dans tout `Engine/Noge/src` | ❌ **non branché** | rien (peut être fait seul, CPU) |
| **NKFont** | ✅ oui (dépendance morte, idem NkSL) | ❌ **zéro** `#include` NKFont ; `Text/NkTextPath.h` est un module *vector text path* (SVG-like), sans rapport conceptuel avec le rendu de glyphes | ❌ **non branché** | rien (peut être fait seul, CPU) |
| **NKNetwork** | ✅ oui (ajouté 2026-07-24, Phase I1) | ✅ oui — `ECS/Replication/NkNetWorld.h/.cpp` = adaptateur mince réel (`NkNetSystem` + `ecs::NkNetEntity`) sur `net::NkNetWorld`, prouvé par `NkNetWorldDemo` (18/18 OK, loopback réel) | ✅ **branché** (Phase I1, 2026-07-24) | NKECS |

### Découverte majeure — NKNetwork a déjà une implémentation réelle à adapter, pas à réécrire

`Kernel/System/NKNetwork/src/NKNetwork/Replication/NkNetWorld.{h,cpp}`
(609 + 634 lignes, daté 2026-07-12, **pas** un stub) implémente déjà tout le
protocole de réplication server-authoritative : snapshots delta/keyframe,
`NkNetInterpolator`, `SendInput`, sérialisation bit-précise via
`NkBitStream`. Son en-tête le dit explicitement :

> *« NKNetwork (couche System) ne connaît PAS NKECS (couche Runtime) : la
> réplication fonctionne par ENREGISTREMENT d'objets applicatifs... Le pont
> vers l'ECS... se fait dans les couches supérieures (Noge), **exactement
> comme pour NKPhysics**. »*

C'est très exactement le même schéma que l'incrément `NkEditableMesh` /
`renderer::NkEditMesh` fait plus tôt aujourd'hui, et le même schéma déjà
appliqué avec succès pour `NkPhysicsSystem` (adapter ECS mince par-dessus
NKPhysics, zéro réimplémentation). **`ECS/Replication/NkNetWorld.h` de Noge
ne doit donc pas être implémenté comme une nouvelle spec** — il doit devenir
un adaptateur fin (`NkNetEntity` composant + `NkNetSystem`) qui enregistre/
désenregistre les entités ECS auprès de `net::NkNetWorld` (déjà écrit,
déjà réel) et appelle son `Update(dt)` chaque tick, sur le modèle exact de
`NkPhysicsSystem`.

---

### Phases d'intégration séquencées (contrainte : GPU occupé par l'entraînement NKAI)

**Phase I1 — CPU-only, immédiat — ✅ FAIT (2026-07-24)**

1. ✅ **NKNetwork — câblage + adaptateur ECS** (était le seul des 8
   sous-systèmes totalement absent du build Noge) — **livré et prouvé par
   exécution réelle** :
   - ✅ `NKNetwork` ajouté à `includedirs`/`dependson` de
     `Engine/Noge/Noge.jenga` (même schéma que les autres modules).
   - ✅ `ECS/Replication/NkNetWorld.h` (ancienne spec aux includes cassés
     `"Protocol/..."`) **réécrit entièrement** comme adaptateur mince sur
     `net::NkNetWorld` réel + nouveau `.cpp` (~240 lignes) : classe
     `nkentseu::NkNetSystem` (ecs::NkSystem, PostUpdate) qui POSSÈDE un
     `net::NkNetWorld`, enregistre/désenregistre les entités ECS
     (`RegisterEntity(id, prefabId, authority)` -> `AllocateNetId` +
     writeState/readState par défaut = NkTransform position+rotation via
     `Binding{world, entity}` à adresse stable NKMemory), spawn/despawn
     distants via les callbacks `onEntitySpawn`/`onEntityDespawn` du
     protocole réel, `Execute()` = `Update(dt)` + `DrainAll`/`HandleMessage`.
     Nouveau composant ECS `ecs::NkNetEntity`
     (`ECS/Components/Network/NkNetComponents.h`, données pures, types
     réseau réels `net::NkNetId`/`NkPeerId`/`NkNetAuthority`). ZÉRO
     réimplémentation du protocole.
   - ✅ **Jalon testable ATTEINT** : `Applications/NkNetWorldDemo` (même
     contournement policy que `NkEditableMeshDemo`) compilé + exécuté —
     **18/18 assertions OK, 0 FAIL** : handshake réel 127.0.0.1:48521,
     2 entités ECS enregistrées via `NkNetSystem` côté serveur, spawn
     automatique côté client, **convergence des positions client == serveur
     après snapshot** (1,2,3) et (-5,0.5,9), re-convergence après delta
     (42,-8,100), despawn répliqué détruit l'entité ECS cliente. Zéro GPU.
   - Pièges réels rencontrés (documentés dans les sources) : (a)
     `net::NkNetEntity` vs `ecs::NkNetEntity` = même nom → jamais de
     `using namespace net` dans les fichiers du pont ; (b) l'umbrella
     `NKNetwork/NKNetwork.h` injecte `nkentseu::NkNetSystem`/`NkNetEntity`
     (alias) qui collisionnent avec l'adaptateur Noge → inclure les headers
     précis, pas l'umbrella, dans tout code utilisant le pont ; (c)
     `NkFunction` ne prend PAS un pointeur de fonction brut (résolution de
     surcharge piégée par le ctor multi-binding `T*`) → toujours passer une
     lambda à `writeState`/`readState`.
2. ✅ **Nettoyage des doublons morts** — supprimés le 2026-07-24 :
   `Core/NkEngineLayer.h` (v1), `ECS/Components/Rendering/NkRenderer.h`,
   `ECS/Components/Physics/NkPhysicsComponents.h` (+ retrait des 2 derniers
   `#include` résiduels dans `ECS/Entities/NkActor.h` et `Nkentseu.h` —
   l'audit disait « aucun fichier ne les inclut », c'était vrai pour les 2
   premiers, pas pour `NkPhysicsComponents.h`). **Jalon ATTEINT** :
   `Noge.lib` recompile 0 erreur après suppression (build 34/34 SUCCESS,
   2026-07-24).

**Phase I2 — CPU-only**

- **NKCollision/NKPhysics sur l'ECS gameplay : déjà fait**, en avance sur la
  prémisse de la mission — `NkPhysicsSystem` est réel et branché (cf. tableau
  ci-dessus). Rien à planifier ici pour la partie rigid-body/collision.
  Le vrai reste à faire est **ailleurs** : le module design
  `Physics/NkPhysicsMesh.h` (jiggle-bone/ragdoll/mocap = CPU pur, à faire ici
  ; cloth/hair/soft-body = compute shaders, Phase I4) — déjà tracké dans la
  section *Phase A* de l'audit ci-dessous, pas dupliqué ici.
- **Jalon testable** : identique à celui déjà prévu Phase A de l'audit pour
  `NkJiggleBoneSystem` — pas de nouveau jalon à inventer, ce point est déjà
  couvert par la section existante.

**Phase I3 — GPU léger (quand un peu de marge existe)**

1. **NKImage — chargement texture CPU-only d'abord** : un type ressource
   `NkTextureAsset` côté Noge qui appelle `NKImage` pour décoder un fichier en
   buffer de pixels CPU (largeur/hauteur/canaux), **sans** toucher
   `NKRHI::CreateTexture`. **Jalon testable** : démo console qui décode un
   PNG/JPEG connu et vérifie dimensions + quelques pixels attendus par
   assertions — zéro device GPU ouvert. L'upload GPU final (`CreateTexture`)
   reste une étape séparée, à faire seulement quand le GPU est libre (ou en
   fenêtre de marge courte, d'où Phase "légère").
2. **NKFont — rasterisation glyphe/atlas CPU-only d'abord**, même logique :
   `NKFont` produit des bitmaps de glyphes et un layout d'atlas en mémoire
   CPU ; l'upload de la texture d'atlas est différé. **Jalon testable** :
   démo console qui rasterise une police connue, vérifie la métrique d'un
   glyphe (avance, bbox) par assertions.

**Phase I4 — GPU lourd (rendu réel, quand le GPU est libre)**

1. **NKRenderer — réparer la résolution de matériau** : ajouter dans
   `NkMaterialComponent`/`NkRenderSystem` un lazy-resolve
   `matSys->Import(mat.slots[slot].materialPath)` symétrique à celui déjà
   fait pour les meshes (`meshSys->Import(mesh.meshPath)`), pour que
   `NkMatInstHandle` cesse d'être toujours `0`.
2. **NkSL — premier point de contact réel** : construire le pont
   matériau Noge → programme NkSL compilé → instance matériau NKRenderer
   (aujourd'hui : zéro usage de NkSL dans Noge, dépendance de build morte).
3. **Jalon testable (fin de la chaîne complète)** : `NkApplication` démarre
   une fenêtre réelle, une entité avec mesh + matériau texturé (chargé via
   NKImage, Phase I3) + shader NkSL s'affiche à l'écran — premier test
   end-to-end GPU réel de la chaîne Noge → NKRenderer → NkSL → NKImage.
4. `Viewport/NkGizmo.h`, `NkSelectionBuffer.h`, `Sculpt/NkSculpting.h`,
   `Facial/NkSkinMaterial.h`, VFX graphe/simulation GPU, pipeline FBO
   `ViewportLayer` : déjà trackés Phase C de l'audit ci-dessous, non
   dupliqués ici.

---

## Correctifs de cet audit (2026-07-23)

Avant de lire la suite, deux points structurants :

1. **`Noge.lib` compile avec succès aujourd'hui** (18 `.cpp`, 0 erreur, 8
   avertissements bénins). Les bugs bloquants documentés dans l'audit
   précédent (2026-05-26) — double définition de `Run()`, typo
   `NkkLambdaStorage` dans `NkEventBus::Subscribe` — **sont déjà corrigés**
   dans le code actuel (vérifié en lisant `NkApplication.cpp`/`NkEventBus.h`
   ligne à ligne, puis confirmé par une compilation réelle). Ne pas re-traiter
   ces items.
2. **Le docx/pdf externe `Nkentseu_Roadmap_Complete.{docx,pdf}`** (avril 2026,
   sections MA1-MA4/D1-D3) affirme que `NkEditableMesh`, `NkGizmo`,
   `NkIKSolver`, `NkTween`, `NkAtlas2D`, `NkGLTFIO` sont **"PRODUIT"**
   (livrés). C'est **faux pour le code actuel** : aucun de ces fichiers
   n'avait de `.cpp` avant cet audit, et certains ne compilaient même pas
   (includes cassés, cf. plus bas). **Ce document est un plan aspirationnel
   antérieur, à ignorer comme source de vérité sur l'état du code** — n'y
   faire confiance que pour le vocabulaire/les idées de fonctionnalités, pas
   pour le statut. `Nkentseu_ModelingAnimation.{docx,pdf}` (même dossier) est
   dans la même catégorie (non audité en détail, présumé du même ordre).

---

## Synthèse par sous-système (état réel du code, pas des intentions)

Légende : ✅ Livré et prouvé (test réel) · 🔶 Partiel/réel mais non prouvé ·
🩹 Stub honnête (log + no-op, pas de fausse réussite) · ❌ Header de spec
seul, jamais compilé · 🎮 Dépend du GPU (RHI/rendu actif)

| Sous-système | Statut | GPU ? | Notes |
|---|---|---|---|
| Core — NkApplication/LayerStack/EventBus/MainApp | ✅ | 🎮 (Init device) | Boucle réelle, bugs 2026-05-26 corrigés. `NkProfiler` **sans `.cpp`** (Begin/End/GetStats non définis, lien cassé si appelé). |
| Core — NkEngineLayer (`Layers/`) | 🔶 | 🎮 | Orchestration réelle (Scheduler/SceneMgr/RenderSystem) mais **jamais inclus par `Nkentseu.h`** — accessible seulement via include direct. `Core/NkEngineLayer.h` (v1) est un **doublon mort** avec includes cassés (`Nkentseu/Core/Layer.h` inexistant) — à supprimer. |
| ECS Gameplay (GameObject/Actor/Pawn/Character/BehaviourHost) | ✅ | non | Entièrement implémenté (header-inline + `.cpp`), plus mature que l'audit précédent ne le disait. |
| ECS Composants (Core/Rendering/Physics/Audio/Animation/UI/SceneComponent) | 🔶 | non | Données solides. **Duplication** : `Rendering/NkRenderComponents.h` et `Rendering/NkRenderer.h` redéfinissent `NkColor4/NkRect2D/NkLightType/NkBlendMode` à l'identique (le commentaire de `NkRenderComponents.h` dit *"NkRenderer.h est SUPPRIME"* — faux, toujours présent ; pas d'ODR-clash actuel car rien n'inclut `NkRenderer.h`, mais à supprimer). Idem `Physics/NkPhysicsComponents.h` (simple) vs `Physics/NkPhysics.h` (complet, celui réellement utilisé par `NkPhysicsSystem.cpp`) — le premier est mort. |
| ECS Prefab (`NkPrefab`) | 🔶 | non | Instanciation top-level réelle, mais **l'application des composants d'un prefab à l'entité instanciée est un stub documenté** (`NkPrefab.cpp:99-113` — désérialise dans un buffer temporaire puis le jette, TODO explicite : il manque un point d'insertion type-erased dans `NkTypeInfo`/`NkWorld`). |
| ECS Scene (SceneGraph/SceneManager/SceneScript/LifecycleSystem) | ✅ | non | Réel et cohérent ; seul `NkSceneManager::Update()` (animation des transitions Fade) est un TODO explicite (non bloquant). |
| ECS Sérialisation (`NkSceneSerializer`) | 🩹 | non | Save/Load fichier fonctionnent, mais **la sérialisation par-composant est un stub qui produit des archives vides** (`SerializeEntity`/`DeserializeEntity` : boucle sur les sérialiseurs enregistrés sans jamais résoudre le pointeur du composant — commentaire "stub — Phase 2 complet" dans le code). Round-trip actuellement non fonctionnel pour les données de composants. |
| ECS Scripting C++ natif (`NkScriptComponent`/`NkScriptSystem`) | ✅ | non | Cycle de vie complet, header-inline. Utilise `<memory>/<vector>` (STL) — à noter, contrevient à la convention "zéro STL" affichée ailleurs dans le projet. |
| ECS Scripting Lua/Python/C#/DLL (`NkScriptBridge`, `Scripting/CSharp`, `Scripting/Python`) | ❌ | non | Specs seules. |
| ECS Visual Scripting (`NkBlueprint`, `NkBlueprintHotReload`, `NkValidGraph`) | ❌ | non | Spec seule, **utilise `<vector>/<string>/<functional>/<memory>` (STL)** — à corriger si implémenté un jour (convention projet). |
| ECS Réplication réseau (`NkNetWorld`) | ❌ | non | Spec seule ; `#include "Protocol/NkConnection.h"` / `"Protocol/NkBitStream.h"` **introuvables dans le repo** (chemin probablement erroné/futur module). |
| Modeling (`NkEditableMesh`) | ✅ **prouvé** | non | **Implémenté dans cet audit** (voir "Incrément Phase A" ci-dessous). Adaptateur fin sur `renderer::NkEditMesh` (NKRenderer, demi-arête n-gon déjà en prod) plutôt qu'une réimplémentation. `NkMeshModifier`/`NkProceduralMesh`/`NkUndoStack` restent des headers de spec (non touchés). |
| Topology (`NkHalfEdge`, `NkBooleanOp`) | ❌ | non | Spec seule ; **doublonnait** ce qui existe déjà dans `renderer::NkEditMesh` — à réévaluer : soit adapter par-dessus `NkEditMesh` (comme `NkEditableMesh`), soit abandonner au profit direct de `renderer::NkEditMesh`. Includes cassés (`"Nkentseu/Modeling/NkHalfEdge.h"`, mauvais préfixe/mauvais module). |
| UV (`NkUVEditor`) | ❌ | non | Spec seule, include cassé (`"Nkentseu/Modeling/..."`). |
| Anim 3D (`NkLocomotion`, `NkFootIK`, `NkMotionMatch`, `NkCrowdAgent`) | ❌ | non (logique) / 🎮 seulement au rendu final | Spec seule. Include cassé (`"Nkentseu/Rigging/NkIKSolver.h"`). Bon candidat Phase A (algorithmes purs). |
| Anim 2D (`NkAtlas2D`, `NkTween`) | ❌ | non | Spec seule, includes propres (pas de bug de chemin). Bon candidat Phase A. |
| Crowd (`NkCrowdSim` = `NkCrowdGrid`/`NkCrowdManager`) | ❌ | non | Spec seule. Include cassé + **utilise `std::pair` (STL)** dans `NkVector<std::pair<...>>`. |
| Color (`NkColor`, `NkPalette`, `NkHarmony`) | 🔶 | non | **Partiellement démarré (2026-07-24, incrément SVG)** : `NkColorManager.cpp` créé avec UNIQUEMENT `NkColor::FromSRGB` + conversions sRGB↔linéaire (requis au lien du chemin d'import SVG) ; tout le reste (HSL/LAB/OKLab, palettes, harmonies, picker) reste spec sans corps. Bon candidat Phase A (pur calcul). |
| Design Raster (`NkRasterCanvas`, `NkBrushEngine`) | ❌ | non (CPU) puis 🎮 à l'upload | Spec seule, includes propres. |
| Design Vector (`NkVectorPath`, `NkPaint`) | 🔶 | non | **Partiellement démarré (2026-07-24, incrément SVG)** : `NkVectorPath.cpp` créé avec UNIQUEMENT `MoveTo/LineTo/Close` (construction de chemin) ; Béziers/formes haut niveau/transforms/booléens/métriques/tessellation/sérialisation restent specs sans corps. |
| Doc (`NkHybridDocument`, `NkVectorDocument`) | 🔶 | non | **Partiellement démarré (2026-07-24, incrément SVG)** : includes cassés de `NkVectorDocument.h` corrigés (compilé pour la première fois), `NkVectorDocument.cpp` créé avec UNIQUEMENT `AddArtboard/AddLayer/AddPath` + destructeur ; Save/Load/Export/symboles/presse-papier/undo réel restent specs sans corps. `NkHybridDocument` intact (spec seule). |
| Facial (`NkFacialRig`, `NkSkinMaterial`) | ❌ | 🎮 (SSS, rendu peau) | Spec seule, includes propres. |
| IO (`NkOBJIO`, `NkGLTFIO`, `NkFBXImporter`) | ✅ **prouvé** | non | **Adapté dans cet incrément** (voir "Incrément Phase G1" ci-dessous). Fins adaptateurs sur `renderer::LoadOBJ/LoadGLTF/LoadFBX` (loaders CPU réels de NKRenderer, zéro réimplémentation de parsing). |
| IO (`NkSVGIO`) | ✅ **prouvé (import minimal, 2026-07-24)** | non | **SVG import minimal ajouté a posteriori** (après la décision "non adapté" du 2026-07-23) : `NkSVGIO::Import` réel sur le codec `NkSVGCodec` + sous-ensemble de construction `NkVectorPath::MoveTo/LineTo/Close` et `AddArtboard/AddLayer/AddPath` (+ correctifs d'includes/ambiguïtés dans Design/Doc/Color, désormais compilés pour la première fois). Prouvé : `NkSVGImportDemo`, 24 OK/0 FAIL. Le reste (export, Béziers préservées, dégradés/texte/groupes, tessellation, booléens) reste en spec sans corps — voir détail ci-dessous. |
| Physics (`NkPhysicsMesh`, `Systems/NkPhysicsSystems.h`) | ❌ | 🎮 (Cloth/Hair/SoftBody = compute shaders) | Spec seule. `NkJiggleBoneSystem`/`NkRagdollSystem`/`NkMocapSystem` sont CPU-only (bons candidats Phase A) ; Cloth/Hair/SoftBody sont explicitement GPU compute (Phase C). Include cassé. |
| Rigging (`NkIKSolver`, `NkConstraintSystem`) | ❌ | non | Spec seule, includes propres. FABRIK/CCD/TwoBone/Spline = pur CPU, bon candidat Phase A. |
| Sculpt (`NkSculpting`) | ❌ | 🎮 (brushes temps réel) | Spec seule, include cassé (`"Nkentseu/Viewport/NkViewportCamera.h"`). |
| Selection (`NkSelectionSystem` = masque raster) | ❌ | non (CPU) puis 🎮 à l'upload | Spec seule, includes propres. |
| Sequencer (`NkSequencer`) | ❌ | non (logique) / 🎮 au rendu | Spec seule, includes propres. |
| Text (`NkTextPath`) | ❌ | non | Spec seule, includes cassés. |
| Viewport (`NkGizmo`, `NkSelectionBuffer`, `NkViewportCamera`) | ❌ | `NkViewportCamera` = **non** (pure maths caméra) ; `NkGizmo`/`NkSelectionBuffer` = 🎮 (dessin/readback GPU) | `NkViewportCamera` est un excellent candidat Phase A (aucune dépendance GPU malgré le nom du dossier). |
| VFX (`NkParticleEmitter`/`NkParticleSystem`) | ✅ | 🎮 (via `NKRenderer::NkVFXSystem`) | Livré (pont ECS→VFX réel, `.cpp` présent). Graphe de nodes/simulation IA = non démarré (cf. section dédiée, hors scope CPU-only). |
| Noge / Nogee (éditeur, `Applications/Nogee`) | 🔶 | 🎮 (viewport) | **Bien plus avancé que l'audit précédent ne l'indiquait** : `AssetManager`, `CommandHistory`, `NkEditorCamera`, `NkGizmoSystem`, `NkSelectionManager`, `ProjectManager`, `EditorLayer`, `UILayer`, `ViewportLayer` ont tous un `.cpp` (pas de simples coquilles). Non ré-audité en détail (hors scope de cette passe, focalisée sur `Engine/Noge/src/Noge/`). |
| PV3DE (`Applications/PV3DE`) | 🔶 | mixte | **L'audit précédent ("dossier vide, docs seulement") est faux/obsolète** : le dossier contient ~30 fichiers `.cpp/.h` réels (NkDiagnosticEngine, NkEmotionFSM, NkFaceController/V2, NkBodyController, NkBreathController, NkConversationEngine + backends Claude/Ollama/Rules, NkSpeechEngine, NkPatientRenderer, NkFHIRExport, Panels, `PatientVirtualApp`). Non ré-audité en détail (hors scope). |

---

## Incrément Phase A réalisé dans cet audit : `NkEditableMesh`

**Choix** : parmi tous les items CPU-only de la Phase A, `NkEditableMesh` a
été retenu comme premier incrément car c'est la fondation la plus citée par
les autres headers : `Topology/NkHalfEdge.h`, `UV/NkUVEditor.h`,
`Sculpt/NkSculpting.h`, `Physics/NkPhysicsMesh.h`, `Modeling/NkMeshModifier.h`,
`Modeling/NkProceduralMesh.h`, `IO/NkFBXImporter.h`, `IO/NkGLTFIO.h`,
`IO/NkOBJIO.h` l'incluent tous (9 fichiers dépendants directs).

**Découverte en cours de route (retour de Rihen)** : la première version
réimplémentait sa propre structure demi-arête avec des faces à **taille
fixe** (`NkEditFace::kMaxVerts = 4`) — incapable de représenter un n-gon à 5+
côtés. Or `renderer::NkEditMesh`
(`Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkEditMesh.h`) existe déjà en
production avec une demi-arête n-gon complète (`Vert`/`Hedge`/`Face` sans
limite de côtés), des commandes d'édition (`ExtrudeSelectedFaces`,
`MergeSelectedVerts`, `SubdivideSelectedFaces`, `LoopCutFromSelectedEdge`,
`BisectByPlane`, `Quadify`), un historique undo/redo (`NkEditHistory`) et une
pile de modificateurs (`NkModifierStack` : Mirror/Array/Subsurf). **Décision
appliquée** : `NkEditableMesh` (Noge) est réécrit comme une **fine couche
d'adaptation** au-dessus de `renderer::NkEditMesh` — zéro réimplémentation de
demi-arête côté Noge.

### Ce qui a été corrigé dans `NkEditableMesh.h` (ne compilait pas avant)

- `#include "NKRenderer/src/Resources/NkResourceDescs.h"` — **fichier
  inexistant** dans l'arborescence NKRenderer actuelle. Remplacé par
  `#include "NKRenderer/Mesh/NkEditMesh.h"` (qui inclut lui-même
  `NkMeshSystem.h` pour `NkMeshDesc`/`NkVertexLayout` et `NkRendererTypes.h`
  pour `NkAABB`).
- `NkAABB` utilisé non-qualifié alors qu'il vit dans `nkentseu::renderer`, pas
  `nkentseu::math` — qualifié explicitement (`renderer::NkAABB`).
- `NkSpan<T>` utilisé sans son include (`NKContainers/Views/NkSpan.h`) —
  ajouté.
- Ambiguïté `NkVertexLayout` (existe à la fois dans `nkentseu::NkVertexLayout`
  (NKRHI) et `nkentseu::renderer::NkVertexLayout`) — qualifiée explicitement
  dans le `.cpp`.

### Design de l'adaptateur (`NkEditableMesh.cpp`)

- Stockage unique : `renderer::NkEditMesh mEdit` (plus de tableaux dupliqués
  côté Noge). `NkEditVertex`/`NkEditEdge`/`NkEditFace` sont maintenant des
  **alias de type** vers `NkEditMesh::Vert/Hedge/Face` (continuité de nommage
  demandée, zéro duplication de représentation).
- `AddVertex`/`AddTri`/`AddQuad`/**`AddPolygon`** (nouveau, n-gon quelconque)
  passent par le round-trip public `ToPolygons()`/`BuildFromPolygons()` — le
  rechaînage demi-arête (twins, next) reste **toujours** délégué à
  `NkEditMesh`, jamais manipulé directement par Noge.
- `ExtrudeFaces`/`LoopCut`/`Subdivide` **délèguent** aux commandes natives
  (`ExtrudeSelectedFaces`/`LoopCutFromSelectedEdge`/`SubdivideSelectedFaces`)
  via la sélection par sommet (`Vert::sel`).
- `FlipNormals`/`Triangulate`/`MergeByDistance` n'ont **pas d'équivalent**
  dans la couche de commandes de `NkEditMesh` (qui ne fait que fusionner toute
  une sélection en un point, pas de fusion par paires sous un seuil de
  distance) : implémentés comme manipulations pures du CSR polygones
  (`ToPolygons` → transformation → `BuildFromPolygons`), donc sans dupliquer
  la logique de twin/next.
- `ToMeshDesc` délègue entièrement à `NkEditMesh::Triangulate()` (export GPU
  déjà résolu côté NKRenderer).
- `FromMeshDesc` implémenté pour le cas standard (layout `NkVertex3D`) via
  `NkEditMesh::BuildFromIndexed()`.
- **Honnêtement non implémenté** (aucun équivalent dans `renderer::NkEditMesh`
  — corps présent, log d'avertissement, pas de fausse réussite) :
  `BevelEdges`, `SmartUVProject`, `CubeProject`, `CylindricalProject`,
  `GrowSelection`, `ShrinkSelection`.

### Preuve d'exécution

- **Build** : `jenga build --target Noge --config Debug --platform Windows`
  → `Noge.lib` compile avec succès (`NkEditableMesh.cpp` inclus, 0 erreur).
- **Test unitaire écrit** : `Engine/Noge/tests/test_editable_mesh.cpp` (11 cas,
  framework `Unitest` du projet — même style que
  `Kernel/Foundation/NKContainers/tests/`). **Non exécutable dans cet
  environnement** : `jenga test` / le projet généré `Noge_Tests` sont
  bloqués par une politique de workspace indépendante de ce code
  (`"Unit-test compilation/execution is disabled by workspace policy"`,
  vérifié : **tous** les `*_Tests` du workspace sont concernés, pas
  spécifique à Noge).
- **Contournement pour prouver l'exécution réelle** : application console
  `Applications/NkEditableMeshDemo` (enregistrée dans `Nkentseu.jenga`),
  exécutant les 11 mêmes scénarios avec assertions manuelles (`Check()` +
  compteurs pass/fail, pas de dépendance au framework de test bloqué).
  Compilée et **exécutée réellement** :
  `Build/Bin/Debug-Windows/NkEditableMeshDemo/NkEditableMeshDemo.exe` → sortie
  observée : **`=== Resultat : 37 OK / 0 FAIL ===`**, code de sortie 0.
  Scénarios couverts : triangle isolé (topologie/normale/bounds), twin
  résolu sur arête partagée, quad + `Triangulate` (Fan), **n-gon à 5 côtés**
  (impossible avec l'ancienne implémentation à taille fixe — preuve directe
  du gain), normales lissées, `FlipNormals`, `MergeByDistance`,
  `Clone` (deep copy), `ToMeshDesc` (export GPU), sélection
  (All/Deselect/Invert), `ExtrudeFaces` (délégation réelle à
  `NkEditMesh::ExtrudeSelectedFaces` — vérifie que sommets/faces augmentent).

### Fichiers touchés par cet incrément

- `Engine/Noge/src/Noge/Modeling/NkEditableMesh.h` (réécrit)
- `Engine/Noge/src/Noge/Modeling/NkEditableMesh.cpp` (nouveau)
- `Engine/Noge/tests/test_editable_mesh.cpp` (nouveau, non exécutable ici — cf. ci-dessus)
- `Engine/Noge/Noge.jenga` (fix : lien de test `"Nkentseu"` → `"Noge"`, cible inexistante)
- `Applications/NkEditableMeshDemo/` (nouveau, contournement policy)
- `Nkentseu.jenga` (enregistrement du nouveau projet `NkEditableMeshDemo`)

---

## Incrément Phase G1 réalisé (2026-07-23) : Adaptateurs pipeline d'assets

**Contexte (item G1.3)** : `Engine/Noge/src/Noge/IO/{NkFBXImporter,NkGLTFIO,
NkOBJIO,NkSVGIO}.h` étaient des specs mortes (includes cassés, 0 `.cpp`,
jamais compilées). Le vrai import 3D existe déjà en production :
`Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/{NkOBJLoader,NkGLTFLoader,
NkFBXLoader}.cpp` — des fonctions libres CPU-only (`renderer::LoadOBJ/
LoadGLTF/LoadFBX`, aucun `NkIDevice` requis) qui produisent toutes un format
commun `renderer::NkGLTFMeshData`, déjà dispatchées par extension dans
`NkMeshSystem::Import` (`NkMeshSystem.cpp:163-176`) et déjà utilisées par
`NkRenderSystem.cpp`. Pour SVG : `Kernel/Runtime/NKImage/src/NKImage/Codecs/
SVG/NkSVGCodec.h` (réel, mais raster + vue vectorielle en lecture seule).

### Design de l'adaptateur (OBJ/glTF/FBX)

- **Pont CPU→CPU, zéro upload GPU** : au lieu de passer par
  `NkMeshSystem::Import` (qui exige un `NkIDevice` vivant pour
  `NkMeshSystem::Create`), chaque adaptateur appelle directement le loader
  CPU réel (`renderer::LoadOBJ/LoadGLTF/LoadFBX`) puis construit un
  `renderer::NkMeshDesc` transitoire à partir des buffers déjà parsés, et
  délègue à `NkEditableMesh::FromMeshDesc` (pont CPU pur déjà existant,
  incrément précédent) pour obtenir la demi-arête n-gon Noge. Zéro nouvelle
  logique de parsing OBJ/glTF/FBX ; zéro dépendance à un device GPU.
- **`NkOBJIO::Import`** → `NkEditableMesh` (API conservée). Options
  réellement honorées : `scaleFactor`/`flipY` (post-traitement trivial sur
  les vertices déjà parsés). `triangulate=false`/`generateNormals=false`
  sont des no-ops documentés (le loader réel triangule et génère toujours
  des normales lissées). `Export` : non implémenté (aucun écrivain OBJ/MTL
  côté NKRenderer), retourne `false` + log, pas de fausse réussite.
- **`NkGLTFImporter::Import`** → `NkGLTFScene` (mesh fusionné + matériaux
  PBR + hiérarchie de nodes + squelette + animations TRS), tous remappés
  directement depuis `renderer::NkGLTFMeshData` (déjà entièrement parsé —
  le loader réel gère skinning JOINTS_0/WEIGHTS_0 et morph targets). Le
  squelette (`ecs::NkSkeleton`) est reconstruit depuis
  `skinJoints`/`inverseBind`/`nodes` (parenté recalculée depuis les listes
  `children`, `NkBone::name` reste vide — le loader réel ne parse pas
  `nodes[].name`). `SpawnIntoWorld` (instanciation ECS réelle) et l'export
  glTF/GLB : non implémentés, stubs honnêtes (log + no-op / `false`).
- **`NkFBXImporter::Import`** → `NkFBXScene`. Le loader réel FBX
  (binaire ET ASCII, malgré un commentaire interne obsolète disant
  l'inverse) ne supporte QUE la géométrie : `skeletons`/`animations`/
  `materials` restent délibérément vides (pas d'invention), les options
  `importSkeleton/importAnimation/importMaterials/importLights/
  importCameras` sont des no-ops honnêtes (log si demandées).
- **`NkSVGIO` — NON adapté le 2026-07-23 (décision documentée), puis
  IMPORT MINIMAL ajouté a posteriori le 2026-07-24** : l'audit initial
  avait conclu que l'API cible `NkVectorDocument` (`Doc/NkVectorDocument.h`),
  dépendant de `NkVectorPath`/`NkLayerStack`/`NkPaint`/`NkColorManager`
  (Design/Doc/Color) — **4 fichiers sans un seul `.cpp`, 0 % de code** —
  exigeait de construire tout le sous-système. Une investigation plus
  poussée a montré que ce n'était PAS tout ou rien : le codec réel
  (`Kernel/Runtime/NKImage/.../SVG/NkSVGCodec.h`) expose déjà de vraies
  données vectorielles exploitables (`NkSVGShapeView` : contours polylignes
  X/Y, fill/stroke, opacité, fill-rule) — le blocage n'était pas l'absence
  de données sources mais l'absence des méthodes de CONSTRUCTION côté
  `NkVectorPath`/`NkVectorDocument`. **Sous-ensemble minimal implémenté
  (2026-07-24)** :
    - `NkVectorPath::MoveTo/LineTo/Close` (`Design/Vector/NkVectorPath.cpp`,
      nouveau — push simple dans `mCmds`, PAS de tessellation/booléens/
      sérialisation SVG, restés en specs sans corps) ;
    - `NkVectorDocument::AddArtboard`/`NkArtboard::AddLayer`/
      `NkVectorLayer::AddPath` + destructeur (`Doc/NkVectorDocument.cpp`,
      nouveau — hiérarchie document→artboard→calque→objet uniquement ;
      Save/Load/Export/guides/symboles/presse-papier restés sans corps) ;
    - `NkUndoStack::Clear()` (`Modeling/NkUndoStack.cpp`, nouveau — requis
      au lien par `~NkVectorDocument` ; Execute/Undo/Redo restés sans corps) ;
    - `NkColor::FromSRGB` + conversions sRGB↔linéaire IEC 61966-2-1
      (`Color/NkColorManager.cpp`, nouveau — requis au lien par les
      initialiseurs par défaut de `NkGuide`/`NkGrid` ; tout le reste de la
      gestion colorimétrique resté sans corps) ;
    - `NkSVGIO::Import` (`IO/NkSVGIO.cpp`, nouveau) : parse via
      `NkSVGImage::LoadFromFile`, itère `GetShape(i)` → contours
      (`ContourXs/ContourYs` → MoveTo/LineTo…/Close) → `NkPaint::Solid
      (FillColor)` → `AddArtboard`→`AddLayer`→`AddPath` ; seule
      `scaleFactor` des options est honorée (les autres : no-ops loggés).
  **Correctifs de compilabilité indispensables au passage** (ces headers
  n'avaient JAMAIS été compilés) : includes cassés corrigés dans
  `NkVectorDocument.h`/`NkLayerStack.h`/`NkRasterCanvas.h`/`NkSVGIO.h`
  (préfixes `Nkentseu/`→`Noge/`, `src/` en trop, `NkUndoStack.h` et
  `NKContainers/Views/NkSpan.h` et `Functional/NkFunction.h` manquants) ;
  3 types géométriques jamais déclarés nulle part (`NkAABB2f`/`NkIRect`/
  `NkIVec2`) aliasés sur des types NKMath réels dans le nouveau
  `Design/NkDesignGeomTypes.h` ; ambiguïté `nkentseu::NkColor` vs
  `nkentseu::math::NkColor` levée en remplaçant les `using namespace math;`
  de ces headers par des `using` ciblés (un using-directive fuit dans toute
  l'unité de compilation).
  **Limites honnêtes (documentées dans `NkSVGIO.h/.cpp`)** : contours déjà
  aplatis en polylignes par le codec (pas de Béziers préservées) ; pas de
  dégradés/texte/groupes/clip-paths ni `fill-opacity` (non exposés par
  `NkSVGShapeView`) ; `ImportFromString`/`Export`/`PathToSVG`/`SVGToPath`
  non implémentés. **Preuve d'exécution réelle** :
  `Applications/NkSVGImportDemo` (même contournement `jenga test` bloqué
  que les autres démos) — build `jenga build --target NkSVGImportDemo
  --config Debug --platform Windows` → `36/36 projets, SUCCESS` ; exécution
  `Build/Bin/Debug-Windows/NkSVGImportDemo/NkSVGImportDemo.exe` →
  **`=== Resultat : 24 OK / 0 FAIL ===`**, exit code 0. Scénarios : SVG
  synthétique 3 formes écrites sur disque (2 rects + 1 circle — comptes,
  couleurs #ff0000/#00ff00/#0000ff et opacité 0.5 vérifiés par assertions),
  `Resources/Pong/Textures/socials/github.svg` réel (nb objets importés ==
  baseline `NkSVGImage::ShapeCount()` du même fichier), et chemin d'erreur
  (fichier inexistant → `false` + `GetLastError()` non vide, pas de fausse
  réussite).

### Preuve d'exécution

- **Build** : `jenga build --target Noge --config Debug --platform Windows`
  → `Noge.lib` compile avec succès (`NkOBJIO.cpp`, `NkGLTFIO.cpp`,
  `NkFBXImporter.cpp` inclus, 0 erreur, warnings pré-existants sans lien
  avec cet incrément).
- **`jenga test` bloqué** : même politique de workspace que l'incrément
  précédent (`"Unit-test compilation/execution is disabled by workspace
  policy"`). **Contournement** : application console
  `Applications/NkAssetIODemo` (enregistrée dans `Nkentseu.jenga`).
- **Méthode de comparaison** ("résultat identique au chemin
  `NkMeshSystem::Import` direct", jalon de la ROADMAP) : `NkMeshSystem::
  Import` fait exactement 2 choses — (1) appelle le loader CPU réel selon
  l'extension (les MÊMES fonctions que les adaptateurs Noge/IO, zéro
  logique dupliquée) ; (2) `NkMeshSystem::Create(desc)`, qui copie
  `desc.vertexCount`/`indexCount` TELS QUELS dans le mesh GPU
  (`NkMeshSystem.cpp` ~L96-104 : `e.vertexCount = desc.vertexCount;` —
  affectation directe, aucune transformation). Le compte de sommets/faces
  d'un mesh importé via `NkMeshSystem::Import` est donc, par construction,
  celui produit par l'étape (1). La démo appelle l'étape (1) en direct
  (baseline) et la compare aux adaptateurs Noge/IO — preuve d'équivalence
  sans bootstrapper de `NkIDevice` réel. **Limite assumée** : `Create()`
  (étape 2, upload GPU) n'a PAS été exercée — aucun backend RHI n'est
  aujourd'hui instancié par un seul appelant dans tout le dépôt (recherché :
  0 résultat pour `new NkSoftwareDevice`/`NK_GFX_API_SOFTWARE` hors de
  `NKRHI/Software/` lui-même) ; bootstrapper ce chemin GPU pour cette seule
  démo CPU-only sortirait du scope d'un adaptateur IO "fin".
- **Exécuté pour de vrai** sur des assets réels du repo
  (`Build/Bin/Debug-Windows/NkAssetIODemo/NkAssetIODemo.exe`, build
  `jenga build --target NkAssetIODemo --config Debug --platform Windows` →
  `34/34 projets, SUCCESS`) : sortie observée = **`=== Resultat : 53 OK /
  0 FAIL ===`**. Assets couverts :
  - `Resources/Models/tree.obj` (9 verts/12 faces) et
    `Resources/Models/rock/rock.obj` (165 verts/192 faces, 1 matériau MTL).
  - `Resources/Models/rubber_duck/scene.gltf` (5676 verts/11072 faces,
    1 matériau PBR, 4 nodes, non skinné) et
    `Resources/Models/CesiumMan/CesiumMan.glb` (3273 verts/4672 faces,
    **skinné** : 19 joints/19 inverseBind/22 nodes/1 animation — squelette
    `ecs::NkSkeleton` reconstruit avec `boneCount == 19`, vérifié par
    assertion).
  - `Resources/Models/test/cube_ascii.fbx` (FBX ASCII v7400, 24 verts/
    12 faces) et `Resources/Models/Futuristic_Car_2.1_fbx.fbx` (FBX binaire
    v7400, **15 geometries fusionnées**, 18758 verts/8972 faces).
  - Pour chaque asset : compte sommets/faces de l'adaptateur == baseline
    `renderer::LoadOBJ/LoadGLTF/LoadFBX` direct (assertions strictes,
    égalité exacte, pas de tolérance) ; `Export`/`ImportFromMemory`/
    `SpawnIntoWorld` vérifiés en échec honnête (`false`/scène invalide, pas
    de fausse réussite).

### Fichiers touchés par cet incrément

- `Engine/Noge/src/Noge/IO/NkOBJIO.h` (réécrit) + `NkOBJIO.cpp` (nouveau)
- `Engine/Noge/src/Noge/IO/NkGLTFIO.h` (réécrit, API réduite au réalisable)
  + `NkGLTFIO.cpp` (nouveau)
- `Engine/Noge/src/Noge/IO/NkFBXImporter.h` (réécrit) + `NkFBXImporter.cpp`
  (nouveau)
- `Engine/Noge/src/Noge/IO/NkSVGIO.h` (non adapté — commentaire de tête
  réécrit avec diagnostic honnête + include cassé corrigé, aucun `.cpp`)
- `Applications/NkAssetIODemo/` (nouveau, contournement policy)
- `Nkentseu.jenga` (enregistrement du nouveau projet `NkAssetIODemo`)

---

### ⭐ Incrément — Matériaux Phong FBX + textures externes (2026-07-25)

`renderer::LoadFBX` (`Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkFBXLoader.cpp`)
résout désormais le graphe d'objets FBX (`Objects`/`Connections`) pour importer
les matériaux, en plus de la géométrie déjà livrée :

- **Graphe d'objets** : indexation par ID (`FbxIdOf`, ID stocké en `int64`
  EXACT — un ID FBX dépasse souvent 2^53 et perdrait sa précision en
  `double`, un piège qui aurait cassé toute résolution de connexion) +
  parsing de la section `Connections` (`C "OO"`/`C "OP"`, objet-objet et
  objet-propriété-nommée). Résolution `Geometry -> Model propriétaire -> 1er
  Material connecté -> textures connectées` (limite assumée : 1 seul
  matériau par sous-mesh, comme la géométrie qui fusionne déjà toutes les
  `Geometry` — pas de `LayerElementMaterial` par-polygone).
- **Matériau** (`Properties70`) : `DiffuseColor`/`DiffuseFactor` ->
  `baseColorFactor`, `TransparencyFactor` -> alpha, `EmissiveColor`/
  `EmissiveFactor`, `ShininessExponent`/`Shininess` -> rugosité (heuristique
  Phong→PBR grossière assumée, pas de conversion physique — le Phong FBX n'a
  pas de notion metal/rugosité native, matériau non-métallique par défaut).
- **Textures** (`Objects/Texture`, connexions `OP` vers `DiffuseColor`/
  `NormalMap`/`Bump`/`EmissiveColor`) : `RelativeFilename`/`FileName`
  résolus par rapport au dossier du `.fbx` (le chemin peut être absolu côté
  machine d'export — ne garde que le nom de fichier, même précédent que la
  résolution des `.bin` externes glTF). Échec de chargement = warning + slot
  invalide, jamais d'invention de pixels. Non supporté : textures FBX
  EMBARQUÉES (`Video` binaire).
- **Adaptateur Noge** (`NkFBXImporter.cpp`) : `NkFBXScene::materials`
  réellement rempli si `importMaterials` (conversion directe
  `NkGLTFMaterial` -> `NkFBXScene::MaterialData` ; `NkSubMesh::material`
  reste invalide — `NkMaterialSystem` en cours de réécriture, même
  limitation déjà documentée côté loader glTF).
- Corrections de commentaires obsolètes trouvées au passage :
  `NkFBXLoader.h` affirmait « FBX ASCII non supporté » alors que le loader
  supporte binaire ET ascii depuis la revision précédente (résidu).
- Validé : compilation propre `NKRenderer` (67/67) et `Noge` (35/35, seul
  échec du build global = `NkHybridDocument.cpp`, fichier non lié, include
  cassé pré-existant `Nkentseu/Design/NkLayerStack.h`).
- **Reste (chantier séparé, plus gros)** : skeleton/skinning (`Deformer`
  `Skin`/`Cluster`, hiérarchie `Model`, `BindPose`) et animation
  (`AnimationCurveNode`/`AnimationCurve`) — `NkFBXScene::skeletons/
  animations` restent vides, `importSkeleton/importAnimation` restent des
  no-ops honnêtes.

---

## Phase A — CPU-only (priorité immédiate, pendant l'entraînement NKAI)

Tout ce qui ne nécessite aucun contexte GPU actif. Ordre suggéré par valeur
débloquante (ce que chaque item permet de faire ensuite) :

1. ✅ **`NkEditableMesh`** (Modeling) — fait dans cet audit. Débloque
   Topology/UV/Sculpt/Physics-mesh/IO en aval.
2. ⬜ **`Topology/NkHalfEdge.h` + `NkBooleanOp.h`** — réévaluer : peut sans
   doute être **supprimé** au profit direct de `renderer::NkEditMesh`
   (mêmes fonctionnalités déjà présentes), plutôt que réécrit. Décision à
   prendre avant tout travail dessus (éviter une 3e demi-arête dupliquée).
3. ⬜ **`Rigging/NkIKSolver.h`** — FABRIK/CCD/TwoBone/Spline, pur calcul sur
   `NkSkeleton` (déjà livré côté composants ECS). Aucun blocage GPU.
4. ⬜ **`Anim/NkLocomotion.h`** (`NkFootIK`, `NkMotionMatch`, `NkCrowdAgent`)
   — logique pure, seul le raycast de `NkFootIK` dépend de `NkPhysicsSystem`
   (déjà livré, CPU).
5. ⬜ **`Color/NkColorManager.h`** — conversions colorimétriques pures,
   aucune dépendance.
6. ⬜ **`Anim2D/NkTween.h` + `NkAtlas2D.h`** — interpolation/parsing pur CPU.
7. ⬜ **`Viewport/NkViewportCamera.h`** — uniquement des maths caméra
   (orbit/pan/zoom/frustum), zéro appel GPU malgré le nom du dossier. Utile
   avant même d'attaquer le viewport réel (Phase C).
8. ⬜ **`Selection/NkSelectionSystem.h`** (masques raster CPU) — dépend de
   `Design/Raster/NkRasterCanvas.h` (à faire avant, CPU pur).
9. ⬜ **`Design/Raster/NkRasterCanvas.h` + `Design/Vector/NkVectorPath.h`** —
   logique CPU complète (tessellation, tiles) ; seul l'upload GPU final
   (`FlushDirtyTiles`) est Phase B/C.
10. ⬜ **Complétude ECS** : corriger `NkSceneSerializer` (sérialisation par
    composant, actuellement stub — voir tableau), `NkPrefab::Instantiate`
    (application des composants, actuellement stub), nettoyer les doublons
    morts (`Core/NkEngineLayer.h`, `Rendering/NkRenderer.h`,
    `Physics/NkPhysicsComponents.h`).
11. ⬜ **`Sequencer/NkSequencer.h`** — structures de données/évaluation de
    timeline pures (le rendu des tracks caméra/lumière est Phase C).

## Phase B — GPU léger (une fois un peu de marge disponible)

- **Design Raster/Vector — upload GPU** (`FlushDirtyTiles`, tessellation →
  `NkRender2D::FillPolygon`) : la logique CPU (Phase A) est isolable, seul
  l'upload est GPU, testable indépendamment.
- **`Physics/Systems/NkPhysicsSystems.h`** — `NkJiggleBoneSystem` (CPU pur,
  reclasser en Phase A) vs `NkClothSystem`/`NkHairSystem`/`NkSoftBodySystem`
  (compute shaders, Phase B/C selon la charge GPU disponible).
- ✅ **FAIT (2026-07-23)** — **IO** (`NkOBJIO`, `NkGLTFIO`, `NkFBXImporter`) :
  import CPU-only adapté et prouvé (voir Phase G1 item 3 + section dédiée).
  Seul point encore GPU : uploader le résultat via `NkMeshSystem::Create`
  n'a pas été exercé dans la démo (pas de `NkIDevice` vivant bootstrappé —
  voir limite honnête documentée dans `Applications/NkAssetIODemo/src/
  main.cpp`) ; `NkSVGIO` reste non adapté (bloqué sur `NkVectorDocument`,
  0 % de code, cf. Phase G1 item 3).

## Phase C — GPU lourd (rendu/shaders/viewport temps réel complet)

- `Viewport/NkGizmo.h`, `NkSelectionBuffer.h` (readback GPU par Color-ID).
- `Sculpt/NkSculpting.h` (brushes temps réel, `NKRHI::CreateComputePipeline`).
- `Facial/NkSkinMaterial.h` (SSS, shaders peau/yeux/dents).
- Graphe VFX + simulation fumée/feu/fluides GPU (section VFX ci-dessous,
  inchangée depuis le 2026-07-09).
- Pipeline FBO viewport Nogee (`ViewportLayer`), `NkGizmoSystem` visuel.

---

## Bugs / incohérences trouvés dans cet audit (au-delà de ceux déjà corrigés)

- **Collision de nom `NkLayerStack`** : `Core/NkLayerStack.h`
  (`nkentseu::NkLayerStack`, pile de Layer applicatifs) et
  `Design/NkLayerStack.h` (`nkentseu::NkLayerStack`, pile de calques
  design/compositing) définissent **deux classes différentes avec le même
  nom qualifié complet**. Pas de conflit actuel (rien n'inclut les deux dans
  la même unité de compilation), mais collision ODR garantie si un jour un
  fichier design inclut `Nkentseu.h`. À renommer avant que ça arrive
  (suggestion : `NkDesignLayerStack` côté Design).
- **`Core/NkEngineLayer.h` (v1) est un doublon mort** de
  `Layers/NkEngineLayer.h` (v2, celui réellement utilisé/compilé) : includes
  cassés (`"Nkentseu/Core/Layer.h"`, `"Nkentseu/Core/Application.h"` —
  n'existent pas), ne compile pas s'il est inclus. Rien ne l'inclut
  aujourd'hui. À supprimer.
- **`ECS/Components/Rendering/NkRenderer.h` est un doublon mort** de
  `NkRenderComponents.h` (le commentaire de ce dernier affirme
  `NkRenderer.h` "SUPPRIME" — faux). À supprimer réellement.
- **`ECS/Components/Physics/NkPhysicsComponents.h` est mort** :
  `NkPhysicsSystem.cpp` utilise exclusivement les types de
  `Physics/NkPhysics.h` (`NkRigidbody3D`, `NkCollider3D`...) ; personne
  n'utilise `NkRigidbodyComponent`/`NkColliderComponent`. À supprimer ou
  fusionner.
- **`NkEventBus`, `NkLayerStack::PopLayer`, `NkComponentHandle` cache** :
  points mentionnés dans l'audit précédent (comportement sur `PopLayer`
  d'overlays, absence de cache réel malgré la doc) — non re-vérifiés ligne à
  ligne dans cette passe, à confirmer si quelqu'un y touche.
- **Préfixe d'include erroné `"Nkentseu/..."`** dans ~10 headers spec-only
  (`Anim/NkLocomotion.h`, `Crowd/NkCrowdSim.h`, `Doc/NkVectorDocument.h`,
  `Doc/NkHybridDocument.h`, `IO/*.h`, `UV/NkUVEditor.h`,
  `Sculpt/NkSculpting.h`, `Physics/NkPhysicsMesh.h`,
  `Topology/NkBooleanOp.h`, `Text/NkTextPath.h`) : suppose une arborescence
  `src/Noge/Nkentseu/<Module>/...` qui n'existe pas (les fichiers sont
  directement sous `src/Noge/<Module>/...`). Aucun de ces fichiers n'a
  jamais été compilé (aucun `.cpp` ne les inclut) — la casse est passée
  inaperçue jusqu'ici. À corriger au moment d'implémenter chacun (pas de
  valeur à les corriger en avance sans implémentation).
- **`NkNetWorld.h`** inclut `"Protocol/NkConnection.h"` /
  `"Protocol/NkBitStream.h"` — introuvables dans le repo actuel.
- **Violations de la convention "zéro STL"** (observée dans le reste du
  projet) : `NkScriptComponent.h`/`NkScriptSystem.h` (`<memory>`,
  `<vector>`), `NkBlueprint.h` (`<vector>`, `<string>`, `<functional>`,
  `<memory>`, `<cstring>`), `Crowd/NkCrowdSim.h` (`std::pair` dans
  `NkVector<std::pair<...>>`), `NkPrefab.cpp` (`std::malloc/free`,
  `std::vector`, `std::fopen/fread/fclose` dans `LoadFromFile`). Existant
  avant cet audit, non corrigé (hors scope de cette passe) — à garder en tête
  si quelqu'un retouche ces fichiers.
- **Politique de workspace `disableunittestexecution`** : bloque **la
  compilation ET l'exécution** de tous les projets `*_Tests` (`jenga test`,
  `jenga build --target <X>_Tests`) dans ce workspace, pas seulement Noge.
  Contournement utilisé dans cet audit : application console classique
  (`consoleapp()`) au lieu d'un projet `test()`. À garder en tête pour tout
  futur incrément Phase A nécessitant une preuve d'exécution.

---

## Sections précédentes (2026-05-26), état encore valide

Les sections suivantes de l'audit du 2026-05-26 restent globalement exactes
et ne sont pas reproduites en détail ici pour éviter la duplication — se
référer à l'historique git de ce fichier pour le texte complet :

- Phase 5/6 PV3DE (bootstrap, complet) — **à noter : partiellement obsolète**,
  `Applications/PV3DE` contient maintenant du code réel (voir tableau
  ci-dessus), un ré-audit dédié est nécessaire avant de se fier à cette
  section.
- Phase 3/4 Viewport/Panels éditeur — **à noter : partiellement obsolète**,
  `Applications/Nogee` contient maintenant du code réel (voir tableau
  ci-dessus), un ré-audit dédié est nécessaire.
- Section VFX (graphe d'effets & simulation, décision NKGraph 2026-07-09) —
  toujours exacte, hors scope CPU-only immédiat (Phase C).

## Dépendances

### Couches en dessous (Noge utilise)

- **Foundation** : `NKCore`, `NKMath`, `NKMemory`, `NKContainers`
  (`NkVector`, `NkString`, `NkUnorderedMap`, `NkSpan`), `NKLogger`, `NKTime`.
- **System** : `NKPlatform`, `NKWindow`, `NKEvent`, `NKRHI`, `NKSerialization`,
  `NKFileSystem`, `NKFont`, `NKImage`.
- **Runtime** : `NKECS`, `NKRenderer` (`NkRenderer`, `NkRender2D`, `NkRender3D`,
  `NkMeshSystem`, **`NkEditMesh`** — demi-arête n-gon réutilisée par
  `NkEditableMesh`, cf. incrément ci-dessus), `NKPhysics`, `NKCollision`,
  `NKUI`, `NKSL`, `NKGlad`.

### Applications qui dépendent de Noge

- **Applications/Nogee** — voir tableau ci-dessus (plus avancé qu'annoncé
  précédemment).
- **Applications/PV3DE** — voir tableau ci-dessus (code réel existant).
- **Applications/NkEditableMeshDemo** (nouveau, 2026-07-23) — preuve
  d'exécution CPU-only pour `NkEditableMesh` (contournement policy de test).
- **Applications/NkAssetIODemo** (nouveau, 2026-07-23) — preuve d'exécution
  CPU-only pour les adaptateurs `Noge/IO/{NkOBJIO,NkGLTFIO,NkFBXImporter}`
  (contournement policy de test, 53 OK/0 FAIL sur assets réels du repo).
- **Applications/NkSVGImportDemo** (nouveau, 2026-07-24) — preuve d'exécution
  CPU-only pour l'import SVG minimal `NkSVGIO::Import` ajouté a posteriori
  (contournement policy de test, 24 OK/0 FAIL : SVG synthétique à couleurs
  connues + `github.svg` réel comparé à la baseline `NkSVGImage` + chemin
  d'erreur).
- **Applications/Sandbox**, **Pong**, **NkAudioDemo**, **NkImageDemo**,
  **Songoo**, **Model**, **NKPA** — démos directes NKECS/NKRenderer/NKRHI.

---

*Dernière mise à jour : 2026-07-23 — audit Noge complet (105 fichiers, 21
sous-systèmes) + incrément Phase A (`NkEditableMesh` adapté sur
`renderer::NkEditMesh`, prouvé par exécution réelle) + incrément Phase G1.3
(`NkOBJIO`/`NkGLTFIO`/`NkFBXImporter` adaptés sur les loaders CPU réels de
NKRenderer, prouvé par exécution réelle 53 OK/0 FAIL ; `NkSVGIO` non adapté,
décision honnête documentée — bloqué sur `NkVectorDocument` à 0 % de code ;
**puis SVG import minimal ajouté a posteriori le 2026-07-24** : sous-ensemble
`MoveTo/LineTo/Close` + `AddArtboard/AddLayer/AddPath` + `NkSVGIO::Import`
sur le codec réel `NkSVGCodec`, prouvé par exécution `NkSVGImportDemo`
24 OK/0 FAIL — voir « Incrément Phase G1 ») +
correction de Rihen : Lumen/Nanite/MetaHuman et le quintette de scripting
(C#/Python/from-scratch/C++ hot-reload/visuel) reclassés d'« hors scope » à
« ambitions long terme assumées », nouvelle Phase G6 ajoutée à la roadmap
séquencée.*
