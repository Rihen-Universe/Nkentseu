# PV3DE — Roadmap (Patient Virtuel 3D Emotif)

> Audit complet du 2026-07-25, même méthode que `Engine/Noge/ROADMAP.md` (build réel,
> lecture fichier par fichier, verdicts factuels). ⬜ à faire · 🟡 en cours · ✅ fait.

> **Mise à jour 2026-07-25 (même jour, passe R0/R1 exécutée)** : `jenga build --target PV3DE
> --config Debug --platform Windows` passe à **0 erreur** (`PV3DE.exe` compile et link). Les
> ~136 erreurs décrites ci-dessous sont donc l'état **avant** cette passe — détail de ce qui a
> été fait, y compris plusieurs API fictives supplémentaires non couvertes par cet audit initial,
> dans la section « Phase R0 »/« Phase R1 » plus bas (désormais marquées ✅ FAIT).

## Mise à jour 2026-08-18 — UI médicale portée sur NKGui et BRANCHÉE (premier affichage)

**Campagne de retrait NKUI (décision Rodolf 2026-08-16), périmètre Applications/ — fait :**

- **NKUI → NKGui** : `MedicalUILayer` v3 + les 4 panneaux réécrits sur NKGui
  (`BeginPanel` défilable remplace fenêtres+ScrollRegion, `BeginRow(px/poids)`
  remplace SetNextWidth/Grow, helpers partagés `UI/PvGui.h` : TextColored/Label/
  Toggle composés sur les primitives publiques). `PV3DE.jenga` ne cite plus NKUI
  (NKGui + NKGuiIntegration, patron Nogee) ; NKUI ne reste que dans la CLÔTURE
  transitive via NKCanvas (pont `USE_CANVAS_NKUI`, chantier Noge, hors périmètre).
