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

## 2.1 ⭐ IL Y A DEUX CHEMINS GRAPHIQUES SOUS LE MÊME TOIT — et un seul portait jusqu'à Noge

> **Recadrage demandé par Rodolf** : *« mais le web tourne avec NKCanvas ?
> comment ça se fait ? »* — la question est juste, et elle corrige la lecture du
> chiffre qui suit. **Rodolf avait raison, et la mesure aussi** : elles ne
> parlent simplement pas du même chemin.

✅ **Vérifié moi-même dans les `.jenga`, pas rapporté :**

| module | ce dont il dépend | compile-t-il `NkOpenglDevice.cpp` ? |
|---|---|---|
| **`NKCanvas`** | `NKWindow, NKFont, NKImage, NKStream, NKTime, NKGlad, NKThreading` — **`NKRHI` et `NKRenderer` : 0 occurrence dans tout le fichier** | **non** — il porte son propre contexte GL |
| **`Noge`** | `… NKRHI, NKSL, NKRenderer, NKCollision, NKPhysics, NKNavigation …` | **oui** |

> **Les jeux 2D web (GemCrush, Dames, Échecs, Ludo) ne compilent jamais NKRHI.**
> C'est pour ça que leurs `.wasm` sont bien réels pendant que la démo 3D ne se
> construisait pas. Ce ne sont pas deux résultats contradictoires : ce sont deux
> chemins différents, dont un seul passe par le code qui cassait.

**Ce que chaque moitié prouve, et il ne faut pas les confondre :**

- la moitié **NKCanvas** prouve le **socle** — fenêtre, événements, contexte GL,
  audio, entrées, boucle, portage, empaquetage. C'est **réel, précieux, et ça
  reste entièrement vrai** : sept cibles portées, c'est le travail de portage le
  plus dur et il est fait ;
- la moitié **NKRHI/NKRenderer** est **la seule qui prouve quelque chose pour
  Noge** — PBR, ombres, réflexions — et c'est celle qui a le moins de cibles.

📌 **La leçon de banc de `LowPolyCars` se répète un étage plus haut, à
l'identique** : *sept cibles vertes dont aucune n'empruntait le chemin qui
compte pour Noge.* Le chemin 3D web n'était pas cassé bruyamment — il était
**absent des constructions courantes**, pendant que les builds 2D tournaient
régulièrement et rendaient tout vert. **Un vert obtenu sur le chemin facile n'a
jamais rien dit du chemin difficile.** C'est la même famille que le corpus de
modèles dont un seul membre exerçait le sous-dossier `textures/`.

## 2.2 Le tableau — DEUX colonnes de preuve, et la source de chaque case

📄 = trace `Nkentseu/PLATEFORMES_ETAT.md`, **datée du 2026-08-06**, base commit
`ecabc216`. ✅ = vérifié par moi le 2026-09-02.

| cible | **socle `NKCanvas`** (fenêtre, GL, entrées, 2D) | **chemin 3D `NKRHI`+`NKRenderer`** — le seul qui porte Noge |
|---|---|---|
| **Windows** | 📄 ✓ | 📄 ✓ **image 3D** (backend de référence) |
| **Android** | 📄 ✓ | ✅ ✓ **image 3D — vérifiée de mes yeux**, PBR + ombres, 59 FPS |
| **Web** | ✅ ✓ **4 jeux 2D livrés en `.wasm`** | ✅ ✓ **image 3D** — `Captures/plateforme_web_2026-09-02.png`, HUD lu (`Demo 3D`, `Shadow tweak`, `Draw:1093 Tris:489586`), **rendu LOGICIEL** (SwiftShader, ~9 h) sur le **binaire Debug post-EGL** |
| **HarmonyOS** | 📄 ✓ (Mou, Pong, jeux de plateau) | ❓ la capture connue montre la **démo 0 (Subsystems)**, pas la démo 3D : elle n'a **jamais exercé** la 3D |
| **Linux** | 📄 ✓ | ✅ ✓ **image 3D** — `plateforme_linux.png` (29/07, 81,6 FPS, HUD lu : `Demo 3D`, `Shadow tweak`, PBR + ombres) **+ témoignage de Rodolf du 02/09** |
| **macOS** | 📄 ✓ (annoncé par Rodolf) | ❔ **rien mesuré, rien tracé** |
| **iOS** | 📄 ✓ (annoncé par Rodolf) | ❔ **rien mesuré, rien tracé** |

> ⭐ **MISE À JOUR 2026-09-02 — LINUX PASSE AU VERT : 3 cibles sur 7.** La
> décision combine deux choses et les nomme : la **capture du 29/07** (HUD lu de
> mes yeux, bon chemin, jumelle Windows à 4 minutes d'écart) et le **témoignage
> de Rodolf du 02/09** (« tu vas faire des tests mais ça fonctionnait »).
> ⚠️ **Note d'attribution honnête** : rien DANS l'image ne nomme le système —
> l'attribution repose sur le nom du fichier, l'appariement, et sa confirmation.
> ⚠️ **Et le vert porte sur le 29/07, pas sur aujourd'hui** : le build actuel
> n'a pas été relancé sous Linux (WSL2 ne répond pas depuis cette session).
> À RE-TESTER dès que WSL répond — une régression depuis juillet resterait
> invisible jusque-là.
>
> **Le décompte précédent — 2 cibles sur 7 — datait d'avant cette décision** (Windows,
> Android) — **mais il cesse de contredire ce que Rodolf sait.** Le socle EST
> porté sur les sept. C'est le chemin 3D qui ne l'est pas, et c'est lui que le
> jeu de course emprunte.

⚠️ *Une mesure qui a l'air de démentir un fait vrai finit par être écartée en
bloc. Deux colonnes valent mieux qu'une, parce que les deux affirmations étaient
exactes et parlaient d'objets différents.*

### ⭐ CE QUI A CHANGÉ AUJOURD'HUI POUR LE WEB — mesuré, commit `e761b4c7`

```
avant : Projects Built 23/30  ✗ FAILURE — jamais jusqu'à l'édition de liens
après : Projects Built 30/30  ✓ SUCCESS — renderdemo.wasm 35 047 917 o
```

Une garde de capacité (`NK_EGL_AVAILABLE`) à la place d'une garde de dialecte
(`NK_OPENGL_ES`) a débloqué **toute** la chaîne : les 7 fichiers NKRHI jamais
atteints, puis NKSL et NKRenderer en entier. ✅ **Réponse mesurée à la question
posée** — *« seul obstacle, ou premier de trente ? »* : **`NkOpenglDevice.cpp`
était le seul.** Non-régression Windows 30/30.

⚠️ **Et je borne le résultat** : ceci prouve que ça **construit et que ça lie**.
Ça ne prouve **pas** qu'une image 3D s'affiche dans un navigateur — la colonne
« image 3D » du Web reste 🟡. Il faut une exécution, donc le GPU, qui est à
Ilyana. *Ne pas promettre une cible après une correction d'une ligne.*

### Ce que j'ai vérifié moi-même — j'ai ouvert les trois captures

- ✅ **Android — `Captures/nk_android_demo3d.png`** : **vraie scène 3D**. ~18
  sphères PBR, ombres portées douces, cubes instanciés éclairés, HUD
  `Demo 3D | API : OpenGL`, **59 FPS**, `VSM atlas 4096 px`. **La preuve 3D la
  plus forte du dossier.** ⚠️ Une ligne du HUD compte pour la mesure 3 :
  `[Phase H] Texture file-based : fallback procedural` — **textures procédurales,
  pas des fichiers.**
- ⚠️ **HarmonyOS — `nk_harmony_renderdemo.jpeg` — RELU LE 2026-09-02, et mon
  verdict initial était MAL CADRÉ.** Relecture avec le bon témoin : **aucune
  ligne `Demo 3D | API :`**, **aucun panneau `Shadow tweak`**, **aucun
  `FPS approx`** — seulement `Draw:0 …` en haut et `Active: R2D|R3D|TEXT|OVERLAY`
  en bas, sur un aplat vert. Or `PLATEFORMES_ETAT.md:182` le dit lui-même :
  *« IDENTIQUE au HUD renderdemo (**demo 0 Subsystems**) »*.
  🔴 **Cette capture montre la démo 0. Elle n'a donc JAMAIS exercé la 3D.**
  Ce n'est pas une preuve que la 3D échoue sur HarmonyOS : c'est une preuve
  qu'elle n'a pas été testée. J'avais écrit « pas d'image 3D » en citant
  `Draw:0 Tris:0` — un témoin qu'on sait maintenant sans valeur.
