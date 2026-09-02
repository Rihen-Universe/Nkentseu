# Noge — canal d'échange du chantier (inventaire d'ouverture)

> **Où j'écris.** Ce fichier est le canal détaillé du chantier Noge. Une entrée
> courte avec renvoi ici est déposée dans `echanges/noge.questions.md` (convention
> de la maison : l'agent écrit dans `questions`, jamais dans `reponses`).
>
> **Provenance de toutes les mesures ci-dessous** — une mesure sans provenance est
> indéfendable : arbre `Nkentseu-noge`, branche `feat/noge-inventaire`,
> HEAD `1e053708` · `Engine/Noge/src` inchangé depuis le commit `964b3779`
> (2026-08-14) · date des mesures **2026-09-02** · OS Windows 11 ·
> toolchain clang-mingw ucrt64 · `PATH` ucrt64 en tête (piège documenté).
>
> **Convention de lecture** : ✅ = je l'ai vérifié moi-même aujourd'hui.
> 📄 = je le rapporte d'une trace écrite par quelqu'un d'autre, avec sa date.
> Les deux ne valent pas la même chose et ne sont jamais mélangés.

---

## ⚠️ AVERTISSEMENT PRÉALABLE — ce chantier n'ouvre pas sur une page blanche

Un inventaire mesuré de Noge **existait déjà** sur cette même branche
(`Engine/Noge/ROADMAP.md`, blocs des 2026-08-16 et 2026-08-17). Je l'ai relu
avant de mesurer, comme la porte « chercher où la chose pourrait déjà être
faite » l'exige.

✅ **Je l'ai recontrôlé, et il est encore exact** : 88 `.h` / 21 843 lignes,
35 `.cpp` / 6 915 lignes — chiffre pour chiffre. La raison est simple et
vérifiable : `Engine/Noge/src` n'a **pas été touché depuis le 2026-08-14**.

Ce que j'apporte n'est donc pas un second inventaire : c'est **ce que celui-là ne
mesurait pas** — le chrome qui promet, les niveaux de qualité, l'état 3D des sept
cibles, et la scène d'épreuve.

⚠️ **Un écart que je dois signaler** : la branche a divergé de `main`
(499 commits sur main absents ici, 245 ici absents de main). Sur `Engine/Noge/`
la divergence se réduit à **un seul fichier** (`Core/NkApplication.cpp`, 11
lignes — le correctif `Present()` avant `EndFrame()`). L'inventaire vaut donc
pour les deux branches, mais **la fusion reste à faire** et elle n'est pas
triviale hors de Noge.

---

# MESURE 1 — Ce que Noge DÉCLARE contre ce qu'il EXÉCUTE

## 1.1 Le tableau par module

✅ Mesure faite par moi le 2026-09-02. Colonne « consommateurs » = **inclusion
réelle** (`grep -rl "Noge/<module>/"` hors `Engine/Noge`), pas ce que les
`.jenga` déclarent — un `.jenga` déclare une intention de lien, jamais un usage.

| module | `.h` | `.cpp` | l. `.h` | l. `.cpp` | conso. externes | verdict |
|---|---|---|---|---|---|---|
| `ECS` | 48 | 20 | 13 266 | 4 493 | **31** | ✅ **existe et tourne** |
| `Core` | 7 | 2 | 784 | 372 | **14** | ✅ **existe et tourne** |
| `IO` | 4 | 4 | 567 | 645 | 3 | ✅ **existe et tourne** |
| `Color` | 1 | 1 | 228 | 29 | 3 | ✅ existe et tourne |
| `Doc` | 2 | 2 | 683 | 84 | 2 | ✅ existe et tourne |
| `Anim` | 1 | 1 | 410 | 174 | 1 | ✅ existe et tourne |
| `Modeling` | 4 | 2 | 996 | 660 | 1 | ✅ existe et tourne (4 FAIL normales) |
| `Rigging` | 1 | 1 | 278 | 236 | 1 | ✅ existe et tourne |
| `Design` | 4 | 1 | 1 009 | 41 | 1 | ✅ existe et tourne |
| `Layers` | 1 | 1 | 161 | 181 | **0** | 🟡 **existe et mort** (interne seul) |
| `Anim2D` | 2 | 0 | 468 | 0 | **0** | 🔴 **déclaré sans corps** |
| `Crowd` | 1 | 0 | 44 | 0 | **0** | 🔴 déclaré sans corps |
| `Facial` | 2 | 0 | 790 | 0 | **0** | 🔴 déclaré sans corps |
| `Physics` | 1 | 0 | 346 | 0 | **0** | 🔴 déclaré sans corps |
| `Sculpt` | 1 | 0 | 198 | 0 | **0** | 🔴 déclaré sans corps |
| `Selection` | 1 | 0 | 310 | 0 | **0** | 🔴 déclaré sans corps |
| `Sequencer` | 1 | 0 | 416 | 0 | **0** | 🔴 déclaré sans corps |
| `Systems` | 1 | 0 | 277 | 0 | **0** | 🔴 déclaré sans corps |
| `Text` | 1 | 0 | 67 | 0 | **0** | 🔴 déclaré sans corps |
| `UV` | 1 | 0 | 75 | 0 | **0** | 🔴 déclaré sans corps |
| `Viewport` | 2 | 0 | 378 | 0 | **0** | 🔴 déclaré sans corps |

**Le compte** : 9 modules sur 21 tournent · 1 vit mais n'a aucun client externe ·
**11 modules, 3 369 lignes d'en-tête, n'ont aucun corps et aucun appelant.**

⚠️ **Ne pas confondre `Noge/Systems/` (squelettes, 0 `.cpp`) avec
`Noge/ECS/Systems/` (20 `.cpp`, fonctionne).** Les deux existent, le nom se
ressemble, et l'un est vide.

⚠️ **Un écart avec la mesure du 2026-08-17, et il est réel** : ce document
classait `Color`, `Design`, `Doc` et `Rigging` « internes à Noge seulement ».
✅ Ils ont aujourd'hui des consommateurs externes — `NkSVGImportDemo`,
`NkNavCoreDemo`, `NkLocomotionDemo`, `NKMath`. *Le code de Noge n'a pas bougé :
ce sont les consommateurs qui sont arrivés.* Un inventaire de consommation se
périme sans que le sujet mesuré change.

## 1.2 🔴 LE CHROME QUI PROMET — les trois formes trouvées

C'est ce que l'inventaire précédent ne cherchait pas. Instrument : script de
mesure sur les 123 fichiers de `Engine/Noge/src`, plus vérification à la main de
chaque cas retenu. ✅ **Contrôle positif fait** : l'instrument remonte bien des
corps vides réels et distingue les modules qui marchent.

### Forme A — la moitié qui DÉCRIT est écrite, la moitié qui FAIT ne l'est pas

**C'est le cas le plus net du module, et le plus trompeur.**
`Noge/Systems/NkPhysicsSystems.h` (277 lignes, **0 `.cpp`**) déclare six systèmes
ECS complets. Pour chacun :

```
[[nodiscard]] NkSystemDesc Describe() const override { ... }   <- CORPS COMPLET
void Execute(NkWorld &world, float32 dt) noexcept override;    <- AUCUN CORPS
```

`Describe()` retourne un descripteur **entièrement renseigné et fonctionnel** —
`Reads<>`, `Writes<>`, groupe `PostUpdate`, priorité (700, 600, 550, 500, 450,
400), nom. Un ordonnanceur qui lit ces descripteurs les enregistrerait sans
broncher, avec leurs dépendances et leur ordre.

✅ **Mesure des corps de `Execute`** (recherche `<Classe>::` dans tout le dépôt,
avec contrôle positif sur les systèmes qui marchent) :

| classe | définitions hors-ligne | classe témoin qui marche | définitions |
|---|---|---|---|
| `NkJiggleBoneSystem` | **0** | `NkTransformSystem` | 2 |
| `NkHairSystem` | **0** | `NkRenderSystem` | 5 |
| `NkSoftBodySystem` | **0** | `NkPhysicsSystem` | 3 |
| `NkRagdollSystem` | **0** | `NkAudioSystem` | 16 |
| `NkMocapSystem` | **0** | | |

> **Pourquoi personne ne l'a vu** : l'éditeur de liens ne dit rien, parce que
> *personne ne les appelle*. Un module sans corps et sans appelant ne casse rien,
> ne se voit pas, et **se lit exactement comme une fonctionnalité livrée**.
> 3 369 lignes d'en-têtes soignés portant les mots *Physics*, *Sequencer*,
> *Viewport*, *Selection* — ce qu'un étudiant, ou un client, attend d'un moteur.

### Forme B — le drapeau que personne n'honore

✅ **`NkRenderQuality` — l'axe sur lequel toute l'architecture voulue par Rodolf
doit reposer — n'est lu par AUCUNE ligne du dépôt.**

`Kernel/Runtime/NKRenderer/src/NKRenderer/Core/NkRendererConfig.h:68` déclare
six niveaux (`NK_MOBILE`..`NK_CINEMATIC`). Le champ
`NkRendererConfig::quality` (l. 367) est :

- **écrit 7 fois** : 6 presets (`ForGame`, `ForFilm`, `ForArchviz`, `ForMobile`,
  `For2D`, `ForEditor`) + 1 application (`NKARDemo/main.cpp:505`) ;
- **lu 0 fois.** Aucune comparaison, aucun `switch`, aucun passage à un
  sous-système, dans le dépôt entier.

⚠️ **Contrôle positif obligatoire, et il est concluant** : la même méthode de
recherche trouve sans peine un champ du *même struct* qui, lui, **est** honoré —
`mCfg.deferred`, lu à `NkRendererImpl.cpp:829`
(`const bool useDeferred = has3D && mCfg.deferred && hasPP;`). L'instrument sait
donc trouver une lecture ; s'il n'en trouve aucune pour `quality`, c'est
qu'il n'y en a pas.

⚠️ **Et je dois borner l'affirmation, sous peine de la rendre fausse** :
`ForMobile()` **dégrade réellement** le rendu — mais par ses *autres* champs
(`subsystems` sans ombres ni post-process, `hdr=false`, `postProcess.bloom=false`,
`pipeline=NK_FORWARD`), et ceux-là **sont lus** (`Has(NK_SS_*)` 9 fois,
`mCfg.shadow.resolution`, `mCfg.postProcess.*` : vérifiés). Énoncé exact :
**les presets fonctionnent, l'enum `quality` est inerte.** C'est un mot qui
décrit une intention que rien n'exécute.

📌 *Le même fichier porte déjà l'aveu écrit du même défaut, sur les biais
d'ombre : « ces deux champs n'atteignaient JAMAIS les shadow maps virtuelles :
on croyait régler le biais, rien ne bougeait. » Le motif est connu de la maison.
Il n'a simplement jamais été cherché sur `quality`.*

### Forme C — la doublure qui n'est pas crédible

✅ Trois fichiers de `Resources/Models/` sont des **placeholders qui portent le
nom d'un modèle** :

| fichier | taille | contenu réel |
|---|---|---|
| `car.glb` | 168 o | en-tête glTF, générateur `"NK low placeholder"`, mesh **sans primitive** |
| `suzanne.obj` | 191 o | **6 sommets, 8 faces** — un octaèdre. Suzanne en a 507. |
| `tree.obj` | 280 o | 9 sommets, 12 faces — un cube surmonté d'un cône |

Ils portent leur aveu en commentaire (`# low poly placeholder`), donc ils ne
mentent pas au lecteur. **Mais ils mentent au banc** : `NkAssetIODemo` charge
`tree.obj` et compte son import comme réussi. C'est la face n°2 de la grille de
la maison — *réussir pour la mauvaise raison* — et c'est exactement ce que la
règle « une doublure crédible, jamais un trou » vise : ici la doublure existe,
elle n'est **pas crédible**, et elle rend un banc vert.

## 1.3 ❓ NOGE A-T-IL DES NIVEAUX DE QUALITÉ ? — non, et le socle en a un

Réponse à la question ajoutée par Rodolf en cours de chantier.

✅ **Dans `Engine/Noge/src` : zéro.** Aucun `NkQuality`, aucun preset par cible,
aucune notion de niveau. Les seules occurrences de « LOD » sont
`NkLocomotion.h` (LOD de simulation d'agent) et `NkAnimation.h`
(`lodMeshIds[8]` + `lodDistances[8]`) — du LOD **par distance**, pas par
plateforme. `jpegQuality` est de la compression d'image.

✅ **Une couche plus bas, NKRenderer en a un, et il est riche** — la porte
« chercher qui le porte déjà, la couche du dessous d'abord » s'applique en plein :

```
NkSubsystemFlags   13 drapeaux opt-in (RENDER3D, SHADOW, POST_PROCESS, VFX...)
NkPipelineMode     FORWARD / DEFERRED / FORWARD_PLUS / TILED_DEFERRED
NkRenderQuality    MOBILE..CINEMATIC          <- inerte (cf. Forme B)
NkShadowConfig     cascades, resolution, PCSS, biais    <- honore
NkPostConfig       bloom, ssao, fxaa, taa, ssr, dof...  <- honore
presets            ForGame / ForFilm / ForArchviz / ForMobile / For2D / ForEditor
```

🔴 **Mais le choix du preset appartient à l'APPLICATION, et c'est précisément ce
que la règle de Rodolf interdit.** Il n'existe aucun `ForGame()` qui détecte la
cible : `ForGame` et `ForMobile` sont deux fonctions distinctes que l'appelant
choisit. **Aujourd'hui, un jeu qui veut « beau sur bureau, fluide sur mobile »
est structurellement obligé d'écrire `si (mobile) ForMobile() sinon ForGame()`** —
c'est-à-dire d'enfreindre la règle pour obtenir le comportement voulu.

📌 **Ce n'est donc pas un chantier « tout à construire ».** Le vocabulaire, les
sous-configs et les presets existent et sont honorés. Ce qui manque tient en deux
gestes : **(a)** un sélecteur de profil **dans le moteur**, qui lit la cible et
le matériel — `NKPlatform/NkCGXDetect.{h,cpp}` existe déjà et détecte OS, GPU,
vendeur, type, APIs ; **(b)** rendre `quality` opérant, ou le retirer. La règle
de la maison ne laisse d'ailleurs que ces deux sorties pour un drapeau qui
annonce quelque chose au code.

## 1.4 ✅ L'ÉTAT DE LA RÈGLE « une application qui interroge la plateforme a déjà perdu »

Mesure faite au `grep`, comme la règle le demande.

**Dans `Engine/Noge/src` : 6 occurrences, toutes dans le même sous-système** —
`ECS/Scripting/NkScriptBridge.{h,cpp}` et `NkScriptABI.h`, `#if defined(_WIN32)`
/ `__APPLE__` autour de `LoadLibraryA` / `dlopen`. **Ce n'est pas une violation** :
c'est le chargement dynamique, il n'y a pas d'API portable, et c'est du **moteur**,
pas d'une application. Le reste de Noge est propre.

🔴 **Dans `Applications/` : 45 fichiers branchent sur la plateforme.** Dont, en
première ligne, **les jeux** : `Pong` (4 fichiers, `PongConfig.h` inclus),
`Pong2` (7), `Mou` (4, dont `Platform/MouPlatformApp.cpp`).

⚠️ **Je ne les qualifie pas tous de violations, et il faut le dire** : la
majorité porte sur le **point d'entrée et le fenêtrage** (contexte GL, atlas de
police, chemins d'assets), pas sur la qualité de rendu. Ce sont deux dettes
différentes. Ce qui est certain et suffisant pour la décision : **la règle est
neuve (2026-09-02), le parc ne la respecte pas, et le nouveau jeu de course est
le premier à pouvoir naître conforme.** Le tri fichier par fichier des 45 n'a pas
été fait — je ne l'annonce donc pas fait.

---

# MESURE 2 — L'ÉTAT RÉEL DES SEPT PLATEFORMES, EN 3D

## 2.1 La distinction qui change tout

Rodolf indique que les sept ont déjà été prouvées. ✅ **La mesure confirme que
c'est vrai — et qu'une partie de ces preuves ne porte pas sur la 3D.**

La chaîne d'un jeu de course est `NKRenderer` (PBR, ombres, post-process), pas
`NKCanvas`. Le bon témoin n'est donc ni Pong ni les jeux de plateau : c'est
**`renderdemo`** (`Applications/Sandbox`, `RendererSandbox.jenga`) en `--demo=2`,
qui est exactement la scène PBR + ombres + instances.

## 2.2 Le tableau — quatre colonnes, et la source de chaque case

📄 = trace `Nkentseu/PLATEFORMES_ETAT.md`, **datée du 2026-08-06**, base commit
`ecabc216` (2026-07-29). ✅ = vérifié par moi le 2026-09-02.

| cible | compile | lie | ouvre une fenêtre | **affiche une image 3D** |
|---|---|---|---|---|
| **Windows** | 📄 ✓ 29/29 Debug+Release | 📄 ✓ | 📄 ✓ | 📄 ✓ (backend de référence) |
| **Android** | 📄 ✓ Debug+Release, APK 4 ABI | 📄 ✓ | 📄 ✓ | ✅ **OUI — vérifié de mes yeux** |
| **HarmonyOS** | 📄 ✓ Debug+Release, HAP | 📄 ✓ (`nm -D` : 0 indéfini) | 📄 ✓ | ✅ **NON — couleur d'effacement + HUD** |
| **Web** | 📄 ✓ Debug+Release, wasm | 📄 ✓ | 📄 ✓ (canvas WebGL2) | ✅ **NON — couleur d'effacement seule** |
| **Linux** | 📄 documenté et outillé | 📄 ✓ | 📄 ✓ | 🟡 **affirmé, aucune capture trouvée** |
| **macOS** | ❔ | ❔ | ❔ | ❔ **rien mesuré, rien tracé** |
| **iOS** | ❔ | ❔ | ❔ | ❔ **rien mesuré, rien tracé** |

### Ce que j'ai vérifié moi-même — j'ai ouvert les trois captures

- ✅ **Android — `Captures/nk_android_demo3d.png`** : c'est **une vraie scène 3D**.
  ~18 sphères PBR métal/rugosité variées, ombres portées douces au sol, grille de
  cubes instanciés éclairée, axes, damier, HUD `Demo 3D | API : OpenGL`,
  **59 FPS**, `VSM atlas 4096 px`, `framesInFlight 3`. La chaîne NKRenderer tourne
  entièrement sur mobile. **C'est la preuve 3D la plus forte du dossier.**
  ⚠️ Une ligne du HUD mérite d'être retenue pour la mesure 3 :
  `[Phase H] Texture file-based : fallback procedural` — **les textures sont
  procédurales, pas des fichiers.**
- ✅ **HarmonyOS — `Captures/nk_harmony_renderdemo.jpeg`** : fond **vert uni**
  (couleur d'effacement), HUD `Draw:0 Tris:0 GPU:0.00ms Batches:0` et
  `Active: R2D|R3D|TEXT|OVERLAY`. **Zéro triangle dessiné.** Le moteur vit, la
  surface présente, le texte 2D s'affiche — **la 3D n'est pas prouvée.** La trace
  le dit d'ailleurs elle-même : c'est la démo 0, la démo 2 restait à pousser.
- ✅ **Web — `Captures/nk_web_headless.png`** : **1280x720 d'une seule couleur**,
  exactement la couleur d'effacement `(0.05, 0.05, 0.07)` annoncée. Aucune
  géométrie. **La 3D n'est pas prouvée.**

### Ce que je ne peux pas vérifier depuis cette machine, et je le dis

- **macOS et iOS** : exigent un Mac et Xcode. Aucun ici. ⚠️ Et c'est plus qu'une
  impossibilité matérielle : **ces deux cibles ne sont mentionnées nulle part dans
  `PLATEFORMES_ETAT.md`** — le document couvre HarmonyOS, Web et Android. Le wiki
  `NKRHI/Platforms-Build-Run.md` les documente en *intention* (Metal recommandé),
  ce qui n'est pas une preuve d'exécution. **Le backend Metal est dans la
  catégorie « compile » du garde-fou d'honnêteté, pas « validé ».**
- **Linux** : le wiki décrit un état détaillé et crédible (GLX, override WSLg
  `MESA_GL_VERSION_OVERRIDE`, Vulkan lavapipe, correctifs de link nommés) et
  affirme que Vulkan « compile, linke, s'initialise ET rend, démos 2D+3D ».
  ⚠️ **Je n'ai trouvé aucune capture Linux**, alors qu'Android, Web et HarmonyOS
  en ont chacune une. Je classe donc Linux « affirmé, non illustré » — pas
  « faux ». Vérifiable ici via WSL2 sans Mac, c'est la cible la moins chère à
  fermer.

## 2.3 Ce qu'il faut retenir, et l'écart avec ce qu'on croit

> **Sept cibles construisent. Deux affichent une image 3D prouvée** (Windows,
> Android). **Deux affichent une couleur unie** (Web, HarmonyOS) — le plus dur
> est fait, il reste la dernière marche. **Trois n'ont pas de preuve du tout**
> (Linux non illustré, macOS et iOS non mesurés).

⚠️ **La cause des deux blocages est écrite et nommée**, ce qui les rend
chiffrables plutôt que vagues :
- **Web** : les générateurs de shaders émettent `#version 430 core` et des
  `layout(binding=)` ; WebGL2 exige GLSL ES 3.00 strict et refuse les deux. Un
  shim d'adaptation a été écrit et **prouvé côté compilation** (18 programmes
  compilent) ; restaient au 2026-08-06 deux correctifs appliqués **mais non
  reconstruits**. 📄
- **HarmonyOS** : tout le montage NAPI/XComponent est résolu ; il restait à
  pousser `nk_demo.txt=2` sur l'émulateur et à prendre le snapshot. 📄

**Aucune des deux n'est un problème d'architecture.** Ce sont des fins de course.

---

# MESURE 3 — LA SCÈNE D'ÉPREUVE GRAPHIQUE

## 3.1 🔴 L'IMAGE N'A PAS ÉTÉ PRODUITE — et voici pourquoi, vérifié

✅ **La carte graphique est occupée par Ilyana.** Mesure du 2026-09-02 :

```
NKIlyana.exe  (PID 19692)  actif
NVIDIA GeForce RTX 3070 Laptop GPU : 4753 / 8192 Mio, utilisation GPU 100 %
```

La consigne est explicite — la carte est à Ilyana pendant sa campagne de 30
jours. **Je n'ai donc lancé aucun rendu GPU.** C'est un refus assumé, pas un
échec : produire l'image aurait mis en contention un entraînement à ~53 000 pas.

## 3.2 ✅ MAIS TOUT LE RESTE A ÉTÉ MESURÉ, SANS TOUCHER AU GPU

Et le résultat est meilleur que prévu : **la scène d'épreuve ne demande aucun
code neuf, et le modèle de voiture charge déjà.**

### a) Le chemin existe déjà — une variable d'environnement suffit

`Applications/Sandbox/src/Demo/DemoGLTF.cpp:60` :

```cpp
// Variable d'env NK_GLTF_MODEL pour override rapide sans toucher au code.
static NkString PickModelPath() {
    const char *env = getenv("NK_GLTF_MODEL");
    if (env && env[0]) return NkString(env);
    return NkString("Resources/Models/rubber_duck/scene.gltf");
}
```

Et le dispatch couvre **sept formats** : `.gltf/.glb`, `.obj`, `.stl`, `.ply`,
`.fbx`, `.dae`, `.usda`. La commande à lancer **dès que la carte est libre** :

```
NK_GLTF_MODEL=Resources/Models/Futuristic_Car_2.1_fbx.fbx \
  ./Build/Bin/Release-Windows/renderdemo/renderdemo.exe --demo=<index DemoGLTF>
```

### b) Les modèles : trouvés, mesurés, et un piège de périmètre

✅ **Il y a une voiture réaliste, déjà dans le dépôt.**
`Resources/Models/Futuristic_Car_2.1_fbx.fbx` — **FBX binaire v7400**, 302 780 o,
4 293 polygones / 4 448 sommets, riggé et animé, **UV mappées**, 3 textures
(`textures/Futuristic_Car_C.jpg` 1,2 Mo diffuse, `_N.jpg` 2,2 Mo normale,
`_S.jpg` 968 Ko spéculaire). Source documentée (`Readme.txt`, 3dhaupt.com).
Le chargeur `NkFBXLoader.cpp:1695` **gère bien le binaire** (magique
`Kaydara FBX Binary`, version à l'octet 23, chemin 64 bits au-delà de 7500).

⚠️ **PIÈGE DE PÉRIMÈTRE, et il aurait faussé mon rapport** : dans l'arbre
`Nkentseu-noge`, les dossiers `backpack/`, `nanosuit/`, `cyborg/`, `planet/`
contiennent **leurs textures et leur `.mtl` mais pas leur maillage**. J'allais
écrire « les modèles réalistes n'ont pas de mesh ». ✅ **Vérification faite dans
le dépôt principal `D:/Projets/2026/Nkentseu/Nkentseu/` : les `.obj` y sont
tous** — `backpack.obj` 67 907 faces, `nanosuit.obj` 19 058, `cyborg.obj` 5 549.
`Resources/Models/` est **gitignoré (190 Mo)** : chaque arbre a sa copie locale,
et **la mienne est incomplète**. *Un résultat négatif sans sa racine est une
rumeur.*

| candidat | faces | UV | normales | textures | où |
|---|---|---|---|---|---|
| **`Futuristic_Car_2.1_fbx.fbx`** | 4 293 poly | ✓ | ✓ | 3 (C/N/S) | **les deux arbres** |
| `backpack.obj` | **67 907** | ✓ 47 528 | ✓ 36 248 | 5 (dont roughness+ao) | dépôt principal seul |
| `nanosuit.obj` | 19 058 | ✓ 7 567 | ✓ 12 801 | 30 | dépôt principal seul |
| `LowPolyCars.obj` | 1 840 | ✓ 3 731 | ✓ 1 431 | 1 + `.mtl` | les deux arbres |
| `dancing_vampire.dae` | 32 Mo, skinné+animé | — | — | dossier `textures/` **vide** | les deux arbres |

📄 Hors dépôt (rapporté par un balayage disque, non vérifié fichier par fichier) :
`D:/Telechargement/` et `C:/Users/Rihen/Downloads/` contiennent des voitures PBR
réalistes en glTF (`jeep_gladiator.zip` 20,9 Mo, `2022_ford_supervan_4.zip`
16,6 Mo) et **des kits de route complets** (`kenney_city-kit-roads.zip`,
95 modèles GLB ; `kenney_toy-car-kit.zip`, circuit de course ; `Modular Street
Pack by Quaternius`). ⚠️ Ce sont des **archives non extraites** — et c'est le
dossier personnel de Rodolf : je n'ai rien touché. **Aucun `.usd`/`.usdz`/`.abc`
sur tout le disque** — le chargeur `NkUSDALoader` n'a donc aucun sujet.

### c) ✅ CE QUI SE CHARGE ET CE QUI NE SE CHARGE PAS — mesure faite aujourd'hui

`NkAssetIODemo` est **console et CPU seul** : je l'ai donc lancé sans toucher au
GPU (binaire Debug du 2026-09-01, `PATH` ucrt64 en tête, journal vidé avant —
les trois pièges documentés). Il teste **exactement notre voiture**
(`main.cpp:183`).

**Résultat : `52 OK / 1 FAIL`, exit 1** — identique à la trace du 2026-08-16.

Ce que le chargeur dit de la voiture, mot pour mot :

```
[NkFBXLoader] texture introuvable : Resources/Models/Futuristic_Car_C.jpg
              (relatif='textures\Futuristic_Car_C.jpg')
[NkFBXLoader] texture introuvable : Resources/Models/Futuristic_Car_N.jpg
[NkFBXLoader] texture introuvable : Resources/Models/Futuristic_Car_S.jpg
[NkFBXLoader] OK 'Futuristic_Car_2.1_fbx.fbx' (binaire v7400) :
              15 geometries, 18758 verts, 26916 indices, 15 sous-meshes,
              3 materiaux, 3 textures, 16 nodes
[NkFBXImporter] importSkeleton/importAnimation/importLights/importCameras
                sont des no-ops
```

| ce qui arrive | ce qui se perd |
|---|---|
| ✅ géométrie complète : 18 758 sommets, 8 972 faces, 15 sous-maillages | 🔴 **les 3 textures — fichiers présents, chemin mal résolu** |
| ✅ normales et UV (le FBX les porte, le loader les lit) | 🔴 squelette (`importSkeleton` = no-op) |
| ✅ 3 matériaux Phong + 16 nœuds de hiérarchie | 🔴 animation, lumières, caméras (no-ops) |

### d) 🔴 LE DÉFAUT NOMMÉ — trois lignes, et il bloque tout rendu texturé

`Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkFBXLoader.cpp:1119-1127` :

```cpp
NkString fname = rel;
for (nk_size p = fname.Length(); p > 0; --p) {
    char ch = fname.CStr()[p - 1];
    if (ch == '/' || ch == '\\') { fname = NkString(fname.CStr() + p); break; }
}
NkString full = baseDir;  full.Append('/');  full.Append(fname);
```

Le loader **ne garde que le nom de fichier** et jette le dossier. Le commentaire
dit pourquoi : `RelativeFilename` peut contenir un chemin absolu de la machine
d'export — et c'est vrai, notre FBX en porte un
(`D:\Neuer Ordner\...\Futuristic_Car_2.0\textures\Futuristic_Car_C.jpg`).

**La défense est justifiée, mais elle détruit le cas légitime.** Le même FBX
porte aussi le chemin **relatif correct** : `textures\Futuristic_Car_C.jpg`. Le
loader cherche `Resources/Models/Futuristic_Car_C.jpg` ; le fichier est à
`Resources/Models/textures/Futuristic_Car_C.jpg`. ✅ **Un dossier trop haut.**

📌 **Le remède est petit et va dans le sens des deux règles gravées** : essayer
d'abord le chemin **relatif** (antislash normalisé en `/`) sous `baseDir`, et ne
retomber sur le nom seul qu'en cas d'échec. On garde la protection contre les
chemins absolus **et** on récupère le cas relatif : une doublure crédible au lieu
d'un trou. **C'est le premier correctif à faire avant la scène d'épreuve** —
sans lui, la voiture rendra en gris, et on accuserait le PBR.

### e) ⚠️ UNE CONCLUSION DU DOCUMENT PRÉCÉDENT QUI EST FAUSSE, ET DANS L'AUTRE SENS

Le bloc du 2026-08-16 lit le `1 FAIL` comme une régression :
*« une assertion est passée de OK à FAIL entre les deux dates »*, et le classe
parmi les « seuls défauts fonctionnels mesurés du module ».

✅ **J'ai ouvert l'assertion. C'est l'inverse.** `NkAssetIODemo/src/main.cpp:165` :

```cpp
// Le loader reel ne supporte ni materiaux ni squelette/anim : verifie
// que l'adaptateur n'invente rien.
Check(scene.materials.Empty(), "NkFBXImporter::Import: 0 materiau ...");
```

Le banc **exige zéro matériau**. Or le commit `d28a3728`
(*« matériaux Phong + textures externes dans le loader FBX »*) a **ajouté** le
support : le loader en produit maintenant **3**. Le `FAIL` ne signale pas une
perte — **il signale que le chargeur a progressé et que sa garde ne le sait pas.**

> C'est le motif que le dépôt paye tout le mois, sous une forme neuve : *un motif
> exact au moment où on l'écrit devient faux sans que rien ne le signale.* Ici la
> garde codait une limitation ; la limitation a été levée ; la garde est restée.
> Et la lecture « c'est une régression » est passée sans résistance parce
> qu'**elle allait dans le sens de ce qu'on cherchait** — un défaut.

**Ce qu'il faut faire** : retourner l'assertion (`!scene.materials.Empty()`,
3 attendus) et **ajouter le contrôle qui manque vraiment** — que les textures
soient **résolues**, pas seulement déclarées. Le banc actuel serait vert avec
trois matériaux sans la moindre image.

---

# CE QUE JE PROPOSE ENSUITE (rien n'est engagé sans accord)

**Immédiat, sans GPU :**
1. Corriger la résolution du chemin de texture FBX (§3.d) — quelques lignes.
2. Retourner l'assertion matériaux de `NkAssetIODemo` et lui ajouter un contrôle
   de **texture résolue** (§3.e).
3. Trancher `NkRenderQuality` : le rendre opérant ou le retirer (§1.2 Forme B).
   La règle de la maison ne laisse pas de troisième sortie.

**Dès que la carte est libre :** la scène d'épreuve (§3.a), captures Windows
avec et sans le correctif de textures — l'écart nommera ce qui manque au PBR.

**À décider par Rodolf :**
- **Linux** est fermable ici même via WSL2, sans Mac. Je le fais ?
- Les 11 modules sans corps : `Physics`, `Sequencer`, `Viewport`, `Selection`
  portent des mots qu'un jeu de course réclamera. **On les implémente, ou on les
  retire ?** Les laisser est le seul choix qui coûte sans rien rendre.
- Le sélecteur de profil dans le moteur (§1.3) — c'est le chantier qui rend la
  règle « le moteur porte la qualité » applicable au lieu qu'écrite.

**Question ouverte** : la voiture FBX est *rigged + animated*, et le loader
FBX ignore squelette et animation. Pour un jeu de course les roues doivent
tourner et braquer. **Anime-t-on par le squelette FBX (chargeur à étendre), ou
par la hiérarchie de nœuds (16 nœuds déjà lus) ?** La seconde est bien moins
chère et suffit à une voiture.
