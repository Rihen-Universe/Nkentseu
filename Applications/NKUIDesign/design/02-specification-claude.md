# NkUIDesign — Spécification d'implémentation
### Document 2/3 — destiné à un agent d'implémentation (Claude)

> Ce document traduit `01-specification-humaine.md` en contraintes exploitables.
> Il donne : jetons, structures, contrats, règles de vérification, **témoins**, et
> ordre d'implémentation. Le document 1 porte le contexte et les justifications ;
> le document 3 porte les écrans et le visuel.
>
> **Cible technique** : C++ **zero-STL** sur **NKGui** (primitives) et
> **NKEditorKit** (coquille et composants). Les documents de design du dépôt sont
> rédigés en vocabulaire web — c'est une **langue de conception, jamais une
> architecture à reproduire**.
>
> **Trois règles de lecture, avant tout le reste :**
> 1. **Ce qui est marqué ✅ existe** — ne le réécrivez pas, lisez-le et appelez-le.
> 2. **Ce qui est marqué 📝 n'existe pas** — n'écrivez nulle part une phrase qui
>    suppose le contraire.
> 3. **Aucune section n'est réputée faite sans son témoin.** Un contrôle qui ne
>    peut pas échouer ne mesure rien.

---

## 0. Ce qui existe déjà — l'inventaire à lire avant d'écrire une ligne

**La règle du dépôt** : une application ne réécrit pas *sans raison écrite* ce
que la bibliothèque porte. Le test avant chaque ligne d'interface : *est-ce que
ça existe déjà ? est-ce que ça devrait exister dans la bibliothèque plutôt que
chez moi ?* Et si « ça existe mais ça ne me va pas » — **on améliore la
bibliothèque, on ne la contourne pas**.

### 0.1 Dans `Engine/NKEditorKit/` — la forme de déclaration

Cinq fichiers, **et l'ordre de lecture compte** :

| fichier | ce qu'on y cherche |
|---|---|
| `Components/NkComponentLayout.h` | l'arbre, la taille, l'agencement, **et les mots du fichier** |
| `Components/NkComponentDecl.h` | ce qu'un composant déclare : rôle, éléments, provenance, résolution d'un nombre |
| `Components/NkComponentRole.h` | le catalogue des capacités — **9 rôles** |
| `Components/NkComponentCheck.h` | **la porte unique** des vérifications |
| `Components/NkLayoutSolve.h` | la **sémantique** : la position est un résultat |

Autour : `NkComponentInstance.h` (les écarts sauvés, la provenance persistée),
`NkComponentPaint.h` (le peintre vu par un composant), `NkRecordingPaint.h` (le
peintre enregistreur, sans GPU), `NkComponentRegistry.cpp` (le registre).

Deux composants complets : `NkContentBrowserModel.h`/`NkContentBrowserDraw.cpp`
et `NkTreeViewModel.h`/`NkTreeViewDraw.cpp`.

### 0.2 Dans `Engine/NKEditorKit/` — la coquille et le thème

| fichier | ce qu'il porte |
|---|---|
| `NkEditorShell.h` | ancrage, panneaux, barre de titre + logo, **palette de commandes**, barres d'activité, barre d'état, menus, dépôt de fichiers, sauvegarde/chargement de disposition, `graphicsApi` |
| `NkTheme.h` / `NkTheme.inl` | rôles nommés append-only, thèmes *Sombre* et *Clair*, **héritage au chargement**, registre de rôles d'application, mesure de contraste |
| `NkShortcutTable.h` | les raccourcis comme **données** |
| `NkEditorCommand.h` | les commandes de la palette |
| `NkEditorInspector.h`, `NkEditorModal.h`, `NkEditorContextMenu.h`, `NkFilePicker.h`, `NkEditorCombo.h`, `NkEditorTextField.h`, `NkEditorScrollbar.h`, `NkEditorTooltip.h` | dialogues et petits composants |

⚠️ `NkEditorExport.h` = macros `dllexport`/`dllimport`. **Aucun rapport avec
l'export d'interfaces** (§ 9).

### 0.3 Dans `Applications/NKUIDesign/src/NKUIDesign/`

| fichier | ce qu'il fait |
|---|---|
| `main.cpp` | modes `--probe` / fenêtre, enregistrement des 5 panneaux et des 4 commandes |
| `Backend.h` | la **résolution du backend graphique**, en fonction **pure** : `main` l'appelle, `--probe` vérifie **la même fonction** |
| `Document.h` | le document = un arbre de nœuds, **zéro coordonnée**, provenance, cadres |
| `Layout.h` | le solveur du document — **miroir** de la sémantique du kit |
| `Renderers.h` | table `nom → fonction de dessin`, **cartouche** pour un composant non branché |
| `Panels.h` | les cinq panneaux |
| `DesignAI.h` | la **place** de l'IA : prompt, backend remplaçable, greffe, provenance, rejeu |
| `Probe.h` | la sonde sans écran — **72 essais** |

### 0.4 Les chiffres du jour (2026-08-18, fin de journée, commit `002566f7`)

```
jenga build --target NKUIDesign   :  20/20 SUCCESS
NKUIDesign --probe                : 103/103
banc de la forme (NkFormProbe)    :  43/43, dont une série de témoins qui doivent rougir
banc de neutralité                :  vert, avec ses témoins qui échouent bien
jenga build --target NKEditorKit  :  19/19 SUCCESS
l'application                     :  OUVRE UNE FENÊTRE (OpenGL), 3 lancements identiques,
                                     fermeture propre
composants déclarés au registre    :  2 (content_browser, tree_view)
```

⚠️ **Mis à jour le 2026-08-18 dans la nuit** : 78/78 → **89/89** (familles 34
« géométrie » et 35 « configuration »). Reste vrai plus bas : 72/72 → **78/78**, et le magenta est
**sorti**. Les six essais nouveaux sont la famille 33 (canonisation + repli
franc) ; la famille 32 a changé de nature. **Critère d'échec posé avant le
correctif, et mesuré** : canonisation désactivée → **77/78, code de sortie 1**,
l'essai `33e` nommant les **23 jetons** fautifs un par un. Mutation retirée,
résidu vérifié à zéro, 78/78 retrouvé.

⚠️ **Ces chiffres ont une date, et elle compte.** Une première rédaction de ce
document, quelques heures plus tôt, portait 58/58, 34/34 et « la fenêtre n'a
jamais été ouverte ». Tout cela était vrai à l'écriture. **Revérifiez contre le
dépôt avant de vous appuyer sur un chiffre d'ici** ; ne le recopiez pas.

⚠️ **Ce que « l'application tourne » ne dit pas** : aucune conformité aux planches
de référence n'est mesurée. La fenêtre s'ouvre et se ferme proprement.

### 0.5 Les deux défauts que le premier regard a trouvés — à lire avant d'écrire un témoin

**68 essais verts ne les avaient pas vus.** Ce ne sont pas des anecdotes : ce sont
les deux formes d'aveuglement les plus productives de ce projet.

**Défaut 1 — le code sans témoin était exactement le code que le GPU touchait.**
Le journal du backend imprimait ses propres accolades au lieu des valeurs : la
trace **existait et ne disait rien**. La cause n'est pas la faute de formatage,
c'est **l'emplacement** : la résolution vivait dans le point d'entrée, donc hors
de portée d'une sonde sans écran.

> **Règle qui en sort** : si un morceau de logique n'est atteignable que par le
> chemin qui exige un écran, **sortez-le en fonction pure** et faites-le vérifier
> par la sonde. C'est ce qui a été fait (`Backend.h`).

**Défaut 2 — un résolveur qui dit oui à tout ne peut pas voir un nom faux.**
Le composant se peignait en **magenta franc** : les noms canoniques de rôles de
thème sont en `snake_case`, des rôles étaient déclarés en `PascalCase`, aucun ne
tombait juste, et le thème repliait sur sa couleur « ça doit sauter aux yeux ».
**Le repli a fait son travail.** Mais la sonde ne pouvait rien voir : son
résolveur **hachait n'importe quel nom** vers une valeur valide et ne rendait
jamais « inconnu ».

> **Règle qui en sort** : un doublure de résolution doit pouvoir rendre **échec**.
> Une doublure totale valide n'est pas une doublure, c'est un masque.

✅ **CORRIGÉ le 2026-08-18 après-midi — et c'est la CAUSE qui l'a été, pas les
noms.** La mesure disait que ce n'était pas une inattention : **10 jetons sur 10**
dans `NkContentBrowserModel.h`, **13 sur 13** dans `NkTreeViewModel.h` — deux
fichiers, deux auteurs, **23 sur 23**. Renommer 23 chaînes aurait rendu l'écran
juste ce soir-là et laissé le vingt-quatrième jeton refaire la même erreur.

Trois gestes, dans cet ordre :

1. **le résolveur de la sonde a été SUPPRIMÉ** — pas amélioré. La résolution de
   l'application est désormais la valeur **par défaut** de `NkDocumentHost` : il
   n'y a plus qu'une résolution dans le programme, sonde comprise. Sa
   justification (« rester injectif pour comparer des couleurs distinctes ») ne
   tenait pas : le résolveur réel **est** injectif sur les 30 rôles du cœur, et
   là où deux jetons partagent un rôle, les peindre pareil est **la vérité de
   l'écran** ;
