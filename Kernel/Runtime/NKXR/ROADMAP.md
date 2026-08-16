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
SIMULATEUR desktop + démo `NKXRDemo` (stéréo côte à côte, souris = tête),
auto-tests **148/148**. En attente de validation par Rihen (cycle TEST →
VALIDATION → INTÉGRATION).

> 🔧 **Ce chiffre disait « 66/66 » jusqu'au 2026-08-17.** Il ne comptait qu'un
> banc sur deux : **66** (`test_xr`) **+ 82** (`test_ar`) = **148**. Le module
> **sous-déclarait de moitié ce qu'il vérifie** — un chiffre public faux, même
> dans le sens de la modestie, reste faux : le jour où quelqu'un compte, il ne
> sait plus lequel des deux croire. Corrigé après avoir relancé les deux bancs le
> 2026-08-17 (Release, C++17, `origin/feat/nkxr`) : **66 OK / 0 échec** et
> **82 OK / 0 échec**. Le troisième binaire, `outil_ar_image`, **n'entre pas dans
> ce total** et la raison est écrite plus bas.

---

## ✅ 2026-08-17 — le mode de fusion est DEMANDÉ au runtime, plus écrit en dur

`EndFrame` soumettait `XR_ENVIRONMENT_BLEND_MODE_OPAQUE` **en dur**
(`NkXrOpenXRBackend.cpp`, ancienne l. 1321), alors que
`xrEnumerateEnvironmentBlendModes` était chargée et jamais appelée pour décider.
*Une valeur écrite en dur est une hypothèse non mesurée* — même famille que les
réglages fantômes de NKCamera et que les gardes de macros corrigées le même jour.

**La règle de sélection, décidée UNE fois à l'initialisation :**

| cas | mode soumis | effet |
|---|---|---|
| le runtime annonce OPAQUE | **OPAQUE** | **identique à avant, au bit près** |
| le runtime n'annonce PAS OPAQUE | son **premier mode annoncé** | corrige une soumission **invalide** au regard de la spec |
| énumération en échec ou absente | **OPAQUE** | repli — le comportement d'avant |

⚠️ **Sur tout runtime qui annonce OPAQUE — c'est-à-dire tout ce que ce dépôt peut
exercer aujourd'hui — ce correctif est inerte.** Il ne devient utile que là où
l'ancien code était fautif : les casques à écran transparent, qui n'annoncent
qu'`ADDITIVE` et pour lesquels soumettre `OPAQUE` violait la spec.

### 🚫 Ce que ceci ne fait PAS

**Le passthrough n'est ni disponible, ni plus proche.** Le runtime PC mesuré le
2026-08-17 n'expose **pas** `XR_FB_passthrough` (36 extensions, absente), et un
passthrough Quest exige un **APK autonome que ce dépôt ne produit pas**. Ce code
sert à **dire la vérité** le jour où cet APK existera. Le chemin AR n'est pas
touché.

### ⚠️ État de vérification — compilé, PAS exercé

| | |
|---|---|
| compile (`jenga build --target NKXRDemo --config Release`) | ✅ **26/26**, Release |
| exercé sur le runtime OpenXR | ❌ **NON** |

`NKXRDemo` **n'atteint pas** l'initialisation XR sur cette machine : il s'arrête
dans `NkRendererImpl::Initialize`, *step 2 `NkShaderLibrary::Init`* — soit avant
toute ligne de NKXR. Preuve, pas raisonnement : `logs/app.log` contient
**0 occurrence** d'`OpenXR` ou de `xrCreateInstance`. Le blocage est donc situé
en amont de ce correctif, mais il **empêche de le vérifier**, et c'est écrit ici
plutôt que passé sous silence.

*La journalisation de la liste reçue a lieu **une seule fois**, à
l'initialisation ; `EndFrame` ne fait que relire un champ. Rien ne se journalise
par image.*

### 🔧 Un champ fantôme créé puis retiré dans le même geste

Un booléen « l'énumération a-t-elle répondu ? » a été ajouté, mesuré à
**1 écrivain / 0 lecteur**, puis retiré — exactement le défaut que ce dépôt
traque depuis une semaine, commis en le corrigeant. Les trois cas sont déjà
séparés par trois messages distincts. *Un état de plus n'aurait servi qu'à
donner l'illusion d'une capacité interrogeable.*

### ✅ Dette SOLDÉE le même jour : le banc compile désormais dans le dialecte du dépôt

