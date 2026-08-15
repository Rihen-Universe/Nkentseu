# Dette de lisibilité — découpage, repères, garde-fous

> Décidé avec Rihen le 12 août 2026, après un état des lieux mesuré du dépôt.
> **Ces chantiers se font ENTRE deux fonctionnalités, jamais pendant.**
> Ordre convenu : d'abord finir les **matériaux** et l'**éditeur**, ensuite ceci.

## Pourquoi ce document

Le dépôt compte ~2 300 fichiers source et près d'un million de lignes, pour un
moteur multi-backend et quatre applications. Le constat n'est **pas** que le
code serait obscur : les commentaires expliquent le *pourquoi* et gardent la
trace de ce qui a été essayé — c'est au-dessus de la moyenne de l'industrie, et
c'est ce qui permet de corriger un défaut sans en créer trois.

Le constat est que le code est **mal rangé**. La différence compte : mal
expliqué se soigne difficilement, mal rangé se soigne mécaniquement. Un
développeur compétent devrait être autonome en une semaine ; aujourd'hui il
passerait un mois à cartographier.

## La règle, valable pour TOUTES les applications

NK3DModeler, NKCode, NkForma, NkAnima, NkScena, Nogee, Sandbox : même exigence.

1. **Un fichier = une responsabilité qu'on peut nommer en une phrase.** Si la
   phrase contient « et », le fichier doit être coupé.
2. **Le nom dit le contenu.** Un fichier nommé « Demo » qui porte tout le
   viewport d'une application de production est un nom qui ment, et un nouveau
   venu ne le cherchera jamais là.
3. **Plafond indicatif : ~2 000 lignes.** Au-delà, on ne relit plus, on
   cherche. Ce n'est pas une règle absolue (un décodeur, une table de données
   embarquée peuvent dépasser) mais un signal.
4. **En-tête de fichier utile** : ce que le fichier contient, ce qu'il ne
   contient **pas**, et vers quoi renvoyer pour le reste. Deux lignes suffisent.

## Les chantiers

> Cinq au 12 août 2026. **Douze depuis le 15 août** : la confrontation des
> feuilles de route au code a ajouté le fork du lecteur PDF (n° 6, chantier de
> **code**) et l'inventaire de dette documentaire (n° 7). Le n° 1 a par ailleurs
> été **réglé pour moitié** entre-temps.

### 1. Découper et renommer `NkDemo3D.cpp` — **17 395 lignes** (relevé du 14/08)

Le premier obstacle, et le plus rentable. Ce fichier porte **tout** le viewport
du modeleur sous un nom qui annonce une démo. Le renommage seul supprime la
confusion ; le découpage par domaine (caméra/vue, matériaux hôte, édition de
maillage, gizmos, sortie/rendu, outils) rend chaque partie relisible.

Déjà noté comme dette ailleurs : à faire **après** la sauvegarde de scène
poussée, **jamais** pendant un chantier — un découpage au milieu d'une
fonctionnalité produit des conflits et des régressions muettes.

**Deux mises à jour du 2026-08-14, dont une bonne nouvelle :**

- Le chiffre a bougé : **16 653 → 17 395 lignes** en deux jours. Ce n'est pas une
  correction de mesure, c'est le fichier qui grossit pendant qu'on documente
  l'intention de le couper.
- ⚠️ **Le nom `NkModelerViewport` n'est plus disponible.** La refonte d'interface
  du 13/08 a créé `Shell/NkModelerViewport.h` (1 894 l.), qui est un **extrait de
  `NkModelerScreens.h`**, pas le renommage prévu ici. Le découpage de `NkDemo3D`
  devra donc choisir un autre nom — ou fusionner les deux intentions
  explicitement. Décider **avant** de commencer : découvrir la collision en cours
  de route est le meilleur moyen de produire deux fichiers au nom voisin et au
  contenu sans rapport.

**`NkModelerScreens.h` : chantier FAIT le 13/08 — ne pas le refaire.**
Le fichier est passé de **13 963 à 1 476 lignes** (commit `4eabf396`, « refonte
de l'interface — decoupage »), réparti sur 19 fichiers dans `Shell/`. Une dette
réglée qu'on croit ouverte coûte aussi cher qu'une dette ouverte qu'on croit
réglée : on y retourne, on ne trouve pas le monstre annoncé, on doute de la
mesure et on perd la confiance dans le document.

Le nouveau plus gros du dossier est **`NkModelerProperties.h`, 7 780 lignes** —
issu du même découpage. C'est lui, désormais, le candidat au traitement, avec
`NkCodeState.h` (**7 754**, chiffre inchangé et vérifié).

### 2. Refaire `ARCHITECTURE.md` — il existe (540 lignes) mais il MENT

Vérification faite le 12 août, l'écart avec le dépôt est frontal. Le Kernel a
**cinq familles** — `Foundation`, `Runtime`, `System`, `AI`, `Bare` — dont deux
que le document ignore complètement :

- `Kernel/AI/` — 15 modules (NKAgent, NKTensor, NKTrain, NKGen, NKRL, NKNN…) ;
- `Kernel/Bare/` — 14 modules (NKBoot, NKScheduler, NKDriver, NKInterrupt…).

Et dans `Runtime` :

| | |
|---|---|
| **Cités, inexistants** | NKScene, NKAnimation, NKBody, NKEmotion, NKFace, NKPatientRenderer, NKDiagnosticEngine, NKScript, NKSpeech |
| **Réels, jamais cités** | NKECS, NKSL, NKGui, NKCanvas, NKXR, NKMedia, NKGraph, NKCamera, NKCollision, NKNavigation, NKSimulation |

Le document décrit l'architecture **prévue** au départ (Noge, PV3DE, un ECS
nommé NKScene) ; le dépôt a évolué ailleurs. Un nouveau venu qui s'y fie
cherche des modules absents et passe à côté de ceux qui portent le travail.

À noter : `Kernel/AI/` et `Kernel/Bare/` ont **chacun leur propre
`ARCHITECTURE.md`**. La documentation par famille existe donc — c'est le
document racine qui n'a pas suivi. Il doit répondre en une page à « je veux
modifier X, où est-ce ? », et renvoyer aux documents de famille pour le détail.

### 3. Créer `SHADERS.md` — les pièges du dialecte NkSL