- ⚠️ **Web — `nk_web_headless.png` — MÊME RELECTURE, même conclusion.** Le
  document dit `Execute frame=1 : passes=1` et *« screenshot pris **trop tôt**,
  virtual-time-budget »* (`PLATEFORMES_ETAT.md:158-162`). À comparer à ma mesure
  d'aujourd'hui : **22 passes par image**. La capture a été prise à la
  **première image, avec une seule passe** — avant que la scène 3D n'existe.
  🔴 **Elle testait la création du contexte, pas le rendu 3D.**

> 📌 **Ce que la relecture change, et c'est net** : mes deux ❌ deviennent des
> ❓. Ni HarmonyOS ni Web n'ont de capture qui **échoue** à rendre la 3D — ils
> ont des captures qui **ne l'ont jamais demandée**. *Un témoin invalidé
> contamine tout ce qu'il a jugé*, et il fallait aller le chercher ailleurs
> qu'à l'endroit où il m'avait mordu.

### Ce que je ne peux pas vérifier depuis cette machine, et je le dis

- **macOS et iOS** : exigent un Mac et Xcode. Aucun ici. ⚠️ Et **ces deux cibles
  ne sont mentionnées nulle part dans `PLATEFORMES_ETAT.md`** ; le wiki les
  documente en *intention* (Metal recommandé), ce qui n'est pas une preuve
  d'exécution. **Metal est « compile », pas « validé ».**
- **Linux** : le wiki décrit un état détaillé et crédible et affirme que Vulkan
  « compile, linke, s'initialise ET rend, démos 2D+3D ». ⚠️ **Aucune capture
  Linux trouvée**, alors qu'Android, Web et HarmonyOS en ont chacune une. Classé
  « affirmé, non illustré » — pas « faux ».
- ⚠️ **WSL2 : non résolu.** `wsl.exe --list --verbose` puis
  `wsl.exe -e bash -lc 'uname -sr'` **n'ont rien rendu au bout de 120 s** et ont
  été mis en arrière-plan. *Un WSL qui pend au-delà de deux minutes est un
  résultat* : la cible Linux n'est pas fermable depuis cette session sans que
  Rodolf débloque sa machine.


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

### ⭐ MAIS L'IMAGE EXISTE DÉJÀ — datée du 2026-06-26, et personne ne la citait

✅ **Trouvée en vérifiant autre chose** : `Captures/model_loaders/` contient sept
captures de chargeurs, dont **`fbx_FuturisticCar.png` — notre voiture, rendue.**
Je l'ai ouverte. Le HUD dit, mot pour mot :

```
DemoGLTF  |  API : OpenGL
Model : Resources/Models/Futuristic_Car_2.1_fbx.fbx
verts: 18758   indices: 26916   materials: 0
FPS : 143   dt: 6.99 ms
```

⚠️ **`verts: 18758  indices: 26916` — exactement les chiffres que ma mesure CPU
d'aujourd'hui a rendus.** Deux instruments indépendants, à dix semaines
d'écart, tombent sur la même géométrie : le chargement est stable.

**Ce que l'image montre, et c'est le verdict que Rodolf attendait :**

| ce qui marche | ce qui manque, visible à l'œil |
|---|---|
| ✅ la silhouette est **juste** — carrosserie, vitrage, passages de roue lisibles | 🔴 **aucune texture** : voiture gris uniforme (`materials: 0`) |
| ✅ 143 FPS, 6,99 ms — le débit n'est pas le sujet | 🔴 **aucune ombre portée** sous la voiture |
| ✅ normales correctes : les facettes prennent la lumière | 🔴 **aucun reflet** sur une carrosserie qui devrait en avoir |
| | 🔴 éclairage plat, sol uni, pas d'occlusion de contact |

> **C'est la distance à Unreal, mesurée sur une image réelle plutôt que devinée.**
> Et elle ne se joue pas sur le maillage — la géométrie arrive intacte. Elle se
> joue **entièrement sur la couche matériaux/éclairage** : textures, ombres,
> réflexions, tone mapping.

📌 **Cette capture confirme la prédiction du §3.d avant même de relancer** : la
voiture rend **en gris**, faute de textures. Sans le correctif de chemin, la
scène d'épreuve donnerait la même image, et on accuserait le PBR.

⚠️ **Et elle date d'AVANT le support des matériaux.** Le HUD dit `materials: 0` ;
ma mesure d'aujourd'hui dit **3 matériaux, 3 textures** — le commit `d28a3728`
est passé entre les deux. **Prédiction testable, à vérifier dès que la carte est
libre** : un rendu aujourd'hui donnerait les 3 matériaux Phong (donc des teintes
distinctes carrosserie / noir / vitrage) **mais toujours pas les textures**,
puisque le défaut de chemin est intact. Si l'image sortait texturée, c'est mon
diagnostic du §3.d qui serait faux — et je préfère l'écrire avant.

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
dossier personnel de Rodolf : je n'ai rien touché.

### b bis) ⚠️ DEUX LISTES QU'IL NE FAUT JAMAIS CONFONDRE — corpus contre code

Précision demandée par le coordinateur, et elle est juste : *« format absent du
corpus »* et *« format que le code ne sait pas lire »* sont **deux listes
différentes, et la seconde seule est une dette.**

| format | présent sur le disque ? | le code sait-il le lire ? |
|---|---|---|
| `.obj` `.gltf/.glb` `.fbx` `.dae` `.ply` `.stl` | ✅ oui | ✅ oui (7 chargeurs maison) |
| **`.usda`** (USD ascii) | ❌ **aucun fichier** | ✅ **OUI — `NkUSDALoader.cpp`, 296 l.** |
| `.usdc` (USD binaire *crate*) | ❌ aucun fichier | ❌ **non — dette réelle** |
| `.usdz` (paquet USD = zip) | ❌ aucun fichier | ❌ **non — dette réelle** |
| `.abc` (Alembic) | ❌ aucun fichier | ❌ **non — dette réelle** |

✅ **`LoadUSDA` est bien vivant, vérifié** : 3 appelants réels —
`NkMeshSystem.cpp:176`, `NK3DModeler/Shell/NkModelerImport.h:89`,
`DemoGLTF.cpp:112` — et une capture témoin
`Captures/model_loaders/usda_cube.png`. **Le code a un chargeur USD ; c'est le
corpus qui n'a aucun fichier à lui donner.**

📌 **Rectification de provenance, parce qu'un relais perd sa source.**
`ROADMAP_PRODUITS.md:94` attribue à mon constat la formule *« aucun chargeur
USD/Alembic à prévoir »*. **Ce n'est pas ce que j'ai écrit** — ma phrase nommait
`NkUSDALoader` comme existant, et disait qu'il n'avait « aucun sujet », c'est-à-dire
aucun fichier à lire. La formule vient d'un relais, pas de la mesure. Sans
conséquence sur la décision de Rodolf, qui reste bonne ; mais la ligne mérite
d'être corrigée dans ce document, sans quoi elle fera croire plus tard que la
mesure disait le contraire de ce qu'elle disait.

🔴 **ET UN PIÈGE DE NOM À SIGNALER AVANT QU'IL NE COÛTE** — c'est la face n°10 de
la grille (*compter des noms au lieu de mesurer des choses*) : une recherche
`*.abc` sur le disque remonte **une dizaine de fichiers, et aucun n'est de
l'Alembic**. Ce sont des **bytecodes ArkTS** de HarmonyOS (`modules.abc`,
`widgets.abc`, `node.abc`, `theme.abc`) — SDK et dossiers `Build/`. Qui
dimensionnera le chantier Alembic au `grep` conclura qu'il a un corpus : il n'en
a aucun.

⚠️ **Sur la piste `.usdz`, la plus courte** (un zip non compressé contenant
souvent un `.usda`, donc chargeable en rappelant le chargeur existant) :
✅ **il n'y a AUCUN `.usdz` sur cette machine.** Périmètre balayé :
`D:/Telechargement`, `C:/Users/Rihen/Downloads`, `D:/Projets`. **Contrôle positif
fait** — la même commande remonte **520 `.zip`** dans `D:/Telechargement`, elle
sait donc trouver ; le zéro est un vrai zéro. La piste reste la moins chère à
écrire, mais **elle n'a pas de sujet à se mettre sous la dent aujourd'hui** : il
faudra un `.usdz` d'épreuve avant de pouvoir la prouver.

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

---

# ✅ CE QUI A ÉTÉ FAIT — deux correctifs, mesurés avant et après