Le banc NKXR compilait ses trois tests en **C++20** alors que le dépôt est en
**C++17** (`cppdialect("C++17")`, 205 projets). *Un banc qui n'est pas compilé
comme le code qu'il juge ne mesure pas le même code* — c'est ce qui a piégé le
témoin `NkToWide` (`char16 = uint16` en C++17, `char16_t` en C++20).

⚠️ **La question n'était pas « les contrôles sont-ils faux ? » mais « que
mesurent-ils ? »** — et c'est pire, parce qu'un résultat faux finit par se voir,
alors qu'un résultat hors-sujet reste vert indéfiniment.

**Mesuré AVANT de corriger** — les deux dialectes, mêmes sources, mêmes conditions :

| banc | C++20 | C++17 | écart |
|---|---|---|---|
| `test_xr` | **66 OK, 0 ÉCHECS** | **66 OK, 0 ÉCHECS** | **aucun** |
| `test_ar` | **82 OK, 0 ÉCHECS** | **82 OK, 0 ÉCHECS** | **aucun** |
| `outil_ar_image` *(instrument)* | code 1 (emploi) | code 1 (emploi) | **aucun** |

Et le contrôle qui vaut plus que les compteurs : **les journaux des trois
binaires, horodatage retiré, sont identiques entre les deux dialectes.** Pas
seulement le même total — la même sortie, ligne pour ligne.

**Zéro contrôle tombait.** La dette était un réglage de script, pas un problème
de portabilité — et maintenant on le sait au lieu de l'espérer. Le script est
passé en C++17 et rejoué à travers lui-même : 66/66 et 82/82, inchangés.

### ✅ Le compte public corrigé : 148, pas 66

Le module annonçait **« 66/66 »** en tête de ROADMAP alors qu'il y a **148**
contrôles — **66** (`test_xr`) **+ 82** (`test_ar`). Il **sous-déclarait de
moitié ce qu'il vérifie**. Un chiffre public faux, même dans le sens de la
modestie, reste faux : le jour où quelqu'un compte, il ne sait plus lequel des
deux croire. Corrigé en tête de ce fichier.

### ✅ `test_ar_image` → `outil_ar_image` : il n'était NI compté NI exclu

Troisième état, le pire : construit par `build_tests.sh`, absent de tout total,
et sortant en **code 1** sans argument — donc **il ressemblait à un échec** à qui
lisait la sortie du banc.

**Tranché : il sort explicitement du décompte, et il est renommé.** La raison est
plus forte que « il attend un argument » :

> **ce programme n'a AUCUN verdict.** Il journalise ce qu'il trouve et rend `0`
> qu'il détecte cinq marqueurs ou zéro. **Il ne peut pas échouer.**

L'ajouter au total avec une image versionnée aurait donc créé **un contrôle
incapable de tomber** — exactement le « repos acheté, pas d'information » qu'on
vient de retirer ailleurs. Un attendu manquait, pas une image.

Ce qui a été fait :
- **renommé `outil_ar_image.cpp`** — le nom `test_*` dans `tests/` était la cause
  de l'ambiguïté, pas sa conséquence ;
- **son message d'emploi dit ce qu'il EST** (« INSTRUMENT de diagnostic, pas un
  test ») avant de dire comment on l'appelle ;
- **ses trois codes de sortie sont documentés**, avec la précision qui compte :
  *`0` signifie « j'ai tourné », pas « j'ai réussi »* ;
- **la condition pour qu'il rejoigne un jour le décompte est écrite** : une image
  versionnée **et** le nombre de marqueurs exigé, avec échec si le compte diffère.

### 🔧 Et le `sed` de la correction a réécrit sa propre documentation

Le remplacement `c++20 → c++17` a touché **aussi la phrase de commentaire qui
racontait l'historique**, la transformant en « ce script a compilé en C++17
jusqu'au 2026-08-17 » — l'inverse de la vérité. Rattrapé à la relecture.

C'est la forme déjà rencontrée sur `NkToWide` : **la documentation se met à
corroborer l'état faux.** Le nom du drapeau n'est donc plus écrit en toutes
lettres dans la phrase d'historique, pour qu'un futur remplacement ne puisse plus
la retourner.

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

## 🎓 ÉTAT VR / AR / MR — pour la décision d'enseignement (2026-08-17)

> **Question posée** : que peut-on faire faire à des étudiants en AR/VR/MR à
> partir de la semaine du 8 septembre ? Trois colonnes, et la troisième est la
> seule qui compte pour un cours : **ce qui a tourné sur un appareil**.