Trois paragraphes qui économisent des jours. Ils sont connus, ils ont coûté
cher, et ils ne sont écrits nulle part de façon centralisée :

- **Aucun gradient dans une boucle de lumières** (`dFdx`/`dFdy`, `texture()`
  implicite) : fxc refuse de dérouler (X3570 puis X3511), la création du shader
  échoue, le pipeline garde un blob périmé — et l'objet rend **noir** sans autre
  trace que le journal.
- **L'alpha de sortie est ce que la fusion utilise.** `pbr.frag` a longtemps
  fini par `vec4(color, 1.0)` : toute la file transparente était mélangée à
  100 % alors que curseur, routage et pipeline étaient corrects.
- **Les emplacements ont changé.** Disposition moderne : `MaterialUBO` en
  `set=2 binding=8`, textures `set=2 binding=3..7`, varyings
  `vWorldPos/vNormal/vUV/vColor` en location 0..3. Les anciens `.vk.glsl` visent
  des slots que le système de matériaux ne remplit pas — ils « existent » sans
  pouvoir rien rendre.

### 4. Vérifier les layouts UBO automatiquement

Les blocs partagés entre C++ et shaders doivent correspondre **au champ près**,
sans aucune vérification aujourd'hui. Le commentaire de `CameraUBO` l'admet :
« une erreur d'UN seul vec4 ici décale tout le bloc et corrompt le rendu ».

`ObjBlock` a déjà son `static_assert(sizeof(...) == 224)`. Étendre ce réflexe à
tous les blocs partagés transforme une corruption silencieuse en **erreur de
compilation**. C'est peu de travail pour beaucoup de sûreté.

### 5. Quelques tests sur les briques pures

Math, conteneurs, générateur NkSL : des fonctions déterministes, sans GPU, où un
test coûte cinq minutes et rattrape des régressions invisibles.

**Pas de tests sur le rendu** : la validation par capture d'écran reste la bonne
méthode là, parce que le défaut est visuel et qu'aucune assertion ne remplace un
œil sur une image.

### 6. ~~Supprimer le FORK du lecteur PDF~~ — ✅ **RÉGLÉ le 2026-08-14**

> Le fork est **supprimé** (commit `a52e99e4`, 5 211 lignes) et les quatre bancs
> PDF sont **re-racinés** vers le module NKMedia (`804abc23`). Vérifié le 14/08 :
> `Applications/NKCode/src/NKCode/Pdf/` n existe plus. Le texte ci-dessous est
> conservé parce qu il documente le COÛT du défaut et la façon dont il a été
> trouvé — pas parce qu il reste à faire.

> Ajouté le **2026-08-14**, après confrontation des feuilles de route au code.
> ⚠️ **Chantier de CODE, pas de documentation.** Rien n'a été supprimé ni déplacé
> en le rédigeant. À arbitrer séparément.

Le lecteur PDF existe **deux fois**, sous les mêmes noms de fichiers :

| | `Applications/NKCode/src/NKCode/Pdf/` | `Kernel/Runtime/NKMedia/src/NKMedia/Pdf/` |
|---|---|---|
| espace de noms | `nkentseu::nkcode::pdf` | `nkentseu::media::pdf` |
| lignes | **5 211** | **10 990** |
| créé | 2026-07-31 | 2026-08-10 |
| dernière modification | 2026-07-31 | 2026-08-13 |

**Ce n'est pas une divergence lente, c'est un fork littéral** : `NkPdfRaster.cpp`
fait **588 lignes des deux côtés**, `NkPdfShading.cpp` **358 des deux côtés**. La
copie du Kernel est partie de l'autre, puis a évolué seule pendant deux semaines.

**Trois faits établis, à traiter dans cet ordre :**

1. **La copie NKCode est morte, et pourtant compilée.** Aucun fichier hors de
   `src/NKCode/Pdf/` n'inclut `"NKCode/Pdf/…"` — les consommateurs réels
   (`Shell/NkPdfViewer.h:18-19`, `Shell/NkPdfWorker.h:19-20`) incluent
   `"NKMedia/Pdf/NkPdf.h"`. Mais la cible NKCode déclare `files(["src/**.cpp"])`
   avec `location(".")` : les 5 211 lignes entrent dans le binaire sans que
   personne les appelle.
2. **Quatre cibles de banc déclarent des chemins qui n'existent pas.**
   `NKCode.jenga` l. 393, 428, 454 et 481 (`NkPdfProbe`, `NkPdfRasterTest`,
   `NkPdfRenderProbe`, `NkFileWorkerTest`) listent `src/NKMedia/Pdf/**.cpp` sous
   `location(".")` = `Applications/NKCode/`. Or `Applications/NKCode/src/` ne
   contient que `NKCode/`. Les chemins ont été réécrits lors du déménagement du
   code **sans être re-racinés**, et aucune de ces cibles ne déclare `NKMedia`
   dans ses dépendances.
   ⚠️ **« Chemins inexistants » est ce qui a été constaté ; « bancs cassés » ne
   l'a pas été** — jenga n'a pas été lancé. Savoir si ces cibles échouent ou
   globent à vide en silence demande un build, et c'est la première chose à
   faire en ouvrant ce chantier.
3. **La cause est documentaire, et elle est connue.** `Applications/NKCode/ROADMAP.md`
   a porté jusqu'au 14/08 la ligne « Afficheur PDF ⬜ — Rien n'existe
   aujourd'hui — vérifié ». Elle était juste le 30 juillet ; le lecteur a été
   écrit le 31. **Quelqu'un a lu « rien n'existe » et a reconstruit ce qui
   existait.** C'est la divergence la plus chère trouvée dans le dépôt, et la
   seule dont on puisse nommer le coût : un fork de 5 211 lignes.

### 7. Dette documentaire — inventaire, à ne PAS corriger au fil de l'eau

> Ajouté le **2026-08-14**. Ces points sont **relevés, pas corrigés** :
> individuellement chacun coûte cinq minutes, ensemble ils coûtent trois jours et
> noient les corrections qui comptent. Ils vivent ici pour être traités **en un
> passage**, quand un passage sera décidé.

**7.a — Citations de fichiers fausses.** Le format ROADMAP impose de « citer les
fichiers réels ». Relevé le 14/08 :