2. **(a) la résolution canonise** (`Roles.h`) — `PascalCase` → `snake_case`,
   essayée **après** le nom brut, donc elle ne peut que rattraper ;
3. **(b) le repli est franc** — l'audit **compte et nomme**, au journal du
   démarrage (avant l'ouverture de la fenêtre), dans un bandeau de l'aperçu, et
   dans le rapport de sonde.

🟡 **Reste ouvert, et il est porté au canal (Q64)** : la canonisation vit dans
`Applications/NKUIDesign/src/NKUIDesign/Roles.h`, alors que son foyer est
`NkRoleRegistry::Find` (`NKEditorKit/NkTheme.inl`). **Tant qu'elle est là, elle
ne protège que NkUIDesign** — Nogee, NK3DModeler, NkAnimaEditor et PV3DE
rencontreront le même magenta. La fonction est écrite **pure** et éprouvée
(famille 33) précisément pour être déplacée telle quelle. Les **9 graphies**
`PascalCase` distinctes restent listées à chaque sonde : c'est la liste de
travail de la correction à la source, et sans elle (a) rendrait les déclarations
fausses **invisibles**.

### 0.6 Le second regard — la capture confrontée aux planches, écart par écart

**Fait le 2026-08-18 après-midi.** `design/temoin_visuel.png`, garde PID verte
(fenêtre au premier plan = PID de l'application), OpenGL, RTX 3070, fenêtre
1456×939, sortie 0. Empreinte : 96,0 Mo de working set, 131,6 Mo de mémoire
privée, 381 handles, 10 threads — conforme aux trois lancements du matin.

⚠️ **Périmètre de cette comparaison, écrit avec elle** : elle porte sur les
**deux composants dessinés** (navigateur de contenu, arbre), confrontés à
`Applications/Nogee/design/AetherionContentBrowserDark.png` et
`AetherionWorldOutlinerDetailsLight.png`. Les **panneaux de l'éditeur** (palette,
composition, propriétés, IA) **n'ont pas été confrontés** — c'est la moitié qui
reste.

⚠️ **Et ce n'est pas une mesure de pixels** : les planches sont en 4320×2160, la
capture en 1456×939, et les données affichées ne sont pas les mêmes. C'est une
comparaison **structurelle** — présence, position, nature d'un élément — et rien
n'est affirmé sur une couleur ou une distance.

#### ⚠️ Deux lectures faites sur l'image, et **la mesure les a démenties toutes les deux**

Le 18/08 au soir, deux affirmations concurrentes sur la même capture : la mienne
(« icônes **rognées au bord gauche** », écart n° 11) et celle du canal (« la
colonne d'arborescence **se superpose** à la première rangée de cartes », jugé
plus grave). **Aucune des deux n'était un instrument.** La famille 35 — pardon,
**34** — de la sonde tranche : elle ne compare pas des pixels, elle lit les
**rectangles** que le composant a émis.

| mesure | valeur |
|---|---|
| débordement colonne → grille | **0** commande, 0,0 px |
| débordement grille → colonne | **0** commande |
| hors du panneau, à **gauche** | **0,0 px** — donc rien n'est « rogné » |
| hors du panneau, à **droite** | **8,0 px**, 1 commande |

**Le seul débordement réel n'est pas celui qu'on cherchait**, et il est
instructif : `NkContentBrowserDraw.cpp:129` passe `header.w` à un texte posé à
`header.x + card_pad` — donc **8 px de plus que le panneau**. Le clip le masque à
l'écran, et c'est justement ce qui le rend coûteux : **le texte croit disposer de
8 px qu'il n'a pas**, donc son point de troncature est calculé sur une largeur
fausse. Même famille que « les libellés tronqués ». Fichier d'un autre agent :
compté et nommé, pas corrigé.

⚠️ **L'instrument s'est trompé deux fois avant de dire vrai**, et c'est la partie
à relire : (1) il prenait le **premier** `PushClip` — celui du panneau entier —
et passait vert avec une frontière à `x=0,0` ; ce qui l'a démasqué est d'avoir
**publié la valeur intermédiaire** à côté du verdict. (2) Sans borne verticale, il
accusait « Créer » (barre d'outils) et « niveau1 » (fil d'Ariane) : **un
instrument mal borné aurait confirmé la lecture qu'il devait départager.** D'où le
**contrôle positif 34e** : après avoir élargi deux fois les bornes, on injecte un
chevauchement de 50 px et le **même** détecteur doit le compter.

**Navigateur de contenu — 11 écarts** *(l'écart n° 11 est corrigé ci-dessus : il
n'y a pas de rognage ; il reste 10 écarts réels)*.

| # | ce que la planche a, et pas nous | à qui |
|---|---|---|
| 1 | un champ de **recherche** dans la barre du haut | composant |
| 2 | un bouton **Importer** à droite de la barre du haut | composant |
| 3 | une bande de **filtres par nature** (Mesh / Material / Texture / Blueprint / Sound) | composant |
| 4 | un **curseur de taille de vignette** + bascule grille/liste | composant |
| 5 | une ligne de **compte** (« 18 éléments · 1 sélectionné ») et un **tri** | composant |
| 6 | une **pastille de nature** posée sur la vignette — chez nous la nature est une 2ᵉ ligne de texte sous le nom | composant |
| 7 | une **vignette** — chez nous un aplat de couleur de rôle | hôte (aucun rendu d'aperçu d'asset n'existe) |
| 8 | la carte **active** entourée d'un liseré accent (écart n° 3 déjà déclaré dans la déclaration) | composant |
| 9 | une **barre d'état** en bas (nom, type, taille, date) | composant |
| 10 | des **chevrons** et une indentation dans la colonne de gauche | composant |
| 11 | les **icônes de dossier de cette colonne sont rognées** au bord gauche du panneau | composant, et c'est le seul qui ressemble à un défaut plutôt qu'à un manque |

⚠️ **Vérifié avant d'attribuer** : `NkContentBrowserDecl()` ne porte que 4
paramètres (`thumb_size`, `show_tree`, `show_footer`, `tree_width`). Les écarts
1-5, 9 ne sont donc **pas des réglages laissés à zéro** — ils n'existent pas dans
le composant. La distinction change qui doit agir.

**Arbre — 7 écarts.**

| # | ce que la planche a, et pas nous | à qui |
|---|---|---|
| 12 | des **chevrons** | **hôte** |
| 13 | les icônes **œil** et **cadenas** | **hôte** |
| 14 | une **icône de nature** devant le libellé | **hôte** |
| 15 | ligne active = fond teinté **+ barre d'accent à gauche + libellé en accent** ; chez nous un aplat plein | arbitrage de Rodolf, déjà déclaré dans `active_mark` |
| 16 | un **entonnoir de filtre** à droite du champ de recherche | composant |
| 17 | un **point de marque** à droite de la ligne active | composant |
| 18 | le **pied** existe des deux côtés (« 12 nœud(s), 1 sélectionné(s) ») — **conforme** | — |

⚠️ **Cause commune de 12, 13 et 14, et elle est CHEZ MOI** : `NkTreeViewIcons`
n'est **jamais rempli** par l'hôte, donc toutes les poignées valent 0 et rien ne
se peint. **Les colonnes sont pourtant réservées** : l'espace est pris, rien n'y
apparaît. Conséquence concrète, pas cosmétique — **un arbre sans chevron ne se
plie pas**. Ce n'est pas un défaut du composant : il fait ce qu'on lui dit, et on
ne lui dit rien. Le blocage réel est qu'**aucun atlas d'icônes n'existe dans
l'application** (Palier 4, tâche 17).

---

## 1. Les jetons

### 1.1 Le principe non négociable

**Aucune couleur, aucune taille, aucun rayon écrit en dur dans un composant.**
Un jeton est un **nom stable** que le thème résout. Le test à appliquer à chaque
constante littérale rencontrée dans du code de dessin : *si le thème change,
cette valeur suit-elle ?*

### 1.2 Jetons de couleur — ils existent, et voici leur forme

Un rôle = **une entrée d'énumération** (accès du code, indexation directe,
vérifiée à la compilation) **+ un nom stable** (accès du fichier).
**L'énumération est APPEND-ONLY** : un rôle inséré au milieu décale tous les
suivants et un thème enregistré relit les mauvaises couleurs.

⚠️ **LE NOM CANONIQUE D'UN RÔLE EST EN `snake_case`** — `panel_bg`,
`panel_header`, `input_bg` — et la résolution compare **octet pour octet**. Un
rôle déclaré en `PascalCase` ne tombe juste sur rien, rend « inconnu », et se
peint en **magenta franc**. Ce défaut est arrivé, il a été trouvé **à l'œil au
premier lancement**, et aucun des 68 essais d'alors ne pouvait le voir (§ 0.5).
Les noms d'énumération C++ restent en `PascalCase` ; **les noms écrits et
résolus sont en `snake_case`**. Ne les confondez jamais.

Groupes existants : structure (`WindowBg`, `PanelBg`, `PanelHeader`, `Border`,
`InputBg`, `LabelCol`) · texte (`Text`, `TextMuted`, `TextOnAccent`) · accents
(`AccentUi`, `AccentSel`) · états d'élément (`ElemActive`, `ElemSelected`,
`ElemIdle`) · axes (`AxisX/Y/Z`) · types (`TypeMesh`, `TypeAnim`, `TypeMat`,
`TypeTex`, `TypeFolder`) · graphe de nœuds (`NodeDataHeader`,
`NodeDataHeaderHot`, `NodeActionHeader`, `NodeBody`, `NodeWire`) · vue 3D
(`ViewportTop`, `ViewportBottom`, `GridLine`).

Valeurs de référence (pour les maquettes du document 3 — **ne jamais les recopier
dans du code**) :

| rôle | Sombre | Clair |
|---|---|---|
| `WindowBg` | `#010409` | `#F5F5F5` |
| `PanelBg` | `#0D1117` | `#FFFFFF` |
| `PanelHeader` | `#161B22` | `#EAEAEA` |
| `Border` | `#30363D` | `#0000001F` |
| `InputBg` | `#0D1117` | `#FFFFFF` |
| `LabelCol` | `#161B22` | `#F0F0F0` |
| `Text` | `#FFFFFF` | `#1A1A1A` |
| `TextMuted` | `#8B949E` | `#0000008C` |
| `AccentUi` | `#1F6FEB` | `#0E5FA6` |
| `AccentSel` | `#F2980E` | `#C97A08` |
| `NodeDataHeader` | `#0A545E` | *(hérité)* |
| `NodeActionHeader` | `#F2980E` | *(hérité)* |
| `NodeWire` | `#8B949E` | *(hérité)* |
| `TypeFolder` | `#E3B341` | *(hérité)* |

Rayons : quatre crans nommés (`Sm`, `Md`, `Lg`, `Xl`) — ils font partie du thème
au même titre que les couleurs.

### 1.3 Rôles propres à NkUIDesign — à **enregistrer**, jamais à ajouter au cœur

L'outil aura besoin de rôles que les autres applications n'ont pas. Ils passent
par le **registre d'extension**, sous un préfixe :

```
nkuid.selection_noeud        contour du nœud sélectionné dans l'aperçu
nkuid.survol_noeud           contour au survol
nkuid.cadre                  un nœud sans composant (il arrange, il ne dessine pas)
nkuid.cartouche_absent       le cartouche d'un composant déclaré non branché
nkuid.poignee_taille         les poignées de redimensionnement
nkuid.guide_agencement       les repères d'agencement (ligne, colonne, grille)
nkuid.cle_manquante          l'affichage d'une clé de traduction absente
```

Conséquence recherchée, et elle est la raison du mécanisme : **un thème écrit
pour NkUIDesign se charge sans erreur dans une autre application** — ses rôles
inconnus sont **comptés, pas rejetés**.

### 1.4 Jetons de métrique — 📝 le manque, et où il vit aujourd'hui

Le thème ne porte **pas** de métriques (hors les quatre rayons). Les longueurs
d'un composant vivent dans **sa déclaration** (`metrics`), ce qui est correct
pour ce qui lui est propre (écart entre cartes) et **insuffisant** pour ce qui
devrait être commun (hauteur de ligne, pas de grille, gouttière).

**À faire, dans cet ordre** : (1) inventorier les métriques répétées entre les
deux composants existants ; (2) **seulement alors** proposer un jeu de jetons de
métrique au niveau du thème. Écrire le jeu avant l'inventaire, c'est deviner.

---

## 2. Le document — structure et invariants

### 2.1 L'invariant qui gouverne tout le fichier

> **Aucune coordonnée n'est jamais enregistrée.**

Test exécutable, et il doit rester dans la sonde : parcourir les champs d'un
nœud sérialisé et **échouer** si l'un s'appelle `x`, `y`, `position`, `left`,
`top`, ou toute variante. Ce n'est pas une politesse de style : c'est ce qui
sépare un outil de design d'un constructeur d'interfaces au pixel.

### 2.2 Ce qu'un nœud porte

| champ | rôle |
|---|---|
| `nom` | identifiant dans le document, éditable |
| `composant` | le nom d'un composant **du registre** — **vide = cadre** |
| `écarts` | les valeurs qui diffèrent de la déclaration (`NkComponentInstance`) |
| `taille` | `fixed` · `content` · `fraction` · `weight` · `expand`, avec `min` et `max` |
| `agencement` | pour ses enfants : `none` · `row` · `column` · `grid` · `anchor` |
| `alignement` | `start` · `center` · `end` · `stretch` |
| `provenance` | `auteur` · `vérifié` · `corrigé` |
| `enfants` | l'arbre |

**Pas de type « document » distinct du type « nœud ».** Le document est la
racine. Le jour où un sous-arbre devient un composant de bibliothèque, **c'est la
même structure qui part**, pas une conversion.

### 2.3 Les mots du fichier

Les mots **écrits dans un fichier** sont **anglais et minuscules** —
`fixed/content/fraction/weight/expand`, `none/row/column/grid/anchor`,
`start/center/end/stretch`, ancres `"ltrb"` — parce que la cible de convergence
est un format dont tout le vocabulaire l'est.

⚠️ **Les LIBELLÉS affichés restent en français.** Ne confondez jamais les deux :
le mot du fichier est un identifiant, le libellé est du texte traduisible (§ 5).

### 2.4 La souris est traduite, jamais enregistrée

| geste | ce qui est écrit |
|---|---|
| tirer un bord | une **taille** ou un **poids** |
| déplacer un nœud | un **parent** et un **rang** |
| poser depuis la palette | un **enfant** de plus, avec sa taille par défaut |

**Jamais un point.** Si vous vous surprenez à vouloir stocker la position de la
souris, c'est que l'action manque d'une traduction — écrivez la traduction, pas
la position.

### 2.5 ⚠️ Une seule sémantique d'agencement

Le sens de `extensible`, `à poids`, `fraction`, `min`, `max`, `aligné` est fixé
**dans `NkLayoutSolve.h`** et nulle part ailleurs. Le solveur du document
**appelle** ses fonctions, il ne les recopie pas.

Pourquoi un second parcours existe malgré tout : les deux échelles n'ont pas la
même **forme de données** — table compilée avec liens par nom d'un côté, arbre
mutable avec liens par index de l'autre. **Même sémantique, deux structures.**

Les trois règles à ne jamais modifier d'un seul côté :

1. la répartition du reste est **itérative** (un enfant ramené par sa borne rend
   sa part aux autres), plafonnée à 8 passes ;
2. l'alignement principal ne s'applique **que s'il ne reste rien à poids** — sinon
   les poids ont déjà tout pris, par définition ;
3. en grille, la hauteur de rangée est le **maximum** de ses enfants, et une
   hauteur à poids retombe sur une cellule carrée.

📝 **Témoin manquant, et il est nommé** : *rien ne vérifie que les deux solveurs
rendent le même résultat pour la même déclaration.* Ils sont d'accord ; tant que
rien ne le vérifie, **ils rediveregeront**. Le témoin à écrire : une déclaration
transformée en document, résolue des deux côtés, **rectangles comparés**.

### 2.6 La zone sûre — 📝 une seconde ancre dans le solveur

> **Rien n'existe** : aucune notion de zone sûre, aucune ancre, aucune
> simulation d'appareil.

#### 2.6.1 La règle, et elle a la même forme que « pas de coordonnée »

**La zone sûre n'est pas une constante.** Elle change à la rotation, à
l'ouverture du clavier, en écran partagé, et d'un appareil à l'autre. Elle **se
demande à la plateforme à l'exécution**, jamais en dur.

Conséquence directe sur ce qui est **enregistré** : le document ne contient
**aucune valeur d'insets**. Il contient, par élément, **le choix d'ancre**. Les
insets sont une **entrée du solveur**, au même titre que la taille de surface —
ils vivent à côté de l'échelle et de la position de souris dans l'objet que
l'hôte construit une fois par fenêtre et par image, jamais dans une variable
globale et jamais dans le fichier.

⚠️ Le réflexe à interdire, parce qu'il est le plus naturel : ajouter
`marginTop = 44` quelque part. C'est **la même faute** que d'enregistrer un `y` —
une valeur qui dépend du contexte d'affichage figée dans la donnée. *Une marge
fixe est juste sur l'appareil qu'on a sous la main et fausse sur le suivant.*

#### 2.6.2 Ce que l'élément déclare : **deux ancres, par élément**

```
ancre = safe    |    ancre = edge
```

| `edge` — jusqu'au **bord** | `safe` — dans la **zone sûre** |
|---|---|
| fonds, images de couverture, dégradés, listes qui défilent sous la barre | texte, boutons, champs — **tout ce qui se lit ou se touche** |

**Défaut proposé** (et c'est une proposition, pas une décision) : `safe` pour tout
élément dont le rôle est `label`, `button`, `toggle`, `text_field`, `slider` ;
`edge` pour un `container` qui ne porte que du fond. Le défaut ne dispense pas
du champ : il rend seulement le cas courant silencieux.

⚠️ **Jamais un réglage global.** « L'application respecte la zone sûre » produit
des bandes disgracieuses derrière les fonds ; « l'application l'ignore » produit
des boutons **inatteignables** sous l'indicateur de geste. Un interrupteur unique
choisit donc lequel des deux défauts on veut — c'est-à-dire aucun des deux.
**Le choix est par élément, il appartient à la déclaration.**

#### 2.6.3 Comment ça entre dans le solveur

Le solveur reçoit deux rectangles au lieu d'un : la **surface** et la **zone
sûre** (la surface moins les insets demandés à la plateforme). Pour chaque nœud,
le rectangle du parent utilisé comme base est celui que **son ancre** désigne. Le
reste de la sémantique (`fixed`, `content`, `fraction`, `weight`, `expand`, min,
max, alignement) est **inchangé** — la zone sûre ne modifie aucune de ces règles,
elle change seulement le rectangle de départ.

⚠️ **Et comme toujours : une seule sémantique.** Si l'ancre entre dans le solveur
du document sans entrer dans celui du kit, on retombe exactement dans le défaut
du § 2.5 — deux sémantiques pour une même déclaration. **L'ancre se pose des
deux côtés, dans le même mouvement**, ou pas du tout.

#### 2.6.4 La simulation dans l'éditeur — sans elle, le défaut se découvre trop tard

L'éditeur doit **simuler des zones sûres** : encoche haute, indicateur de geste
bas, coins arrondis, rotation portrait/paysage, clavier ouvert. Sinon le défaut
se découvre **sur le téléphone**, au moment où il coûte le plus cher.

Forme attendue : un jeu de profils d'appareil (nom, taille, insets par
orientation), un sélecteur dans la barre d'outils du panneau Aperçu, et un
affichage des **zones masquées par-dessus le rendu**, activable.

⚠️ Un profil simulé est une **donnée d'essai**, pas une vérité plateforme. Il ne
remplace pas l'appel réel : il permet de le tester avant de l'avoir.

#### 2.6.5 ⚠️ LE TÉMOIN — et il discrimine dans les deux sens

Avec le peintre enregistreur, sans GPU :

```
1. définir une zone masquée simulée (par exemple 44 px en haut, 34 px en bas)
2. dessiner un document contenant DEUX éléments :
     - un fond      ancré « edge »
     - un bouton    ancré « safe »
3. examiner la géométrie produite dans la zone masquée
```

| élément | attendu | ce qu'un échec signifie |
|---|---|---|
| élément ancré **`safe`** | **aucune géométrie** dans la zone masquée | l'ancre n'est pas honorée : sur l'appareil, le bouton sera partiellement ou totalement **inatteignable** |
| élément ancré **`edge`** | **de la géométrie** dans la zone masquée | l'ancre est appliquée globalement : sur l'appareil, le fond laissera des **bandes** |

**Les deux moitiés sont nécessaires.** Un témoin qui ne vérifie que la première
passerait avec un réglage global « tout respecte la zone sûre » — c'est-à-dire en
validant précisément la moitié du défaut. *Un contrôle qui ne sait dire que oui
ne mesure rien.*

**Contrôles d'encadrement, dans le même esprit que le § 11.1** :

- **témoin de bruit** : insets à zéro, deux passes → **zéro** différence ;
- **contrôle positif** : insets 0 → 44/34 → la géométrie de l'élément `safe`
  **se déplace**, celle de l'élément `edge` **ne bouge pas** ;
- **contrôle de rotation** : portrait → paysage change les insets **et** la
  surface ; vérifier que l'élément `safe` suit **les deux**, pas seulement la
  surface — c'est là qu'une implémentation qui a codé les insets en dur se
  trahit, parce qu'elle continuera d'écarter 44 px en haut en paysage.

---

## 3. La déclaration d'un composant

### 3.1 Les neuf choses déclarées

`paramètres` (avec bornes) · `jetons de thème` · `métriques` · `variantes` ·
`points de greffe` · `événements` (avec charge) · `rôle` · `arbre d'éléments` ·
`provenance`.

Plus, transversalement et portées par chaque élément : les **propriétés de taille
et d'agencement** (§ 2.5) et l'**ancre** — zone sûre ou bord (§ 2.6). L'ancre est
📝 à ajouter ; elle suit la règle du § 3.2 (**tout champ neuf s'ajoute à la
FIN**).

### 3.2 Les règles structurelles, et pourquoi

| règle | raison |
|---|---|
| **tout champ neuf s'ajoute à la FIN** de la structure | C++17 n'a pas d'initialiseurs désignés ; une déclaration s'écrit en liste **positionnelle**. Un champ inséré au milieu décale tout. ⚠️ **Le cas bénin est le vrai danger** : avec deux champs de types incompatibles, ça rougit ; avec deux `const char*` voisins, **ça compile** et décrit un autre composant. |
| **la table d'éléments est PLATE**, chaque ligne nomme son parent | un parent apparaît toujours plus haut que ses enfants : **un cycle devient impossible à écrire**, et le résolveur n'a pas besoin d'être récursif |
| **constante de compilation, zéro allocation** | une déclaration **se lit**, elle ne s'instancie pas ; elle se vérifie **sans rien lier** |
| **aucun pointeur de fonction dans la déclaration** | une adresse d'exécution dans une donnée qui n'en contient aucune casserait la propriété précédente |
| **le même nom dans deux tables est REFUSÉ** | sinon l'ordre de lecture tranche en silence : deux vérités pour un nombre |

### 3.3 La frontière avec la réflexion d'objets — à ne pas franchir

> **Si la chose peut être INSTANCIÉE, c'est la réflexion ; si elle peut être
> DESSINÉE, c'est la déclaration de composant.**

Interdit des deux côtés, et c'est la seule interdiction : ajouter ici de quoi
décrire un objet de données (héritage, conteneurs, fabrique par nom) ; demander à
la réflexion de porter une variante ou un jeton de thème.

**Point de contact unique** : un panneau de propriétés affiche les propriétés
d'un objet réfléchi **dans** un composant déclaré ici. L'hôte passe l'un à
l'autre ; le composant ne lit pas la réflexion.

⚠️ La réflexion **n'est ni dépréciée ni concurrencée** — elle a un autre domaine.

### 3.4 Les événements

Forme : `nom(arg: Type, …) -> Void`, avec des types pris **exactement** dans la
table du format cible : `Void | Bool | Int | Float | String | Color | Vec2 |
Enum[…]`. **Pas un type de plus.** C'est cette propriété qui rend l'émission du
bloc de contrat possible.

Deux règles :

1. **Une charge interprétable seulement en possédant l'objet émetteur n'est pas
   une charge, c'est un pointeur déguisé.** D'où `path` à côté d'`index`.
2. **Un événement nomme un FAIT, pas un GESTE** : `onActivate`, pas
   `onDoubleClick` — un double-clic active, la touche Entrée aussi.

⚠️ **La signature d'un composant doit porter l'ENTRÉE.** Un composant qui dessine
sans recevoir l'entrée n'émet jamais rien : ses crochets seraient des pointeurs
que rien n'appelle, et l'application les remplirait en attendant. Ce trou a été
trouvé et bouché — ne le rouvrez pas.

### 3.5 Les deux manques mesurés — l'un est réparé, l'autre n'est pas à vous

1. **Une charge ne sait pas porter une collection.** Aucun des huit types n'est
   une liste, et c'est délibéré. Une sélection multiple **n'est donc pas
   observable depuis un graphe**, et ce n'est pas un oubli. **Deux mains ont
   rencontré ce mur indépendamment.** Deux issues (ajouter `List[T]` à la
   spécification ; ou introduire la **propriété exposée** — un état lisible sans
   être une charge). **La décision est à Rodolf. Ne tranchez pas.**
2. ✅ **Le gabarit répété est FAIT** (18/08). Un élément déclare `once` /
   `per_entry` / `per_entry_tree`, **et rien d'autre** : ni compteur, ni source de
   données — *la donnée appartient à l'application, jamais au kit*. Le champ a été
   **ajouté à la fin**, et un essai vérifie qu'une table écrite avant lui compile
   encore. La réparation est bien **générale** : le même manque tombait sur les
   cartes d'un navigateur **et** sur les lignes d'un arbre.

---

## 4. Les thèmes — ce qu'il reste à écrire

| # | à écrire | témoin |
|---|---|---|
| T1 | **basculer Clair/Sombre à l'exécution**, depuis le menu et les paramètres | un dessin enregistré avant/après : le nombre de commandes est **identique**, les couleurs **toutes** différentes |
| T2 | **éditer un rôle** avec application immédiate | modifier un rôle, redessiner : **exactement** les commandes qui utilisent ce rôle changent, pas une de plus |
| T3 | **enregistrer / recharger** un thème, avec le compte d'inconnus **affiché** | recharger un thème contenant 3 clés inconnues → l'interface affiche `3`, et les autres sont appliquées |
| T4 | **afficher le contraste** en continu, avec la pire paire nommée | un rôle poussé sous le seuil → l'avertissement apparaît et **nomme la paire** |
| T5 | **enregistrer les rôles propres à l'outil** (§ 1.3) | un thème d'une autre application se charge : ses rôles inconnus sont **comptés, pas rejetés** |

⚠️ **On édite un rôle, jamais un élément.** Une palette « libre » où l'utilisateur
poserait une couleur sur un élément particulier rouvrirait la porte de la valeur
en dur, du côté utilisateur.

---

## 5. Les langues — la section à lire deux fois

> **Un seul point existe** : la forme de déclaration décide que **le libellé EST
> la clé** (§ 5.3, point 4). **Aucun catalogue de traduction, aucune résolution,
> aucun changement à chaud, aucune invalidation de mesure** — ni dans NkUIDesign
> ni dans NKGui.

### 5.1 Où l'écrire — et ce n'est pas dans cette application

**Le foyer est NKGui.** Écrire un système de traduction dans NkUIDesign serait
exactement la faute que le dépôt vient de mesurer sur les peintres : la même
chose écrite deux fois, par deux mains, sans contact. **Toutes les applications
doivent en hériter.**

Ce que **NkUIDesign** écrit, lui : la **consommation** (§ 5.5) et
l'**exposition** (le sélecteur, la barre d'état, le compteur de clés manquantes).