| | écrit | compile | **a tourné sur un appareil** |
|---|---|---|---|
| **VR** — session OpenXR, Vulkan, états runtime, `xrLocateViews` | 2 947 l. (backend) | ✅ | ✅ **Quest 2 réel** — session créée, **300 images**, 2080×2096 par œil (11/08) |
| **VR** — manettes Touch, poses, saisie, haptique | inclus | ✅ | ✅ **validé manettes en main par Rihen** (12/08) |
| **VR** — mains sans manettes (`XR_EXT_hand_tracking`) | ✅ écrit, 26 articulations | ✅ | ❌ **bloqué côté Meta** — le runtime Link n'expose pas l'extension (36 listées, vérifié) |
| **AR** — détection de marqueurs (Otsu, contours, quad, homographie, pose) | 923 l. | ✅ | ✅ **Galaxy S22+** — marqueurs détectés, ~2,5 ms/image |
| **AR** — monde ancré (`NkArWorld`, carte, pose caméra) | 506 l. | ✅ | ✅ **S22+** — objet posé qui tient en place |
| **AR** — calibration caméra (Zhang) | 711 l. | ✅ | ✅ **S22+** — fx 918,9 / fy 923,5, **erreur de reprojection 1,83 px** sur 6 vues, appliquée à chaud (13/08) |
| **AR** — suivi par l'image entre marqueurs (`NkArFlow`) | 748 l. | ✅ | 🔶 **partiellement** — tourne sur l'appareil, jamais mesuré séparément |
| **AR** — centrale inertielle (`NkArImu`) | 252 l. | ✅ | ❌ **défaut connu** : partage le `Looper` de l'application et **fige la boucle**. Coupé par défaut depuis le 14/08 |
| **MR / passthrough** | **0 ligne** | — | ❌ **inexistant** |

### 🔬 MESURE DU 2026-08-17 — le runtime PC **n'offre aucun passthrough**

Le module interroge désormais `xrEnumerateEnvironmentBlendModes` et **écrit ce
que le runtime annonce** (observation seule — le mode soumis reste `OPAQUE`).

Ce qui a pu être mesuré **sans casque**, sur cette machine :

```
runtime charge ...... Oculus 1.206.0 (Meta Horizon, LibOVRRTImpl64_1.dll)
extensions offertes .. 36
xrGetSystem .......... ECHEC, XrResult -35 (aucun casque connecte)
```

⚠️ **Aucune extension de passthrough dans les 36.** Les seules `XR_FB_*`
annoncées sont `color_space`, `display_refresh_rate`, `haptic_amplitude_envelope`,
`haptic_pcm`, `touch_controller_pro`, `touch_controller_proximity`. **Pas de
`XR_FB_passthrough`.**

**Ce que ça établit** : le passthrough n'est pas exposé par le runtime **PC via
Link**. Le MR ne sera donc pas atteignable depuis un PC relié au casque, quelle
que soit la valeur du mode de fusion.

**Ce que ça n'établit PAS** : l'état sur un **APK autonome Quest**, où le runtime
est différent et expose habituellement le passthrough. Or cet APK **n'existe pas
encore** (ligne ❌ « APK Quest 2 via la chaîne jenga Android » ci-dessus).

**Reste à mesurer, casque connecté** : la liste des modes de fusion elle-même —
`xrEnumerateEnvironmentBlendModes` exige un `systemId` valide, donc un casque
présent. Le code est en place et journalise ; il suffira d'un lancement.

### 📏 MESURE DU SUIVI PAR L'IMAGE — `NkArFlow`, seul (2026-08-17)

748 lignes qui tournaient depuis le 13/08 **sans avoir jamais été mesurées
séparément**. C'est pourtant ce qui tient la scène entre deux marqueurs — donc
la première chose qu'un étudiant verra en détournant la caméra du marqueur.

**Protocole** (`tests/bench_ar_flow.cpp`) : image **réelle** du téléphone
(1280×720, vraie texture et vrai bruit), rotations imposées par **décalage entier
de pixels** — sous le modèle sténopé, un lacet θ décale l'image de fx·tan(θ) —
avec les **intrinsèques mesurées** du 13/08 (fx = 918,9), sans quoi la conversion
pixels → angle fausserait la vérité elle-même.

| décalage | vérité | mesuré | points | verdict |
|---|---|---|---|---|
| 1 px | 0,062° | — | 12 | **refusé** |
| 2 px | 0,125° | — | 23 | **refusé** |
| **4 px** | 0,249° | **0,250°** | 15 | ✅ |
| **8 px** | 0,499° | **0,499°** | 23 | ✅ |
| **16 px** | 0,998° | **0,998°** | 15 | ✅ |
| **32 px** | 1,994° | **1,995°** | 24 | ✅ |
| 48 px | 2,990° | — | **0** | **refusé** |
| 64 px et + | — | — | **0** | **refusé** |