| Document | Cité | Réel |
|---|---|---|
| `NKRenderer/ROADMAP.md` l. 119 | `PP_FXAA/NkSL/pp_fxaa.nksl` | `Shaders/PP_FXAA/pp_fxaa.{vert,frag}.nksl` (pas de sous-dossier `NkSL/`) |
| `NKRenderer/ROADMAP.md` l. 122 | `Skin/NkSL/skin.vert.nksl` | `Shaders/Skin/NkSL/skin.nksl` (source unique, pas de `.vert`) |
| `NKRenderer/ROADMAP.md` l. 480, 665 | `pbr.frag.nksl` | `Shaders/PBR/NkSL/pbr.nksl` |
| `NK3DModeler/ROADMAP.md` | `NkModelerCore.cpp`, `NkModelerHost.h` | **n'existent nulle part** |
| `NKCode/ROADMAP.md` | `NkRootPicker.h` | **n'existe nulle part** |
| `NKECS/ROADMAP.md` | `NkScheduler.cpp` | **n'existe pas** (`NkScheduler.h` seul) |

**7.b — Fichiers « copy » committés.** `NKRHI/NkSWShaderBridge copy.h` (508 l.),
`NKRHI/src/NKRHI/Opengl/NkOpengl{Device,CommandBuffer} copy.hpp`,
`NKUI/src/NKUI/Tools/Gizmo/NkUIGizmo copy.{h,hpp}`, `Applications/Pong copy/`,
`Applications/Pong/Apps copy.cpp`, une dizaine dans `Sandbox/`. Du bruit — mais
du bruit qui compte comme du code dans toute mesure du dépôt, et qu'un moteur de
recherche remonte à égalité avec l'original.

**7.c — `NkFontRasterizer.cpp` (1 212 l.) n'a pas d'en-tête à son nom**, et aucun
symbole `NkFontRasterizer` n'est déclaré dans les en-têtes de NKFont. Le fichier
le plus lourd du rendu de glyphes est introuvable par son nom.

**7.d — Silos identifiés, avec le verdict rendu sur chacun.** Les trois sont
**signalés, pas à corriger** :

- **`NkSWPixel.h` en double** — `NKCanvas/Backend/Software/` (307 l.) et
  `NKRHI/Software/` (335 l.), **~90 % identiques** (60 lignes de différence après
  normalisation des espaces).
  **Verdict : la décision d'architecture est SAINE, c'est son PRIX qui n'était
  écrit nulle part.** La règle d'exclusivité NKCanvas/NKRenderer (CLAUDE.md)
  impose que NKCanvas possède ses propres backends et ne soit pas client de
  NKRHI ; deux devices ne peuvent pas présenter dans la même fenêtre. La
  conséquence mécanique est que la couche de format de pixel du rastériseur
  logiciel s'entretient **deux fois**. Ne pas remettre la décision en cause :
  savoir qu'une correction de l'un doit être portée dans l'autre suffit, et
  c'est précisément ce que personne ne pouvait savoir.
- **Trois sélecteurs de dossier** — `NKEditorKit/NkFilePicker.h` (892 l., annoncé
  « cœur RÉUTILISABLE, INDÉPENDANT de toute application »),
  `NKEditorKit/NkDirBrowser.h` (126 l., annoncé « partageable par d'autres
  éditeurs ») et `NK3DModeler/Shell/NkModelerFileDialog.h` (355 l., créé le
  12/08, annoncé « générique dès le départ, ça va servir ailleurs »). Seul
  `NKCode/Shell/NkOpenWs.h` réutilise vraiment, en dérivant de `NkDirBrowser`.
  **Verdict : le décompte n'est pas ce qui compte — c'est que la règle existait
  déjà et n'a pas tenu.** `NKCode/ROADMAP.md` avait constaté la duplication en
  juillet (« DEUX sélecteurs de dossier ») et posé la règle « ne plus
  réimplémenter — réutiliser/consolider ». Un mois plus tard un troisième est
  apparu, **avec la même justification que les deux premiers**. Une règle écrite
  dans la roadmap d'une application ne protège pas les autres applications : tant
  qu'elle n'est pas dans le document que tout le monde lit, elle ne s'applique à
  personne.
- **Deux rastériseurs de chemin anti-aliasés** —
  `NKFont/Core/NkFontRasterizer.cpp` (1 212 l., scanline à aire exacte de
  trapèzes) et `NKMedia/Pdf/NkPdfRaster.cpp` (588 l., sur-échantillonnage
  `kSub = 16`). **Algorithmes différents, pas un copier-coller.**
  **Verdict : ne pas consolider.** `NkPdfRaster.h` l. 4-9 justifie son existence
  dans son propre en-tête — un seul rastériseur pour les formes *et* le texte du
  PDF, alimenté par les contours de NKFont. Signaler, ne pas agir.

**7.e — Six modules ont du code et aucune `ROADMAP.md`.** Voir la liste et le cas
`NKSL` dans le `CLAUDE.md` (§ 2). Écrire ces six feuilles de route demande une
enquête par module : ce n'est pas de la dette de rangement, c'est du travail
d'inventaire, et il ne doit pas être fait à la va-vite — une roadmap inventée
est pire que pas de roadmap.

### 8. π n'existe nulle part dans Foundation — trois macros locales, deux précisions

> Relevé le **2026-08-14** pendant l'extraction de NKAnimation. **Nommé, pas
> traité.** Ne casse rien aujourd'hui.

`NKMath` expose `NK_PI_F` et `NK_PI_D` (via `constants::kPiF` / `kPi`,
`NkFunctions.h:301-304`). Mais **`NK_PI` tout court n'y est pas**. Trois fichiers
se le sont donc redéfini chacun de leur côté, en macro locale :

