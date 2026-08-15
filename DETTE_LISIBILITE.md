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

> Cinq au 12 août 2026. **Dix depuis le 14 août** : la confrontation des
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