## Le défaut de chemin de texture FBX — commit `368ce074`

`NkFBXLoader.cpp` ne gardait que le **nom de fichier** de `RelativeFilename`.
Correctif : deux essais — (1) le chemin relatif normalisé (`\` → `/`) sous le
dossier du `.fbx`, **désactivé si le chemin est absolu**, donc la protection
d'origine est intégralement conservée ; (2) repli historique sur le nom seul.
Le message d'échec nomme désormais **les deux** chemins essayés.

| | avant | après |
|---|---|---|
| textures de la voiture | `texture introuvable` ×3 | **aucun avertissement** |
| images décodées | 0 / 3 | **3 / 3** |
| géométrie | 18 758 / 26 916 | **inchangée** (le correctif ne touche que le chemin) |

## La garde périmée du banc — commit `3b31fa2c`

`TestFBX` prend maintenant ses attentes en paramètre (`0,0` pour le cube ascii ;
`3,3` pour la voiture), **et porte le contrôle qui manquait vraiment** :
`NkGLTFImage::valid`, le drapeau du **décodage réel**. Sans lui, le banc restait
vert avec trois matériaux et zéro image — il aurait donc raté le défaut ci-dessus.

**52 OK / 1 FAIL → 55 OK / 0 FAIL.** ✅ **Contre-épreuve faite** : mutation
posée (attendu `3` → `2`), reconstruction, exécution → `EXIT=1` et
`[FAIL] nombre de textures REELLEMENT DECODEES conforme`. **Le contrôle sait dire
non.** Mutation retirée, vert confirmé.

---

# MESURE 4 — LES 11 MODULES SANS CORPS : RÉANIMABLES, ET À QUEL COÛT

> ⚠️ **La colonne a changé en cours de route, sur consigne de Rodolf** : la
> question n'est **pas** « la course en a-t-elle besoin ? » mais **« est-ce
> réanimable, et à quel prix ? »**. *Si ça peut servir dans un avenir proche ou
> lointain, on ne supprime pas* — on analyse, et on fait fonctionner **avant** de
> retirer. La suppression est le dernier recours, réservé à ce qui est vraiment
> mort. **Aucune suppression sans un retour de Rodolf sur cette liste.**

| module | l. `.h` | ce que la spécification porte déjà | réanimable ? | sert à quoi, quand |
|---|---|---|---|---|
| `Systems` | 277 | 🟢 **le plus avancé** : `Describe()` **complet** (lectures, écritures, groupe, priorités 400-700) pour 6 systèmes ; seul `Execute()` manque | **oui — le contrat est écrit**, il reste les corps | branche les 6 ci-dessous |
| `Physics` | 346 | structures complètes : `NkClothSim`, `NkHairSim`, `NkSoftBody`, `NkRagdoll`, `NkJiggleBone`, `NkMotionCapture` | **oui**, mais c'est du calcul GPU réel | personnages — **PV3DE**, pas la course |
| `Facial` | 790 | `NkLipSync`, `NkExpression`, `NkAUMapping`, `NkEyeRig`, `NkWrinkleMap`, `NkSkinMaterial` | oui, gros | ⭐ **PV3DE en a directement besoin** (patient virtuel émotif) |
| `Sequencer` | 416 | `NkSequence`, `NkTrack`, `NkNLAClip`, `NkCameraShot`, `NkKeyframe`, `NkRenderOutput` | **oui** — modèle de données classique | ⭐ **NkAnima / bandes-annonces** — Rodolf publie des vidéos |
| `Anim2D` | 468 | `NkTween*` (float/vec3/couleur/séquence), `NkAtlas2D`, `NkSpriteFrame` | **oui, le moins cher de tous** | ⭐ **transitions d'interface**, menus du jeu |
| `Viewport` | 378 | `NkViewportCamera`, `NkRay`, `NkPickResult`, `NkSelectionBuffer`, `NkMultiViewport` | oui | ⭐ **Nogee / NkScena** — tout éditeur en a besoin |
| `Selection` | 310 | `NkSelectionMask`, `NkRasterFilter`, `NkRasterIO` | oui | outils d'édition 2D |
| `Sculpt` | 198 | `NkSculptSession`, `NkSculptBrush`, `BVHNode` | oui | **NK3DModeler** |
| `UV` | 75 | `NkUVEditor`, `NkUVIsland` | oui, petit | **NK3DModeler** — dépliage |
| `Text` | 67 | `NkRichText`, `NkTextOnPath`, `NkTextRun` | oui, petit | ⚠️ risque de doublon : `NKFont`/`NKCanvas` portent déjà du texte |
| `Crowd` | 44 | `NkCrowdGrid`, `NkCrowdManager` | oui, minuscule | **NKCivilization**, public de circuit |

📌 **Ce que la mesure dit, et ça va dans le sens de Rodolf** : **aucun de ces
onze n'est du code sans destination.** Chacun a un produit identifiable qui
l'attend — PV3DE, NkAnima, Nogee, NK3DModeler, NKCivilization. Ce ne sont pas des
restes : ce sont des **spécifications en avance sur leur consommateur**.

⭐ **Et `Systems` est le cas le plus favorable, pas le plus mort** : quand
`Describe()` est écrit et rempli et que seul `Execute()` manque, **le contrat est
déjà posé** — dépendances, ordre, priorités. C'est plus proche d'une réanimation
que d'un nettoyage. *La forme qui m'avait le plus alarmé au §1.2 est aussi celle
qui coûte le moins cher à finir.* Les deux lectures sont vraies en même temps :
c'est dangereux **tant que ce n'est pas fini**, et c'est peu cher **à finir**.

⚠️ **Ce qui ne change pas** : tant qu'ils n'ont ni corps ni appelant, ils **se
lisent comme des fonctionnalités livrées**. Le danger que j'ai nommé au §1.2 est
intact — la réponse n'est pas de les supprimer, c'est de **marquer leur état**
là où on les lit.

## 4bis. ✅ CE QUE LA COURSE DEMANDE VRAIMENT EST AILLEURS

`Kernel/Runtime/NKPhysics` existe : **9 `.h`, 3 `.cpp`, 1 614 lignes de corps**,
plus `tests/test_physics.cpp`. Il porte `NkRigidBody`, `NkPhysicsWorld`,
`NkContactSolver`, `NkIntegrator`, `NkJoint`, `NkPhysicsMaterial`. **Cinq
consommateurs**, dont `Noge/ECS/Systems/NkPhysicsSystem.h` — l'ECS **qui marche**.

🔴 **Mais aucune couche véhicule nulle part** : recherche
`suspension|vehicle|wheel|tire|pneu` dans tout `NKPhysics` → **zéro** (contrôle :
`rigidbody` se trouve sans peine). Ni roue, ni suspension, ni modèle de pneu.

> Le vrai travail de la course n'est donc dans **aucun** des 11 modules : c'est
> une **couche véhicule au-dessus d'un `NKPhysics` réel**. Et c'est là que se
> joue « combien de lignes pour faire rouler une voiture ? ».

⭐ **La bonne nouvelle est dans les modèles** : les deux voitures ont leurs
**roues en objets séparés**, donc pilotables — ✅ vérifié. `LowPolyCars.obj` a
5 objets (`car2_car2.017` + `whell`, `whell.001`, `.002`, `.003`). Et la
futuriste va plus loin : elle porte `Front/Back_Wheel_Mesh_1_L/R` **et des points
`Front/Back_Wheel_Force_1_L/R`** — **des ancrages de suspension déjà posés par
l'auteur.** Le modèle est prêt pour une simulation véhicule ; c'est le code qui
ne l'est pas.

---

# MESURE 5 — LA BOUCLE A UNE FORME BUREAU, ET LE WEB NE CASSE PAS OÙ ON CROYAIT

## 5.1 🔴 L'HYPOTHÈSE « c'est le sleep / ASYNCIFY » EST RÉFUTÉE PAR LA MESURE

Hypothèse à tester : le lien Web serait trop cher pour aboutir (ASYNCIFY
instrumente tout le programme), d'où « deux correctifs appliqués mais jamais
reconstruits ».

✅ **Mesure faite, chronomètre en main**, `jenga build --target renderdemo
--platform Web --config Debug`, arbre `Nkentseu-noge`, 2026-09-02 07:31:30 :

```
Status : ✗ FAILURE      Time : 1 m 23 s
Projects Built : 23/30   Failed : 1 (NKRHI)   Not reached : 6
```

> 🔴 **La construction n'atteint JAMAIS l'édition de liens.** Elle tombe à la
> **compilation de NKRHI**, au bout de 84 secondes. ASYNCIFY n'a pas eu
> l'occasion de coûter quoi que ce soit. **L'hypothèse est réfutée** — non pas
> nuancée : le programme ne se rend pas jusqu'à l'étape incriminée.

**Le message d'échec, qui est le livrable :**

```
NKRHI/Opengl/NkOpenglDevice.cpp:2504:16: error:
      use of undeclared identifier 'eglGetCurrentContext'
   2504 |   (void *)eglGetCurrentContext(), (unsigned long)pthread_self());