| Fichier | Valeur |
|---|---|
| `NKRenderer/Mesh/NkMeshSystem.cpp:17` | `3.14159265358979323846f` |
| `NKRenderer/Tools/Render2D/NkRender2D.cpp:9` | `3.14159265358979f` |
| `NKAnimation/NkAnimation.cpp` (repris de l'original) | `3.14159265358979f` |
| `Applications/NKDiffusionTest/main.cpp:47` | `3.14159265358979323846f` (const, pas macro) |

**Deux précisions différentes pour la même constante**, dans le même moteur.
L'écart est de l'ordre de **1e-14** en double, mais ces macros sont en `float` :
après quelques opérations trigonométriques cumulées, l'écart observable est de
l'ordre de **1e-7**.

**Pourquoi c'est une dette et pas un bug** : rien ne casse. Chaque fichier est
cohérent avec lui-même. Le défaut se révélera le jour où deux résultats calculés
dans deux fichiers différents seront comparés — une non-régression qui échoue de
1e-7, six mois plus tard, sans que personne ne pense à π.

**Remède, quand on y viendra** : un seul `NK_PI` dans `NKMath/NkFunctions.h`, à la
précision de `constants::kPiF`, et les trois macros locales supprimées. Ce n'est
pas urgent ; c'est juste à faire **avant** d'écrire un test qui compare des
trajectoires calculées dans deux modules.

⚠️ **La découverte compte autant que la dette** : ce `#define` a été perdu lors de
la coupe du 14/08 et **rattrapé par le build en une compilation**. Sans lui, le
code ne compilait pas — donc personne n'a jamais utilisé un `NK_PI` valant autre
chose que ce qu'il croyait. La chance a tenu à ce que le symbole soit absent
plutôt que faux.

### 9. `DemoRW/main.cpp` — corrigé à l'aveugle, jamais exercé

> Relevé le **2026-08-14**. **Catégorie 3 : modifié, jamais compilé.**

`Applications/DemoRW/src/DemoRW/main.cpp` inclut le substrat d'animation et a été
recâblé pendant l'extraction (`NKAnimation/NkAnimation.h`, types qualifiés
`anim::`). Mais **`DemoRW` n'est déclaré dans aucune cible du workspace** : aucun
`jenga build` ne peut le valider, aujourd'hui ni demain.

La correction est mécanique et symétrique de dix autres qui, elles, ont été
vérifiées par compilation. Mais elle repose sur la lecture seule — et cette
session a montré trois fois que la lecture manque ce que le build trouve.

**Ce n'est pas à corriger, c'est à SOLDER** : la dette disparaît au premier build
complet vert, qui compilera ce fichier ou prouvera qu'il est mort. Elle est donc
liée au chantier 10 ci-dessous.

### 10. Combien de fichiers ne sont couverts par aucune cible ?

> La question posée par trois découvertes du même jour, le **2026-08-14**.

Trois cas rencontrés en une journée, tous découverts par accident :

1. **les quatre bancs PDF de `NKCode.jenga`** — chemins déclarés vers un
   répertoire inexistant ; personne ne s'en était aperçu (corrigé depuis,
   commit `804abc23`) ;
2. **`DemoRW`** — code source sans cible (ci-dessus) ;
3. **`Sandbox/DemoNkentseu/Base03/NkRHIDemoText.cpp`** — utilise `NK_LOAD_KERNING`
   et `NkFontResult`, symboles qui **n'existent plus** dans l'API NKFont. **Bloque
   le build complet du workspace à 68/203**, sur `main` pur comme sur toute
   branche. Vérifié le 14/08 sur les deux.

Aucun des trois n'a été trouvé par un outil : deux par un build complet lancé
pour autre chose, un par un `grep` d'inventaire.

**Le chantier n'est pas de les corriger un par un**, c'est de **rendre le build
complet vert** — parce qu'un build complet qui échoue depuis assez longtemps
cesse d'être lancé, et tout ce qui se casse ensuite devient invisible. C'est
exactement ce qui s'est produit ici.

Premier pas concret et borné : réparer ou retirer `NkRHIDemoText.cpp`, obtenir un
**203/203**, et seulement ensuite compter ce qui reste hors couverture.

---

### 11. Rouvrir le BUILD COMPLET — bloqueur par bloqueur, chiffre à chaque pas

> Ouvert le **2026-08-15**. Un bloqueur levé, le suivant diagnostiqué non traité.
> **Le livrable est un nombre, pas un build vert** : il dit si la suite est une
> demi-journée ou une semaine.

| Relevé | Cibles atteintes | Reste | Bloqueur |
|---|---|---|---|
| 0 — état trouvé | **60 / 205** | 145 | `NkRHIDemoText.cpp` — API NKFont disparue |
| 1 — après levée | **79 / 203** | 124 | `Texture2D.cpp` + `ViewerApp.cpp` — API NKImage changée |

> ⚠️ **Chiffres corrigés le 15/08 : le compteur de Jenga surestime.**
> `Reporter.py:1038-1042` incrémente `_projects_built` **même en échec** (mesuré
> par l agent Jenga sur un banc dédié). Les footers lus disaient 61/205 et 80/203 ;
> comme le mode par défaut s arrête au premier échec (`Builder.py:2508-2510`), il y
> a **exactement un échec** par mesure — mes deux sorties portaient `Failed: 1`.
> D où **−1 sur chaque absolu**. **La progression, elle, est inchangée : +19.**
> Provenance : mesuré le 15/08 sur `feat/nkanimation` après fusion de `main`.
> ⚠️ Ce raisonnement ne tient QUE tant qu il y a un seul échec : sous `--verbose`
> ou avec le futur `--keep-going`, la surestimation vaut le nombre d échecs et le
> compteur doit être réparé avant de servir de mesure.

Le total passe de 205 à 203 : deux cibles désactivées, avec leur raison écrite.

**Ce que les deux premiers bloqueurs ont en commun, et c'est le vrai sujet.**
Aucun n'est un bug. Les deux sont des **API qui ont changé sans que leurs
consommateurs suivent** — et personne ne l'a su parce que la seule mesure capable
de le dire ne tournait plus. Un blocage qui dure assez longtemps fait cesser de
lancer la mesure, et tout ce qui casse ensuite devient invisible. Preuve trouvée
en chemin : deux des quatre cibles du bloqueur 1 étaient **déjà commentées, sans
un mot de raison**. Le même défaut avait été rencontré, neutralisé en silence, et
oublié.

#### Bloqueur 1 — ✅ levé (commit `251f49d2`)
Quatre démos de texte consomment une génération d'API NKFont entière et disparue
(`NkFontLibrary`, `NkTextShaper`, `NkFontResult`, `nk_handle`, `NK_LOAD_*`).
Désactivées avec leur raison. **À arbitrer** : porter vers
`NkFontAtlas`/`NkRasterizer`/`NkShape`, ou supprimer — `NkFontDemo` et `NkFDV2`
couvrent déjà NKFont sur la nouvelle API. Sept fichiers de plus utilisent la même
API morte **sans appartenir à aucune cible**.