**Ce que ça établit :**
- **précision : exacte** — erreur ≤ 0,001° sur toute la bande utile ;
- **bande utile : 0,25° à 2,0° par image**, soit **~60°/s à 30 images/s** ;
- **hors bande, il REFUSE au lieu de mentir** : trop petit, pas assez de signal ;
  trop grand, aucun point ne vote. Un estimateur qui se tait quand il ne sait pas
  vaut mieux qu'un estimateur qui invente.

⚠️ **La portée annoncée dans l'en-tête est optimiste d'un facteur 1,7.** Le
commentaire de `searchRadius` dit « 40 px valent 4°, soit 125°/s » — calculé avec
une focale **supposée de 550 px**. Avec la focale **mesurée** de 918,9, 40 px ne
valent que **2,5°, soit 75°/s**. Le banc mesure une coupure encore plus tôt, vers
32 px. *Un chiffre calculé sur une valeur d'attente, resté dans un commentaire
après l'arrivée de la vraie mesure.*

**Ce que ce banc NE mesure PAS** : des décalages entiers sont le cas le plus
favorable — aucun rééchantillonnage, donc aucun flou d'interpolation. Le terrain
ajoute le flou de bougé, l'obturateur déroulant, les changements d'éclairage et la
parallaxe d'une translation. **Ces chiffres sont une borne supérieure.**

**Pour l'enseignement** : on peut promettre que la scène tient quand on tourne
*lentement* (sous ~60°/s). On ne peut pas promettre un balayage rapide — et
l'application le saura, puisque le suivi renvoie `valid = false`.

### 🔍 DIAGNOSTIC — pourquoi `NkArImu` fige la boucle (2026-08-17)

**Cause trouvée, par lecture du code : ce n'est ni un fil, ni un verrou.**

`NkArImu::Initialize` attache sa file d'événements au looper **de l'appelant** —
donc, sur le fil principal, à celui de l'application :

```
NkArImu.cpp:49-53
    ALooper *looper = ALooper_forThread();
    ASensorManager_createEventQueue(manager, looper, kLooperId, nullptr, nullptr);
                                                                ^^^^^^^ AUCUN rappel
```

Et la pompe d'événements Android est écrite ainsi :

```
NkAndroidEventSystem.cpp:624-643
    while (true) {
        pollResult = ALooper_pollOnce(0, ...);
        if (pollResult < 0) break;      // sort UNIQUEMENT quand plus rien n'est prêt
        ...
    }
```

**Le mécanisme exact** : la file est enregistrée avec l'identifiant `0x4E4B` et
**sans rappel**. `ALooper_pollOnce` rend donc `0x4E4B` — une valeur ≥ 0 — et
`source` vaut nul, donc personne ne consomme les événements. Le descripteur reste
**prêt en lecture**, `pollOnce` le re-signale immédiatement, `pollResult` n'est
jamais négatif : **la boucle ne sort jamais.** À 200 Hz (`kSamplingPeriodUs =
5000`), il y a toujours de quoi lire.

Les seuls à vider cette file sont `NkArImu::Poll()`… appelé plus loin dans la
trame, qui n'arrive jamais. **La boucle s'affame elle-même.**

**Coût de la réparation, puisque c'est la question** :

| piste | coût | remarque |
|---|---|---|
| **rappel de vidage** — passer une fonction à `createEventQueue` au lieu de `nullptr` | **~15 lignes** | le looper appelle le rappel, la file se vide, `pollOnce` finit par n'avoir plus rien : la boucle sort |
| fil dédié avec son propre looper | ~½ journée | solution classique, isole complètement, coûte un fil + une synchronisation |
| vider dans la pompe elle-même | ~1 h | ⛔ coupleraient NKWindow à NKXR : à écarter |

**Ce n'est donc pas un mois — c'est une quinzaine de lignes.**

⚠️ **Non réparé délibérément.** Le correctif ne peut pas être vérifié sans
téléphone, et c'est précisément ce composant qui a déjà été livré avec un défaut
connu actif par défaut (`preferSensors = true`, corrigé le 14/08). **Livrer un
correctif invérifiable sur le composant qui a déjà mordu serait refaire la même
faute.** À faire dès que l'appareil revient, avec sa mesure.

### ⚠️ Et le MR est aussi **empêché par une ligne**