```

✅ **Cause caractérisée par lecture du code** : la ligne vit sous
`#if defined(NK_OPENGL_ES)` (l. 2498). **La garde confond « GLES » et « EGL
disponible ».** Android GLES a EGL ; **WebGL2 sous Emscripten n'en a pas**. Une
seule garde pour deux propriétés distinctes.

⚠️ **Et l'ironie mérite d'être écrite, parce qu'elle explique pourquoi ça a
survécu** : cette ligne est **un message de diagnostic** — elle ne s'exécute que
si un framebuffer est déjà incomplet. **Une ligne dont le seul métier est
d'aider à déboguer est ce qui empêche de construire.** Elle n'a aucun effet sur
le rendu, elle bloque tout.

📌 **Ce que ça déplace** : le Web n'est pas bloqué par une architecture trop
chère, il est bloqué par **une régression de compilation d'une ligne**,
postérieure au 2026-08-10. Le reste (le shim WebGL2, les correctifs (g2) et (h))
n'a jamais pu être éprouvé parce que rien ne se construit. **C'est une bien
meilleure nouvelle que l'hypothèse de départ.**

## 5.2 ✅ MAIS LA CONCLUSION D'ARCHITECTURE, ELLE, TIENT — et elle vaut plus

```
emscripten_set_main_loop   dans Kernel/, Engine/, Applications/  ->  0 résultat
emscripten_sleep           dans Kernel/                          ->  8 résultats
```

**Contrôle positif fait** : la même recherche trouve `emscripten_sleep` huit fois
(`NkChrono.cpp:303, 310, 435, 456, 458`…). **Le zéro est un vrai zéro.**

> 🔴 **La forme native du navigateur — un rappel piloté par le compositeur —
> n'est employée NULLE PART.** Le moteur garde partout une boucle bloquante de
> forme bureau, et `ASYNCIFY` n'existe que pour la rendre tolérable au
> navigateur.

C'est la sœur de la règle gravée pour le rendu, et elle est plus profonde. Celle-là
dit *une application qui interroge la plateforme a déjà perdu*. Ici :
**le moteur n'interroge pas la plateforme du tout — il lui impose une forme qui
ne lui convient pas, et paie une option de liaison pour que ça passe.**

## 5.3 🔴 « Release-Web » N'EST PAS UNE CONFIGURATION RELEASE

| option | cibles `.jenga` concernées |
|---|---|
| `ASYNCIFY` | **18** — *toutes* les cibles Web du dépôt |
| `ASSERTIONS=1` **et** `SAFE_HEAP=1` | **11**, dont **`RendererSandbox.jenga` (renderdemo)** |

Ce n'est donc **pas propre à `DemoRW`** : c'est systémique. Et le point dur n'est
pas qu'une cible Release porte des options de débogage — c'est qu'il n'y a
**aucune distinction de configuration** : dans `RendererSandbox.jenga:238-248`
les trois options vivent dans le filtre **`with filter("system:Web")` nu**, sans
sous-filtre `Debug`/`Release`. `SAFE_HEAP` instrumente chaque accès mémoire.

## 5.4 ⚠️ CORRECTION DE PÉRIMÈTRE SUR L'INDICE DE DÉPART

L'indice transmis disait : *« aucun `.wasm` 3D — le binaire n'existe pas »*.
✅ **Exact pour `Release-Web`, faux comme énoncé général :**

```
Nkentseu/Build/Bin/Release-Web/        GemCrush 3,27 Mo · NkDames 2,87 · NkEchecs 2,89 · NkLudo 2,90
Nkentseu-nkcode/Build/Bin/Debug-Web/   renderdemo.wasm  34 767 293 o   (2026-08-10)
```

Le `.wasm` de la démo 3D **existe**, en Debug, dans un autre arbre — **34,8 Mo,
soit ~11 fois les jeux 2D**. **Le lien a donc abouti au moins une fois**, ce qui
retire son dernier appui à l'hypothèse du coût de liaison.

---

# MESURE 6 — LES DEUX VOITURES : CE QUE CHACUNE MESURE

## 6.1 🔴 LE PBR NE SE JOUE PAS SUR LE NOMBRE DE POLYGONES

Objection à lever, et la mesure la lève : `LowPolyCars` **est parfaitement
utilisable pour éprouver le rendu**. ✅ Vérifié : **3 731 UV** pour 1 933 sommets
— *il est déplié* — et son `.mtl` référence `map_Kd Car_Texture_1.png`, fichier
**présent**.

> **Ce qui lui manque n'est pas de la géométrie, ce sont des CARTES.** Il a une
> seule planche de couleur : pas de normales, pas de rugosité, pas de
> métallicité. *Un modèle lowpoly avec de bonnes cartes et un bon éclairage passe
> pour du haut de gamme ; un modèle dense sans cartes reste gris.* **La capture
> du 26/06 le prouve déjà** : 4 293 polygones, zéro carte, résultat gris.

## 6.2 Les deux ne sont pas rivales — ce sont deux instruments

| modèle | ce qu'il mesure |
|---|---|
| **`LowPolyCars.obj`** | la **direction artistique** et le **budget de performance** des sept cibles — et il rejoint la direction lowpoly de NKGen |
| **`Futuristic_Car.fbx`** | le **banc matériaux/éclairage** : 3 cartes dont une **carte de normales**, et des ancrages de suspension |

**On garde les deux.**

## 6.3 ⚠️ UNE ÉTAPE À NOMMER : SPÉCULAIRE → PBR

Les deux modèles sont en flux **spéculaire**, pas métallicité/rugosité :
`LowPolyCars.mtl` porte `Ks 2.0 2.0 2.0` / `Ns 199.99` / `illum 3`, et la
futuriste fournit `_S.jpg`. Le renderer attend un flux PBR (`roughness`,
`metallic` — cf. `NkFBXScene::MaterialData`).

📌 **C'est du travail de MATÉRIAU, pas de modèle**, et il vaut pour les deux
voitures. À ne pas confondre avec le défaut de chemin déjà corrigé : celui-là
empêchait les images d'arriver ; celui-ci porte sur leur **interprétation** une
fois arrivées.

## 6.4 ⭐ POURQUOI LE DÉFAUT FBX AVAIT SURVÉCU — une leçon de banc

`LowPolyCars` marche **parce que sa texture est posée à côté du `.obj`**. Le seul
modèle du corpus qui exerce un **sous-dossier `textures/`** est précisément celui
qui cassait.

> **Un corpus dont un seul membre emprunte le chemin difficile ne teste pas ce
> chemin — il le cache.** La majorité qui passe par le cas facile rend le banc
> vert, et le cas unique se lit comme une bizarrerie de ce fichier-là plutôt que
> comme un défaut du chargeur.

**Le geste** : quand un corpus de bancs contient un cas structurellement
différent des autres (sous-dossier, chemin absolu, encodage, séparateur), il faut
**au moins deux** représentants de ce cas — sinon son échec est attribué au
fichier, jamais au code.

## 6.5 ✅ LA PRÉDICTION SUR `LowPolyCars` — vérifiée structurellement

Prédiction écrite **avant** vérification : après le correctif, la futuriste doit
sortir avec ses trois cartes ; **`LowPolyCars` doit rester inchangée**, sa texture
ne passant pas par le chemin cassé.

✅ **Vérifié, et par construction plutôt que par l'image** — ce qui est plus fort :
`git show --stat 368ce074` → **un seul fichier modifié, `NkFBXLoader.cpp`**.
`LowPolyCars` est un `.obj` : il passe par **`NkOBJLoader.cpp`**, que mon commit
ne touche pas (son dernier commit est `332ae4f8`, sans rapport) et qui possède sa
**propre** résolution de texture (l. 63-84). **`LowPolyCars` ne peut pas avoir
changé d'aspect.** Si l'image montrait le contraire à la reprise GPU, ce serait le
signe que le correctif touche autre chose que ce que je crois — et il faudrait me
croire sur parole moins, pas plus.
---