- **BRANCHÉE — la Phase 5 UI n'est plus un TODO** : `PatientVirtualApp::OnInit`
  fait un vrai `PushOverlay(MedicalUILayer)` ; rendu par `NkGuiRHIBackend`
  (Integrations/NKGui) soumis dans la passe Overlay2D du render graph
  (`SetUIOverlayCallback`, patron Nogee/UILayer) ; `ReleaseGpu()` appelé par
  `OnShutdown` AVANT la destruction du device (la LayerStack meurt après).
  Pont NKEvent→NkGuiInput complet (souris, molette accumulée, double-clic,
  texte, touches d'édition, Ctrl C/X/V/A — table de NK3DModeler/main.cpp).
- **Témoin renforcé (2026-08-18, Release, OpenGL, 1280×720)** : c'est la
  PREMIÈRE fois que l'UI médicale s'affiche (la v2 NKUI n'a jamais été attachée).
  Captures garde-PID (scratchpad session b7dabf70, `temoins/pv3de_apres/`) :
  menu 3 titres + panneaux + rapport ; menu Patient déroulé (6 items F1-F6) ;
  clic « Douleur thoracique » → Diagnostic (1 hypothèse) « Infarctus du
  myocarde 28 % » + jauges Douleur 70 %/Anxiété 60 % + FSM « Douleur sévère »
  reflétée dans le panneau ET l'overlay viewport. Bruit pixel AP/AP : 0 px.
  `[ERR]` : 35 = exactement les 35 de l'AVANT (shaders du banc absents, connu).
  Diff app.log AVANT/APRès : 273 lignes communes identiques (diagnostic,
  backends Claude/Ollama, conversation intacts) ; ajouts = police/atlas/
  RebuildRenderGraph/attach/première frame, rien d'autre.
- **Extraction modèle neutre** : le comportement d'export (FHIR JSON, PDF
  minimal, résumé 2 s, nom de fichier horodaté) vit désormais dans
  `Export/NkReportWriter` (zéro dépendance UI, repris tel quel de ReportPanel) —
  le panneau ne garde que la saisie et les boutons.
- **Dettes notées** : pas de hook de couleur par barre/slider dans NKGui (même
  dette qu'en NKUI, notes `(void)color` en place) ; `SliderFloat` NKGui sans
  format printf → valeur formatée dans une cellule Label ; l'émotion forcée
  par le menu (F1-F6) est transitoire — la FSM est re-pilotée par l'état
  clinique à chaque frame (sémantique v2 conservée, à trancher en Phase 6) ;
  viewport 3D patient toujours placeholder (`GetPatientFBO()` invalide tant
  que le rendu GPU Phase 6 n'existe pas — `RegisterTexture` déjà câblé).

## État actuel — un chantier bien plus avancé que ce que le build laisse croire

`jenga build --target PV3DE --config Debug --platform Windows` échoue avec **136 erreurs de
compilation réparties sur 26 fichiers `.cpp` en échec** (log complet dans
`/tmp/pv3de_build.log` au moment de l'audit ; la mission initiale parlait de « 26 erreurs » —
en réalité ce sont **26 fichiers cassés** pour **136 diagnostics clang**, plusieurs fichiers
ayant buté sur `fatal error: too many errors emitted, stopping now [-ferror-limit=]` avant
d'avoir montré tous leurs problèmes — le compte réel de causes racines par fichier est donc un
**plancher**, pas un plafond).

**Constat central de cet audit, à l'opposé du diagnostic initial de « fragments qui ne
s'assemblent pas » :** la très grande majorité du code source de PV3DE (les 29 `.h` + 26 `.cpp`)
est **réelle, complète, cohérente et déjà pensée en profondeur** — diagnostic différentiel avec
critères pondérés, FSM émotionnelle, 46 Action Units FACS avec presets médicaux, respiration
paramétrique (8 patterns cliniques dont Cheyne-Stokes/Kussmaul/Biot), visèmes phonétiques
FR/EN, export FHIR R4 + PDF minimal, moteur de conversation avec construction de prompt
système personnalité→instructions, 4 panels NKUI complets. **Ce n'est pas un chantier de
fragments épars : c'est une application quasi complète qui ne compile pas à cause d'un petit
nombre de causes racines mécaniques répétées**, plus un vrai trou (le rendu 3D) et un vrai
manque (aucun asset, aucune app de démo attachée). Le détail ci-dessous sépare précisément
ce qui est mécanique de ce qui est un vrai chantier.

---

## Décision actée (Rihen, 2026-07-25) — PV3DE doit utiliser Noge

**Ce n'est plus une option à peser : PV3DE utilisera Noge** (`Engine/Noge` — `NkApplication`,
`NkLayerStack`, ECS gameplay) comme framework applicatif, et par ricochet NKRenderer (déjà
branché sur Noge via `NkRenderSystem`) pour son rendu.

**Fait important découvert pendant l'audit : ce n'est pas une migration à faire depuis zéro —
c'est une migration déjà commencée dans le code, jamais terminée côté configuration de build.**
Preuves :
- `PatientVirtualApp.h`/`.cpp` hérite déjà de `nkentseu::Application`, inclut déjà
  `"Noge/Core/Application.h"` et `"Noge/Core/NkApplicationConfig.h"`.
- `PatientLayer.h`/`PatientLayer_v3.h` héritent déjà de `nkentseu::Layer`, incluent déjà
  `"Noge/Core/Layer.h"`.
- `MedicalUILayer.h` hérite déjà de `nkentseu::Overlay`.
- `PatientLayer_v3.cpp` inclut déjà `"Noge/Components/Rendering/NkRenderComponents.h"`
  (composants ECS de rendu de Noge).
- `Applications/PV3DE/src/PV3DE/Unkeny.cpp` est un point d'entrée alternatif qui construit
  explicitement un `Noge::NogeApp` (`#include "Noge/NkCore.h"` / `"Noge/Nkentseu.h"`).

**Mais deux problèmes distincts empêchent que ça compile ou fonctionne, à ne pas confondre :**

1. **`Applications/PV3DE/PV3DE.jenga` n'a jamais été mis à jour pour déclarer la dépendance.**
   Comparaison directe avec `Applications/Nogee/Nogee.jenga` (qui, lui, fonctionne) :
   Nogee déclare `nkentseudependson(["Noge", "NKAgent", "NKRL", "NKTensor", "NKAudio",
   "NKMedia", "NKNetwork", "NKRenderer", "NKSL", "NKCollision", "NKPhysics", "NKNavigation",
   "NKSerialization", "NKFileSystem", "NKECS", "NKRHI", ...])`. PV3DE ne déclare que
   `["NKEvent", "NKWindow", "NKGlad", "NKLogger", "NKMath", "NKTime", "NKStream",
   "NKContainers", "NKMemory", "NKCore", "NKPlatform", "NKThreading", "NKRHI", "NKImage",
   "NKFont", "NKUI", "NKGLSlang", "NKSPIRVCross"]` — **`Noge` n'y est pas**, ni `NKECS`,
   `NKRenderer`, `NKSerialization`, `NKFileSystem`. C'est la cause directe de :
   `'Noge/Core/Layer.h' file not found` (×5), `'Noge/Core/Application.h' file not found`,
   et `NKSerialization/JSON/NkJSONReader.h` introuvable dans `NkCaseLoader.cpp` (le fichier
   existe bel et bien — `Kernel/System/NKSerialization/src/NKSerialization/JSON/NkJSONReader.h`
   — ce n'est pas un chemin cassé dans l'absolu, juste jamais mis sur le chemin d'inclusion
   de PV3DE). `extra_includes=["src", "%{Noge.location}/src"]` est déjà présent dans
   `PV3DE.jenga` (copié depuis le pattern Nogee) mais `%{Noge.location}` ne se résout pas tant
   que `Noge` n'est pas déclaré comme dépendance réelle du projet.

2. **Les noms de classes ont dérivé.** Les vraies classes actuelles (vérifiées dans
   `Engine/Noge/src/Noge/Core/NkApplication.h` et `NkLayer.h`) sont **`NkApplication`** (pas
   `Application`) et **`NkLayer`/`NkOverlay`** (pas `Layer`/`Overlay`), dans des fichiers
   **`NkApplication.h`** et **`NkLayer.h`** (pas `Application.h`/`Layer.h`). PV3DE cible les
   anciens noms partout. **Bonne nouvelle vérifiée en détail : ce n'est qu'un renommage, pas
   une refonte** — les signatures de méthodes que PV3DE surcharge déjà
   (`OnInit`/`OnStart`/`OnUpdate(dt)`/`OnRender`/`OnUIRender`/`OnShutdown`/`PushLayer` côté
   application ; `OnAttach`/`OnDetach`/`OnUpdate`/`OnRender`/`OnEvent`/`OnUIRender` côté layer)
   **correspondent exactement 1:1** à l'API réelle de `NkApplication`/`NkLayer` d'aujourd'hui.
   Aucune logique à réécrire ici, seulement des noms de classes et de fichiers d'en-tête.

**Bénéfice confirmé en lisant `NkApplication.h`** : la classe possède déjà
`renderer::NkRenderer *mRenderer` + `GetRenderer()`. Une fois PV3DE hérité proprement de
`NkApplication`, **NKRenderer arrive automatiquement** — pas besoin de le brancher à la main,
contrairement à ce que suggérait l'audit initial (« PV3DE dépend-il de NKRenderer, NKCanvas,
ou de rien ? »). Réponse vérifiée : **PV3DE ne dépend aujourd'hui que de NKRHI brut** (pas de
NKRenderer, pas de NKCanvas) — `NkPatientRenderer`/`NkBSDriver` appellent des méthodes qui
n'existent nulle part sur `NkIDevice`/`NkICommandBuffer` réels (`LoadMesh`, `LoadTexture`,
`BindShader`, `BindTexture`, `BindUniformBuffer`, `NkShaderDesc::vertPath/fragPath`,
`NkTextureDesc::path/generateMips`, `NkBufferDesc::size/usage/cpuAccess`, type `NkMeshHandle`
inexistant) — cf. détail Phase R3 plus bas. Ce code a été écrit contre une API RHI de
convenance imaginée/prévue mais jamais construite ; la vraie couche équivalente
(chargement de mesh/texture par chemin, matériaux, shaders haut niveau) existe réellement
mais **dans NKRenderer**, pas dans NKRHI (confirmé par `renderer::LoadOBJ/LoadGLTF/LoadFBX`
et les codecs texture réels cités dans `Engine/Noge/ROADMAP.md`, piliers 1 et 8).