Recherche exhaustive sur `passthrough`, `mixed reality`, `environment blend` dans
tout `NKXR` et `NKARDemo` : **une seule occurrence**, et c'est son contraire —
`NkXrOpenXRBackend.cpp:1273` fixe `environmentBlendMode =
XR_ENVIRONMENT_BLEND_MODE_OPAQUE`, **en dur**. Opaque signifie « le monde réel est
masqué ».

Le passthrough se joue précisément là : `XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND`
ou `ADDITIVE`, selon ce que le runtime annonce dans
`xrEnumerateEnvironmentBlendModes`. **Ce n'est pas un chantier de plusieurs
semaines** — c'est une énumération, un choix, et un champ. Mais tant que la valeur
est écrite en dur, aucun étudiant ne verra le monde réel.

### Ce que ça permet de décider

- **VR : utilisable en cours dès maintenant** — c'est le seul volet prouvé de bout
  en bout sur casque, manettes comprises.
- **AR : utilisable sur téléphone Android**, avec des marqueurs imprimés. La
  chaîne complète a tourné : détection, ancrage, calibration. ⚠️ **Ne pas
  promettre le suivi sans marqueur** — l'IMU est coupée, et le suivi par l'image
  n'est pas mesuré.
- **MR : ne rien promettre.** Zéro ligne, et un mode de fusion figé en opaque.

*Écrit après mesure, pas d'après mémoire : lignes comptées, `grep` exhaustif sur
le passthrough, et chiffres d'appareil relus dans le carnet de bord.*

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

**⚠️ Piège mesuré le 2026-08-12 sur ce poste — le pilote Meta capte l'ADB
Samsung.** Symptôme très particulier : les commandes COURTES passent
(`adb devices`, `adb shell ls` répondent correctement) mais tout ce qui dure
tombe — `install`, `push` (les 22 Mo passent à 4 Mo/s puis « failed to read
copy response: EOF », et le fichier n'existe pas), et même `adb tcpip 5555`
qui rend « error: closed ». Autrement dit la liaison meurt **à la fin** des
opérations, jamais au début.

Cause probable, relevée dans les périphériques Windows : l'interface
`SAMSUNG Android ADB Interface` est rattachée à la classe
**`RealityLabsUsbDeviceClass`** — le pilote de Meta Quest Link, installé le
même jour pour le casque. Un téléphone Samsung ne doit pas dépendre du pilote
Meta. Remède : gestionnaire de périphériques → cette interface → mettre à jour
le pilote → choisir le pilote ADB Google/Samsung standard.

**Contournements qui n'ont pas besoin de l'USB :**
1. **Débogage sans fil** (Android 11+, options développeur) : appairage par
   code, puis `adb pair <ip>:<port>` et `adb connect <ip>:<port>`. Indépendant
   du pilote USB, donc immunisé à ce défaut.
2. **Installation à la main** : copier l'APK par transfert de fichiers et le
   toucher sur le téléphone. L'APK est signé, il s'installe seul ; il faut
   seulement autoriser l'installation depuis cette source.

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
| P1 | ✅ **Téléphone — LIVRÉ le 2026-08-12** (Galaxy S22+) | l'AR marqueur dans la main, démontrable en clientèle | fait en un jour |
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

#### P1 — ce qui a réellement été livré le 2026-08-12, et ce qu'il a coûté

L'AR complète tourne sur téléphone : permission demandée seule (JNI), caméra
arrière ouverte d'après la façade déclarée, image redressée d'après
l'orientation **lue du pilote**, marqueur détecté, cube ancré, retour
d'arrière-plan sans écran noir. Le monde se met à jour en **2,5 à 2,8 ms par
image analysée — plus vite que sur le poste de travail**.

Sept défauts levés, dont trois valent d'être retenus car ils se reproduiront :

1. **Une application muette ne se répare pas.** NKLogger n'attachait aucun puits
   console en Release, et son puits fichier écrit dans un chemin relatif — non
   inscriptible sur Android. Un Release sur téléphone n'écrivait donc nulle
   part. Corrigé dans NKLogger : sur Android, logcat EST le journal du système.
2. **Toute transformation de l'image doit l'être aussi du modèle de caméra.**
   Redresser les pixels sans tourner les intrinsèques rabaisse la focale d'un
   facteur 1,8 : l'objet ne peut plus coller. Une image droite avec une focale
   fausse est pire qu'une image couchée — elle a l'air juste.
3. **Ce qui engage durablement exige une preuve dans la durée.** Le monde s'est
   fondé sur un marqueur « 0 » inexistant, vu une seule image ; tout en
   découlait. Un marqueur doit persister avant qu'on lui confie l'origine.

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