#### Bloqueur 2 — ⏳ diagnostiqué, NON traité
`Applications/NkImageDemo` (cible `NkImageDemo`) : `Texture2D.cpp:77` et
`ViewerApp.cpp:450` appellent `NkImage::Load(path, 4)` **en statique**.

Ce n'est pas un correctif de deux lignes, et c'est pour ça qu'il est laissé :

| Avant (ce qu'appelle la démo) | Aujourd'hui (`NkImage.h`) |
|---|---|
| `static NkImage* Load(path, ch)` | `bool Load(path, ch)` — **membre**, l. 262 |
| `img->IsValid()` | **n'existe plus** |
| `Free()` libère pixels **et** wrapper (`nkMalloc` + placement new), « JAMAIS `delete img` » | `Free()` existe encore (l. 775), mais la classe a désormais un **vrai destructeur** (l. 199) et un move-ctor |

Le modèle de **propriété mémoire a changé**. La démo fait circuler des `NkImage*`
entre son fil de décodage et son fil d'upload GL ; reprendre ça à la légère est
la recette exacte du `c0000374` que `CONVENTIONS_CODE`/`CLAUDE.md` interdisent
(mélange allocateur custom et heap CRT).

**Motif de référence, qui marche déjà** — `Nogee/Editor/AssetManager.cpp:91` :
```cpp
NkImage img;                       // valeur, pas pointeur
if (!img.Load(absPath.CStr(), 4) || !img.Pixels() || img.Width() <= 0) { ... }
```
Le port consiste probablement à passer la démo du pointeur à la **valeur** (ou à
un `NkUniquePtr`), pas à réanimer l'ancien contrat. Décider **qui possède
l'image** entre les deux fils est la vraie question, et elle mérite d'être posée
avant d'écrire.

> #### ⚠️ CORRECTION DU 2026-08-15 — mon diagnostic ci-dessus était à moitié faux
>
> J'avais écrit « le modèle de propriété mémoire a changé » et « la démo fait
> circuler des `NkImage*` entre son fil de décodage et son fil d'upload GL ».
> **Les deux affirmations sont réfutées par la mesure.**
>
> **1. Il n'y a aucun fil.** Zéro `NkThread`, zéro `std::thread`, zéro mutex,
> zéro file dans tout `Applications/NkImageDemo/src`. L'asynchronisme n'est
> qu'une **intention écrite dans un commentaire** (`Texture2D.h:29-30` : « Pour un
> chargement asynchrone, utiliser DecodeFromFile() depuis un thread worker »).
> L'usage réel est séquentiel : `Texture2D.cpp:122-125`, `DecodeFromFile` puis
> `UploadFromImage` dans la même fonction. **J'avais diagnostiqué d'après un
> commentaire, pas d'après le code.**
>
> **2. Le contrat de propriété est INTACT.** `NkImage::Free()` fait toujours
> `nkFree(mPixels)` **puis `nkFree(this)`** (`NkImage.cpp:1468-1473`) : il libère
> bien les pixels **et** le wrapper, et `delete` reste interdit. Rien n'a changé
> de ce côté. Il existe même désormais `Unload()` (l. 1479), variante sûre sur la
> pile.
>
> **Ce qui a réellement disparu se réduit à deux choses :**
> - `static NkImage *Load(path, ch)` → il n'y a plus que `bool Load(path, ch)`
>   **membre** (`NkImage.h:262`) ;
> - `IsValid()` → remplacer par `Pixels() != nullptr` (motif en production dans
>   `Nogee/Editor/AssetManager.cpp:91`).
>
> **Le port est donc petit**, et il ne demande aucune décision d'architecture :
> allouer comme NKImage le fait en interne (`nkMalloc(sizeof(NkImage))` +
> placement new, `NkImage.cpp:1331-1334`), appeler le `Load` membre, garder
> `Free()`. Deux sites : `Texture2D.cpp:77` et `ViewerApp.cpp:450`.
>
> ⚠️ **MAIS il révèle une asymétrie d'API dans NKImage, et c'est ça le vrai
> sujet** : on peut toujours `Free()` un `NkImage` du tas, mais **il n'existe plus
> aucune fabrique publique « chemin de fichier → `NkImage*` du tas »**. Les
> fabriques statiques restantes (`Create`, `Alloc`, `Wrap`, `Dispatch`,
> `ConvertToTexture`) partent de dimensions ou d'octets, jamais d'un chemin.
> Chaque consommateur qui veut un `NkImage` du tas depuis un fichier doit donc
> **recopier l'idiome d'allocation interne de NKImage** — exactement le genre de
> duplication que ce dépôt paie ailleurs.
>
> **Deux issues, et c'est un arbitrage technique, pas une évidence :**
> 1. **Porter les deux sites** en recopiant l'idiome. Zéro risque, mais installe
>    la duplication dans une démo.
> 2. **Rendre à NKImage sa fabrique** — une static `NkImage *LoadFile(path, ch)`
>    de trois lignes, qui rétablit la symétrie avec `Free()` et sert tout
>    consommateur futur. Touche un module Kernel pour réparer une démo, donc à
>    décider, pas à faire d'autorité.
>
> Je penche pour la **2** — l'asymétrie est la cause, les deux sites cassés n'en
> sont que le symptôme — mais je ne modifie pas un module Kernel sur ma seule
> lecture.