### 5.2 ⚠️ Langue ≠ backend graphique

| ce qui change | effet |
|---|---|
| **le backend graphique** | **redémarrage** (§ 7) |
| **la langue** | **à chaud**, sans rien fermer |

Si votre implémentation demande un redémarrage pour changer de langue, elle est
**fausse**, quelle que soit sa propreté.

### 5.3 Les quatre points, et le second est celui qu'on rate

#### Point 1 — le texte passe par une clé

Aucun littéral figé dans le dessin. Un libellé s'écrit `T("panneau.palette.titre")`
(nom de fonction indicatif — c'est NKGui qui le fixe), jamais `"Palette"`.

**Témoin** : une recherche de chaînes littérales dans les fonctions de dessin de
l'application **doit rendre zéro** libellé affichable.

#### Point 2 — changer de langue **recalcule la mise en page**

C'est le point qu'une implémentation rate. Il ne suffit **pas** de changer la
chaîne rendue :

> **Tout cache de largeur de texte doit être invalidé au changement de langue.**
> Sinon on affiche la nouvelle langue avec **les mesures de l'ancienne** : rien
> ne plante, tout est décalé, et le défaut ne se voit qu'à l'œil.

Ce qu'il faut invalider, et la liste est à établir en la parcourant, pas de
mémoire :

- toute largeur de texte mémorisée par un widget entre deux images ;
- toute largeur de colonne calculée depuis le plus long libellé ;
- toute taille `content` du solveur d'agencement — **c'est le point de contact
  avec le § 2.5** : une taille « à son contenu » dépend du texte, donc de la
  langue ;
- toute troncature ou ellipse déjà décidée ;
- toute mise en cache d'atlas de police, si la nouvelle langue exige des glyphes
  absents.

**Le mécanisme à retenir** : un **numéro de génération de langue**. Chaque cache
de mesure retient la génération à laquelle il a été calculé ; le changement de
langue incrémente le numéro ; un cache d'une génération antérieure est recalculé.
C'est moins fragile qu'un parcours d'invalidation explicite, parce qu'un cache
ajouté plus tard et **oublié** dans le parcours se trahit tout seul.

##### ⚠️ LE TÉMOIN — sans lui, la chose n'est pas faite

**Une pseudo-langue d'essai** dont chaque traduction est **systématiquement plus
longue** que le français (par exemple : le texte français encadré et rallongé
d'un tiers). Elle ne demande **ni traducteur, ni seconde langue réelle, ni GPU** :
elle tourne dans la sonde, avec le peintre enregistreur.

```
1. dessiner l'écran en français              → enregistrer les rectangles
2. basculer sur la pseudo-langue longue      → redessiner
3. comparer
```

| résultat | ce qu'il signifie |
|---|---|
| **des largeurs changent** | l'invalidation fonctionne — c'est le résultat attendu |
| **aucune largeur ne change** | ❌ **le cache n'a pas été invalidé** : la nouvelle langue est affichée avec les mesures de l'ancienne |
| **le texte change mais pas la mise en page** | ❌ le même défaut, sous sa forme la plus trompeuse — l'écran *a l'air* traduit |

⚠️ **Et le contrôle négatif qui rend le positif lisible** : basculer sur une
pseudo-langue **de même longueur** doit produire **zéro** différence de
rectangle. Sans lui, on ne sait pas si les largeurs changent à cause de la
longueur ou à cause du simple fait d'avoir touché à la langue.

⚠️ **Ce que ce témoin ne prouve PAS**, et il faut l'écrire avec lui : les
métriques de texte du peintre enregistreur sont **fictives**. Il mesure donc que
**la chaîne de recalcul est branchée**, pas que le rendu réel est correct. Le
seul témoin de rendu exige une fenêtre ouverte — **différé, et rien ici ne le
remplace**.

#### Point 3 — une clé manquante **se voit**

Elle s'affiche **comme clé manquante** — le nom de la clé, marqué visuellement
(rôle `nkuid.cle_manquante`) — jamais comme un vide. Un libellé vide se lit comme
un bug de dessin ; une clé affichée se lit comme une traduction à faire, **et se
retrouve par recherche de texte**.

**Témoin** : demander une clé absente → la sortie contient le nom de la clé, et le
compteur de clés manquantes s'incrémente de 1.

#### Point 4 — **les interfaces produites sont traduisibles aussi** — ✅ décidé

Les libellés d'un composant déclaré sont des **clés**, pas du texte. Sinon
l'outil produit des interfaces monolingues.

✅ **C'est tranché dans la forme depuis le 18/08, et la décision est
contre-intuitive : aucun champ n'a été ajouté.** Poser une clé *à côté* du
libellé aurait créé **deux sources de vérité pour une même chose**.

> **Le libellé EST la clé.** Il n'y a rien à synchroniser parce qu'il n'y a qu'un
> champ.

Ce qui existe avec : un contrôle de **forme** de clé (pas d'espace, pas d'accent,
pas de majuscule) et un signalement **en note, jamais en erreur** pour un libellé
encore écrit en clair — rougir aurait cassé le travail d'un autre agent qui
n'avait rien cassé.

⚠️ **LA MIGRATION N'EST PAS FAITE, DÉLIBÉRÉMENT.** L'application affiche encore
les titres tels quels et **aucun catalogue n'existe dans NKGui**. Migrer
maintenant afficherait `content_browser.title` à l'écran à la place de
« Navigateur de contenu ». **L'ordre est : le catalogue d'abord, la migration
ensuite.** Ne l'inversez pas pour faire passer un témoin.

**Témoin, quand le catalogue existera** : sérialiser un document dont un nœud
porte un libellé, relire, vérifier que ce qui a été écrit est **une clé** ; puis
basculer la langue et vérifier que **l'aperçu du document** change aussi — pas
seulement l'interface de l'éditeur. **Ce second point est celui qu'on oublie** :
on traduit l'outil et on laisse le contenu en dur.

### 5.4 La police doit couvrir la langue

Le français passe partout ; une écriture non latine exige un atlas adapté. **À
dire plutôt qu'à découvrir** : au choix d'une langue, si la police ne la couvre
pas, l'interface le signale — elle ne dessine pas des rectangles vides.

### 5.5 Ce que NkUIDesign expose

| élément | où |
|---|---|
| sélecteur de langue, **effet immédiat** | menu Affichage **et** Paramètres |
| langue courante | barre d'état |
| compteur de clés manquantes | barre d'état, cliquable → la liste |
| pseudo-langues d'essai | Paramètres, section développeur — **elles restent dans le produit**, c'est le témoin permanent |

**Minimum exigé : français et anglais**, changeables sans redémarrer. Contexte :
~150 étudiants à la rentrée de septembre.

---

## 6. Les icônes

> **Rien n'existe.** NKGui n'a aucune notion d'icône. L'adaptateur de peinture
> actuel dessine, à la place, un **carré plein** du bon rôle et de la bonne
> taille — donc **la mise en page est déjà juste** le jour où l'atlas arrive.
> 193 glyphes sont définis **deux fois** dans le dépôt.

### 6.1 Les décisions déjà prises — à respecter, pas à rediscuter

1. **Vectorielles** — une icône est un jeu de **chemins**. Elle se recolore par
   jeton de thème et suit le facteur DPI.
2. **La rastérisation en atlas est une étape de SORTIE**, jamais la source.
3. **Le dessin se fait par poignée opaque** (un entier), **jamais par une
   énumération figée** : le vocabulaire d'icônes appartient au projet, pas à la
   bibliothèque. C'est déjà tenu dans le type de l'interface de peinture — donc
   **impossible à oublier à l'arrivée**.
4. **Une icône porte un RÔLE de couleur, pas une couleur.**

### 6.2 Ce que l'éditeur d'icônes doit fournir

| # | fonction | témoin |
|---|---|---|
| I1 | dessiner des chemins (trait, courbe, rectangle, cercle, booléens) | un chemin écrit → relu → **mêmes points** |
| I2 | aperçu simultané aux tailles réelles (16/20/24/32) | la sonde enregistre 4 rendus et vérifie qu'ils **diffèrent** (sinon la taille n'est pas honorée) |
| I3 | aperçu Clair **et** Sombre | idem T1 du § 4 |
| I4 | jeu d'icônes nommé, recherche, rôle de couleur par icône | un jeu écrit → relu → **même compte, mêmes noms** |
| I5 | import des glyphes existants | absorber les 193 doublons sans redessiner ; le témoin est le **compte** après import |
| I6 | export de l'atlas + **table de poignées** | § 9 |

⚠️ **Une icône n'est pas un composant** : ni rôle de capacité, ni événement, ni
variante. Ne la faites pas entrer dans la déclaration de composant.

---

## 7. Le backend graphique

### 7.1 Ce qui existe — à ne pas réécrire

| chose | forme |
|---|---|
| ligne de commande | `--gfx=auto\|opengl\|vulkan\|dx11\|dx12\|metal\|software` |
| environnement | `NK_GFX_API=…` |
| priorité | **ligne de commande > environnement > détection automatique** |
| **la résolution est une fonction PURE** (`Backend.h`) | le programme l'appelle, **et `--probe` vérifie la même fonction** |
| journalisation | *demandé* / *source* / **retenu** / *pourquoi* — **avant toute création de contexte** |
| refus | un backend indisponible **fait échouer le lancement avec la raison** |
| défaut | la **détection automatique** |
| taille réduite | `--small` = 1024×640, **le plancher réel de la coquille** — demander moins donnerait la même fenêtre avec un chiffre faux dans le journal |

⚠️ **« auto » ne se journalise JAMAIS comme retenu.** Il faut écrire ce qu'`auto`
a **effectivement donné** — sur Windows il vaut DX11 — sinon le journal répond
« auto » à la question « sur quoi ai-je mesuré ? », et ne répond donc rien.
**C'est la moitié de la règle**, et c'est la moitié qu'on oublie.

⚠️ **La ligne de journal s'assemble sans formateur.** Elle a déjà échoué en
silence une fois : le journal imprimait ses propres accolades au lieu des
valeurs (§ 0.5). **Une concaténation ne peut pas échouer en silence** ; un
formateur, si. Pour la ligne qui dit *sur quoi on mesure*, ce n'est pas un détail
de style.

⚠️ **`metal`** est **accepté à l'analyse** et **refusé à la résolution** :
l'énumération de la coquille n'a pas d'entrée Metal. Le taire reviendrait à
lancer autre chose en silence sur macOS. **Le manque est dans le kit**, il est
porté au canal — ne le corrigez pas ici.

### 7.2 📝 Ce qu'il reste : le changer **depuis l'interface**

Changer de backend **redémarre l'application**. Séquence à implémenter, dans cet
ordre exact :

```
1. PRÉVENIR       modale explicite : « nécessite un redémarrage »   [avant validation]
2. SAUVER         document + disposition + sélection + langue + thème
3. RELANCER       le même exécutable, avec --gfx=<choix>
4. RESTAURER      document, disposition, sélection
5. SI REFUS       le dire, nommer la raison, NE PAS rebasculer en silence
```

**Témoins** :

| # | témoin |
|---|---|
| B1 | un backend demandé et indisponible → le lancement **échoue**, le journal **nomme la raison**. ⚠️ Ce témoin doit **échouer** si quelqu'un ajoute un repli — c'est son seul intérêt |
| B2 | le backend retenu apparaît dans le journal **avant** la première ligne de création de contexte (ordre vérifié, pas présence) |
| B3 | après un redémarrage provoqué, le document rouvert est **identique octet pour octet** à celui d'avant |
| B4 | la barre d'état affiche le backend **effectivement retenu**, pas celui demandé |

⚠️ **B4 n'est pas cosmétique** : des heures ont été perdues à croire qu'on
mesurait Vulkan alors qu'on mesurait OpenGL. La barre d'état est ce qui rend ce
piège impossible sans ouvrir un journal.

---

## 8. Les panneaux et la coquille

### 8.1 Ce que la coquille porte déjà — appelez, n'écrivez pas

Ancrage et panneaux (ajouter, focaliser, fermer, détacher, maximiser, replier),
barre de titre + logo par poignée de texture, **palette de commandes**, barres
d'activité avec icônes par identifiant opaque, barre d'état, menus (barre,
Fichier, applicatif, panneaux, contextuel avec sous-menus), dépôt de fichiers,
**sauvegarde et chargement de la disposition**, géométrie de fenêtre,
réinitialisation.

### 8.2 La règle de lecture des panneaux — à préserver absolument

> **Aucun panneau ne connaît le nom d'un composant.** Palette, arbre, réglages et
> catalogue d'IA bouclent tous sur le **registre** ou sur les **tables de la
> déclaration**.

Le seul endroit du programme où un nom de composant est écrit en clair est la
table `nom → fonction de dessin` — parce qu'il faut bien appeler une fonction.

**Conséquence à vérifier après chaque ajout** : ajouter un paramètre à un
composant le fait apparaître dans l'éditeur **sans toucher au fichier des
panneaux**. Si vous avez dû l'éditer, vous venez de casser la propriété.

⚠️ **Les bornes des curseurs viennent de la DÉCLARATION**, jamais de l'éditeur.
Deux vérités pour une borne, c'est un utilisateur qui apprend une règle fausse.

### 8.3 📝 Le risque nommé : une table de dessin par hôte

Aujourd'hui : un hôte, deux entrées — ça ne coûte rien. À quatre hôtes et huit
composants, ce seront quatre tables à tenir, et **la troisième oubliera une
entrée** : le composant sera déclaré, visible dans toutes les palettes, et
invisible à l'écran dans une application. La piste retenue — une table
d'enregistrement d'exécution à côté du registre, qui laisse la déclaration
constante — **est du ressort du kit**, pas de l'application.

**Le garde-fou qui existe déjà** : un composant sans fonction de dessin peint un
**cartouche portant son nom et la mention qu'il n'est pas branché**, à la bonne
place. Ne remplacez jamais ce cartouche par un rectangle vide.

### 8.4 📝 Ce qui manque dans les panneaux

| # | manque | priorité |
|---|---|---|
| P1 | **annuler / rétablir** | 🔴 la plus haute — un outil de design sans annulation se manipule avec peur |
| P2 | remplir les **menus** (§ 5.5 du doc 1) | 🔴 |
| P3 | remplir la **barre d'état** (backend, sélection, compte, langue, modifié) | 🔴 |
| P4 | **enregistrer la disposition** entre sessions (la coquille sait le faire, on ne l'appelle pas) | 🟠 |
| P5 | le **titre** dit le document et l'état de modification | 🟠 |
| P6 | panneau **Comportement** (événements + graphe) | 🟠 |
| P7 | panneau **Icônes** | 🟠 |
| P8 | fenêtre **Aperçu interactif** | 🟠 |
| P9 | **écran d'accueil** | 🟢 |
| P10 | **gestionnaire de callbacks** | 🟢 |

⚠️ **Sur P8** : dans le panneau Aperçu, un clic **sélectionne un nœud** ; dans
l'aperçu interactif, le même clic **active le bouton**. Deux sens pour un même
geste — d'où deux fenêtres, et jamais un mode caché dans la même.

---

## 9. L'export

> **Rien n'existe.** ⚠️ `NkEditorExport.h` = macros de liaison. Piège de nom.

### 9.1 Les sorties, par ordre d'importance

1. **la déclaration** — le format natif, la seule qui compte vraiment ;
2. **le bloc de contrat** (événements et callbacks) — **il s'écrit déjà**, la
   sonde l'imprime ;
3. **l'atlas d'icônes** + sa table de poignées ;
4. **le thème** en texte ;
5. **les clés de traduction** du document ;
6. une **image**.

### 9.2 La contrainte dure

**L'aller-retour ne dégrade pas.** Écrit → texte → relu → **même mise en page**,
avec **zéro inconnu**. C'est déjà mesuré sur le document ; ça doit le rester pour
chaque sortie ajoutée.

### 9.3 Ce que l'export n'est pas

**Pas de génération de code.** Une sortie qui ne se relit pas est une voie sans
retour : ni reprise à la souris, ni continuation par l'IA. Le dialogue peut
**montrer** le bloc de contrat à recopier ; il ne produit pas l'implémentation.

### 9.4 Le rapport de validation — il s'affiche **toujours**

Composants déclarés non branchés · rôles annoncés non honorés · clés de
traduction manquantes · événements sans callback · nœuds sans composant (les
cadres — information, pas erreur).

⚠️ **Un rapport qui n'apparaît qu'en cas d'erreur n'apprend jamais à le lire.**

---

## 10. L'IA

### 10.1 Ce qui est livré : **la place**

Point d'entrée de prompt en français · backend **remplaçable** · sortie qui
atterrit dans l'arbre **par la fonction qu'utilise le copier-coller** ·
provenance remplie automatiquement · **rejeu** comme vérificateur.

⚠️ **Aucune intelligence n'est livrée, et aucune n'est revendiquée.** Le modèle
spécialisé s'entraînera sur des déclarations, et il n'y en a presque pas encore.

### 10.2 Les invariants à ne jamais casser

| # | invariant | ce qui arrive si on le casse |
|---|---|---|
| A1 | l'IA produit **la déclaration**, jamais du code | voie sans retour : ni reprise à la souris, ni continuation |
| A2 | la sortie passe par **la même porte** que la main | l'annulation, le collage et l'IA divergent ; « indiscernabilité » devient faux |
| A3 | le **prompt système est engendré** par les mêmes fonctions que l'écrivain du format | au premier ajout de mot, l'IA produit du texte que l'outil ne relit plus |
| A4 | la **provenance** se remplit toute seule | le corpus apprend la moyenne |
| A5 | une proposition qui **ne se rejoue pas** est écartée | *une paire fausse est apprise fidèlement* |
| A6 | l'IA reçoit le **document courant** | « continuer un travail » devient impossible |

**Témoin de A3** : comparer le vocabulaire du prompt et le vocabulaire de
l'écrivain — ils doivent être **engendrés de la même source**. Ce témoin existe
(essai 28b de la sonde).

⚠️ **Ce que le rejeu n'attrape pas, et qui doit être écrit avec lui** : une
interface **parfaitement bien formée et laide** passe sans broncher. Le rejeu
vérifie la forme, jamais le goût.

### 10.3 📝 Ce qui manque

- **aucun backend réseau.** Ne recopiez pas les sockets écrites à la main
  ailleurs dans le dépôt : ce serait une **troisième copie**, dans une
  application, alors que la directive est l'inverse. **Le client HTTP doit monter
  dans un module partagé** — manque porté au canal.
- le **backend FICHIER n'est pas un pis-aller** : il rend l'outil utilisable dès
  aujourd'hui avec n'importe quel modèle, y compris à la main.
- pas de conversation multi-tours, pas de flux, pas d'asynchrone.

---

## 11. La discipline des témoins

Cette section est la raison pour laquelle les chiffres du § 0.4 valent quelque
chose. **Elle s'applique à tout ce que vous ajouterez.**

### 11.1 Les trois contrôles qui rendent les autres lisibles

| contrôle | ce qu'il fait | pourquoi |
|---|---|---|
| **témoin de bruit** | deux passes identiques, **sans rien changer** | le peintre enregistreur est déterministe : le plancher attendu est **exactement zéro**. Vérifié, pas supposé |
| **contrôle positif** | un écrasement **connu** doit produire une différence | sinon on ne sait pas si l'instrument voit quoi que ce soit |
| **contrôle négatif** | une clé **inconnue** de la déclaration → **zéro** écrasement retenu, **zéro** différence | il prouve que la différence vient de la **déclaration**, et pas du simple fait d'avoir touché à l'instance |

### 11.2 Les pièges déjà rencontrés — ne les rejouez pas

1. **Le succès à vide.** Un essai vérifiait « la charge n'est pas vide » et
   **passait** parce qu'aucune charge n'avait été produite. **Une assertion sur
   une charge doit d'abord exiger qu'une charge existe.**
2. **Le point de clic en dur.** Un point d'essai figé tombait dans une colonne où
   rien ne se passait. **Le point se calcule depuis la déclaration.**
3. **Le binaire périmé.** Trois agents écrivent dans le même répertoire : *un
   exécutable a l'ancienneté d'une compilation, pas d'un fichier.* **Rebâtir
   avant de conclure**, surtout quand l'échec accuse le changement qu'on vient de
   faire — un résultat qui confirme ce qu'on craint mérite la même sévérité qu'un
   résultat qui nous disculpe.
4. **Le code sans témoin est celui qui casse.** Ce qui n'est atteignable que par
   le chemin exigeant un écran échappe à la sonde — et c'est exactement là que le
   défaut s'est logé. **Sortez-le en fonction pure**, faites-la appeler par le
   programme **et** vérifier par la sonde. (§ 0.5, défaut 1.)
5. **Une doublure qui dit oui à tout est un masque.** Le résolveur de rôles de la
   sonde acceptait n'importe quel nom : il ne pouvait donc pas voir un nom faux,
   et le composant se peignait en magenta à l'écran pendant que 68 essais
   restaient verts. **Toute doublure de résolution doit pouvoir rendre échec.**
   (§ 0.5, défaut 2.)
6. **Un témoin se vérifie PAR MUTATION.** Un essai vert ne prouve rien tant qu'on
   n'a pas réinjecté le défaut pour le voir rougir. C'est ce qui a été fait sur
   les nouvelles familles d'essais du backend — le défaut remis en place fait
   passer trois essais au rouge et la sonde rend un code d'échec.
7. **Le témoin séquentiel qui ne discrimine pas.** Vérifier qu'une échelle
   traverse en changeant la valeur puis en redessinant **passerait aussi avec une
   variable globale**. Le seul témoin qui discrimine est **simultané** — deux
   fenêtres à DPI différents au même instant. Il exige un GPU : **différé, et
   rien ne le remplace**.

### 11.3 Ce qui doit être écrit **avec** chaque résultat

Le **régime couvert** et le **régime non couvert**. Exemple de la formulation
attendue :

> *Couvert : 12 entrées, panneau 900×600, échelle 1.0, variantes `grid` et
> `dense_list`. Non couverts : liste vide, filtre actif, échelle ≠ 1 en
> simultané, panneau plus étroit qu'une carte, variante `columns` (déclarée,
> rendue comme `dense_list`). Les métriques de texte du peintre enregistreur sont
> **fictives** : le banc ne peut rien dire de l'ellipse, de la troncature ni du
> centrage.*

**Un régime non couvert écrit est une information ; un régime non couvert tu est
un piège.**

---

## 12. Le code produit — lisibilité

Critère de Rodolf, et il est **vérifiable** :

> **Quelqu'un qui n'a jamais vu ce fichier peut-il y ajouter un module sans
> demander à personne ?**

1. **Un en-tête de fichier** qui dit **pourquoi ce fichier existe** et ce qu'on
   vient y chercher — pas ce que fait chaque ligne.
2. **Le point d'extension est nommé et visible.** *« Où ajouter le composant
   suivant ? »* — une ligne de commentaire à cet endroit vaut dix pages ailleurs.
3. **Les décisions se commentent, pas les évidences.** *La raison d'un choix est
   ce qui empêche qu'on le défasse.*
4. **Les noms portent le sens** — français pour les libellés, conventions du
   dépôt pour les identifiants.
5. **Une organisation prévisible** : deviner le fichier avant de l'ouvrir.
6. **Ce qui est temporaire est marqué**, avec sa **condition de retrait**.

⚠️ **Et ça vaut d'abord parce que plusieurs agents écrivent en parallèle** :
quand quatre mains produisent une même bibliothèque, **la lisibilité est la seule
chose qui empêche quatre styles**.

---

## 13. Ordre d'implémentation recommandé

Cet ordre suit un principe : **d'abord ce qui rend l'outil utilisable sans peur,
puis ce qui le rend complet.**

### Palier 1 — l'outil se manipule sans crainte

| # | tâche | § |
|---|---|---|
| 1 | ✅ **fait** : la fenêtre s'ouvre. |  0.4 |
| 1a | 🟡 **le second regard — commencé, à moitié fait.** Les **deux composants** sont confrontés aux planches : **18 écarts comptés** (§ 0.6). Les **panneaux de l'éditeur** (palette, composition, propriétés, IA) ne le sont pas encore | 0.6 |
| 1b | ✅ **fait autrement que prévu, et c'est le point** : ce n'étaient pas 10 jetons mais **23 sur 23**, donc une classe de défaut et non une inattention. **La cause a été corrigée, pas les noms** — la résolution canonise, le repli est franc, et le résolveur permissif de la sonde a été supprimé. La correction **à la source** (déplacer la canonisation dans `NkRoleRegistry::Find`) est portée au canal en Q64 | 0.5 |
| 1d | ✅ **fait dans la nuit du 18/08 — le design des panneaux.** Réponse honnête à Rodolf : c'était **le mécanisme rendu visible**. Corrigé par des **conteneurs**, pas des ellipses — `BeginFlow` (boutons à la largeur de leur texte), `BeginChild` (arbre défilable), **contrôle segmenté** au lieu de 5 `Selectable` empilés, lignes **clé : valeur**, sections `CollapsingHeader`. Plus aucun libellé tronqué dans les panneaux | 0.6 |
| 1e | ✅ **fait — la config lue par défaut.** `--gfx` > `NK_GFX_API` > **`nkuidesign.cfg`** > détection ; prouvé **bout en bout** (fichier sur disque, journal qui nomme la source) et en sonde (famille 35, 6 essais). Un backend indisponible **nommé par le fichier démarre quand même**, le crie, et **la config n'est pas réécrite** ; nommé par `--gfx`, il refuse toujours | 7 |
| 1f | ✅ **fait — Préférences → écrire la config.** Prouvé **à la souris** : clic sur « Enregistrer », mtime changé, et le fichier **préservé à l'octet près** (commentaire manuscrit + clé inconnue intacts). Redémarrage **annoncé**. Écriture **atomique** (temporaire à côté puis renommage, aucun `.tmp` survivant), **relecture juste avant écriture**, clé commentée laissée commentée. Un fichier présent mais **illisible** est désormais un troisième état, qui **se dit** et **ne s'écrase pas**. Famille 37, 8 essais. ⚠️ Panneau et non catégorie de la fenêtre Préférences de la coquille : `DrawPreferences` est **privée** et le kit n'expose aucun point de greffe — porté au canal | 7.2 |
| 1i | ✅ **montré, et par mon propre instrument.** `designkit::UiRects` : l'interface **publie les rectangles qu'elle a dessinés** (`--dump-ui`) — l'équivalent, pour les panneaux, de ce que la famille 34 fait pour les composants. Clic visé par rectangle publié : `gfx = opengl` → `gfx = vulkan`, commentaire manuscrit et clé inconnue intacts. **Aucune coordonnée calculée, aucun balayage de pixels, aucune ligne dans NKGui.** Plus des raccourcis `Ctrl+1..4` (aucune coordonnée par construction) | 7.2 |
| 1j | ✅ **PROUVÉ — poser un composant depuis la palette et l'imbriquer.** Séquence entièrement souris + clavier, jugée sur le **document enregistré** : `Ctrl+J` (Palette) → clic `palette.composant.tree_view` → clic `palette.poser` → `Ctrl+K` (Composition) → clic `compo.imbriquer` → `Ctrl+S`. Résultat : Racine `enfants = 1 2`, Corps `enfants = 3 4 5`, le nœud 5 portant `composant = tree_view`. Deux causes réelles étaient dans **mon** code : ma `FocusPanel` réimplémentait `NkEditorShell::FocusPanel` (public, qui **ouvre** un panneau fermé — la mienne ne le faisait pas), et `Ctrl+1` ne se déclenche jamais (`TryRunShortcut` n'accepte qu'un nom de touche `NK_X`, quatre caractères) | 8 |
| ~~1j~~ | ~~**NON prouvé** — poser un composant depuis la palette et l'imbriquer.** Le scénario complet tourne, sortie 0, et **le document enregistré porte toujours ses 5 nœuds** : le clic n'a pas déclenché `Place()`. Piste probable *non vérifiée* : `Ctrl+1` n'a pas ramené la Palette au premier plan. ⚠️ **Ce qui manque à l'instrument** : publier **quel panneau est actif** — le registre dit où sont les rectangles, pas si le panneau qui les a produits est celui qu'on voit | 8 |
| 1k | ❌ **RETIRÉ — le défaut NKGui que j'ai signalé n'existe pas.** J'avais conclu de `1×28` / `0×28` que `nkgui::Selectable` publiait un rectangle faux hors rangée explicite. **Les trois relevés ont été pris pendant que le panneau n'était pas dessiné** (région de largeur 0). Panneau ouvert, le même `Selectable` en flot publie **120×28**. Rectifié au canal : *une mesure prise dans un état qu'on n'a pas vérifié n'accuse que celui qui la publie* | — |
| 1l | ✅ **le registre refuse de publier une cible inatteignable** : `UiRects::Note` ne publie rien quand la région du panneau fait moins de 4 px. Avant, un panneau caché publiait `palette.poser = 167,7×28` et l'essai cliquait **à travers**, sur le panneau qui occupait la place — en se croyant réussi | 8 |
| ~~1i~~ | ~~**non montré**~~ : le clic sur une ligne du contrôle segmenté des Préférences. Le bouton, l'écriture et la préservation sont prouvés ; la sélection à la souris ne l'est que par la sonde | 7.2 |
| ~~1f~~ | ~~**RESTE : Préférences → écrire la config**~~ + redémarrage **annoncé**. La moitié « fichier → démarrage » est faite, la moitié « interface → fichier » ne l'est pas | 7.2 |
| 1g | ✅ **fait — le chevron.** ⚠️ **Mais la mesure a d'abord démenti le diagnostic** : « pas de chevron = un arbre qui ne se plie pas » était une **déduction faite sur une image**, la troisième de la journée. La famille 36 mesure : le clic dans la zone du chevron **plie et déplie déjà**, sans aucune icône (`hitChevron` est géométrique). Le manque était **le signe, pas le geste**. Corrigé par `Icons.h` (six poignées + `NkDesignPaint`, qui surcharge `Icon` **et rien d'autre**) et **une ligne** dans `Renderers.h`. Vérifié à la souris : ▼ → clic → ▶ et les enfants disparaissent | 0.6 |
| 1h | ✅ **fait — le débordement de 8 px** (`NkContentBrowserDraw.cpp`) : `header.w` → `header.w - 2·card_pad`. Personne ne tenait ce fichier (`git status` des worktrees vérifié). La sonde passe de **8,0 px à 0,0 px**, et la quarantaine `34b` — écrite **monotone** (`<= 1`, jamais `== 1`) — a pu être resserrée à `== 0` sans jamais rougir | 0.6 |
| 1c | 🟡 **l'œil, le cadenas, l'icône de nature** — toujours des carrés (repli du kit), et c'est **assumé** : ce sont des *informations*, pas des commandes. On lit l'arbre sans elles ; on ne peut pas le plier sans savoir où cliquer. Attend l'atlas NKGui. *(anciennement « remplir `NkTreeViewIcons` », fait pour le chevron en 1g)* — cause commune de 3 écarts, et **un arbre sans chevron ne se plie pas**. Bloqué par l'absence d'atlas d'icônes (tâche 17) | 0.6 |
| 2 | **annuler / rétablir** | 8.4 P1 |
| 3 | **barre d'état** : backend retenu, sélection, compte, modifié | 7.2 B4 |
| 4 | **menus** + toute action aussi commande de palette | 8.4 P2 |
| 5 | **titre** = document + état de modification | 8.4 P5 |

