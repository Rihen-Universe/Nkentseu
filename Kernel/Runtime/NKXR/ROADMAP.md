# NKXR — Roadmap

> 📖 **Comment s'en servir : [USAGE.md](USAGE.md)** — le guide complet (les cinq
> idées de l'XR, la boucle dans l'ordre exact, entrées par actions, mains,
> casque réel, tous les réglages, les repères de performance mesurés et le
> tableau symptôme → cause → remède). Le chapitre **AR** y sera ajouté quand
> l'étage 3 sera livré, sur le même modèle que la partie VR.

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
| **Étage 3 — AR (desktop d'abord, Android ensuite)** | | | |
| Marqueurs from scratch (Otsu, contours de Moore, quads, homographie DLT, marque d'orientation, pose plane) — self-test 24/24 | ✅ | — | — |
| Session AR : conversion, suivi par identifiant, vie d'un marqueur, lissage slerp | ✅ | — | — |
| NKARDemo : caméra → vidéo plein écran → **cube + axes ancrés sur le marqueur** — **PROUVÉ à l'écran** (id 45 à 0,74 m, capture Rihen 2026-08-12) | ✅ | — | — |
| `NkArWorld` : carte de marqueurs, objets en coordonnées MONDE, extension de proche en proche | ✅ | — | — |
| `NkArFlow` : rotation de la caméra mesurée SUR L'IMAGE quand aucun marqueur n'est vu (points saillants + vignettes + ajustement rotation pure) — self-test 60/60 | ✅ | — | — |
| **Suivi par l'image en PYRAMIDE (multi-échelle) — proposé par Rihen, chantier suivant** | ⏳ | M | **P1** |
| Coût mesuré du monde (suivi image compris) : **3,1 à 4,7 ms/image** en 640×480 sur RTX 3070 | 🔶 | S | P2 |
| Translation de la caméra (parallaxe) : non mesurable sans profondeur — d'où l'IMU puis le SLAM plus bas | 🚫 | — | — |
| Vidéo en vrai FOND 3D (quad texturé/fond de graphe) pour un objet PBR ombré au lieu du filaire | ❌ | M | P1 |
| Calibration caméra au damier (aujourd'hui : intrinsèques supposées, ~10 % d'erreur de distance) | ❌ | M | P2 |
| Seuillage adaptatif : validé caméra, à rendre fiable sur images de synthèse | 🔶 | S | P2 |
| Portage Android (caméra du téléphone + IMU) | ❌ | L | P2 |
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

## 🎯 PLAN DE REPRISE (arrêté avec Rihen, 2026-08-12) — dans cet ordre

1. **ANTI-ALIASING** *(priorité n°1 absolue)* — que tout devienne lisse dans le
   casque. Trois leviers, du moins au plus intrusif : supersampling
   (`NK_XR_RENDER_SCALE` ≥ 1,2 — zéro code, coûte du GPU, donc dépend de 2-3) ;
   **MSAA sur les cibles d'œil** (le bon outil en forward — touche la création
   des cibles et les passes → note de coordination NKRenderer) ; TAA stéréo
   (⚠️ DEUX historiques, un par œil, sinon fantômes garantis).
2. ✅ **`XR_KHR_visibility_mask`** — géométrie exposée côté NKXR (mesuré sur
   Quest 2 : 52 sommets / 52 triangles cachés par œil). Reste sa
   CONSOMMATION (prépasse profondeur/stencil) dans NKRenderer → coordination.
3. ✅ **`XR_META_performance_metrics`** — livré, et **décisif** : c'est lui qui
   a montré `app CPU 10-24 ms / app GPU 0,01 ms`, donc un blocage CPU et non
   une charge GPU → bulle de synchronisation levée → **39-51 i/s instables
   sont devenus 65-70 i/s** (2026-08-12). Mesurer AVANT d'optimiser : la
   leçon est acquise deux fois.
4. ✅ **`XR_FB_display_refresh_rate`** — livré (`NK_XR_HZ`). Ce Quest 2 via
   Link ne propose que 72 Hz : rien à choisir ici, mais l'API est prête pour
   un Quest 3 (72/80/90/120).
5. Puis : graphe partagé deux-vues (ombres/culling ×1), profondeur soumise au
   compositeur, intégration Noge, main gauche `_LEFT` (requis Camrail).

## 👁️ VOIR CE QUE VOIT LE CASQUE — sur écran, puis sur plusieurs postes
*(besoin Camrail : « le moniteur voit ce que fait son étudiant et vice-versa »)*

| Niveau | État | Comment |
|---|---|---|
| **Écran local** — la fenêtre PC montre la vue du porteur | ✅ | `NK_XR_SPECTATOR` (défaut avec casque) : un œil plein écran, proportions respectées. Coût nul, l'image est déjà rendue. |
| **Caméra libre du formateur** (regarder ailleurs que le stagiaire) | ❌ S | Un 3ᵉ renderer avec sa propre caméra sur le même device — patron déjà éprouvé (c'est exactement ce que fait la démo pour les 2 yeux). |
| **Poste distant — état répliqué** *(voie recommandée)* | ❌ M | NKNetwork réplique poses + état de scène ; chaque poste REND en local. Bande passante minuscule, image nette, et le formateur peut regarder où il veut. C'est ce qu'attend un simulateur de formation. |
| **Poste distant — flux vidéo** *(voie de secours)* | ❌ M | NKMedia a déjà l'encodeur **H.264 from scratch** + le lecteur : encoder la vue moniteur, l'envoyer, la décoder. Utile pour un poste sans GPU ou une diffusion salle. |
| **Plusieurs casques dans la MÊME scène** (formateur + stagiaire en VR) | ❌ L | Réplication d'état + une session XR par poste. L'API NKXR est déjà par-session, rien n'y fait obstacle. |

## En cours / TODO immédiat
- ⏳ Validation Rihen de l'étage 1 (frustum décentré) — preuves : FOV symétrique
  = 0,000 % d'écart avec la référence à travers TOUT le pipeline ; FOV
  asymétrique (-49/39/41/-45°) = 51,5 % des pixels, verticales droites.
- Journal des retours étage 0 (corrigés, validés par Rihen) : dérive de tête
  souris immobile (piège NKEvent `MouseRawDeltaX` jamais consommé → le backend
  accumule lui-même, `0b8a6bb1`) ; clignotement des ombres en mouvement
  (swimming des cascades → `autoFitDirectional` sur les deux yeux, `87e80007`).

## À venir

### 📱 Déployer sur téléphone — la bonne commande

`jenga run` **installe et lance** sur l'appareil : inutile d'appeler `adb`
soi-même (rappel de Rihen, 2026-08-12).

```
jenga run NKARDemo --platform android --config Release --build
jenga run NKARDemo --platform android --config Release --device RFCT701YSSM
```

`--device` (alias `--target`) ne sert que si plusieurs appareils sont branchés —
un émulateur qui traîne suffit à rendre le choix ambigu. Pour raccourcir
l'attente pendant la mise au point : `--android-abis arm64-v8a` au build, car
l'APK universel construit les quatre architectures.

Pré-requis vérifiés le 2026-08-12 sur ce poste : `JAVA_HOME` = JDK 17,
`ANDROID_SDK_ROOT` = `C:\Android`, `debug.keystore` présent (2666 octets).
`adb` n'est PAS dans le PATH : il vit à `C:\Android\platform-tools\adb.exe`.

**Si l'appareil passe `offline`** : ce n'est pas le paquet, c'est la liaison.
Déverrouiller l'écran et accepter la demande d'autorisation de débogage ;
sinon basculer le mode USB sur « transfert de fichiers », ou révoquer les
autorisations de débogage dans les options développeur et rebrancher.

### 🌍 PERCEPTION COMPLÈTE — décision de Rihen, 2026-08-12 : « la totale »

Demande explicite : SLAM, détection de lignes, de plans, de surfaces, de formes.
J'ai signalé le coût, Rihen a confirmé : c'est donc le cap. Ce qui suit n'est pas
une promesse mais un ordre de marche, bâti sur une règle : **chaque étage doit
servir seul et se prouver seul**. Aucun ne dépend de l'achèvement du suivant.

**La méthode, avant la liste.** Tout ce qui a marché aujourd'hui a marché parce
qu'on fabriquait la scène dont on connaissait la réponse (image tournée par
K·R·K⁻¹, sujet fixe collé, mur uni) et qu'on mesurait l'écart. Tout ce qui a
échoué a échoué sur un seuil posé au jugé. **Chaque étage ci-dessous s'ouvre par
son test à vérité connue, et ne se referme que sur un chiffre.** C'est ce qui
rend un tel chantier faisable seul ; sans cela il s'effondre en devinettes.

| # | Étage | Ce qu'il apporte SEUL | Coût honnête |
|---|---|---|---|
| P1 | **Téléphone** (cible Android NKARDemo) | l'AR marqueur dans la main, démontrable en clientèle | jours |
| P2 | **Gyroscope** (`ASensorManager`) | rotation exacte sans texture ; rend `NkArFlow` inutile pour la rotation | jours |
| P3 | **Carte multi-marqueurs** (déjà écrite, à éprouver) | **translation exacte** partout où un marqueur est vu | ~1 semaine (épreuve + correction de dérive) |
| P4 | **Suivi planaire par homographie** | translation en s'éloignant du marqueur, tant qu'on regarde SON plan | 1–2 semaines |
| P5 | **Détection de lignes** (gradient → non-max → segments) | structure de la scène ; entrée des points de fuite et des rectangles | ~1 semaine |
| P6 | **Détection de plans sans marqueur** | sol et murs trouvés seuls. Raccourci réel : l'accéléromètre donne la VERTICALE gratuitement, donc la normale du sol | 1–2 semaines après P4 |
| P7 | **SLAM / VIO monoculaire** | la pièce reconnue sans aucun marqueur | **3 à 6 mois** |
| P8 | **Formes** (rectangles, cercles, primitives) | objets reconnus, pas seulement des plans | semaines après P5+P6 |

**Détail de P7, pour qu'il ne soit pas un mot magique.** Il se décompose en :
front-end de points suivis (partiellement acquis avec `NkArFlow`) · initialisation
par homographie ou matrice essentielle · triangulation · pose depuis
correspondances 3D-2D (PnP) · images-clés et carte locale · **ajustement de
faisceaux** (optimisation creuse, Gauss-Newton/Levenberg-Marquardt avec
complément de Schur) · préintégration inertielle · fermeture de boucle et
relocalisation. La pièce la plus dure n'est aucune des briques mais
**l'optimiseur creux**, à écrire : NKMath n'a pas de solveur creux, et
l'autodiff de NKAI ne remplace pas un LM structuré par blocs.

**Honnêteté sur l'échelle.** ARCore et ARKit ont mobilisé des équipes pendant des
années. Ce qui est visé ici n'est pas de les égaler mais d'obtenir un système qui
tient debout sur nos cas : une carte, une table, une pièce éclairée. Le dire
ainsi dans toute communication — ne jamais laisser croire à un ARCore maison.

**Dépendance matérielle à ne pas oublier** : P2, P6 et P7 supposent l'accès aux
capteurs Android (`ASensorManager`), qui **n'existe nulle part dans le dépôt** à
ce jour. C'est le premier vrai manque, avant même le SLAM.

### 🔺 Suivi par l'image en PYRAMIDE — idée de Rihen (2026-08-12)

**L'idée, telle qu'il l'a posée** : « pourquoi ne pas comparer les différents
niveaux d'image pour déterminer ou pas la rotation de la caméra, même si en
faisant ça, si une personne traverse la caméra ça peut donner l'impression d'un
déplacement de la caméra ». Elle est juste, et la réserve qu'il y ajoute
lui-même est exactement le bon garde-fou.

**Pourquoi c'est le remède au défaut qui reste.** L'obstacle mesuré n'est pas
l'algorithme mais la MATIÈRE : dans une pièce aux murs clairs et peu éclairée,
la webcam rend une image dont le relief local est du même ordre que son bruit.
Le suivi trouve alors 16 à 22 points et n'en retient que 0 à 7 selon l'instant,
d'où un cumul qui avance par à-coups (mesures relevées : −5,4° puis −3,5°).
Réduire l'image de moitié fait la moyenne de quatre pixels : le bruit décroît
comme la racine du nombre d'échantillons, tandis que les grandes structures
— l'angle du mur, le bord de l'écran, une porte — restent intactes. À l'échelle
grossière, une scène pauvre redevient donc riche.

**Ce que la pyramide apporte, en plus de la robustesse :**
1. Les grands mouvements se trouvent à l'échelle grossière pour presque rien
   (un pixel grossier = quatre fins), ce qui supprime le compromis actuel entre
   rayon de recherche et coût.
2. L'estimation grossière sert de POINT DE DÉPART à l'échelle fine : la
   recherche fine se réduit à ±2 pixels. Le coût total baisse au lieu de monter.
3. Le suivi cesse de dépendre d'un seuil de relief, qui a été la source des
   trois régressions de la journée.

**Comment le faire (esquisse) :** deux ou trois niveaux par moyenne 2×2 de
l'image de référence ET de l'image courante ; sélection + appariement au niveau
le plus grossier ; propagation du décalage trouvé au niveau suivant comme
décalage initial, avec recherche ±2 ; ajustement de la rotation au niveau le
plus fin. Conserver l'image de référence (acquis du 2026-08-12) et le choix
d'hypothèse par ÉTENDUE — c'est précisément lui qui répond à la réserve de
Rihen : une personne qui traverse ne déplace qu'une région, une caméra qui
tourne déplace tout.

**Risque connu à ne pas rouvrir sans mesure** : chaque garde-fou de la journée
(seuil de relief, vote d'immobilité, quota par région) a d'abord été posé en
valeur ABSOLUE et a affamé la scène suivante. Tout seuil ajouté à la pyramide
doit être relatif à ce que l'image offre.

**État au moment de la pause** : la rotation EST suivie et le cube sort
désormais du champ en se faisant couper (capture 15h40 : cumul −5,4°, cube
coupé au bord gauche), mais par intermittence. Les six tests de non-régression
sont en place et verts, dont la scène pauvre et le sujet fixe.

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