> #### 🌉 AU MOMENT DE PASSER À JENGA v2.3.0 — MESURER DANS LES DEUX RÉGIMES
>
> ✅ **L'ATTENTE EST LEVÉE — vérifié le 2026-08-15.** La PR #24 est **fusionnée
> sur `main`** : `git log --oneline -5 main` dans `D:/Projets/MacShared/Projets/Jenga`
> donne **`a6578d1` « feat(build): --keep-going, cibles de test = racines, et un
> compteur qui ne ment plus (#24) »**. Mesuré, pas relayé — la note antérieure
> « pas encore fusionnée » est périmée. **Le chantier 11 peut reprendre**, et le
> pont ci-dessous est à faire **à la première exécution**, pas plus tard.
>
> La v2.3.0 sort les racines de test du build par défaut : **272 → 206 cibles**.
> Le dénominateur change donc, et **les relevés 0 et 1 ci-dessus (60/205 et
> 79/203) cessent d'être comparables aux suivants**. Une série longitudinale se
> détruit en silence exactement comme ça — les nombres continuent de sortir, plus
> rien ne dit qu'ils ne parlent plus de la même chose.
>
> **Le pont coûte une exécution, et il faut la faire AU MOMENT de la bascule** —
> après, la version précédente n'est plus là pour la produire :
>
> ```
> jenga build --config Debug --tests     # ancien régime : dénominateur 272/205
> jenga build --config Debug             # nouveau régime : dénominateur 206
> ```
>
> Les deux chiffres du même arbre, le même jour, donnent le **point de raccord**.
> Sans lui, personne ne pourra jamais dire si le dépôt s'est amélioré entre août
> et septembre : on saura seulement qu'on a changé de règle.
>
> ⚠️ Et noter les deux avec leur provenance — date, branche, régime. Un
> dénominateur est une provenance au même titre qu'une date : deux mesures justes
> sous deux régimes différents se contredisent sans que personne ait tort.
> *(Suggestion de l'échange, R7 du 15/08.)*

#### Comment reprendre
```
jenga build --config Debug 2>&1 | grep -E "Compilation Error|Projects Built|Status"
```
Un bloqueur, un commit, un relevé. **L'objectif n'est pas 203/203 : c'est zéro
échec inexpliqué.** Une cible qui ne peut pas construire ici et dont on sait
pourquoi est réglée.

⚠️ **Ce chantier solde la dette n° 9** (`DemoRW/main.cpp` corrigé à l'aveugle) :
elle disparaît au premier build complet qui compile ce fichier — ou qui prouve
qu'il est mort.

---

### 12. `NkImage` — deux modèles de vie sur un seul type : recenser avant de trancher

> Ouvert le **2026-08-15**, sur **arbitrage de Rodolf**. **Chantier à part**, à ne
> pas confondre avec le port de `NkImageDemo` (chantier 11, bloqueur 2), qui peut
> se faire indépendamment.
>
> **ÉTAT au 2026-08-15 : étapes 1 et 2 FAITES** (recensement + voie unique
> proposée, avec son argument de robustesse). **L'étape 3 — la migration — n'est
> PAS engagée** et ne doit pas l'être avant le chantier 11 : elle est mécanique
> mais invérifiable tant que le build complet ne tourne pas. **En attente de
> l'arbitrage de Rodolf** sur la voie proposée.

#### Le fait, vérifié à la ligne

`NkImage.cpp:1468-1473` — relu, c'est bien ce que tout le raisonnement suppose :

```cpp
void NkImage::Free() noexcept {
    if (mOwning && mPixels) nkFree(mPixels);
    mPixels = nullptr;
    nkFree(this);              // libère le struct NkImage lui-même
}
```

**Aucun garde** — ni `mOnHeap`, ni `mOwnsSelf` : rien ne distingue une instance du
tas d'une instance valeur. Or la classe a désormais un **vrai destructeur**
(l. 199) et un **move-ctor** (l. 209).

**Les deux modèles coexistent donc sur le même type :**

| Voie | Création | Destruction |
|---|---|---|
| **tas** | fabriques statiques (`Create`, `Alloc`, `Wrap`, `Dispatch`, `ConvertToTexture`) | `Free()` — libère pixels **et** `this` |
| **valeur** | `NkImage img;` | destructeur, ou `Unload()` (l. 1479) |

`Free()` appelé sur une instance valeur libère **une adresse qui n'est pas du
tas** ; si l'objet survit à l'appel, son destructeur passe ensuite sur de la
mémoire libérée. C'est le `c0000374` que `CLAUDE.md` interdit.

⚠️ **Le piège est déjà documenté — et c'est ce qui le rend pire.** Juste au-dessus,
l. 1460-1462 : « USAGE : uniquement sur les images créées via les fabriques
statiques. Ne JAMAIS appeler `Free()` sur une image allouée sur la stack. »
**Contrat par commentaire** : la règle est écrite, rien ne l'applique. Même
famille que le « thread worker » de `Texture2D.h:29-30`, qui a fait diagnostiquer
deux fils inexistants le même jour.

Asymétrie qui a révélé le tout : on peut `Free()` un `NkImage` du tas, mais
**aucune fabrique publique ne sait plus en créer un depuis un chemin de fichier**
(`static Load(path)` a disparu ; les autres partent de dimensions ou d'octets).

#### La méthode imposée par Rodolf — trois étapes, dans cet ordre

> « Si plusieurs modèles posent problème, propose une **voie unique**. Mais
> vérifie que la sélection retenue est **la plus robuste**, et **avant de retirer
> l'autre, vérifie qu'il n'est pas utilisé par d'autres systèmes**. »

**1. Recenser les consommateurs des DEUX voies — avant toute proposition.**
Qui appelle `Free()`, qui s'appuie sur le destructeur, qui fait circuler des
`NkImage*`, qui passe des valeurs. Dans **tout** le dépôt : NKRenderer, NKCanvas,
NKMedia, Noge, applications, démos. **Un chiffre par voie.** C'est l'étape qui
décide — une voie utilisée par trente sites ne se retire pas comme une voie
utilisée par deux.

**2. Proposer la voie unique, avec l'argument de robustesse.** Pas « celle que je
préfère » : **celle qui rend l'erreur impossible plutôt que détectable**. Critère
concret ici — quelle voie fait qu'un appel erroné **ne compile pas**, au lieu de
libérer une adresse qui n'est pas du tas. Rodolf demande la justification, pas
seulement le choix.

**3. Ne retirer qu'après.** Si le recensement montre que l'autre voie est
largement utilisée, **la migration est un chantier, pas un correctif** — le dire
au lieu de la forcer.

#### ÉTAPE 1 — RECENSEMENT : ✅ **EXÉCUTÉ le 2026-08-15**

> **Provenance de tous les chiffres ci-dessous** : mesurés le **2026-08-15** dans
> le worktree `Nkentseu-nkanim`, branche `feat/nkanimation`, au commit
> **`5f79e9b4`**. **Périmètre** : tout le worktree, fichiers `.h .hpp .cpp .c
> .inl`. **Exclusions déclarées** : `./Build/` (artefacts) et `./.git/`.
> `./Externals/` **n'est pas exclu** — il contient **0** occurrence de `NkImage`,
> mesuré séparément. **4 537 fichiers** parcourus.

**Un chiffre par voie, comme Rodolf l'a demandé :**

| Voie | Mesure | Commande (re-jouable, depuis la racine du worktree) |
|---|---|---|
| **tas** | **66** fichiers portent un `NkImage *` | `grep -rlE "NkImage[[:space:]]*\*" --include=*.h --include=*.hpp --include=*.cpp --include=*.c --include=*.inl . \| grep -vc "^\./Build/"` |
| **tas** | **120** appels `Free()` sur un `NkImage` | résolution du type du receveur (grep seul en trouve 168 toutes classes confondues — voir la note de méthode) |
| **tas** | **56** appels de fabrique statique | `grep -rhoE "NkImage[[:space:]]*::[[:space:]]*(Create\|Alloc\|Wrap\|Dispatch\|ConvertToTexture)[[:space:]]*\(" … \| wc -l` |
| **valeur** | **29** fichiers déclarent une instance valeur, dont **19 en valeur pure** (aucun pointeur, aucun `Free()`) | résolution des déclarations |
| **valeur** | **40** fichiers prennent `NkImage &` / `const NkImage &` | `grep -rlE "NkImage[[:space:]]*&" … \| grep -vc "^\./Build/"` |
| **valeur** | **4** appels `Unload()` sur un `NkImage` (7 au total, 3 sur d'autres classes) | `grep -rnE "(->\|\.)[[:space:]]*Unload[[:space:]]*\(\)" …` |

⚠️ **Note de méthode, parce que le chiffre le plus important est le moins
grep-able.** `Free()` est un nom **partagé** : `grep` en trouve 168 avec receveur
dans tout le dépôt, dont la majorité appartient à d'autres classes
(`NkFramebuffer::Free()`, etc.). Les **120** ci-dessus sont ceux dont le receveur
a été **résolu comme `NkImage *`** — variables locales, membres déclarés dans
l'en-tête jumeau, champs de struct. **2** appels supplémentaires portent sur
`NkSVGImage` (voir plus bas) et ne comptent pas ici. Un `grep -c "Free()"` seul
sur ce chantier donne un nombre **faux** ; c'est noté pour la session qui reprendra.

**Répartition par zone** (les 120 appels `Free()`) :

| Zone | `Free()` | fichiers `NkImage *` | fichiers valeur |
|---|---|---|---|
| `App/Sandbox` | 35 | 9 | 4 |
| `Kernel/NKImage` | 35 | 27 | 3 |
| `Kernel/NKRenderer` | 9 | 4 | 3 |
| `App/NK3DModeler` | 8 | 5 | 4 |
| `App/NKCode` | 7 | 1 | 2 |
| `App/NkImageDemo` | 7 | 4 | 0 |
| `App/Pong2` · `App/Pong` · `App/Songoo` | 4 · 3 · 1 | 4 · 2 · 2 | 0 · 1 · 0 |
| `Kernel/NKMedia` · `NKCamera` | 4 · 3 | 3 · 1 | 3 · 0 |
| `App/Mou` · `NKImageCodecTest` · `NkCameraDemos` | 2 · 1 · 1 | 1 · 1 · 1 | 0 |
| `Kernel/NKCanvas` · `NKXR` · `Engine/Noge` + 5 apps | 0 | 0 | 8 |
| **TOTAL** | **120** | **65** | **29** |

Dont **18 appels dans 5 fichiers dupliqués** (`… copy.cpp`, `Pong copy/`) —
**qui sont de vraies cibles de build** : `config/modules.jenga:240` nomme
« Pong copy » parmi 4 projets réels. **102 appels sont dans le code non dupliqué.**

#### Ce que le recensement a révélé — et qui change la question

**1. La voie valeur PRODUIT de la voie tas.** C'est le fait décisif, et il n'était
dans aucune des analyses précédentes. **Cinq méthodes d'instance `const`
retournent un `NkImage *` du tas** : `Convert` (l. 2281), `Copy` (2334),
`CopyAs` (2382), `Crop` (2992), `Resize` (3029).

Donc `NkImage img; img.Load(p); NkImage *r = img.Resize(…);` — **une instance
valeur fabrique une instance tas**. Les deux voies ne sont pas séparables par
site d'usage : elles coexistent **dans la même expression**. C'est pourquoi
**10 fichiers sont mixtes**, et ce n'est pas de la négligence.

**2. Le piège a déjà sauté, en production, deux fois.**
`NK3DModeler/Viewport/NkDemo3D.cpp:11366-11372`, écrit sur place :

> « UNLOAD, PAS FREE. `NkImage::Free()` … fait `nkFree(this)` et libère L'OBJET
> LUI-MÊME. Appelée sur ce canevas statique, elle rendait à l'allocateur une
> adresse qui ne lui appartenait pas — **l'application se fermait net** juste
> après avoir écrit le fichier (**constaté deux fois par Rihen**). »

Ce n'est donc plus un piège théorique : c'est un `c0000374` **déjà payé**,
corrigé par un `Unload()` et **un commentaire**.

**3. Trois sites portent le même avertissement manuscrit** —
`NkDemo3D.cpp:11366`, `NKImage/tests/TestEXR.cpp:60`,
`NKRenderer/Core/NkTextureLibrary.cpp:218-220`. Les deux derniers montrent le
voisinage exact du danger :

```cpp
rgba->Free();   // rgba vient de Convert() (heap) → Free() OK
// `img` est sur la PILE : ne JAMAIS appeler img.Free() (qui ferait
// nkFree(this) sur une adresse pile → heap corruption c0000374).
```

**Deux objets du même type, dans la même portée, avec des règles de vie
opposées — séparés par un commentaire.** Règle du dépôt : *quand plusieurs
endroits documentent le même défaut, ce n'est pas une convention, c'est une
plainte.*

**4. L'idiome a essaimé.** `nkFree(this)` existe à **2** endroits réels :
`NkImage.cpp:1472` et `NkSVGCodec.cpp:2642` (`NkSVGImage::Free()`). La décision
prise ici fera donc **précédent pour au moins une deuxième classe**.

**5. Rien ne bloque la voie valeur :** **0** déclaration `NKIResource *` ou
`NKIResource &` dans tout le worktree — `NkImage` hérite de l'interface mais
**n'est stocké polymorphiquement nulle part**. La contre-épreuve a été faite :
la commande trouve bien `NKIResource` (9 occurrences, dont la définition
`NKStream/NKIResource.h:33`), elle ne trouve aucun pointeur.

#### ÉTAPE 2 — LA VOIE UNIQUE PROPOSÉE : **la valeur**, et Free() supprimée

**Argument de robustesse, sur le critère exact de Rodolf** — *quelle voie fait
qu'un appel erroné ne compile pas* :

- **Voie valeur retenue → `Free()` disparaît de l'API.** L'appel erroné n'existe
  plus **comme expression** : il n'y a plus rien à appeler de travers. C'est le
  seul des deux qui rend l'erreur **impossible** plutôt que documentée.
- **Voie tas retenue** exigerait d'interdire la déclaration sur pile
  (constructeur/destructeur privés + fabriques amies). C'est également vérifié
  par le compilateur — mais ça force une allocation pour **toute** image
  temporaire, et ça **contredit ce que la classe déclare déjà d'elle-même** :
  son en-tête dit que l'API d'instance est « pensée pour une utilisation en
  valeur (`NkImage img; img.Load("foo.png")`) », et `Unload()` est documentée
  « sûr sur pile comme heap ».

**La classe a déjà choisi la valeur ; ce sont 120 sites qui n'ont pas suivi.**
C'est **exactement le motif du chantier 11** — une API qui évolue, des
consommateurs qui ne suivent pas, et rien qui les y oblige.

#### ÉTAPE 3 — C'EST UN CHANTIER, PAS UN CORRECTIF. Et je ne le force pas.

Conformément à la troisième étape de Rodolf, le recensement montre que l'autre
voie est **largement utilisée** — 120 sites, 66 fichiers, 4 modules Kernel. À
faire, dans cet ordre :

1. basculer les **5 fabriques d'instance** (`Convert`, `Copy`, `CopyAs`, `Crop`,
   `Resize`) et les **6 fabriques statiques** sur un retour **par valeur**
   (le move-ctor existe déjà, NRVO fait le reste) ;
2. supprimer `Free()` — **le compilateur signale alors les 120 sites**, aucun ne
   peut être oublié en silence ;
3. relire les **10 fichiers mixtes** en premier : ce sont eux qui portent le
   risque.

⚠️ **Ce chantier ne peut pas se vérifier tant que le build complet ne tourne
pas** (chantier 11). La migration est *mécaniquement sûre* — le compilateur
attrape tout — mais « le compilateur attrape tout » ne vaut que si **on
compile**. **Ordre imposé : chantier 11 d'abord, migration NkImage ensuite.**

**Palier intermédiaire, si on veut désarmer le piège sans attendre la
migration** (à peser, non engagé) : remplacer `void Free()` par
`static void Free(NkImage *&img)`. Une instance valeur **ne se lie pas** à un
`NkImage *&` → l'appel erroné **ne compile pas**, et la mise à `nullptr` du
pointeur supprime au passage l'usage-après-libération. Coût : 120 réécritures
mécaniques `img->Free()` → `NkImage::Free(img)`, **toutes signalées par le
compilateur**. Ça n'unifie pas les voies — donc ça ne répond pas à la demande de
Rodolf — mais ça transforme le contrat par commentaire en erreur de compilation.

#### Matière rassemblée en chemin

- `Free()` : 3 appels dans le seul `NkImageDemo/Texture2D.cpp` (l. 83, 94, 117),
  tous supprimés par le port du chantier 11.
- Motif « valeur » en production : `Nogee/Editor/AssetManager.cpp:91`.
- **0** appel `Free()` sur une instance valeur détecté aujourd'hui : le piège est
  **armé partout mais ne tire nulle part** depuis le correctif de `NkDemo3D`.
  ⚠️ Détection limitée aux variables résolues dans le fichier — un `Free()` sur
  une valeur reçue par référence depuis un autre module échapperait à la mesure.
- ❌ **Correction d'une erreur de mes rapports antérieurs** : j'avais écrit que
  `IsValid()` avait été **supprimée**. **C'est faux** — elle existe
  (`NkImage.h:564`, override de `NKIResource`) et `NkImageDemo/Texture2D.cpp:78`
  l'appelle. La seule disparition réelle est la fabrique `static Load(path)`.

---

## Candidat identifié, EN ATTENTE D'ARBITRAGE — `NkEditMesh`

> Relevé le 2026-08-14 en mesurant `NKRenderer/`. **Aucune décision n'a été prise
> sur ce cas** ; il est consigné pour ne pas être re-découvert dans six mois.

`Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkEditMesh.{h,cpp}` fait
**6 551 lignes** (1 153 + 5 398). C'est la structure d'**édition** de maillage —
modèle BMesh, arête de premier plan, cycles radial et disque, soudure,
opérations. Elle ne rend rien.

C'est **exactement le raisonnement** qui a conduit à la décision « Substrats
animation et comportement » du 14/08 (bloc dans `CLAUDE.md`) : un sous-système
qui n'a de graphique que son adresse, rangé dans le module de rendu, et que tout
consommateur doit donc payer en entier. La différence est que ce cas-là **n'a
pas été arbitré par Rodolf** — il n'est donc **pas** dans le bloc de décision, et
rien ne doit bouger tant qu'il ne l'a pas été.

Ce qu'on sait déjà, pour le jour où la question se posera : le harnais de
non-régression existe (`Applications/NKEditMeshHarness`, 3 082 l.), ce qui rend
un déplacement mesurable plutôt qu'un pari.

## Dettes de capacité repérées en chemin

Ce ne sont pas des chantiers de rangement, mais des **plafonds statiques** qui
tiendront tant qu'on travaille sur des scènes d'essai et sauteront au premier
modèle importé pour de vrai :

- **64 matériaux par projet** (`kNkvpMaxProjMats`). Un `.gltf` d'objet réel en
  aligne couramment vingt ou trente ; un décor complet dépasse. La liste de
  matériaux *par objet* suit désormais cette borne (12 août), donc la lever
  bénéficierait aux deux d'un coup.
- **160 nœuds par scène** (`kNkvpMaxNodes`).

Le remède est le même dans les deux cas : passer du tableau statique à une
collection. À faire quand l'import de modèles réels arrivera, pas avant.

## Ce que ça n'est pas

Ce n'est pas une réécriture, ni un changement d'architecture. Les décisions
techniques du moteur sont saines ; c'est leur **rangement** qui est en cause.
Aucun de ces chantiers ne doit changer un comportement observable.
