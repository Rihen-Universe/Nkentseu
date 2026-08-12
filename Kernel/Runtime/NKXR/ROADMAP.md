# NKXR — Roadmap

Runtime VR/AR/XR **from scratch** de Nkentseu (`nkentseu::xr`, zéro STL) : sessions,
espaces, poses horodatées et prédites, entrées par actions, swapchains par œil,
couches de composition. Plan directeur : `XR_MISSION_IA.md` (racine). Philosophie :
NOTRE runtime, posé à l'étage 2 sur le loader OpenXR (Khronos, Apache 2.0 — seul
accès réaliste aux casques) comme nos codecs se posent sur les specs des formats.

État actuel (2026-08-10) : **étage 0 livré côté code** — module + backend
SIMULATEUR desktop + démo `NKXRDemo` (stéréo côte à côte, souris = tête), self-test
**66/66**. En attente de validation par Rihen (cycle TEST → VALIDATION → INTÉGRATION).

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| **Étage 0 — module + simulateur (aucun matériel)** | | | |
| Types : temps XR (ns), yeux, FOV asymétrique signé (XrFovf-like) | ✅ | — | — |
| Projection perspective asymétrique (xrCreateProjectionFov-like, [-1,1] et [0,1]) | ✅ | — | — |
| NkXrPose : monde/vue analytiques, composition, inverse | ✅ | — | — |
| Vitesses (linéaire+angulaire) + extrapolation bornée ±100 ms (prédiction) | ✅ | — | — |
| Espaces VIEW / LOCAL (ancre yaw-seul au Begin) / STAGE (sol) | ✅ | — | — |
| NkXrSwapchain : discipline Acquire/Release, images opaques (app) | ✅ | — | — |
| Couches : projection (pose+FOV du rendu, prêt pour la reprojection) + quad | ✅ | — | — |
| Entrées par ACTIONS (usage sémantique, états bool/float/vec2/pose) | ✅ | — | — |
| NKIXrBackend + fabrique (pattern NkDeviceFactory) | ✅ | — | — |
| NkXrSession : machine d'états OpenXR, garde-fous journalisés | ✅ | — | — |
| Backend SIMULATEUR : souris raw = tête, ZQSD/WASD, latence simulée, pose scriptable | ✅ | — | — |
| Self-test numérique (66 CHECK : projection, poses, prédiction, cycle, actions) | ✅ | — | — |
| Démo NKXRDemo : scène NKRenderer en stéréo simulée, crochets d'agent | ✅ | — | — |
| Validation Rihen de l'étage 0 | ✅ | — | — |
| **Étage 1 — stéréo DANS NKRenderer (coordination requise)** | | | |
| Note de coordination dans la ROADMAP NKRenderer | ✅ | — | — |
| Frustum décentré `NkCamera3D` (useFovAsym, convention XrFovf) + démo qui le consomme | ✅ | — | — |
| Validation Rihen de l'étage 1 | ⏳ | — | P0 |
| Anti-aliasing PERFORMANT en XR (exigence Rihen 2026-08-10 ; MSAA œil / TAA stéréo / supersampling) | ❌ | L | P1 |
| Rendu deux vues sans double graphe (une shadow map, un culling ; puis multiview Vulkan) — NOUVELLE note requise | ❌ | L | P2 |
| **Étage 2 — VR réelle : backend OpenXR → Quest 2** | | | |
| 2a — En-têtes OpenXR 1.1.49 (Externals, Apache 2.0 au registre) + loader DYNAMIQUE + découverte runtime actif + instance/système/tailles — **PROUVÉ sur Quest 2 réel** (« Oculus Quest2 — 2080x2096 par œil », Link, 2026-08-11) | ✅ | — | — |
| 2b.1 — Liaison Vulkan (XR_KHR_vulkan_enable + crochet NKRHI pickPhysicalDevice), session réelle, états pilotés par le runtime, xrWaitFrame/Begin/End (sans couches), xrLocateViews/xrLocateSpace — **PROUVÉ sur Quest 2** (session créée, 300 frames, 2026-08-11) | ✅ | — | — |
| 2b.2 — Swapchains Vulkan du runtime + soumission des couches → l'IMAGE dans le casque (échelle de rendu réglable dès le départ) | ❌ | L | P1 |
| 2b.3 — Actions réelles : profils Touch + simple, états bool/float/vec2, poses de main 6DoF, HAPTIQUE, locomotion stick (démo : main dessinée) | ✅ | — | — |
| Validation Rihen : manettes en main — **OK 2026-08-12** (« c'est bon pour l'instant ») | ✅ | — | — |
| Main GAUCHE symétrique : usages _LEFT (pose/grab/haptique) + liaisons — indispensable Camrail (pupitre à deux mains) | ❌ | S | P2 |
| Vraies mains sans manettes : XR_EXT_hand_tracking (26 articulations/main, rayon + validité par articulation ; démo = squelette de perles) | ✅ | — | — |
| Préréglage VR vers 72 Hz : bloom OFF, ombres 1024/2 cascades (NK_XR_BLOOM, NK_XR_SHADOW_RES, NK_XR_SHADOW_CASCADES) | ✅ | — | — |
| **ANTI-ALIASING — PRIORITÉ N°1 DE LA REPRISE (Rihen, 2026-08-12)** : MSAA sur cibles d'œil, ou supersampling ≥1,2× ; le FXAA ne lisse pas assez en casque | ❌ | L | **P0** |
| Validation Rihen : mains sans manettes — ⚠️ BLOQUÉ CÔTÉ META : le runtime Link n'expose pas `XR_EXT_hand_tracking` (36 extensions listées, vérifié `NK_XR_LIST_EXT`). Activer « Fonctionnalités du runtime pour développeurs » (Meta Horizon → Paramètres → Bêta). Notre chaîne s'activera sans recompiler. | 🔶 | — | P1 |
| `XR_KHR_visibility_mask` : ne pas rendre les pixels invisibles à travers la lentille (~15-25 % des pixels) — meilleur gain/risque vers le 72 Hz | ❌ | M | P1 |
| `XR_META_performance_metrics` : timings CPU/GPU du compositeur (mesurer au lieu de deviner) | ❌ | S | P2 |
| `XR_FB_display_refresh_rate` : choisir 72/80/90 Hz au lieu de la subir | ❌ | S | P2 |
| Accessoires en main : maillage quelconque sur AIM/GRIP (arme, gant, levier) — boucle NK3DModeler → VR | ❌ | S | P3 |
| Actions → profils d'interaction (traduction usage → chemins, DANS le backend) | ❌ | M | P2 |
| APK Quest 2 via la chaîne jenga Android existante | ❌ | M | P1 |
| Pico (même code, second runtime = preuve de portabilité) | ❌ | S | P3 |
| **Étage 3 — AR téléphone (Android d'abord)** | | | |
| Caméra plein écran (NKCamera) + rendu 3D par-dessus | ❌ | M | P2 |
| Marqueurs from scratch (seuillage, quads, homographie, PnP) | ❌ | L | P2 |
| IMU (gyro/accél via NKEvent Android) pour stabiliser | ❌ | M | P3 |
| SLAM/VIO (chantier recherche, lien NKAI) | 🚫 | — | — |
| **Étage 4 — XR : composition passthrough + API unifiée** | ❌ | L | P3 |

Légende : ✅ Livré · 🔶 Partiel · ⏳ En cours · ❌ TODO · 🚫 Différé (assumé)

---

## Preuves (ce que le self-test discrimine)

| cas | pourquoi il discrimine | résultat |
|---|---|---|
| bords du FOV asymétrique → x/w, y/w = ±1 exacts | une symétrisation en douce (moyenne des angles) rate les 4 bords | ✅ |
| cas symétrique == `NkMat4::Perspective` (16 coefficients) | NKXR et NKRenderer doivent parler la MÊME projection | ✅ |
| `ToViewMatrix * ToMat4 == identité` | LA propriété d'une matrice de vue ; un conjugué oublié la casse | ✅ |
| yaw +90° : avant (-Z) → (-X) ; pitch +45° : avant monte en +Y | un signe ou un ordre yaw/pitch inversé se voit immédiatement | ✅ |
| écart des yeux == IPD, porté par la droite TOURNÉE de la tête | un offset en coordonnées monde (non tourné) échoue à yaw 90° | ✅ |
| vue à T+50 ms avec ω = 1 rad/s → yaw 0,15 rad | un LocateViews qui ignore displayTime rend 0,10 | ✅ |
| prédiction à +10 s == +100 ms | la fenêtre d'extrapolation doit être bornée | ✅ |
| LOCAL : tête du Begin à l'origine, avant -Z, même tournée en STAGE | c'est la définition de LOCAL (ancre yaw-seul) | ✅ |
| double Acquire refusé ; EndFrame refusé si image acquise | le bug de synchro que les runtimes réels sanctionnent en silence | ✅ |
| 2e AttachActionSet refusé ; Begin hors READY refusé | les garde-fous que le Quest exigera | ✅ |

Self-test : `Kernel/Runtime/NKXR/tests/test_xr.cpp` — **66 OK, 0 échec**
(sortie dans `logs/app.log` ; l'unique `[ERR]` du journal est le refus ATTENDU
du cas « EndFrame avec image encore acquise »).

---

## Livré

### Étage 0 — module + simulateur + démo (2026-08-10)
- `src/NKXR/NkXrTypes.h` — temps ns (int64, aligné XrTime), yeux, états de
  session (machine OpenXR complète dès le simulateur), `NkXrFov` en radians
  signés, `NkXrProjectionFromFov` (la perspective asymétrique n'existait nulle
  part dans NKMath ; elle vit ici car un HMD est son seul consommateur).
- `src/NKXR/NkXrPose.h` — pose = XrPosef (rotation puis translation), vue =
  inverse analytique, `NkXrAngularVelocity` (log du delta) + `NkXrExtrapolate`
  (exp), `NkXrTimedPose::PredictAt` borné ±100 ms.
- `src/NKXR/NkXrSpace.h` · `NkXrSwapchain.h` · `NkXrLayer.h` · `NkXrInput.h` —
  voir la synthèse. Choix assumé : les liaisons passent par un **usage
  sémantique** (enum), la traduction vers les chemins OpenXR vivra dans LE
  backend OpenXR, jamais dans les applications.
- `src/NKXR/NKIXrBackend.h` + `NkXrSession.{h,cpp}` — session vigilante : toute
  transition illégale est refusée ET journalisée.
- `src/NKXR/Backend/NkXrSimulatorBackend.{h,cpp}` — tête yaw/pitch en radians
  bruts (le wrap de NkAngle casserait un yaw cumulé), orientation par matrices
  RotY*RotX puis quaternion (ordre non ambigu), souris brute (`MouseRawDeltaX/Y`,
  sans accélération OS), ZQSD accepté à côté de WASD (AZERTY), historique
  horodaté 64 échantillons + latence simulée (`NK_XR_SIM_LATENCY_MS`), pose
  scriptable (`NK_XR_SIM_POSE`) pour les captures déterministes.
- `Applications/NKXRDemo/` — compositeur For2D (possède la frame) + un renderer
  ForGame PAR ŒIL en offscreen partagé (patron NK3DModeler/NkAnimaEditor,
  AUCUNE passe NKRenderer modifiée), composition côte à côte par
  `Render2D::DrawImage`, boucle de frame XR complète, action « sélectionner »
  branchée (clic gauche → le cube rougit). Crochets : `NK_XR_SHOT`,
  `NK_XR_SHOT_PREFIX`, `NK_XR_EXIT` ; animation cadencée sur l'index de frame
  (déterminisme des captures).

## En cours / TODO immédiat
- ⏳ Validation Rihen de l'étage 1 (frustum décentré) — preuves : FOV symétrique
  = 0,000 % d'écart avec la référence à travers TOUT le pipeline ; FOV
  asymétrique (-49/39/41/-45°) = 51,5 % des pixels, verticales droites.
- Journal des retours étage 0 (corrigés, validés par Rihen) : dérive de tête
  souris immobile (piège NKEvent `MouseRawDeltaX` jamais consommé → le backend
  accumule lui-même, `0b8a6bb1`) ; clignotement des ombres en mouvement
  (swimming des cascades → `autoFitDirectional` sur les deux yeux, `87e80007`).

## À venir

### Intégration Noge (demande Rihen, 2026-08-12) — dès que l'étage 2 est stable
Faire de l'XR une CAPACITÉ du framework, pas un bricolage par application :
tout ce que `NKXRDemo` câble à la main (session avant device, exigences
Vulkan → NkDeviceInitInfo, un renderer par œil, SubmitEyes, pompe
d'événements XR) remonte dans un sous-système Noge (`NkXrSubsystem` ou couche
dédiée de la LayerStack) : l'app déclare « je veux l'XR », Noge orchestre —
boucle de frame calée sur xrWaitFrame quand un casque est lié, tête/mains
exposées au gameplay (composants ECS ou contrôleur caméra), actions XR
pontées vers l'abstraction d'entrées de Noge, repli simulateur/desktop
automatique. ⚠️ `Engine/Noge` est tenu par un autre chantier
(CONTINUATION.md) : NOTE DE COORDINATION obligatoire avant d'y écrire —
précédent à imiter : `Integrations/NKGui/NkGuiRHIBackend`.
- **Anti-aliasing performant en XR** (exigence Rihen, 2026-08-10) : le FXAA du
  préréglage ForGame ne suffira pas en casque (l'aliasing scintille au moindre
  mouvement de tête). Pistes consignées dans la note de coordination NKRenderer ;
  à arbitrer AVEC ce chantier, pas en silo.
- Étage 1 (suite) : partage d'un graphe pour deux vues (une shadow map, un
  culling — aujourd'hui tout est ×2), puis multiview Vulkan — NOUVELLE note de
  coordination obligatoire (touche RenderGraph et passes).
- Étage 2 : loader OpenXR (Externals/ + licence au registre), backend Quest 2, APK.
- Étage 3 : AR Android (NKCamera + marqueurs from scratch + IMU).

## Bugs / quirks connus
- Piège NKEvent documenté (à remonter au chantier NKEvent) :
  `NkInput.MouseRawDeltaX/Y` garde le delta du DERNIER événement sans reset par
  frame — tout consommateur qui l'intègre par frame dérive. NKXR s'en protège
  en accumulant ses propres `NkMouseRawEvent`.
- Les swapchains portent des handles d'images OPAQUES à l'étage 0 (les cibles
  GPU appartiennent à la démo) ; le runtime OpenXR fournira les vraies images à
  l'étage 2. La discipline Acquire/Release, elle, est déjà réelle et testée.
- Redimensionner la fenêtre recrée cibles d'œil + swapchains (reconstruction de
  graphe ×2) : correct mais coûteux — ne pas redimensionner en continu.

## Dépendances
- **Couches en dessous (utilisées)** : NKCore (types), NKMath (Vec/Mat/Quat/Angle),
  NKMemory (fabriques), NKContainers (NkVector), NKLogger, NKTime (NkChrono
  monotone — horodatage des poses), NKEvent (NkInput : souris brute, clavier),
  NKWindow (fenêtre hôte du simulateur, taille/minimisation)
- **Couches OS (à venir)** : loader OpenXR (étage 2), Camera2/IMU Android (étage 3)
- **Modules au-dessus qui en dépendent** : `Applications/NKXRDemo` (étage 0) ;
  à terme NKRenderer (stéréo, étage 1) et la chaîne AR (NKCamera)