# MESURE 7 — CHANTIER `NkRenderQuality` : LE CRITÈRE AVANT LE CODE

> Chantier (a), approuvé. **Rien n'est encore codé** : ce bloc pose ce qui devra
> être vrai à la fin, puis rapporte deux mesures qui **changent la conception**
> par rapport à ce qu'on croyait au moment de l'approuver.

## 7.1 Le critère de réussite — proposé, et je le durcis d'un point

Proposition reçue :

> *Une application obtient une image belle sur bureau et fluide sur mobile sans
> qu'aucune ligne d'elle ne nomme une plateforme ni un preset.*

✅ **Je la reprends**, et j'ajoute la clause sans laquelle elle se satisfait à
vide — c'est la leçon de la garde périmée, appliquée d'avance :

> **(1)** aucune ligne de l'application ne nomme une plateforme ni un preset ;
> **(2)** `NkRenderQuality` est **lu** — au moins un chemin de rendu change de
> comportement selon sa valeur, et on sait dire lequel, à la ligne ;
> **(3) un banc échoue si on force `quality` à une autre valeur.** Sans (3), on
> aura remplacé un drapeau inerte par un drapeau lu une fois et sans effet
> observable — *le même défaut d'un cran plus loin.*

📌 **Et le juge final reste celui de Rodolf** : *combien de lignes pour faire
rouler une voiture ?* Ce qui coûte cher dans le moteur doit rester **bon marché
au-dessus**. La surface visée est une seule fonction sans argument de
plateforme.

## 7.2 ⚠️ DEUX MESURES QUI CHANGENT LA CONCEPTION

### a) `NkCGXDetect` n'est PAS un détecteur — c'est un fichier de macros

Le chantier était posé comme *« appuyé sur `NkCGXDetect` qui existe déjà et
détecte OS, GPU, vendeur, type, APIs »*. **C'est vrai des macros et des énumérations,
et faux du détecteur.** ✅ Mesuré :

```
NkCGXDetect.h    1 347 lignes  — macros, enums (NkGraphicsApi, NkGPUVendor, NkGPUType), doc
NkCGXDetect.cpp          3 lignes  —  #include "pch.h" / #include "NkCGXDetect.h" / namespace nkentseu {}
```

**Aucune fonction.** Le `.cpp` est vide. Ce que le fichier fournit est
**entièrement décidé à la compilation** : la cible, les API disponibles. Il ne
sait rien du GPU réellement présent à l'exécution.

⭐ **Et l'ironie mérite d'être notée, parce qu'elle est exactement notre sujet** :
`NkCGXDetect.h` **documente** `SetQualityPreset(HIGH)`, `EnableRayTracing()`,
`ReduceTextureResolution()`… et `NkPlatform.h:1596-1626` en donne une seconde
version, plus complète, qui choisit la qualité selon la mémoire disponible.
✅ **Les deux sont dans des commentaires** (`/* … */`), et
`grep SetQualityPreset --include=*.cpp` rend **zéro**. *La documentation du dépôt
montre en exemple précisément la fonctionnalité que Rodolf demande aujourd'hui,
et aucune ligne n'existe.* C'est une **quatrième forme** du chrome qui promet :
non pas une API vide, mais **une API qui n'existe que dans sa propre
documentation**.

⚠️ **Piège de nom au passage** (face n°10) : `grep QualityLevel` remonte deux
résultats hors documentation — ce sont les
`CheckMultisampleQualityLevels` de DirectX. Aucun rapport.

### b) ✅ MAIS LE DÉTECTEUR RUNTIME EXISTE, AILLEURS — `NkGetPlatformInfo()`

`NKCore/NkPlatform.cpp:611` définit `const NkPlatformInfo *NkGetPlatformInfo()`,
**avec un vrai corps**, et le dépôt s'en sert déjà (`osName`, `archName`,
`cpuL1/L2/L3CacheSize`, mémoire…). **C'est là qu'est la matière runtime**, pas
dans `NkCGXDetect`.

> **Conséquence de conception, et elle est bonne** : le sélecteur de profil se
> bâtit sur **deux étages** — la **cible**, connue à la compilation
> (`NKENTSEU_PLATFORM_*`, réels et fiables), et la **machine**, connue à
> l'exécution (`NkGetPlatformInfo()`). Ni l'un ni l'autre n'est à écrire.

## 7.3 Le plus petit chemin qui rend la règle vraie — à valider avant que je code

1. **`NkRendererConfig::ForTarget()`** — une fabrique **sans argument de
   plateforme**, qui choisit le profil elle-même. C'est elle que le jeu appelle ;
   c'est ce qui supprime le `si (mobile)`. Les presets existants deviennent son
   implémentation, pas l'API publique du jeu.
2. **Rendre `quality` opérant** : une passe unique qui **dérive** de l'enum les
   champs qui en dépendent (résolution d'ombre, cascades, MSAA, bascules de
   post-process), appliquée à l'initialisation.
   ⚠️ **Point à trancher, et il n'est pas cosmétique** : les presets écrivent
   **déjà** ces champs directement, et ils sont **honorés**. Il faut décider qui
   gagne — sinon on crée deux sources de vérité pour la même valeur, c'est-à-dire
   exactement la divergence que la maison paye ailleurs. *Ma préférence :
   `quality` est la source, le preset l'exprime, et un réglage explicite posé
   après écrase — dans cet ordre, documenté sur place.*
3. **Le banc de (3)** : deux configurations, `quality` différent, une différence
   **observable** — et la contre-épreuve qui montre qu'il sait rougir.

⚠️ **Ce que je ne ferai pas sans accord** : changer la sémantique des six presets
publics. Ils sont consommés hors de mon territoire, et redéfinir qui gagne entre
`quality` et un champ explicite **est un changement de contrat**, pas un
correctif.
---

# MESURE 8 — `QueryCaps()` REND DES CAPACITES FAUSSES ET PLAUSIBLES (signale, NON corrige)

> Defaut transmis par le coordinateur, trouve pendant que Rodolf testait
> `renderdemo`. **Je ne le corrige pas** : `Noge/Systems` passe devant. Verifie
> moi-meme avant d'etre inscrit ici.

## 8.1 Le mecanisme, verifie a la lecture

`NkOpenglDevice.cpp:995-1026` — **17 requetes partagent UNE SEULE variable** :

```
GLint v = 0;                       // <- declaree UNE fois, jamais reinitialisee
glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v);   mCaps.maxTextureDim2D = (uint32)v;
...                                        (15 autres)
```

`glGetIntegerv` **n'ecrit rien quand il echoue**. Sous WebGL2, sept requetes
echouent (`INVALID_ENUM`) : SSBO (ES 3.1+), compute (**inexistant en WebGL2**),
anisotropie (extension). Chaque capacite ratee garde donc **la valeur de la
derniere requete reussie**.

⚠️ **Et ce n'est pas « chacune herite de sa voisine » : il y a une COULEE de
six.** `GL_MAX_UNIFORM_BLOCK_SIZE` est la derniere qui reussit, puis echouent a
la file :

| capacite | valeur qu'elle recoit reellement |
|---|---|
| `maxStorageBufferRange` | `MAX_UNIFORM_BLOCK_SIZE` |
| `maxComputeGroupSizeX/Y/Z` | `MAX_UNIFORM_BLOCK_SIZE` — **les trois identiques** |
| `maxComputeSharedMemory` | `MAX_UNIFORM_BLOCK_SIZE` |
| `maxSamplerAnisotropy` | `MAX_UNIFORM_BLOCK_SIZE` (~65536 : absurde, et credible) |
| `minStorageBufferAlign` | `UNIFORM_BUFFER_OFFSET_ALIGNMENT` |

> **Les capacites ne sont pas manquantes : elles sont fausses et plausibles.**
> Aucune n'est zero, toutes ont l'air renseignees. C'est la face n°11 de la
> grille — *l'instrument fabrique une valeur credible* — appliquee non pas a un
> affichage mais a **la table sur laquelle le moteur choisit ses chemins**.

📌 **Meme famille que la garde EGL corrigee au commit `e761b4c7`** : demander a
une cible ce qu'elle n'a pas. **Deux fois dans le meme fichier, a cinq cents
lignes d'ecart.** Le remede est de la meme forme — remettre `v` a zero avant
chaque requete et vider la file d'erreurs, ou mieux **ne pas poser la question**
quand la cible n'a pas la fonctionnalite.