**Effort estimé pour la migration structurelle** (comparé à la réhabilitation Nogee 68→0,
qui a été bouclée en une seule session de travail le 2026-07-24) : le renommage
`Application`→`NkApplication`/`Layer`→`NkLayer`/`Overlay`→`NkOverlay` + noms de fichiers touche
seulement **5 fichiers** (`PatientVirtualApp.h/.cpp`, `PatientLayer.h`, `PatientLayer_v3.h`,
`MedicalUILayer.h`) et la mise à jour du `nkentseudependson` de `PV3DE.jenga` est une copie
quasi directe de la liste de `Nogee.jenga`. **Estimation honnête : 0,5 à 1 jour**, nettement
plus rapide que Nogee (qui partait de zéro sur le câblage RHIShell) puisque PV3DE avait déjà
anticipé la bonne architecture, juste avec des noms obsolètes.

---

## Audit fichier par fichier

### Core / Diagnostic / Émotion / Corps / Parole / Export — ✅ réel, cohérent, compile
seul (les erreurs viennent uniquement des causes racines listées en catégorie 1 ci-dessous,
pas d'un problème de conception)

| Fichier | Verdict | Note |
|---|---|---|
| `Core/NkClinicalState.h` | ✅ réel | `DeduceEmotion()` : logique physiologie→émotion complète, un bug de seuil `Panic` déjà corrigé (commentaire daté dans le fichier) ; utilise déjà `math::NkClamp` qualifié — ne casse pas la compilation |
| `Core/NkPersonality.h` | ✅ réel, complet | Big Five (OCEAN) + traits médicaux dérivés (`PainUnderstatement`, `CryProbability`, `Verbosity`...) + 3 presets patients + `SavePersonality`/`LoadPersonality` (déclarés, pas encore implémentés — pas dans les `.cpp` audités) |
| `Diagnostic/NkDiagnosticEngine.h/.cpp` | ✅ réel, 255 lignes | BDD embarquée (15 symptômes, 5 pathologies dont IDM/pneumonie/appendicite/migraine/panique) avec critères pondérés + `required`, calcul différentiel réel. `LoadSymptomDatabase`/`LoadDiseaseDatabase` (JSON) = **stubs TODO explicites** — `Data/symptoms.json`/`Data/diseases.json` existent sur disque mais **ne sont jamais lus**, tout tourne sur `RegisterBuiltinData()` en dur |
| `Diagnostic/NkCaseLoader.h/.cpp` | ✅ réel, complet (350 lignes) | Format `.nkcase` entièrement spécifié et implémenté (Load/Save/Validate/ScanDirectory/GetPendingEvents/FindAnswer) + `NkCaseRunner` (exécution temporelle). **Aucun fichier `.nkcase` n'existe sur disque** — jamais testé avec un vrai cas |
| `Emotion/NkEmotionFSM.h/.cpp` | ✅ réel | FSM avec table de transitions (blend time + seuil), sortie voix+corps par état |
| `Face/NkActionUnit.h` | ✅ réel | 46 AU FACS + 5 presets médicaux (`kAU_PainMild/Severe/Anxious/Nauseous/Exhausted`) |
| `Face/NkFaceController.h/.cpp` | ✅ réel, testé en usage | 58 slots AU, interpolation, clignement auto (bug d'intervalle figé déjà corrigé, commentaire daté), regard, solveur blendshapes |
| `Face/NkFaceControllerV2.h/.cpp` | ✅ réel | Étend V1 : flash de micro-expressions, asymétrie faciale. Un `#include "../../PV3DE/src/PV3DE/Face/NkFaceController.h"` fragile (chemin relatif inutilement profond) — fonctionne mais à nettoyer |
| `Body/NkBodyController.h/.cpp` | ✅ réel | Respiration/tremblement/pose/balancement, bruit multi-fréquence pour le tremblement |
| `Body/NkBreathController.h/.cpp` | ✅ réel, le plus poussé | 8 patterns respiratoires cliniques distincts (Normal/Dyspnée/Bradypnée/Hyperpnée/**Cheyne-Stokes**/Kussmaul/**Biot**/Paradoxal) avec vraies formes d'onde |
| `Speech/NkSpeechEngine.h/.cpp` | ✅ réel | Mapping graphème→visème FR (règles digrammes + fallback caractère), 15 visèmes Preston Blair, lip-sync avec blend, réponses génériques câblées |
| `Export/NkFHIRExport.h/.cpp` | ✅ réel | Bundle FHIR R4 (Patient/Observation/Condition/DiagnosticReport) + résumé texte ; bug de champ âge absent déjà corrigé (commentaire daté) |
| `Layers/PatientLayer.h/.cpp` (v2) | ✅ réel, pipeline complet | Diagnostic→Emotion→Face→Body→Breath→Speech→Renderer câblé bout en bout ; **appelle `LoadSymptomDatabase`/`LoadDiseaseDatabase` qui échouent silencieusement** (stub, cf. ci-dessus) |
| `Layers/PatientLayer_v3.h/.cpp` | ✅ réel | Remplace le switch d'émotion manuel par `NkAIDriver` (behavior + conversation), `NkFaceControllerV2` |
| `Panels/DiagnosticPanel.*`, `PatientStatePanel.*`, `ReportPanel.*`, `SymptomInput.*` | ✅ réels, complets | UI NKUI complète : classement différentiel coloré, jauges physio + alarmes clignotantes, export FHIR/PDF avec un vrai mini-générateur PDF texte sans dépendance, recherche/cases à cocher symptômes |
| `Layers/MedicalUILayer.h/.cpp` | ✅ réel | Layout 3 panels + barre rapport + viewport, menu (F1-F6 raccourcis émotion), gestion input souris/clavier/texte |

**Sur cette liste, aucun stub caché, aucune logique bidon.** Le patient a déjà une vraie
physiologie paramétrique — ce qui manque n'est pas la profondeur de simulation mais l'état
caché/le but propre (Phase R2) et le rendu (Phase R3).

### IA conversationnelle — ✅ réel et déjà bien pensé, PAS des fragments qui ne s'assemblent pas

| Fichier | Verdict | Note |
|---|---|---|
| `AI/Conversation/NkConversationEngine.h/.cpp` | ✅ réel, complet | Orchestrateur avec historique borné, **`BuildSystemPrompt()` traduit personnalité→instructions comportementales en langage naturel** (extraversion→verbosité, névrotisme→anxiété, conscienciosité→minimisation de la douleur...) + état clinique + règles impératives (« jamais de jargon médical », « jamais d'auto-diagnostic »). C'est le cœur du patient conversationnel, déjà écrit sérieusement |
| `AI/Backends/NkRulesBackend.h/.cpp` | ✅ réel, fonctionnel tel quel | Fallback sans réseau par mots-clés, toujours disponible |
| `AI/Backends/NkOllamaBackend.h/.cpp` | ✅ réel | Client HTTP POSIX/WinSock écrit à la main (pas de lib externe), construction/parsing JSON Ollama `/api/chat` |
| `AI/Backends/NkClaudeBackend.h/.cpp` | ✅ réel, mais **inerte sans libcurl** | Implémente l'API Messages Anthropic correctement (`x-api-key`, `anthropic-version`, system séparé des messages) **derrière `#ifdef NK_HAVE_CURL`** — sans ce define + le link `-lcurl`, `HttpsPost` retourne toujours `false`. `PV3DE.jenga` ne définit pas `NK_HAVE_CURL` ni ne linke curl aujourd'hui |
| `AI/NkAIDriver.h/.cpp` | ✅ réel | Orchestrateur behavior+conversation, réponse asynchrone via `NkThread`, callback thread-safe |
| `Behavior/NkHumanoidBehavior.h/.cpp` | ✅ réel, 493 lignes, le plus élaboré | Pipeline stimulus→intention→filtrage personnalité→filtrage rôle→sortie (AU/regard/parole/pleurs), mémoire épisodique bornée (32 entrées, décroissance temporelle) |

**Seul vrai trou du sous-système** : aucun `NkNKAIBackend` (branché sur `Kernel/AI/NKGpt`)
n'existe encore — cohérent avec l'état de NKGpt (Palier 6, ~36M paramètres, encore en
formation, pas prêt à porter un dialogue patient réaliste). Voir Phase R4.

### Rendu 3D — ⚠️ le vrai trou architectural, à ne pas patcher à la marge

| Fichier | Verdict | Note |
|---|---|---|
| `Renderer/NkPatientRenderer.h/.cpp` | ⚠️ écrit contre une API qui n'existe pas | Appelle `mDevice->LoadMesh(path)`, `mDevice->LoadTexture(desc)`, `cmd->BindShader/BindTexture/BindUniformBuffer/DrawMesh`, `cmd->SetUniformMat4/Vec3/Vec4/Float` — **aucune de ces méthodes n'existe sur `NkIDevice`/`NkICommandBuffer` réels** (vérifié dans `Kernel/Runtime/NKRHI/src/NKRHI/Core/NkIDevice.h` et `Commands/NkICommandBuffer.h` via les diagnostics clang : `NkShaderDesc` n'a pas `vertPath`/`fragPath`, `NkTextureDesc` n'a pas `path`/`generateMips`, `NkBufferDesc` n'a pas `size`/`usage`/`cpuAccess`, pas de type `NkMeshHandle`). Ce n'est pas un chemin cassé à corriger, c'est une couche de convenance qui n'a jamais existé sous ce nom dans NKRHI |
| `Renderer/NkBSDriver.h/.cpp` | ⚠️ même problème | Pilote GPU des blendshapes, mêmes appels RHI inexistants (`NkBufferUsage::NK_UNIFORM_BUFFER` au lieu du vrai `NkBufferType`, `MapBuffer` retourne `NkMappedMemory` pas `void*`) |

**Verdict et recommandation** : ne pas corriger ces deux fichiers appel par appel contre le
NKRHI brut — les réécrire directement sur **NKRenderer** (une fois disponible via
`NkApplication::GetRenderer()` après la migration Phase R0), en suivant le même schéma
« adaptateur mince sur le vrai système » déjà appliqué 4 fois aujourd'hui côté Noge
(`NkAudioSystem`, `NkAgentSystem`, `NkNetWorldDemo`, `NkNavigationSystem`) : chargement mesh/texture par `renderer::LoadOBJ/LoadGLTF` + codecs réels, matériau/shader custom (Skin/Eye) via le pipeline NKRenderer réel. `NkBSDriver` (upload de poids blendshapes) reste une extension légitime — **NKRenderer n'a pas de système de morph target** aujourd'hui — mais doit être réécrit contre le vrai `NkBufferDesc`/`MapBuffer` et branché comme entrée uniforme d'un matériau custom plutôt que des appels `cmd->Bind*` qui n'existent pas.

**Fait supplémentaire découvert pendant l'audit, à traiter dans la même phase** : **aucun
asset 3D n'existe nulle part dans le dépôt pour PV3DE.** `NkPatientRenderConfig` référence
`Assets/Meshes/patient_body.nkmesh`, `Assets/Meshes/patient_eyes.nkmesh`,
`Shaders/Skin.vert/frag`, `Shaders/Eye.vert/frag`, 7 textures — **aucun de ces fichiers
n'existe** (`find` exhaustif sur `Applications/PV3DE` : zéro `.nkmesh`/`.png`/`.vert`/`.frag`).
Le premier jalon de rendu ne peut donc pas viser le mesh sculpté 46-blendshapes tout de suite —
il doit commencer par un placeholder géométrique simple (capsule/sphères) avec un matériau PBR
standard NKRenderer, la tête FACS réelle étant un chantier d'asset séparé et non estimé ici
(même honnêteté que la section MetaHuman de `Engine/Noge/ROADMAP.md`).

### Point d'entrée — 🔴 conflit réel à résoudre, pas juste un chemin cassé

| Fichier | Verdict | Note |
|---|---|---|
| `PatientVirtualApp.cpp` | ✅ réel, complet, LE bon point d'entrée | `CreateApplication` configure fenêtre/device/titre correctement, pousse `PatientLayer` (v2, pas v3) |
| `Unkeny.cpp` + `UkConfig.h` | 🔴 mort, cassé indépendamment, EN CONFLIT avec `PatientVirtualApp.cpp` | Définit un **second** `nkentseu::CreateApplication` — une fois les deux fichiers compilables, ce serait une violation ODR (erreur de link, symbole dupliqué), pas juste une erreur de compilation. `UkConfig.h` inclut en plus `"NKApplication/Core/NkApplicationConfig.h"` (chemin qui n'existe pas non plus — le vrai est `Noge/Core/NkApplicationConfig.h`), référence une variable globale `args` jamais déclarée, et appelle `std::strcmp` sans `#include <cstring>`. **Recommandation : supprimer `Unkeny.cpp` et `UkConfig.h`** — scaffolding jamais terminé, `PatientVirtualApp.cpp` est déjà la version complète et correcte |
| `Renderer/NkBSDriver` mis à part, PatientVirtualApp::OnInit` | ⚠️ n'attache que `PatientLayer` | `PushOverlay(new MedicalUILayer(...))` est un simple commentaire `// TODO Phase 5` — **l'UI médecin (4 panels) n'est aujourd'hui jamais attachée à l'application**, même une fois la compilation réparée |

---

## Catégorisation des 136 erreurs — 7 causes racines, toutes mécaniques sauf la 6ᵉ

| # | Cause racine | Fichiers touchés (échantillon) | Nature du fix |
|---|---|---|---|
| 1 | **Fonctions NKMath non qualifiées** (`NkMin`/`NkMax`/`NkClamp`/`NkLerp`/`NkSin`/`NkTwoPi`/`NkPi`/`NkPiHalf`/`NkSmoothStep` vivent dans `nkentseu::math`, la plupart des `.cpp` de PV3DE n'ont pas `using namespace nkentseu::math;` ni ne qualifient `math::`) | `NkHumanoidBehavior.cpp`, `NkBodyController.cpp`, `NkBreathController.cpp`, `NkFaceController.cpp`, `NkEmotionFSM.cpp`, `NkSpeechEngine.cpp`, `NkDiagnosticEngine.cpp`, `NkCaseLoader.cpp`, `NkConversationEngine.cpp`, `NkBSDriver.cpp`, tous les Panels | Ajouter `using namespace nkentseu::math;` (déjà fait correctement dans `NkPatientRenderer.h`/`.cpp` et `PatientLayer.h` — juste à généraliser) — **catégorie la plus nombreuse, zéro risque de conception** |
| 2 | **Chemins d'include relatifs faux entre sous-dossiers frères** | `NkHumanoidBehavior.h` inclut `"NkPersonality.h"` (le vrai fichier est `PV3DE/Core/NkPersonality.h`) ; `NkOllamaBackend.h`/`NkClaudeBackend.h`/`NkRulesBackend.h` incluent `"NkConversationEngine.h"` (le vrai est `PV3DE/AI/Conversation/NkConversationEngine.h`) | Remplacer par le chemin complet depuis la racine `src` (convention déjà suivie ailleurs, ex. `"PV3DE/Core/NkClinicalState.h"`) |
| 3 | **Renommage de classes Noge** (`Application`→`NkApplication`, `Layer`/`Overlay`→`NkLayer`/`NkOverlay`, noms de fichiers d'en-tête assortis) | `PatientVirtualApp.h/.cpp`, `PatientLayer.h`, `PatientLayer_v3.h`, `MedicalUILayer.h` | Cf. section « Décision actée » ci-dessus — mécanique, signatures déjà compatibles |
| 4 | **Dépendances jenga manquantes** (`Noge`, `NKECS`, `NKRenderer`, `NKSerialization`, `NKFileSystem` absents de `nkentseudependson` dans `PV3DE.jenga`) | Tout fichier incluant `Noge/*` ou `NKSerialization/JSON/NkJSONReader.h` | Copier la liste `nkentseudependson` de `Nogee.jenga` (fusion R0/R1) |
| 5 | **API NKUI renommée** (`nkui::NkUIRect` n'existe plus — le vrai type est `nkui::NkRect`, confirmé par le message clang « did you mean 'NkRect'? » et absent de `Kernel/Runtime/NKUI`) | `DiagnosticPanel.*`, `PatientStatePanel.*`, `ReportPanel.*`, `SymptomInput.*`, `MedicalUILayer.h` (struct `Layout`) | Renommer `NkUIRect`→`NkRect` partout (à vérifier précisément le nom d'en-tête source pendant R1, la définition n'a pas été localisée avec certitude dans l'audit) |
| 6 | **Rendu écrit contre une API RHI inexistante** — seule cause racine qui n'est PAS un renommage mécanique | `NkPatientRenderer.h/.cpp`, `NkBSDriver.h/.cpp` | Réécriture ciblée sur NKRenderer, cf. Phase R3 |
| 7 | **API conteneurs dérivée** (`NkVector::EraseAt`→le vrai est `RemoveAt`/`Erase` ; `NkString::IsEmpty()`→le vrai est `Empty()`, confirmé par grep direct sur `NkString.h`/`NkVector.h`) | `NkHumanoidBehavior.cpp`, `NkDiagnosticEngine.cpp`, et probablement d'autres fichiers non encore révélés par clang (`-ferror-limit` atteint avant) | Renommage mécanique par recherche globale sur le module |

Corollaire pratique pour R1 : catégories 1, 2, 4, 5, 7 sont un pur travail de recherche/
remplacement disciplinée fichier par fichier, sans risque de régression logique — chaque
fonction/méthode a un équivalent réel exact dans le moteur actuel. Catégorie 3 est couverte
par R0. Catégorie 6 est le seul vrai chantier de conception de cette phase de réhabilitation.

---

## Phases

### Phase R0 — Migration structurelle vers Noge (préalable acté, pas optionnel) — ✅ FAIT (2026-07-25)

Renommage `Application`→`NkApplication`/`Layer`,`Overlay`→`NkLayer`,`NkOverlay` (5 fichiers),
mise à jour `PV3DE.jenga::nkentseudependson` sur le modèle de `Nogee.jenga`, suppression de
`Unkeny.cpp`/`UkConfig.h` (scaffolding mort, en conflit ODR avec `PatientVirtualApp.cpp`).

**Jalon testable** : `PatientVirtualApp` hérite proprement de `NkApplication`, `PatientLayer`
de `NkLayer` — plus aucune erreur de la catégorie 3/4 dans le build. **Atteint.**

**Écart avec l'audit, découvert en exécutant R0** : l'ancien point d'entrée
`nkentseu::Application *nkentseu::CreateApplication(config)` (dans `PatientVirtualApp.cpp`)
ne correspond à **aucun contrat réel du framework actuel** — ni à l'ancien (classe
`Application` supprimée), ni au nouveau (`NkApplication`). L'audit ne l'avait pas détecté car
`PatientVirtualApp.cpp` échouait avant sur les `#include "Noge/Core/Application.h"` manquants
(fatal error), donc clang n'atteignait jamais la définition de `CreateApplication` en bas du
fichier. Le vrai contrat, confirmé en lisant `NKWindow/Core/NkMain.h` (`WinMain` appelle
`nkmain(state)` à portée globale) et `Applications/Nogee/Nogee.cpp` (seul exemple actuellement
compilé de ce pattern) : un point d'entrée global
`int nkmain(const nkentseu::NkEntryState &state)` qui construit lui-même `NkApplicationConfig
config(state);`, instancie l'app, appelle `Init()`/`Run()`. `PatientVirtualApp.cpp` a été
réécrit sur ce modèle (cf. commentaire dans le fichier). Le header `Noge/Core/NkMainApp.h`
documente un pattern alternatif (`NkMainApplication()` + `nkmain()` fourni par le framework)
mais ce header est lui-même cassé (déclaration hors namespace référençant des types non
qualifiés) et n'est utilisé par aucun projet du repo actuellement — non retenu.

### Phase R1 — Réhabilitation compilation (catégories 1, 2, 5, 7 + stub propre du rendu) — ✅ FAIT (2026-07-25)

Qualification `math::`, correction des chemins d'include relatifs, renommage
`NkUIRect`→`NkRect`, renommage `EraseAt`→`RemoveAt`/`Erase` et `IsEmpty`→`Empty` partout où
clang les signale (et vérification manuelle du reste du fichier une fois le premier lot
d'erreurs levé, `-ferror-limit` ayant masqué une partie du signal). **Le rendu
(`NkPatientRenderer`/`NkBSDriver`) est stubé proprement pour cette phase** (méthodes vides
retournant `false`/no-op avec un `logger.Warn` — même principe que la consigne « stuber
proprement, ne pas supprimer » déjà donnée pour les backends LLM, étendu ici au rendu pour
obtenir un premier **build vert** rapidement sans faire la réécriture NKRenderer deux fois).

**Jalon testable** : `jenga build --target PV3DE --config Debug --platform Windows` → **0
erreur**. **Atteint** — `PV3DE.exe` compile et link (`Build/Bin/Debug-Windows/PV3DE/PV3DE.exe`).
Non vérifié dans cette passe : le lancement réel de l'exe (pas de GPU disponible sur la
machine d'exécution de cette tâche — contrainte explicite, compilation uniquement).

**Écarts avec l'audit, découverts en itérant sur de vraies erreurs clang (le `-ferror-limit`
masquait beaucoup plus que prévu — l'audit l'avait anticipé sans pouvoir chiffrer l'ampleur)** :
au-delà des 7 causes racines de l'audit initial, plusieurs **autres couches d'API fictives**
(écrites contre une convenance imaginée, jamais construite dans le moteur réel) ont bloqué la
compilation et ont dû être réécrites contre les vraies signatures :
- **Fonctions string libres inexistantes** (`NkStrEqual`, `NkStrChr`, `NkStrLen`, `NkStrNCpy`)
  utilisées dans `NkCaseLoader.cpp`, `SymptomInput.cpp`, `ReportPanel.cpp`,
  `NkHumanoidBehavior.cpp` → remplacées par `strcmp`/`strchr`/`strlen`/`strncpy` (`<cstring>`,
  déjà inclus partout).
- **`nk_isize` inexistant** (`NkDiagnosticEngine.cpp`, `NkHumanoidBehavior.cpp`) → le vrai type
  signé est `nkentseu::isize` (`NKCore/NkTypes.h`).
- **Constantes trigo inexistantes** `NkTwoPi`/`NkPiHalf` (`NkBreathController.cpp`,
  `NkBodyController.cpp`, `PatientStatePanel.cpp`) → seules `NkPi`/`NkPis2` existent réellement
  dans `NKMath/NkFunctions.h` ; `NkSmoothStep` inexistant (`NkSpeechEngine.cpp`) → formule
  standard `t²(3-2t)` inlinée (edges déjà 0/1 et t déjà clampé dans l'appelant).
- **`NkStyleVar` incomplet** : `ProgressFill`/`SliderFill`/`CheckboxMark` n'existent pas dans
  l'enum réel (`NKUI/NkUIContext.h` : seulement `NK_ITEM_SPACING`, `NK_PADDING_X/Y`,
  `NK_CORNER_RADIUS`, `NK_BORDER_WIDTH`, `NK_ALPHA`, `NK_BUTTON_BG`, `NK_BUTTON_TEXT`,
  `NK_TEXT_COLOR`) — NKUI n'a aujourd'hui aucun hook de couleur par widget sur
  `ProgressBar`/`SliderFloat`/`Checkbox`. Les `PushStyleColor`/`PopStyle` correspondants ont
  été retirés (régression cosmétique documentée en commentaire, pas fonctionnelle) plutôt que
  d'ajouter cette fonctionnalité à NKUI (hors scope R1, impact partagé avec d'autres apps).
- **`NkFileMode`/`NkFile` mal utilisés** dans `ReportPanel.cpp` (`NkFileMode::Write`/`::Text`/
  `::Binary` n'existent pas — vrais flags `NK_WRITE`/`NK_WRITE_BINARY` ; `file.WriteString()`
  n'existe pas — la vraie méthode est `file.Write(const NkString&)`).
- **`NkJSONReader`/`NkArchive`/`NkJSONWriter` API entièrement différente** dans
  `NkCaseLoader.cpp` : `NkJSONReader::ReadArchive()` est **statique** et parse du **texte JSON
  déjà en mémoire** (pas un chemin fichier — il fallait `NkFile::ReadAllText()` d'abord) ;
  `NkArchive::GetInt`/`GetFloat`/`GetNode` n'existent pas (vrais noms : `GetInt64`/`GetFloat32`/
  `GetObject`, ce dernier prenant un `NkArchive&` et pas un `NkArchiveNode&`) ; `NkJSONWriter`
  n'a pas de méthode d'instance `Write(archive, path)` (vrai : `NkJSONWriter::WriteArchive()`
  statique retournant une `NkString`, à écrire soi-même via `NkFile::WriteAllText()`). Les
  méthodes `Load`/`LoadMeta`/`Save` de `NkCaseLoader` ont été réécrites intégralement contre
  cette vraie API (logique métier inchangée).
- **`NkDirectory` mal utilisé** (`ScanDirectory`) : pas de méthode d'instance `Open()`/
  `GetFiles(out, pattern)` — l'API réelle est entièrement statique :
  `NkDirectory::Exists(dir)` + `NkVector<NkString> NkDirectory::GetFiles(dir, pattern)`.
- **`NkThread` signature de callback** (`NkAIDriver.cpp`) : `NkThread::ThreadFunc =
  NkFunction<void(void*)>`, pas `void()` — la lambda passée au constructeur doit accepter un
  paramètre `void*` (même inutilisé), sinon échec de résolution de surcharge dans
  `NkFunction.h`.
- **Événements souris NKEvent** (`MedicalUILayer.cpp`) : `NkUIFontManager::GetDefault()`
  n'existe pas (vrai : `Default()`) ; `NkMouseButton::NK_LEFT`/`NK_RIGHT` n'existent pas (vrais :
  `NK_MB_LEFT`/`NK_MB_RIGHT`, accessibles aussi via les raccourcis `IsLeft()`/`IsRight()`) ;
  `NkMouseButtonEvent` est une **base abstraite non instanciable** (pas de `GetStaticType()`,
  donc `e->As<NkMouseButtonEvent>()` échoue à la compilation) — il faut tester séparément les
  classes feuilles réelles `NkMouseButtonPressEvent`/`NkMouseButtonReleaseEvent` ; `IsPressed()`
  n'existe pas sur l'événement (l'état pressé/relâché est porté par la classe feuille, pas une
  méthode). `NKEvent/NkTextEvent.h` n'existe pas non plus — le vrai type est
  `NkTextInputEvent`, déclaré dans `NKEvent/NkKeyboardEvent.h`.
- **Bug d'include isolé côté `Engine/Noge`** (pas PV3DE) : `Noge/ECS/Components/Rendering/
  NkRenderComponents.h` utilise la macro `NK_COMPONENT(Type)` sans inclure
  `NKECS/Core/NkTypeRegistry.h` où elle est définie — invisible tant que ce header n'est inclus
  qu'à travers le PCH de Noge (qui la pull transitivement) ; devient une erreur dès qu'un
  consommateur externe (ici `PatientLayer_v3.cpp`) l'inclut seul. Corrigé par un include direct
  dans le header Noge (fix minimal, mécanique, ne change aucune logique).
- Chemin d'include supplémentaire cassé non listé par l'audit :
  `PatientLayer_v3.cpp` incluait `"Noge/Components/Rendering/NkRenderComponents.h"` (chemin
  inexistant) au lieu du vrai `"Noge/ECS/Components/Rendering/NkRenderComponents.h"`.

**Progression du nombre d'erreurs** (rebuilds réels, pas des estimations) : 128 erreurs/18
fichiers après R0 (dépendances jenga + renommages Application/Layer/Overlay + suppression
Unkeny) → 21 erreurs/7 fichiers après un premier passage sur les catégories 1/2/5/7 documentées
par l'audit → **0 erreur** après traitement des API fictives ci-dessus, non anticipées par
l'audit initial. Le chiffre de départ exact « 136 erreurs/26 fichiers » de l'audit n'a pas été
re-mesuré tel quel avant de commencer les corrections (le premier build réel de cette session,
après avoir déjà posé les dépendances jenga du R0, donnait 128/18 — cohérent avec l'attente
« R0 réduit une partie des erreurs de catégorie 3/4 » mais ne permet pas de confirmer le 136
initial au chiffre près).
**Estimation initiale : 1–1,5 jour — effort réel plus proche de la borne haute** compte tenu du
volume d'API fictives supplémentaires découvertes (string utils, NkFile, NKSerialization,
NkThread, NKUI StyleVar, NKEvent mouse events) qui n'étaient pas dans le périmètre des 7 causes
racines de l'audit.

**Reste explicitement HORS scope de cette passe R0/R1** (conforme au ROADMAP, reporté aux
phases suivantes) :
- `MedicalUILayer` toujours **jamais attachée** dans `PatientVirtualApp::OnInit` (toujours un
  commentaire `// TODO Phase 5`) — attachement prévu Phase R5, pas R0/R1.
- `PatientLayerV3` (avec `NkAIDriver`) toujours **jamais instanciée** au niveau application —
  seule `PatientLayer` v2 (sans IA) est poussée dans `PatientVirtualApp::OnInit` — arbitrage
  v2/v3 prévu Phase R5.
- Rendu 3D toujours stubé (`NkPatientRenderer::Init()` retourne `false` volontairement) — vraie
  réécriture NKRenderer prévue Phase R3.
- Lancement réel de `PV3DE.exe` non vérifié (compilation uniquement, contrainte machine sans
  GPU pour cette tâche).

### Phase R2 — État caché + comportement propre (NKAgent/NKCivilization)

Brancher le patient sur les fondations livrées aujourd'hui côté `Kernel/AI` : le patient
devient une entité **NKCivilization/NKAgent** avec perception limitée aux questions posées/
tests exécutés (pas un accès direct à `NkClinicalState` complet), mémoire des interactions
(déjà amorcée côté `NkHumanoidBehavior::RememberEvent`/`RecallEvent`, à connecter à la vraie
mémoire `NkAgentMemory` avec pondération d'importance livrée le 2026-07-25), et un **but caché**
au sens NKAgent (ex. « dissimuler ma toxicomanie », « minimiser ma douleur par fierté »)
distinct de l'état clinique réel — la connaissance du patient sur sa propre condition devient
une variable indépendante de la vérité terrain (peut savoir et cacher, peut ignorer). C'est un
vrai nouveau sous-système : ni `NkClinicalState` (état terrain) ni `NkHumanoidBehavior` (réaction
à stimulus) n'ont aujourd'hui de notion de « ce que le patient sait de lui-même » vs « ce que
l'étudiant a découvert » — ces deux vues doivent diverger par construction.

**Jalon testable** : un cas `.nkcase` avec pathologie réelle X caché derrière un discours du
patient qui la minimise/l'ignore selon son but ; le différentiel affiché à l'étudiant
(`NkDiagnosticEngine`) ne progresse QUE via les symptômes que l'étudiant a explicitement
ajoutés (déjà le cas aujourd'hui) ET les réponses du patient reflètent le but caché (nouveau).
**Estimation : 2–3 jours** — plus long que les ponts ECS déjà livrés le même jour côté Noge
(`NkAgentComponent`/`NkAgentSystem`, `NkAudioSystem`...) car ceux-ci reliaient deux systèmes
déjà fonctionnels l'un à l'autre ; ici il faut en plus concevoir le mécanisme de « vision
partielle / vérité terrain qui diverge » qui n'existe nulle part encore, ni côté NKAgent ni
côté PV3DE.

### Phase R3 — Rendu réel via NKRenderer

Réécriture de `NkPatientRenderer`/`NkBSDriver` contre le vrai NKRenderer (accessible via
`NkApplication::GetRenderer()` depuis R0) : chargement mesh/texture réel
(`renderer::LoadOBJ/LoadGLTF` + codecs), matériau/shader Skin+Eye custom, buffer blendshapes
sur la vraie API `NkBufferDesc`/`MapBuffer`. Un placeholder géométrique simple (capsule corps +
sphères yeux, matériau PBR standard) sert de premier jalon — **aucun asset 3D n'existe
aujourd'hui**, la tête sculptée 46-blendshapes est un chantier d'art séparé, non estimé ici.

**Jalon testable** : premier viewport patient affiché dans `MedicalUILayer` (actuellement du
texte placeholder « Patient 3D — en attente du rendu GPU »), au minimum une forme géométrique
simple texturée qui respire (déplacement thoracique de `NkBreathController` visible) et qui
cligne des yeux — même sans le mesh facial final.
**Estimation : 1,5–2,5 jours** (réécriture du code de rendu ~1–1,5 jour sur le modèle
« adaptateur mince » déjà prouvé 4× côté Noge aujourd'hui, + création/sourcing d'un
placeholder géométrique minimal ~0,5–1 jour).

### Phase R4 — Backend conversationnel (Ollama/Claude maintenant, NKAI en tête de cible)

`NkConversationEngine` a déjà l'interface backend-agnostique voulue
(`NkIConvBackend`, même schéma que `NkIEditorRenderer` vu ailleurs aujourd'hui) — rien à
redessiner ici. Travail réel : (1) réparer la compilation des backends existants via R1,
(2) lier `libcurl` + définir `NK_HAVE_CURL` dans `PV3DE.jenga` pour activer réellement
`NkClaudeBackend` (aujourd'hui toujours `false` faute de lien), (3) tester `NkOllamaBackend`
avec un modèle local réel, (4) ajouter une **classe stub `NkNKAIBackend`** (implémente
`NkIConvBackend`, retourne indisponible) enregistrée dans `NkConvBackendType` — **place
réservée, pas d'intégration réelle avant que `Kernel/AI/NKGpt` soit prêt** (actuellement
Palier 6, ~36M paramètres, encore en formation).

**Ordre de priorité à respecter, tel que fixé par Rihen** : Ollama/Claude sont les options
utilisables **maintenant** pour prototyper/tester le patient ; **NKAI/NKGpt est la cible
stratégique par défaut** dès qu'il sera mûr pour porter un dialogue patient réaliste — ce
choix doit rester documenté ici, pas décidé une fois pour toutes en dur dans le code (d'où
l'interface déjà backend-agnostique).

**Jalon testable** : une question texte posée depuis `MedicalUILayer` obtient une réponse
réelle d'Ollama local (ou du `NkRulesBackend` en repli), affichée + parlée (visèmes) par le
patient.
**Estimation : 0,5–1 jour** (la logique difficile — traduction personnalité→prompt — est déjà
écrite ; il s'agit de compilation, de liaison libcurl, et de tests d'intégration).

### Phase R5 — Boucle de diagnostic jouable (première démo)

Pas besoin d'une app de démo séparée (contrairement à Noge) : `PV3DE` est déjà une
`windowedapp` interactive — **PV3DE.exe lui-même devient la démo**, une fois R0-R4 faits.
Reste concrètement : (1) attacher `MedicalUILayer` dans `PatientVirtualApp::OnInit` (aujourd'hui
un simple commentaire `// TODO Phase 5`), (2) trancher `PatientLayer` (v2) vs `PatientLayerV3`
(v3, avec `NkAIDriver`) comme couche canonique de l'app — actuellement seule v2 est instanciée,
v3 n'est câblée nulle part au niveau application, (3) écrire un premier `.nkcase` réel (aucun
n'existe sur disque aujourd'hui) avec une pathologie cachée simple (ex. IDM déjà modélisé dans
`NkDiagnosticEngine::RegisterBuiltinData`), (4) vérifier la boucle complète : questions au
patient (texte ou clic rapide) → prise de constantes (sliders déjà réels dans `SymptomInput`)
→ différentiel affiché (déjà réel) → verdict de l'étudiant comparé à `NkCaseData::correctDiagnosis`
(champ déjà présent, comparaison finale à écrire).

**Jalon testable** : un cas complet jouable de bout en bout — lancement de PV3DE.exe,
interrogatoire + un test vital + verdict final comparé à la vérité terrain cachée, sans aucun
menu qui révèle directement la pathologie.
**Estimation : 1–1,5 jour** (surtout de l'assemblage et un premier contenu clinique, le code
sous-jacent existe déjà à ~90 %).

---

## Total honnête

**R0+R1 (build vert) : 1,5–2,5 jours · R2 (état caché) : 2–3 jours · R3 (viewport minimal) :
1,5–2,5 jours · R4 (conversation) : 0,5–1 jour · R5 (boucle jouable) : 1–1,5 jour.**
**Total : ~6,5 à 10,5 jours** pour un patient virtuel jouable minimal avec état caché réel,
premier rendu NKRenderer, et un canal conversationnel fonctionnel — cohérent en forme avec la
vélocité réelle déjà observée sur ce dépôt (plusieurs piliers Noge livrés et prouvés par
exécution en une seule journée chacun, 2026-07-23 à 2026-07-25), sans sous-estimer les deux
vrais chantiers de conception (R2 état caché, R3 assets/rendu) qui ne sont pas de simples
renommages.

Aucune implémentation n'a été commencée pendant cet audit — document soumis pour validation
avant tout développement, même processus que pour Noge.