### Palier 2 — thème et langue, les deux réglages visibles

| # | tâche | § |
|---|---|---|
| 6 | bascule **Clair/Sombre** à l'exécution + rôles propres enregistrés | 4 T1, 1.3 |
| 7 | **multilingue dans NKGui** — clé, génération, cache invalidé, clé manquante visible | 5 |
| 8 | **pseudo-langue longue** + son contrôle négatif, dans la sonde | 5.3 |
| 9 | **migrer les libellés vers des clés** — décidé, **volontairement non fait tant qu'aucun catalogue n'existe** : le catalogue (n° 7) passe avant, sinon on affiche des clés à l'écran | 5.3 point 4 |
| 10 | **changement de backend depuis l'interface** : prévenir, sauver, relancer, restaurer | 7.2 |

### Palier 3 — la description s'étend

| # | tâche | § |
|---|---|---|
| 11 | témoin **« les deux solveurs sont d'accord »** | 2.5 |
| 12 | **l'ancre `safe` / `edge`** — dans la déclaration **et** dans les deux solveurs, avec son témoin à deux moitiés | 2.6 |
| 13 | **profils d'appareil simulés** dans le panneau Aperçu (encoche, indicateur, rotation, clavier) | 2.6.4 |
| 14 | **décision de Rodolf** sur la charge collection — *rien ne bouge sans elle* | 3.5 |
| 15 | **promouvoir un sous-arbre en composant** | doc 1, § 9.8 |
| 16 | **consommer le gabarit répété** dans le dessin et l'aperçu — le champ existe et **la vérification le lit ; aucun dessin ne le consomme encore** | 3.5 |