⚠️ **Ce que ca vaudra pour le chantier qualite** : `ApplyQuality()` derive
aujourd'hui d'un enum, pas des capacites. Le jour ou un profil lira `mCaps` pour
decider, il lira ces valeurs-la. **A corriger avant ce jour-la, pas apres.**

## 8.2 ✅ TROIS CAUSES ELIMINEES POUR L'ECRAN NOIR DE RODOLF

Le journal s'arrete a `step 4: NkTextureLibrary::Init`. J'ai teste a peu de
frais les trois explications benignes les plus probables — **les trois tombent**,
ce qui resserre la recherche au lieu de l'elargir :

1. 🔴 **Ce n'est PAS un artefact de tampon.** Le correctif (h) du 2026-08-06 est
   **bien en place** : `NkConsoleSink.cpp:245-253`, `fflush` **apres chaque
   message** sous `NKENTSEU_PLATFORM_EMSCRIPTEN`, avec son commentaire. Donc le
   journal s'arrete vraiment la — *« ne depasse pas step 4 » est a prendre au
   mot.*
2. 🔴 **Ce n'est PAS l'anisotropie faussee du §8.1.** ✅ `maxSamplerAnisotropy`
   est **ecrit et lu nulle part** (une seule ecriture GL, une Vulkan, un defaut
   dans `NkIDevice.h`). La valeur absurde n'atteint aucune creation de sampler.
   *Les capacites fausses sont latentes, pas la cause.*
3. 🔴 **Ce n'est PAS `GL_CLAMP_TO_BORDER`.** Le correctif (g2), donne « rebuild a
   refaire » le 2026-08-06, **est present** : `ToGLWrap` l. 3128-3136 replie sur
   `GL_CLAMP_TO_EDGE` sous `NK_OPENGL_ES`, commentaire a l'appui.

**Ce qui reste, et c'est la que je regarderais** : `NkTextureLibrary::Init`
(`NkTextureLibrary.cpp:22`) se scinde en deux branches selon
`mResources->IsReady()`. La branche « prete » enveloppe les textures et samplers
de `NkResources` ; la branche de repli appelle `CreateSampler` puis quatre
`CreateTexture` 1x1. **Aucune des deux ne journalise entre step 4 et step 5** :
il n'y a donc aucun moyen de savoir laquelle est prise ni ou elle s'arrete.
*Avant de chercher la cause, il manque un instrument* — c'est le motif
« NKGui n'a aucune API d'introspection » sous une autre forme.
---

# MESURE 9 — LE SQUELETTE « EXACTEMENT COMME UNREAL » : fait, prouvé, et la question de placement

## 9.1 ✅ Le résultat central