### Palier 4 — icônes, comportement, export

| # | tâche | § |
|---|---|---|
| 17 | **éditeur d'icônes** + atlas + poignées | 6 |
| 18 | panneau **Comportement**, onglet Événements (la liste existe déjà) | doc 1, § 4.2 E3 |
| 19 | **graphe** de blueprint — le gros morceau | doc 1, § 9.7 |
| 20 | **aperçu interactif** | 8.4 P8 |
| 21 | **export** + rapport de validation | 9 |
| 22 | **éditeur de thème** | 4 |
| 23 | **écran d'accueil**, **gestionnaire de callbacks** | 8.4 |

⚠️ **Le calendrier n'est pas à l'agent** : la **rentrée de septembre passe
avant**. Cet ordre dit *dans quel ordre*, pas *quand*.

---

## 14. Ce qui ne se décide pas ici

1. **La charge collection** (§ 3.5) — décision de Rodolf.
2. **Le format de fichier définitif** et la convergence avec le format cible.
3. **Le rôle exact de l'IA spécialisée.**
4. **Le calendrier.**
5. **La charte visuelle** : deux sources divergent (les documents antérieurs du
   dossier parent contre les jetons du kit). **Deux specs qui se contredisent :
   on ne tranche pas**, la question monte. En attendant, **le code utilise les
   jetons du kit** — c'est la règle « aucune valeur en dur », qui, elle, ne
   dépend pas de l'arbitrage.