`sizeof(NkSkeleton)` : **77 064 → 88 octets** (facteur ~875). `NkSkeletonDef` =
l'actif partagé (l'équivalent d'un `USkeleton`), référencé par `NkSharedPtr`,
jamais copié ; `NkBonePose` = la pose locale par instance (elle vivait DANS
`NkBone`, mêlée à la définition — c'est elle qui rendait le partage impossible) ;
pose et matrices en `NkVector` **au nombre réel d'os**. Le plafond
`kMaxBones=256` disparaît, et avec lui la troncature silencieuse de l'importeur.

**Consommateurs** : recensés, migrés, reconstruits, **exécutés** — Noge 39/39,
LocomotionDemo 9/0, AssetIODemo **55/0** (textures 3/3 comprises),
SystemsRevivalTest **34/0**, NavDemo et EditableMeshDemo reconstruits. **Nogee
(le plus gros consommateur applicatif) : 45/45 SUCCESS** — la reconstruction
des consommateurs est close.

⚠️ **Le recensement par nom de type a raté un consommateur** : `NkAssetIODemo`
atteint le type par un membre (`scene.skeletons[0].boneCount`) sans jamais
écrire son nom. C'est le compilateur qui l'a trouvé. *Un recensement de
consommateurs se fait par les CHAMPS autant que par le type.*

## 9.2 ✅ Le banc d'arbitrage de copie — et le trou que la contre-épreuve a trouvé

Le banc vérifie : taille ≤ 128 ; 4 os → structures de taille 4 ; la copie
**partage** la définition (identité de pointeur) et **duplique** le
par-instance ; et le **témoin historique** — quatre squelettes de 64 os **sur la
pile**, là où quatre exemplaires plantaient en `0xC00000FD`.

🔴 **La première mutation a trouvé un trou dans le BANC, pas dans le code** :
`FromDef` muté en copie profonde (l'anti-Unreal) laissait le banc **vert** — son
assertion ne couvrait que la copie de composant, pas la création. Assertion
ajoutée (deux instances tirées du même actif partagent **le même**
`NkSkeletonDef`), mutation rejouée : `33 OK / 1 FAIL`, exit 1, sur exactement
elle. *Un banc troué se découvre en essayant de le faire rougir.*

## 9.3 🔎 LE PAYSAGE DES SQUELETTES — mesuré sur l'indice du coordinateur

L'indice : « le nom apparaît aussi dans NKRenderer — deux vérités parallèles ? »
✅ Mesure faite, **tous les porteurs du mot dans le dépôt** :

| type | où | ce que c'est | verdict |
|---|---|---|---|
| `ecs::NkSkeleton`/`NkSkeletonDef` | Noge | **le** squelette complet | le sujet de cette mesure |
| `renderer::TagRSkeleton` | NKRenderer | struct **vide** de marquage du handle GPU (`NkSkeletonHandle`) | **homonyme**, pas une donnée |
| `anim::NkRetargetSkeleton` | **Kernel/Runtime/NKAnimation** | descripteur de **reciblage** : `parent`+`bindLocal`+`names`+`topo`, déjà dynamique | vraie donnée, **convention différente** |
| `physics::NkBoneDef` | NKPhysics/NkRagdoll.h | définition d'un **corps** de ragdoll | ⚠️ **collision de nom créée par MON changement** (`ecs::NkBoneDef`) |

**Donc : pas deux squelettes complets parallèles** — un seul (Noge), plus un
descripteur de reciblage spécialisé dans le noyau. Mais deux écarts à ne pas
laisser filer :
- `NkRetargetSkeleton` stocke la pose de repos **en LOCAL** (relative au
  parent) ; `ecs::NkBoneDef` stocke `bindPose`/`inverseBindPose` en matrices.
  **Deux conventions.** Les unifier de mon propre chef casserait l'animation
  partout à la fois — c'est précisément ce que le mandat interdit ;
- la collision `NkBoneDef` : espaces de noms distincts, ça compile, mais deux
  `NkBoneDef` désignant deux choses est le motif `NkShaderStage`.

## 9.4 ❓ LA QUESTION DE PLACEMENT — écrite pour Rodolf, pas tranchée

Rodolf : *« le squelette doit être utile […] à tout système qui gère les
animations squelettiques. »* Or `ecs::NkSkeletonDef` vit dans **Noge**, et les
modules du noyau (`NKAnimation` — le substrat extrait de NKRenderer le 14/08 —,
`NKAnimPhysics`) **ne peuvent pas dépendre du moteur**. ✅ Mesuré aussi : Noge ne
dépend aujourd'hui **ni** de `NKAnimation` **ni** de `NKAnimPhysics` — le
déplacement ajoute une dépendance, il ne déplace pas seulement un fichier.

La forme Unreal suggère la coupe : **la DÉFINITION descend** (l'actif, comme un
`USkeleton`), **le composant reste** dans l'ECS (`NK_COMPONENT(NkSkeleton)`,
mesuré). Mais trois choix restent à Rodolf, et je ne tranche aucun :
1. **quel étage** pour `NkSkeletonDef` — `NKAnimation` (où vit déjà le
   reciblage) ou `NKAnimPhysics` ?
2. **quelle convention** de pose de repos — matrices (l'existant Noge) ou
   locale (l'existant reciblage) ? L'une des deux devra se convertir ;
3. **le nom** — renommer `ecs::NkBoneDef` (le mien) ou `physics::NkBoneDef` pour
   dissoudre la collision.
---

---

# MESURE 11 — L'IMAGE WEB 3D EXISTE, ET LE TÉMOIN Draw:/Tris: SE DATE

## 11.1 ✅ WEB PASSE AU VERT — 4 cibles sur 7 — avec ses deux réserves écrites

La capture headless lancée ce matin par le coordinateur (SwiftShader, 1280×720,
`?demo=2`) a abouti après **~9 heures de rendu logiciel**. **Ouverte et lue par
lui puis par moi** : ligne `Demo 3D | API : OpenGL`, panneau `Shadow tweak`
complet (`VSM atlas 4096`, `quality 4`, `slots 14`, `framesInFlight 3`),
`[Phase H] Texture file-based : test_pattern.png LOAD OK`, sphères PBR, grille
de cubes instanciés, sol texturé à pois. Archivée :
**`Captures/plateforme_web_2026-09-02.png`** (344 797 o), à côté de ses sœurs
Windows/Linux du 29/07.

**Les deux réserves, comme pour Linux :**
1. **rendu LOGICIEL** — SwiftShader via Edge headless, pas le GPU réel. La
   confirmation GPU réel reste les 2 minutes de navigateur de Rodolf (bloc 1) ;
2. **binaire = l'arbre Debug du serveur 9001** (35 047 917 o) : **postérieur au
   correctif EGL** (sans lui, rien ne construit), **antérieur au correctif
   `PP_AutoExposure`**.

⚠️ **Deux écarts de scène, dits plutôt qu'arrondis** : les ombres portées sont
**plus ténues** que sur Windows/Linux, et les décalques de route (motif rose au
sol des trois autres captures) sont **absents** — cohérent avec les shaders
`Decal`/VFX « non déployés » déjà documentés (`shader id=0`, defaut
pré-existant). À revoir sur le GPU réel avant d'accuser quoi que ce soit.

## 11.2 📌 Le corollaire AutoExposure — une question fermée

L'image est **lumineuse et tone-mappée** alors que le shader `PP_AutoExposure`
était **encore cassé** dans ce binaire (`valid=0`). C'est la preuve directe que
le **repli d'exposition fonctionne** : la question « le noir venait-il de
l'auto-exposition ? » est fermée — **non**.

## 11.3 🔴 LE TÉMOIN Draw:/Tris: SE DATE — troisième lecture, la bonne

`Draw:1093 Tris:489586` sur le Web — alors que Windows affichait `Draw:0` à
142 FPS. Hypothèse du jour : « câblé selon le backend » (GL vs DX). ✅ **Réfutée
sans lancer le GPU, par le code et par git** :

- les deux captures étaient **le même backend** (`API : OpenGL`, HUD lus) ;
- les compteurs sont incrémentés dans **`NkICommandBuffer`, la classe de BASE**
  (`++mCbStats.drawCalls` aux méthodes `Draw*` de l'interface) — **tous
  backends, par construction** ;
- `git log -S` date leur branchement : **commit `7f3ada7b`, 5 août 2026** —
  *« NKRHI : les compteurs de rendu comptent enfin »*. Le commentaire du code le
  dit lui-même : *« ces compteurs étaient AFFICHÉS depuis toujours mais jamais
  alimentés — le cadran existait, l'aiguille n'avait jamais été posée »*.

> **Le discriminant n'est ni le backend ni le câblage : c'est LA DATE DU
> BINAIRE.** Captures du 29/07 (Windows, Linux, Android) : antérieures au 05/08
> → `Draw:0` ne voulait rien dire. Binaires d'aujourd'hui : les compteurs font
> foi, sur tous les backends.

📌 **Raffinement de la porte du témoin, 3ᵉ version et j'espère la dernière** :
*« jamais `Draw:`/`Tris:` » était lui-même trop large.* Un témoin peut être
**posé un jour donné** — il ne vaut rien sur les images d'avant et fait foi sur
celles d'après. **Avant de faire d'une valeur un critère : la lire sur un cas
réussi DU MÊME BINAIRE.** Le contrôle positif du témoin doit partager la date de
ce qu'il juge.

---

# MESURE 10 — CE QUE NOGE CONSOMME DU NOYAU : la table, les trois familles, et les greffons

> Mandat de Rodolf (02/09) : *« vérifie dans tout le système Nkentseu les
> bibliothèques qui doivent être consommées par Noge et fais que Noge les
> consomme »*, complété avant son départ par : *« ou alors définir un système de
> greffons »*. Méthode imposée : **mesure d'abord, câblage ensuite** — une
> dépendance déclarée que rien n'exerce est du chrome, et `NkRenderQuality` est
> né exactement comme ça.
>
> **Instruments** : `_DEPS` (Noge.jenga:109) et `_INCLUDE_DIRS` (l.60-97) pour
> « déclaré » ; `grep -rE '#include ["<]MODULE/'` sur `Engine/Noge/src` pour
> « exercé », avec vérification que l'en-tête inclus appartient à un module
> VIVANT (les 11 modules sans corps ne comptent pas comme exercice). Résultat
> contrôlé : tous les exercices faibles (1-2 includes) viennent d'en-têtes à
> corps (`NkNavigationSystem.cpp`, `NkAudioSystem.cpp`, `NkUISystem.cpp`,
> `NkLocomotion.cpp`, `NkApplication.h`…).

## 10.1 La table — couche Runtime (celle qui se joue)

Colonnes : **dû ?** (jugement moteur de jeu, raison en trois mots) · **déclaré**
(`_DEPS`) · **exercé** (includes réels, dont N fichiers `.cpp`) · **famille** ·
**liaison recommandée** (CŒUR = lié statiquement, toujours là ; GREFFON = chargé
dynamiquement, optionnel par jeu).

| module | dû ? | déclaré | exercé | famille | liaison |
|---|---|---|---|---|---|
| `NKECS` | oui — cœur du moteur | ✅ | ✅ 127 incl. (5 cpp) | consommé | CŒUR |
| `NKRenderer` | oui — rendu | ✅ | ✅ 24 (6 cpp) | consommé | CŒUR |
| `NKRHI` | oui — sous le rendu | ✅ | ✅ 10 | consommé | CŒUR |
| `NKCollision` | oui — physique | ✅ | ✅ 4 | consommé | CŒUR |
| `NKPhysics` | oui — physique | ✅ | ✅ 3 | consommé | CŒUR |
| `NKNavigation` | oui — IA de déplacement | ✅ | ✅ 1 (système vivant) | consommé | CŒUR |
| `NKAudio` | oui — son | ✅ | ✅ 2 (système vivant) | consommé | CŒUR |
| `NKEvent`/`NKWindow` | oui — boucle, entrées | ✅ | ✅ 4 / 6 | consommé | CŒUR |
| `NKFont`/`NKImage` | oui — texte, textures | ✅ | ✅ 2 / 2 | consommé | CŒUR |
| **`NKAnimation`** | **oui — NkAnima, la bibliothèque** | 🔴 **absent de `_DEPS`** | ✅ 1 (`NkLocomotion`, prouvé par démo 9/0) | **exercé-mais-non-déclaré** | CŒUR — **✅ déclaré ce jour** |
| **`NKCanvas`** | oui — UI en jeu | 🔴 **absent de `_DEPS`** | ✅ 1 (`NkUISystem.cpp`) | **exercé-mais-non-déclaré** | CŒUR — **✅ déclaré ce jour** |
| **`NKAnimPhysics`** | **oui — NkAnima, 2ᵉ moitié** (masse, équilibre, appuis) | include-dir seul (l.79) | 🔴 **0 include** | **déclaré-mais-inerte** | CŒUR (à exercer, pas à retirer : l'éditeur NkAnima le consomme déjà) |
| **`NKXR`** | **oui — VR/AR (nommé par Rodolf)** | 🔴 absent | 🔴 0 | **absent-et-dû** | **GREFFON** (§10.4) |
| **`NKCamera`** | **oui — AR (nommé par Rodolf)** | 🔴 absent | 🔴 0 | **absent-et-dû** | **GREFFON** (§10.4) |
| `NKNetwork` | oui — multijoueur | ✅ | ✅ 4 (`NkNetWorld`, 18/0) | consommé | **GREFFON candidat** (un jeu solo n'embarque pas le réseau) |
| `NKMedia` | oui — vidéo/codecs | ✅ (« transitif NKAudio, Opus » — documenté) | 🔴 0 direct | déclaré-transitif assumé | GREFFON candidat |
| `NKSL` | transitif (NKRHI) | ✅ | 🔴 0 direct (documenté l.75) | déclaré-transitif assumé | CŒUR (suit NKRHI) |
| `NKGui`/`NKUI` | **non** — UI d'éditeur, vit au-dessus | 🔴/incl.-dir | 0 | non-dû | — |
| `NKGraph` | pas encore — substrat du nodal futur (§10.5) | 🔴 | 0 | non-dû aujourd'hui | (greffon nodal, plus tard) |
| `NKSimulation` | **rien à consommer** : docs seuls, 0 source | 🔴 | 0 | vision | — |

**Couches Foundation/System** : toutes déclarées, toutes exercées (NKMath 35,
NKContainers 93, NKLogger 13, NKMemory 12, NKCore 7, NKSerialization 3,
NKFileSystem 7, NKTime/NKThreading 1 vivant chacun) — sauf **`NKPlatform` et
`NKGlad` : 0 include direct**, transitifs de compilation/lien (NKCore inclut
NKPlatform ; le GL vit dans RHI). Classés *déclarés-transitifs assumés* — à
documenter d'une demi-ligne dans `_DEPS`, pas à retirer. `NKReflection`/`NKStream` :
non déclarés, non dûs directement (transitifs de NKSerialization/NKImage).

**Couche AI** : `NKRL` et `NKAgent` déclarés ET exercés (`NkAgentComponent.h` →
`NkAgentSystem.cpp` vivant, démo 1/0). Le reste de la couche (14 modules) n'est
pas dû au moteur — consommé par les applications d'entraînement. **Couche
Bare** : vision, rien à consommer.

## 10.2 Les trois familles — et ce qui a été fait

- **Consommés** : 24 modules. Rien à faire.
- **Exercés-mais-non-déclarés** (la famille MIROIR, que la consigne ne
  prévoyait pas) : `NKAnimation`, `NKCanvas`. ✅ **Corrigé ce jour** — ajoutés à
  `_DEPS`, Noge reconstruit **41/41 SUCCESS** (41, pas 39 : les deux modules
  entrent dans la fermeture de build, ce qui est exactement ce que la
  déclaration devait faire).
- **Déclarés-mais-inertes** : après vérification, **un seul vrai** —
  `NKAnimPhysics` (include-dir sans le moindre include). Les autres suspects
  (`NKMedia`, `NKSL`, `NKPlatform`, `NKGlad`) portent leur raison transitive,
  écrite sur place pour trois d'entre eux. **Proposition, pas retrait** :
  exercer `NKAnimPhysics` quand Noge jouera l'équilibre/les appuis (la course
  n'en a pas besoin ; PV3DE si).
- **Absents-et-dûs** : `NKXR`, `NKCamera` → **conception greffon** (§10.4),
  décision explicite de ne PAS les câbler statiquement.

## 10.3 ✅ La machinerie de chargement dynamique — mesurée avant d'écrire

`LoadLibrary`/`dlopen` vivent dans **15 fichiers**, et **aucune abstraction
partagée n'existe** (`NkSharedLib`/`NkDynLib`/`NkPlugin` : zéro résultat, avec
contrôle positif). Le motif de la 8ᵉ copie de capture d'écran, version
chargement dynamique : NKAudio (×2), NKCanvas (×3), NKRHI (×2), NKWindow (×3),
NKXR (OpenXR loader), sondes, et **le seul structuré : `Noge/ECS/Scripting/
NkScriptBridge`** — copie fantôme, rechargement à chaud, **prouvé par
`NkHotReloadDemo` (16/0)**. C'est le germe du système de greffons, pas un
seizième site à écrire.

## 10.4 📐 LE SYSTÈME DE GREFFONS — conception posée, chiffrée, PAS lancée

**Un seul système pour la maison** (le moteur, le panneau Greffons de NkUIDesign
— écrans 22-25 différés —, `Extensions/` de NkCode), trois étages :

1. **`NkSharedLib`** (Foundation/NKPlatform, ~150-250 l.) — l'abstraction
   portable charge/résout/décharge qui manque aux 15 sites. Rentable même sans
   greffons : chaque nouveau site cesse de recopier le `#ifdef _WIN32`.
2. **La frontière** (~400-600 l.) — ⚠️ le piège central : **zéro-STL et ABI C++
   à travers une DLL ne passent pas**. Frontière **C pur** : le greffon exporte
   UNE fonction `NkGreffonDecrire()` renvoyant une **table de fonctions C**
   versionnée (`abiVersion` en tête, refus propre si écart) + capacités
   déclarées. Le modèle existe déjà dans la maison : `NkScriptBridge` fait
   exactement cela pour le C++ à chaud, et `NkHarmonyOnNapiInitExtra` montre le
   hook faible. Cycle de vie : découverte (dossier `greffons/` + manifeste),
   `Decrire` → `Init(services)` → tick optionnel → `Arret` — et **doublure
   crédible obligatoire** : un greffon absent = capacité annoncée `false`,
   jamais un trou (la règle gravée s'applique telle quelle).
3. **Les deux premiers greffons** : `greffon_xr` (NkXrSession derrière la
   table ; le **backend simulateur permet le banc sans casque, fenêtre
   `nullptr` acceptée — vérifié dans `NKIXrBackend.h:64-66` : « mode sans tête
   pour les tests numériques, pose scriptée »**) et `greffon_camera`
   (`NkCameraSystem` : `Init/StartStreaming/GetLastFrame` — 6 backends dont
   Noop ; ⚠️ pas de backend Windows natif listé, le banc Windows prouvera le
   cycle de vie et l'honnêteté de l'état, pas des images).

**Chiffrage** (ordre de grandeur, sur la base de NkScriptBridge = 376 l.
éprouvées) : étage 1 ≈ 1-2 jours ; étage 2 ≈ 3-5 jours avec son banc
(chargement, refus d'ABI, déchargement, doublure) ; chaque greffon ≈ 1-3 jours
selon la surface exposée. **Total premier lot : ~1,5 à 2 semaines.** Les trois
conditions de la maison s'appliquent à chaque étage (corps, appelant réel, banc
qui rougit débranché).

**Ce que ça évite, chiffré simplement** : XR + Camera + Network + Media câblés
en dur grossissent TOUS les binaires de TOUS les jeux — le `renderdemo.wasm`
fait déjà 27 Mo en Release ; un jeu de course n'a rien à faire d'OpenXR.

## 10.5 🪪 FICHE `NKGRAPH` — mesure, pas conception

**2 `.h` + 3 `.inl`, 0 `.cpp` — et le « 0 .cpp » ne dit RIEN ici** (leçon
`NkCGXDetect` appliquée) : les corps vivent dans les `.inl`, **1 519 lignes**
réelles. Contenu : `NkNodeGraph` (nœuds, sockets typés entrée/sortie, liens avec
erreurs nommées), `NkGraphHistory` (annuler/refaire), `NkGraphDocument` +
entrées/sorties. **Consommateurs : un seul** (`NKEditMeshHarness`), et **absent
de `Nkentseu.jenga`** (0 occurrence — il compile via includes seulement).
**Nature : substrat de graphe généraliste, sans sémantique métier** — c'est bien
un candidat pour porter le nodal « qui vient par-dessus », et la direction
script/blueprint consignée par Rodolf (`3fb5d3e`) le trouvera prêt. Rien de plus
n'est conclu : c'est une fiche d'identité.

## 10.6 🦴 LA FRONTIÈRE NkAnima — trois candidats, mesurés

Rodolf : *« NkAnima ne consomme pas Noge — c'est Noge qui le consomme. »* La
bibliothèque NkAnima, aujourd'hui, c'est **deux modules du noyau** plus ce qui
traîne côté éditeur :

| morceau | où | volume | consommé par |
|---|---|---|---|
| lecture de clip, mélange (`NkBlendTree1D`), reciblage | `NKAnimation` | 4 `.cpp`, **2 344 l.** | **13 fichiers** — NKRenderer, **Noge** (`NkLocomotion`, démo 9/0), NkAnimaEditor |
| masse, équilibre, appuis (`NkPoseMass`, `NkBalance`, `NkContactDetector`) | `NKAnimPhysics` | 6 `.cpp`, **1 272 l.** | 4 fichiers — NKRenderer (debug), NkAnimaEditor, un banc. **Noge : 0** |
| l'éditeur | `NkAnimaEditor` | 6 fichiers, **1 206 l.** | — (application) |

✅ **La frontière est SAINE** : « jouer un clip » et « mélanger deux poses »
sont déjà dans le substrat, et l'éditeur les consomme d'en haut (vérifié :
`AnimBridge.cpp` inclut `NKAnimation/` et `NKAnimPhysics/`, pas l'inverse).
**Aucun morceau de bibliothèque n'est piégé côté éditeur.** Le seul manque est
celui du §10.2 : Noge n'exerce pas encore `NKAnimPhysics`. Et la question du
bloc 6 (étage de `NkSkeletonDef`) se répond maintenant plus précisément :
**le candidat naturel est `NKAnimation`** — c'est là que vivent déjà le
reciblage et le mélange que le squelette sert.
