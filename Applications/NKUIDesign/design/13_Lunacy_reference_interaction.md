# Référence d'interaction — ce que Lunacy fait, ce que NkUIDesign fait

> **Décision de Rodolf, 2026-09-01 (nuit)** : *« vu que tu as même la doc de
> Lunacy, je te propose de la répliquer entièrement dans notre système pour le
> cas de l'édition de forme et tout. »*
>
> Ce document remplace un mode de travail. Jusqu'ici Rodolf **découvrait les
> manques un par un en testant**, et chaque retour coûtait un aller-retour. La
> colonne d'état ci-dessous **est** le carnet de ce qui manque : elle se lit
> d'un coup d'œil, elle se met à jour, et elle ne demande à personne d'ouvrir
> l'application pour savoir où l'on en est.

---

## 0. Ce qu'il faut savoir avant de lire

### 0.1 Périmètre et date

- **Source** : la documentation publique de Lunacy (Icons8),
  `lunacy.docs.icons8.com`, et son miroir de fichiers `icons8/lunacy-docs`.
- **Date de consultation : 2026-09-01.** ⚠️ **Une documentation évolue, et un
  document non daté vieillit sans le dire.** Toute ligne ci-dessous décrit
  Lunacy *tel que sa documentation le décrivait ce jour-là*. Une divergence
  constatée plus tard n'est pas forcément une erreur de ce document : c'est
  peut-être Lunacy qui a bougé. **On revérifie avant d'accuser.**
- **Ce qui est couvert** : l'édition vectorielle et ce qui l'entoure, dans
  l'ordre demandé — édition de forme, outils de tracé, sélection et navigation,
  transformations, propriétés, toile, calques et groupes, composants, et la
  table des raccourcis.

### 0.2 ⚠️ Comment ce document est écrit — et ce qu'il n'est pas

**On extrait des COMPORTEMENTS, dans nos mots. On ne recopie pas leur prose.**

Suivre des conventions d'interaction est légitime et c'est ce que fait déjà
NK3DModeler avec Blender : un utilisateur qui connaît un outil ne doit pas
réapprendre ses réflexes. Recopier textuellement la documentation d'un produit
commercial dans le dépôt, en revanche, exposerait Rodolf sans rien lui apporter.
Chaque ligne est donc **reformulée**, avec la référence de la page pour que
n'importe qui puisse vérifier. Les seules citations littérales sont **de
quelques mots**, là où le terme exact est ce qui compte — le nom d'une commande,
un raccourci.

En cas de doute : **on écrit moins et on note la source.**

### 0.3 🔴 LA RÈGLE DE LECTURE — le CONTEXTE d'abord, la contradiction en dernier

**Règle donnée par Rodolf, 2026-09-01 (nuit), et elle vaut pour quiconque lira ce
document ensuite** : *« est-ce que la contradiction dont tu parles ne
provient-elle pas soit de versions différentes, soit du panneau dans lequel elle
est exécutée, comme sur Blender ? »*

Il a raison, et sa formulation vaut mieux que la mienne. J'avais trouvé que
`Ctrl` « se contredisait » entre la toile et la Hiérarchie et conclu *« les deux
tables sont différentes par nature »*. C'est vrai, mais **ça décrit le symptôme.**
La cause est plus simple et plus utile :

> **Lunacy, comme Blender, a un keymap PAR CONTEXTE.** Chaque panneau — la toile,
> la liste des calques, l'éditeur de texte, le mode d'édition de forme — a sa
> propre table, et la même touche y signifie autre chose **légitimement**.

**D'où l'obligation qui traverse tout ce document : un raccourci se note avec son
CONTEXTE.** Un raccourci sans son panneau est une demi-information, et c'est
elle qui fabriquera de fausses contradictions plus tard.

**Et devant deux sources qui divergent, on classe AVANT de trancher, dans cet
ordre :**

1. **CONTEXTE DIFFÉRENT** — les deux ont raison, chacune dans son panneau. ⚠️
   **C'est le cas le plus fréquent et le plus piégeux, parce que les deux
   affirmations sont vraies.** On cherche celui-là en premier.
2. **VERSION DIFFÉRENTE** — l'une est périmée. Précédent mesuré dans ce dépôt :
   les maquettes Banani à huit menus contre l'écran 26 à neuf — ce n'était pas un
   désaccord, c'était une version.
3. **VRAIE CONTRADICTION** — seulement une fois les deux premières écartées.
   Alors on mesure, ou on demande. **Jamais avant.**

### 0.4 La colonne d'état, et ce que chaque mot veut dire

| état | ce que ça signifie |
|---|---|
| ✅ **livré** | le geste existe chez nous **et une recette le tient**. Pas « le code a l'air d'y être » — un cas de recette nommé. |
| 🟡 **partiel** | une partie du comportement est là ; la colonne « ce qui manque » dit laquelle ne l'est pas. |
| ❌ **absent** | rien chez nous. La colonne dit ce que ça demanderait au modèle. |
| 🚫 **écarté** | on ne le fera pas, et la raison est en §11. Ce n'est pas un oubli. |

⚠️ **« Livré » se mesure, il ne se déclare pas.** Ce document a été rempli en
relisant les recettes (`--recette-points`, `--recette-gestes`, `--recette-snap`,
`--recette-selection`, `--recette-transfo`, `--recette-document`) et en
inventoriant le code, pas de mémoire. Une ligne ✅ sans cas de recette derrière
serait exactement le genre de vert qui ment que ce chantier chasse depuis un
mois.

---

## 11. ⚠️ Ce qu'on ne suivra pas — et pourquoi

*(Cette section est en tête plutôt qu'en queue, parce qu'elle évite de lire les
autres avec une fausse idée du but.)*

**Lunacy est le PLANCHER, pas le plafond.** L'objectif n'est pas de devenir
Lunacy : c'est que rien de ce qu'un designer attend ne manque *sans qu'on le
sache*. Trois familles restent dehors, et chacune a sa raison écrite.

### 11.1 Ce qui suppose des PIXELS, quand nous décrivons des NŒUDS

Un `.nkuidoc` décrit un arbre de composants déclarés ; il ne contient pas
d'image matricielle. Tout ce qui suppose le contraire n'a pas de sens ici — et
c'est déjà écrit dans le menu contextuel pour deux entrées :

- **Crop** et **Rasterize selection** — supposent une image à découper ou à
  aplatir en pixels ;
- **l'export d'images** (PNG/JPG/WebP, tranches d'export, @2x/@3x) — notre sortie
  est un **document** que le moteur relit, pas une planche à livrer à un
  développeur. *L'export de NkUIDesign, c'est le `.nkuidoc` lui-même.*

*Ce qui n'a pas de sens chez nous se note, il ne s'ébauche pas.*

### 11.2 Ce que notre modèle DÉCLARATIF rend inutile

Lunacy positionne des calques en X/Y absolus. Chez nous **la position se
calcule** : un nœud déclare sa taille et l'agencement de ses enfants. Plusieurs
gestes de Lunacy n'ont donc pas d'équivalent — non par manque, mais parce que
la question ne se pose pas de la même façon (voir la nuance en §11.3).

### 11.3 ⚠️ Ce que NOUS avons et que Lunacy n'a pas — et qui reste

C'est le point que ce document ne doit surtout pas faire perdre de vue. Quatre
notions nous appartiennent, aucune n'existe chez Lunacy, **et aucune ne sera
sacrifiée pour lui ressembler** :

| notre notion | ce qu'elle fait | pourquoi Lunacy ne l'a pas |
|---|---|---|
| **l'agencement** (colonne / rangée / libre / ancré) | la position d'un enfant est un **résultat**, pas une saisie | Lunacy pose des calques, il ne les dispose pas |
| **l'ancrage** | ce qui tient quand la taille change | conséquence du précédent |
| **la cible** (Mobile 390×844, Bureau 1440×900…) et les **points de rupture** | un même document décrit plusieurs formats | Lunacy fige un artboard par taille |
| **les rôles de thème** et **les langues** | une couleur se nomme au lieu de se coder ; un texte a une version par langue | Lunacy écrit `#0969da` et une seule chaîne |

⚠️ **Et le quatrième a un coût dont ce document doit se souvenir** : la
vectorisation d'un texte (§2) **le sort du multilingue**. Chez Lunacy c'est une
conversion anodine ; chez nous elle détruit une capacité que Lunacy n'a pas. Le
même geste n'a pas le même prix dans les deux outils — c'est exactement pourquoi
on **extrait des comportements** au lieu de recopier un produit.

---

## 1. Édition vectorielle — le chapitre que Rodolf a nommé en premier

**CONTEXTE : le mode ÉDITION DE FORME.** Tout ce chapitre ne vaut que dans ce
mode ; hors de lui, les mêmes touches appartiennent à la toile (§4).

Sources : `lunacy.docs.icons8.com/editing_shapes/`, `/tools/#types-of-points`,
`/shortcuts/`, et les notes de version `/rn_before_v10/` pour les modificateurs.

### 1.1 Entrer et sortir du mode

| comportement | geste | état | ce qui manque |
|---|---|---|---|
| Entrer en édition de forme | double-clic sur la forme, ou `Entrée` sur une forme sélectionnée | 🟡 **partiel** | le double-clic est livré et tenu (recettes points 1, 13, 33 ; document 2, 5) ; **`Entrée` n'ouvre pas encore le mode** — un raccourci à brancher, aucun modèle en jeu |
| Sortir | `Échap`, `Entrée` à nouveau, ou clic dans le vide | 🟡 **partiel** | `Échap` et le clic dans le vide sont livrés et tenus (cas 24, le correctif du blocage) ; **`Entrée` ne referme pas** — même raccourci manquant |
| Sortir par un bouton du panneau | « Finish », en accent dans `EDIT SHAPE` | ✅ **livré** | notre bouton **« Terminer »**. ⚠️ Il n'est pas en accent : `designkit::Button` n'a pas de variante accentuée, et écrire un second peintre de bouton à côté de celui du kit coûterait plus que ça ne rapporte |
| Le curseur devient la plume, et la section `Edit shape` apparaît | automatique à l'entrée | 🟡 **partiel** | la **section apparaît** chez nous (livrée le 01/09, cas document 6) ; **le curseur ne change pas** — `NkGuiCursor` n'a que cinq formes (flèche, texte, main, deux redimensionnements) et il faudrait une forme de plus **dans le socle** |

### 1.2 Les points

| comportement | geste | état | ce qui manque |
|---|---|---|---|
| Déplacer un point | le saisir et glisser | ✅ **livré** | cas 11, 26 |
| Sélectionner plusieurs points | `Maj`+clic, ou glisser un rectangle par-dessus | 🟡 **partiel** | `Maj`+clic **livré** (cas 32) ; **l'élastique n'est pas livré** — il demande qu'un appui dans le vide du mode démarre un rectangle, alors que « cliquer dans le vide sort du mode » est la porte de sortie réparée le 01/09. Faisable en distinguant clic et glisser au relâchement ; **non fait tant qu'on n'a pas mesuré** qu'on ne rouvre pas le blocage |
| Déplacer plusieurs points ensemble | glisser l'un des sélectionnés | ✅ **livré** | cas 32bis — et l'écart, pas la position absolue |
| Ajouter un point | cliquer sur le tracé | ✅ **livré** | cas 20 — posé **sur** le côté, la forme ne bouge pas |
| Ajouter un point courbe d'emblée | double-clic sur le tracé (point miroir) | ❌ **absent** | notre double-clic sur le **tracé** n'est pas distingué du simple clic ; le modèle, lui, sait déjà faire un point miroir |
| Supprimer un point | le sélectionner puis `Suppr` | 🟡 **partiel** | `NkSupprimerSommet` **existe** et refuse sous trois sommets ; **aucun geste ne l'appelle** |
| Supprimer un point à la souris | `Alt`+clic | ❌ **absent** | même mécanisme, autre porte |
| Arrondir un coin | champ de rayon, actif **seulement** sur un point droit | ✅ **livré** | le champ « R » de notre section. La règle « seulement sur un point droit » est la nôtre aussi (`RayonActif`, cas 39) — **trouvée indépendamment, puis confirmée par la source** |
| Basculer un point droit ↔ miroir | **double-clic sur le point** | ⚠️ **divergence assumée** | chez nous, le double-clic sur une poignée **cycle le rayon** 0/8/16/32 (cas 21, 33). C'est ce que **Rodolf a demandé** le 01/09 (« si on double-clique sur une poignée sombre on peut l'arrondir »), et on le garde. Mais il faut le savoir : **le même geste ne fait pas la même chose dans les deux outils.** Le jour où l'on voudra la bascule de type, il faudra lui trouver une autre porte |

### 1.3 Les types de point et leurs poignées

⚠️ **La documentation d'interface en nomme QUATRE**, et ses captures du panneau
`Edit shape` en montrent quatre. La spécification du **format de fichier** en
liste deux de plus (`OnlyFrom`, `OnlyTo` — des points à une seule poignée), ce
qui explique probablement la rangée de six icônes ; **aucune page d'interface ne
les décrit**.

| type (nom Lunacy) | les deux poignées | état |
|---|---|---|
| **Straight** — droit | aucune poignée | ✅ **livré** (`LiaisonDroit`) |
| **Mirrored** — miroir | liées en direction **et** longueur | ✅ **livré** (cas 35) |
| **Asymmetric** | ⚠️ **même angle**, longueurs libres | ✅ **livré** (cas 35) |
| **Disconnected** | totalement indépendantes | ✅ **livré** (cas 35) |
| `OnlyFrom` / `OnlyTo` | une seule poignée | ❌ **absent, et non ébauché** — on n'a que leur nom, pas leur comportement décrit. *Implémenter un comportement dont on n'a que le nom, c'est inventer la moitié qui compte.* |

⚠️ **Le piège de vocabulaire, et il a failli coûter deux boutons identiques** :
chez Lunacy, **« Asymmetric » veut dire *même angle, longueurs différentes*** —
ce que Sketch appelle aujourd'hui « Mirror angle ». Pris dans son sens courant
(« les deux font ce qu'elles veulent »), il aurait reçu le comportement de
`Disconnected`.

| comportement | geste | état | ce qui manque |
|---|---|---|---|
| Tirer une poignée de courbe | la saisir et glisser | ✅ **livré** — la poignée gagne sur son sommet quand elles se recouvrent (cas 40) |  |
| Changer le type d'un point | la rangée d'icônes du panneau | ✅ **livré** — quatre boutons, appliqués à **tous** les points sélectionnés |  |
| Casser la liaison pendant le geste | `Alt` → déconnecté, `Ctrl` → asymétrique | ✅ **livré** (cas 40) | ⚠️ la source dit « create a … point », pas explicitement « en glissant la poignée d'un point existant » : c'est l'extension la plus proche de ce qu'elle décrit, et c'est **noté** au mécanisme |
| Escamoter une poignée d'un point déconnecté | ramener sa pointe sur le point | 🟡 **partiel** | le modèle l'exprime (tangente nulle = poignée absente) ; le geste marche mécaniquement, il n'est **pas tenu par un cas** |
| La rangée d'icônes montre des **icônes** | — | ❌ **absent** | nos quatre boutons portent des **mots** (« Droit / Miroir / Asym. / Libre ») faute d'atlas d'icônes vectorielles ; le mot est plus long mais il ne s'apprend pas |

### 1.4 Tracés ouverts, et opérations de forme

| comportement | geste | état | ce qui manque |
|---|---|---|---|
| Fermer un tracé | clic sur le point de départ, ou bouton **« Close path »** | ❌ **absent** | **le modèle ne porte pas la notion** : nos tracés sont fermés **par construction** (`NkContourDe` relie le dernier au premier). Il faudrait un booléen `ferme` par nœud, honoré au peintre et au round-trip |
| Ouvrir un tracé fermé | bouton « Open Path » | ❌ **absent** | ⚠️ **et la source ne le décrit pas.** Le bouton est bien visible sur la capture de Rodolf (`lunacy_2temps_edition_181745.png`), mais **aucune page de la documentation ne le mentionne** — elle ne documente que « Close path ». Notre bouton grisé porte donc le libellé de sa capture ; son comportement exact reste **à observer dans l'application**, pas à déduire |
| Union / Soustraction / Intersection / Différence | `Ctrl+Maj+U` / `P` / `I` / `X` | ❌ **absent** | demande **deux choses** : un modèle de tracé à contours multiples, et un remplissage non convexe. Voir §1.5 — c'est le même maillon que la vectorisation |
| Vectoriser un texte | **« Outline stroke »**, `Ctrl+Maj+O` | ❌ **absent, entrée posée et grisée** | voir §1.5. ⚠️ **Le nom que je croyais** (« Convert to path ») **n'est pas le bon** : chez Lunacy c'est *Outline stroke* qui convertit les bordures en vecteur **et** sert à vectoriser du texte. Notre entrée s'appelle « Vectoriser » — plus clair en français pour l'usage que Rodolf en décrit, et on assume l'écart plutôt que d'importer un nom qui parle de bordures |

### 1.5 ⚠️ Le maillon unique qui bloque trois lignes à la fois

Trois comportements ci-dessus (**booléens**, **vectorisation du texte**, tracés à
plusieurs contours) attendent **la même pièce**, et il vaut mieux le savoir une
fois que de le redécouvrir trois fois :

> **Notre peintre ne sait remplir qu'un polygone CONVEXE.**
> `NkGuiDrawList::AddConvexPolyFilled` le dit dans son propre en-tête : éventail
> depuis le premier sommet, *« non convexe : le résultat est faux »*.

Conséquence mesurée : presque toutes les lettres sont concaves (L, S, C, E, T…)
et beaucoup ont un trou (o, a, e, p, b, d, g, R…). Vectoriser aujourd'hui rendrait
**des taches à la place des lettres**. *Une capacité annoncée qui rend faux est
pire que son absence.*

### 🔴 ET CE N'EST PLUS SEULEMENT UN BLOCAGE FUTUR — C'EST VISIBLE AUJOURD'HUI

La capture `echanges/captures/preuve_courbe_bezier_n9.png`, prise le 01/09 après
la livraison des poignées de courbe, le montre : le **contour** suit parfaitement
la cubique, mais dès que la courbe rend la forme **concave**, le **remplissage**
déborde — l'éventail depuis le premier sommet passe à travers le creux.

⚠️ **Ça change la portée de §1.5, et il faut le dire tout de suite.** Ce n'était
pas « un maillon qui bloque quatre fonctionnalités à venir » : c'est **un défaut
atteignable dès maintenant**, avec un geste que Rodolf peut faire ce soir — tirer
une poignée de courbe vers l'intérieur d'un bouton. Les tangentes sont livrées et
justes ; c'est le peintre qui ne suit pas.

**La vague 4 monte donc en priorité** : elle n'ouvre plus seulement des
fonctionnalités, elle répare ce qu'on vient de livrer.

📌 **Et la pièce existe déjà dans le dépôt, un module plus loin** :
`NKFont/NkEarcut.h` et `NkFontMesh.cpp` triangulent des contours quelconques
**et** classent les trous par profondeur d'imbrication — écrits pour les
maillages de texte 3D. Le travail n'est donc pas « écrire un triangulateur »,
c'est **le faire descendre sous le peintre**.

📌 **Et les contours de glyphes sont déjà atteignables** :
`NkFont::GetGlyphOutlinePoints` rend les vrais tracés de la police, et le chemin
depuis l'application est complet (`costume::Fontes()` → `NkGuiFont::Face()` →
`NkFont`). Ce n'est pas « on n'a que des bitmaps ». Deux réserves à dire quand on
livrera : ce contour public est **aplati** à une finesse fixe (une lettre
agrandie dix fois montrerait ses segments — garder les tangentes demanderait un
accesseur additif sur `NkFont`), et la vectorisation **sort le texte du
multilingue** (§11.3).


---

## 1bis. Les divergences de la source, CLASSÉES selon la règle §0.3

**À lire avant tout chapitre qui cite un raccourci.** La lecture complète de la
documentation a trouvé des raccourcis **différents pour la même action** entre la
page `/shortcuts` et les pages de fond (`/layers`, `/basics`, `/interface`).

⚠️ **La première version de ce paragraphe titrait « sa page de raccourcis se
contredit ». C'était sauter à l'étape 3 de la règle** — et la règle existe
précisément pour ça. Reclassé :

### 1bis.a — CONTEXTE différent (les deux ont raison)

| divergence apparente | ce qui l'explique |
|---|---|
| `Ctrl+Maj+H` = *distribuer horizontalement* **et** *masquer* | **deux panneaux** : distribuer vit sur la **toile** (il faut une sélection multiple d'objets posés), masquer vit dans la **liste des calques**. Aucune ambiguïté à l'usage |
| `Ctrl+L` = *verrouiller* **et** *copier le lien du calque* | idem : l'un est un geste de **liste**, l'autre un geste de **document** |
| `Entrée` = *entrer en édition* **et** *valider une valeur* | **contexte de saisie** : dans un champ, `Entrée` valide ; sur la toile, il ouvre l'édition |

**Ce sont les plus nombreux, et aucun n'est un défaut.**

### 1bis.b — VERSION différente (l'une est périmée)

Plusieurs blocs de la source sont **commentés dans le fichier d'origine** et
n'apparaissent donc pas sur le site publié : la gestion des composants, leur
organisation par catégories, la distinction calques/cadres à la création d'un
composant. Ils décrivent vraisemblablement une interface d'avant la v.12. **On ne
les prend pas pour la réalité d'aujourd'hui.**

### 1bis.c — VRAIE divergence (ce qui reste après les deux tris)

Ce qui suit ne s'explique **ni** par le panneau **ni** par la version — ce sont
des valeurs différentes pour le même geste dans le même contexte :

| action | ce que dit `/shortcuts` | ce que dit la page de fond |
|---|---|---|
| aligner (gauche/droite/haut/bas/centres) | `Alt + A / D / H / W / S / V` | `Ctrl+Maj + flèches / - / \|` |
| miroir H / V | `Maj+H` / `Maj+V` | `Ctrl + ←→` / `Ctrl + ↑↓` |
| grille carrée | `Maj+G` | `Ctrl + '` |
| verrouiller / masquer | `Ctrl+Maj+L` / `Ctrl+Maj+H` | `/basics` donne `Ctrl+L` / `Ctrl+H`, **sans Maj** |
| mettre au premier plan | *absent du tableau* | `Ctrl+Maj+]` |

Plus des coquilles manifestes : `Maj+G` pour la grille sur Mac écrit avec une
apostrophe parasite, *Coller le style* qui reçoit le raccourci de *Copier le
style*, et *« Substract »* pour *Subtract*.

⚠️ **CE QUE ÇA CHANGE POUR NOUS** : *« suivre Lunacy » ne peut pas vouloir dire
« recopier sa table de raccourcis »* — il en reste, après les deux tris, un noyau
de valeurs réellement divergentes. Chaque raccourci que nous adopterons sera donc
**choisi**, avec la variante retenue et sa raison. Mais la leçon la plus utile est
celle de §0.3 : **la plupart des « contradictions » n'en étaient pas.**

---

## 2. Outils de tracé et leurs modificateurs

**CONTEXTE : la toile, un outil de tracé armé.** Les modificateurs de ce
chapitre (`Maj`, `Alt`) ne valent que **pendant** un tracé — ils signifient autre
chose au redimensionnement (§6) et en édition de forme (§1.3).

Source : `/tools`, `/shortcuts`.

| comportement | geste / raccourci | état | ce qui manque |
|---|---|---|---|
| Rectangle | `R` | 🟡 **partiel** | l'outil existe ; **le raccourci `R` n'est pas branché** (aucune touche de lettre n'est lue aujourd'hui) |
| Rectangle arrondi | `R` deux fois — l'outil **mémorise le dernier rayon** | 🟡 **partiel** | notre variante existe ; ni le cyclage par répétition, ni la mémoire du rayon |
| Ovale / Triangle / Polygone / Étoile | `O`, puis `O` répété pour cycler | 🟡 **partiel** | les quatre variantes existent ; **le cyclage par répétition de touche n'existe pas** |
| Ligne / Flèche | `L`, `L` répété | 🟡 **partiel** | idem |
| Plume (tracé point par point) | `P` | ❌ **absent** | c'est **l'outil de création vectorielle** : poser des points un par un, fermer ou non. Demande le tracé **ouvert** (§1.4) |
| Crayon (main levée) | `P` deux fois | ❌ **absent** | demande en plus un lissage de tracé |
| Sélection | `V`, ou `Échap` | 🟡 **partiel** | l'outil existe et `Échap` y ramène ; `V` n'est pas branché |
| Cadre (frame) | `F` ou `A` | 🟡 **partiel** | l'outil existe ; raccourci non branché |
| Poser une forme d'un simple clic, en **100 × 100** | clic sans glisser | ⚠️ **divergence** | chez nous un clic sans glisser donne une forme de **8 px minimum**, pas 100×100. À aligner — c'est une valeur, pas un mécanisme |
| Dimensionner à la volée | cliquer-glisser | ✅ **livré** | cas 27 |
| Depuis le **centre** | `Alt` | ✅ **livré** | cas 27 — vérifié à la source le 01/09 |
| **Proportions** conservées | `Maj` | ✅ **livré** | cas 27 |
| Les deux ensemble | `Alt`+`Maj` | ✅ **livré** | cas 27 |
| Contraindre une ligne à 0°/45°/90° | `Maj` en posant le second point | ✅ **livré** | cas 27 — et une ligne ne se contraint **pas** comme une boîte |
| Éditeur d'arc d'un ovale (*Sweep*, *Ratio*, *Start*) | poignées au centre, ou champs | ❌ **absent** | demande trois champs au modèle de l'ellipse ; c'est ce qui fait les camemberts et les anneaux |
| Nombre de branches d'une étoile / d'un polygone (*Count*) | poignée, ou champ | ❌ **absent** | notre étoile et notre pentagone ont un nombre **figé** dans la table unitaire |
| Ratio d'une étoile | poignées internes, ou champ | ❌ **absent** | idem |
| Arrondi sur la toile des formes prédéfinies | glisser les poignées rondes des coins | ❌ **absent** | nous n'avons que le champ et le double-clic (§1.2) |
| Arrondir **un seul** coin | `Alt` pendant le glissé de la poignée | 🟡 **partiel** | notre modèle porte **un rayon par sommet** (donc le cas est exprimable), mais il n'y a pas de poignée d'arrondi sur la toile |
| Coins lisses type iOS | bouton à côté du rayon | ❌ **absent** | une autre courbe de raccord ; le modèle porte le rayon, pas la famille de courbe |
| Extrémités d'un tracé ouvert (*Caps*) | section Bordure | ❌ **absent** | suppose le tracé ouvert |

---

## 3. Opérations booléennes et de forme

**CONTEXTE : la toile, sélection d'au moins deux formes.**

Source : `/editing_shapes`, `/shortcuts`.

| comportement | raccourci | état | ce qui manque |
|---|---|---|---|
| Union | `Ctrl+Maj+U` | ❌ **absent** | **le maillon de §1.5** : contours multiples + remplissage non convexe |
| Soustraction | `Ctrl+Maj+P` | ❌ **absent** | idem |
| Intersection | `Ctrl+Maj+I` | ❌ **absent** | idem |
| Différence | `Ctrl+Maj+X` | ❌ **absent** | idem |
| Le résultat reste **non destructif** — les formes restent modifiables individuellement | — | ❌ **absent** | ⚠️ **et c'est le point de conception qui compte** : un booléen chez Lunacy est un **groupe** portant une opération, pas une forme aplatie. Si on l'implémente un jour, c'est ce modèle-là qu'il faut, pas le résultat calculé une fois |
| Aplatir (*Flatten*) — fusionne définitivement | menu, **aucun raccourci** | ❌ **absent** | idem, version destructive |
| Vectoriser une bordure / un texte (*Outline stroke*) | `Ctrl+Maj+O` | ❌ **absent** | voir §1.5 |
| Masque (*Toggle mask*) | `Ctrl+M` | ❌ **absent** | demande une notion de masque au modèle et au peintre |

---

## 4. Sélection et navigation

**CONTEXTE : deux panneaux, et c'est le chapitre où la distinction compte le
plus.** La **toile** est spatiale (`Ctrl` y désigne la profondeur) ; la
**Hiérarchie** ne l'est pas (`Ctrl` y ajoute, comme dans tout explorateur). Nos
deux tables `NkGesteToile` / `NkGesteListe` sont **exactement** ce keymap par
contexte — pas une divergence à surveiller.

Source : `/layers`, `/tools`, `/basics`.

| comportement | geste | état | ce qui manque |
|---|---|---|---|
| Sélectionner | clic | ✅ **livré** | recette sélection 1 |
| Sélection multiple | `Maj`+clic | ✅ **livré** | cas 1, 3 |
| Élastique | glisser depuis le vide | ✅ **livré** | cas 7 — et il prend ce qu'il **touche** |
| Un clic sur un groupe prend **le groupe entier** | clic | ✅ **livré** | c'est notre `NkPickTopLevel` |
| Sélection profonde | `Ctrl`+clic | ✅ **livré** | cas 1. ⚠️ **Sur un SOMMET, `Ctrl` ne fait rien — et ce n'est pas un écart, c'est un contexte de plus** : le mode édition de forme a sa table (`NkGesteSommet`), comme la toile et la liste ont les leurs. **Trois contextes, trois tables : c'est la norme, pas l'exception** (§0.3) |
| Descendre dans un groupe | double-clic | ✅ **livré** | cas document 5 |
| Tout sélectionner | `Ctrl+A` | ✅ **livré** | recette gestes — **au niveau du forage**, pas tout le document |
| Sélectionner tous les cadres | `Ctrl+Maj+A` | ❌ **absent** | un geste, aucun modèle en jeu |
| Désélectionner | `Échap` | ✅ **livré** |  |
| Rendre la sélection profonde **permanente** pour un groupe (case à cocher) | panneau | ❌ **absent** | une propriété de nœud à ajouter |
| Sélectionner un calque **verrouillé** par la liste | — | ❌ **absent** | nous n'avons pas de verrouillage (§7) |
| Chercher un calque par son nom | champ au-dessus de la liste | ❌ **absent** | la Hiérarchie n'a pas de champ de recherche |
| Sélectionner tout ce qui utilise une couleur / une police | icône cible | ❌ **absent** | ⚠️ **et ce serait plus utile chez nous que chez eux** : nos couleurs sont des **rôles**, donc « tout ce qui porte ce rôle » est une question qu'on peut poser exactement |

---

## 5. Toile et vue

**CONTEXTE : la toile.**

Source : `/basics`, `/interface`, `/tips`.

| comportement | geste | état | ce qui manque |
|---|---|---|---|
| Défiler / déplacer la vue | molette ; `Espace`+glisser ; bouton du milieu | 🟡 **partiel** | le bouton du milieu et l'outil Main sont livrés ; **`Espace` maintenu n'est pas branché** |
| Zoom | `Ctrl`+molette | ✅ **livré** |  |
| Zoom fin | `Ctrl+Maj`+molette | ❌ **absent** | un pas plus fin, rien au modèle |
| Zoom 100 % / ajuster / sélection / largeur / hauteur | `Ctrl+0` / `1` / `2` / `3` / `4` | ❌ **absent** | cinq raccourcis à brancher ; le cluster de zoom existe déjà à l'écran |
| Outil Zoom (`Z`, `Alt+Z`) | maintenir puis cliquer/glisser | ❌ **absent** |  |
| Règles | `Ctrl+R` | 🟡 **partiel** | l'entrée existe **dans le menu du vide**, elle ne dessine pas encore de règle |
| Guides (créer, déplacer, supprimer) | glisser depuis la règle | ❌ **absent** | suppose les règles |
| Aimantation aux guides et aux objets | automatique | ✅ **livré** pour les **objets** | recette snap 1-11 — bords, centres, page, espacements égaux **et répétés**. ⚠️ **Nous allons plus loin que ce que sa doc décrit** sur ce point |
| **Désactiver** l'aimant pendant un glisser | `Ctrl` maintenu | ❌ **absent** | ⚠️ **manque utile et bon marché** : notre aimant s'éteint par le cluster ou le menu, pas au vol. C'est un modificateur, pas un modèle |
| Grille carrée | `Ctrl+'` (ou `Maj+G` — la source se contredit) | 🟡 **partiel** | notre grille de points existe et a son interrupteur ; ni taille de cellule, ni portée par cadre |
| Grille de mise en page | `Ctrl+\` | ❌ **absent** | l'entrée existe dans le menu du vide et **dit** qu'elle arrive |
| Grille de **pixels** au-delà de 500 % | automatique | ❌ **absent** |  |
| **Pixels au zoom** (rendu réel au-delà de 100 %) | menu Affichage | ❌ **absent** | l'entrée existe dans le menu du vide |
| Mode présentation | `Ctrl+.` | 🟡 **partiel** | l'entrée existe dans le menu du vide, l'effet non |
| Couleur de la toile | panneau, sans sélection | ❌ **absent** |  |
| Cadre précédent / suivant, page précédente / suivante | `Début`/`Fin`, `Pg↑`/`Pg↓` | ❌ **absent** |  |

---

## 6. Transformations

**CONTEXTE : la toile, sélection d'objets** (pas de sommets — pour les sommets,
voir §1).

Source : `/layers`.

| comportement | geste | état | ce qui manque |
|---|---|---|---|
| Déplacer | glisser | ✅ **livré** |  |
| Contraindre à un axe | `Maj`+glisser | ❌ **absent** | un modificateur, rien au modèle |
| Déplacer de 1 px / 10 px | flèches / `Maj`+flèches | ❌ **absent** | ⚠️ **le manque le plus courant de ce tableau** : c'est le geste d'ajustement fin, celui qu'on fait cent fois par heure |
| Position exacte | champs X / Y | ✅ **livré** | section Disposition |
| Redimensionner | poignées | ✅ **livré** | huit poignées |
| Proportions / depuis le centre / les deux | `Maj` / `Alt` / les deux | ❌ **absent** | les modificateurs existent **au tracé** (§2), pas au **redimensionnement** — le chemin frère est identifié, il n'est pas fait |
| Redimensionner de 1 px / 10 px | `Ctrl`+flèches / `Ctrl+Maj`+flèches | ❌ **absent** |  |
| Taille exacte, verrou de proportions | champs L / H | 🟡 **partiel** | les champs existent ; pas le verrou |
| Redimensionner un cadre **sans son contenu** | `Ctrl`+poignée | ❌ **absent** |  |
| Outil Échelle (met aussi bordures et effets à l'échelle) | `K` | ❌ **absent** | demande que bordures et effets sachent se mettre à l'échelle |
| Rotation à la souris | près d'un coin | ✅ **livré** | recette transfo 9 — quatre poignées **en dehors** des coins |
| Rotation chiffrée | champ d'angle | ✅ **livré** | section Apparence |
| Aimantation angulaire | `Maj` | ✅ **livré** | transfo 10 — **15°** chez nous, la source dit « 90° ou 45° ». ⚠️ **Divergence à trancher par Rodolf** : 15° est plus fin et couvre 45/90 ; leur formulation est ambiguë |
| Miroir H / V | boutons, ou raccourci (contradictoire dans la source) | ✅ **livré** | transfo 4, 13 — par les **boutons** de l'Inspecteur ; le raccourci n'est pas branché, et **on ne le branchera pas au hasard** vu la contradiction |
| Aligner (six alignements) | boutons ou raccourcis | ✅ **livré** | la rangée d'alignement existe |
| S'aligner sur les bords du **cadre** quand un seul objet est sélectionné | — | ❌ **absent** | ⚠️ **finesse utile** : chez eux, un objet seul posé sur un cadre s'aligne sur le cadre. Chez nous l'alignement veut deux objets |
| Distribuer H / V | `Ctrl+Maj+H` / `Ctrl+Maj+V` | ❌ **absent** |  |
| Ranger en grille (*Tidy up*) | `Ctrl+Alt+Maj+T` | ❌ **absent** |  |
| Contraintes de redimensionnement (épingler à un bord, figer une dimension) | section Contraintes | ✅ **livré autrement** | c'est **notre ancrage** (§11.3), et il est plus expressif : nous avons en plus l'agencement calculé |
| Mesurer la distance à un autre calque | `Alt` + survol | ❌ **absent** | ⚠️ notre aimant **écrit déjà les distances** pendant un glisser (snap 9-10) ; ce serait la même mesure, à froid |
| Dupliquer par glisser | `Alt`+glisser | ❌ **absent** | `Ctrl+D` existe |

---

## 7. Calques et groupes

**CONTEXTE : la Hiérarchie surtout, la toile pour quelques gestes.**

Source : `/layers`, `/basics`.

| comportement | geste | état | ce qui manque |
|---|---|---|---|
| Grouper / dégrouper | `Ctrl+G` / `Ctrl+Maj+G` | ✅ **livré** | recette gestes — et le groupe naît au **plus proche ancêtre commun** |
| Créer un cadre | `Ctrl+Alt+G` | 🟡 **partiel** | l'entrée existe, grisée, et le dit |
| Faire entrer / sortir un calque d'un groupe | glisser dans la liste | ✅ **livré** | Hiérarchie |
| Ordre de profondeur (4 gestes) | `Ctrl+]`, `Ctrl+Maj+]`, `Ctrl+[`, `Ctrl+Maj+[` | ❌ **absent** | l'entrée « Envoyer derrière » existe, grisée, et le dit |
| **Z-index automatique** (le petit passe au-dessus du grand) | automatique | 🚫 **écarté** | ⚠️ **et c'est une décision, pas un oubli** : un réordonnancement que l'utilisateur n'a pas demandé est exactement ce que ce chantier refuse. *Un outil qui range tout seul est un outil dont on ne prévoit pas le résultat.* |
| Verrouiller / masquer | icônes de la liste | ❌ **absent** | deux booléens par nœud, honorés au pointage et au dessin |
| Renommer | `F2` | ✅ **livré** | dans la Hiérarchie, et le double-clic sur l'étiquette d'une page |
| Dupliquer | `Ctrl+D` | ✅ **livré** | recette gestes — dans le **même** parent |
| Duplication répétée qui **rejoue le dernier décalage** | `Ctrl+D` répété | ❌ **absent** | ⚠️ joli comportement, bon marché : mémoriser le dernier décalage |
| Copier / coller | `Ctrl+C` / `Ctrl+V` | ✅ **livré** | et **un seul pas d'annulation** par geste |
| Copier / coller le **style** | `Ctrl+Alt+C` / `Ctrl+Alt+V` | ❌ **absent** | ⚠️ **très demandé en usage réel**, et notre modèle s'y prête (les remplissages, bordures et effets sont déjà des listes séparées) |
| Grille répétée (*Repeat grid*) | poignée en bas à droite | ❌ **absent** |  |
| Supprimer | `Suppr` | ✅ **livré** |  |
| Replier toute la liste | `Ctrl+~` | ❌ **absent** |  |
| Liste intelligente (n'affiche que le visible) | bouton | ❌ **absent** |  |
| Déplacer vers une autre page | menu contextuel | ❌ **absent** |  |
| **Recherche dans le menu contextuel** | champ en tête du clic droit | ✅ **livré** | nos deux menus contextuels ont leur champ de recherche |


---

## 8. Propriétés — remplissages, bordures, effets, texte

**CONTEXTE : le panneau droit (Inspecteur), sélection non vide.**

Source : `/styling`, `/text`, `/layers`.

### 8.1 La mécanique des sections de style

| comportement | geste | état | ce qui manque |
|---|---|---|---|
| Ajouter un remplissage / une bordure / un effet | `+` à droite du titre de section | ✅ **livré** | les trois listes existent (Q42) |
| En **empiler plusieurs** du même type | `+` à nouveau | ✅ **livré** | ce sont des listes, pas des champs uniques |
| Réordonner (l'ordre change le rendu) | glisser la poignée de gauche | ❌ **absent** | les listes existent, le glisser non |
| Masquer un réglage sans le supprimer | icône œil | ✅ **livré** | notre œil par ligne |
| Supprimer | corbeille | ✅ **livré** |  |
| Appliquer à une multi-sélection ; « **contenu mixte** » signalé | `+` écrase | 🟡 **partiel** | nos champs numériques disent déjà « — » en mixte (recette sélection 6) ; les **listes** ne le disent pas encore |

### 8.2 Remplissages

| comportement | état | ce qui manque |
|---|---|---|
| Remplissage **uni** | ✅ **livré** | ⚠️ chez nous il porte en plus un **rôle de thème** — voir §11.3 |
| **Dégradés** linéaire / radial / angulaire, avec étapes de couleur déplaçables | ❌ **absent** | trois familles au modèle, trois contrôles sur la toile, et un peintre qui sait les rendre |
| Remplissage **image** et ses quatre cadrages (remplir / ajuster / étirer / **mosaïque**) | ❌ **absent** | suppose une source d'image dans le document |
| **Pipette** pour prélever une couleur | ❌ **absent** | ⚠️ **et chez nous elle devrait prélever un RÔLE**, pas une valeur — sinon elle contourne le thème. Un cas où copier Lunacy tel quel serait une régression |
| Saisie HEX / RGBA / HSB / HSL | 🟡 **partiel** | HEX seulement |
| **Opacité par remplissage**, distincte de celle du calque | ✅ **livré** | le champ `%` par ligne |
| Règle de remplissage **non-zero / pair-impair** | ❌ **absent** | ⚠️ **c'est la clé des tracés à trous** — même famille que §1.5 |
| **Mode de fusion par remplissage** | ❌ **absent** | demande des modes de fusion au peintre |
| *Tints* (une teinte unique sur un groupe ou une instance) | ❌ **absent** |  |

### 8.3 Bordures

| comportement | état | ce qui manque |
|---|---|---|
| Couleur et épaisseur | ✅ **livré** |  |
| **Position** intérieure / centrée / extérieure | 🟡 **partiel** | le modèle porte la clé (`NkBordurePos`) ; **le peintre dessine tout au centre** — un champ déclaré qui n'agit qu'à moitié, à corriger |
| Extrémités (*caps*), jonctions (*folds*), têtes de flèche | ❌ **absent** | suppose surtout les tracés ouverts |
| **Pointillés** (tiret / écart) | ❌ **absent** | deux nombres au modèle, et un peintre qui sait pointiller |
| Un tracé ouvert n'accepte que la position « centrée » | ❌ **sans objet** | tant qu'il n'y a pas de tracé ouvert |

### 8.4 Effets

| comportement | état | ce qui manque |
|---|---|---|
| **Ombre portée** (couleur, décalage X/Y, flou, étendue) | ✅ **livré** | le modèle et la liste ; recette IA / round-trip |
| **Ombre interne** | 🟡 **partiel** | le type existe au modèle, le **peintre ne la rend pas** |
| **Flou gaussien** | ❌ **absent** | demande un vrai flou au peintre |
| **Flou d'arrière-plan** | ❌ **absent** | demande de lire ce qui est dessous — c'est-à-dire une passe de rendu de plus |

### 8.5 Texte

| comportement | geste / raccourci | état | ce qui manque |
|---|---|---|---|
| Police, graisse, taille | panneau | ✅ **livré** | section Typographie |
| Gras / italique / souligné | `Ctrl+B` / `I` / `U` | ❌ **absent** | ⚠️ notre modèle n'a **pas de mise en forme partielle** : un nœud texte porte une chaîne, pas des intervalles stylés. C'est un vrai choix de modèle à trancher, pas un raccourci à brancher |
| Interligne, interlettrage, espacement de paragraphe | champs | ❌ **absent** | trois nombres au modèle et au peintre |
| Couleur du texte | pastille | ✅ **livré** | et par **rôle** |
| Alignement horizontal (4) | boutons | 🟡 **partiel** | la clé existe ; le peintre n'honore pas tous les cas |
| Position verticale dans la boîte (3) | boutons | ❌ **absent** |  |
| **Ajustement automatique** : largeur libre / hauteur libre / taille fixe | 3 boutons | ❌ **absent** | ⚠️ **rapprochement intéressant** : c'est très proche de nos modes de taille (`expand`, `fixed`) — notre modèle en est déjà proche, il n'est pas branché sur le texte |
| Casse (Titre / MAJUSCULES / minuscules) | menu | ❌ **absent** |  |
| Décorations (souligné, barré), listes, exposant/indice | menu | ❌ **absent** |  |
| Texte **sur un chemin** | case à cocher | ❌ **absent** | suppose les tracés et une projection de glyphes |
| **Troncature** par points de suspension + nombre de lignes max | champs | ❌ **absent** | ⚠️ **manque très visible en maquette d'interface** : c'est exactement ce qui arrive à un libellé trop long dans une carte |
| Texte généré (contenu factice) | `Maj+R` | ❌ **absent** | ⚠️ **notre pont IA fait mieux et autrement** : il génère du contenu *à propos*, pas du lorem ipsum |

---

## 9. Composants et instances

**CONTEXTE : le panneau gauche (palette) et le panneau droit.**

Source : `/components`.

⚠️ **Chapitre à lire avec la note de conception `12_Dessiner_vers_Composant.md`**, écrite le 01/09 : elle avait déjà tranché deux points avant cette lecture, et la source les confirme.

| comportement | geste / raccourci | état | ce qui manque |
|---|---|---|---|
| Créer un composant | `Ctrl+Alt+K` | ❌ **absent** | ⚠️ **et la condition de déclenchement est déjà écrite** : le chantier commence le jour où **l'extraction ET le détachement** sont prêts tous les deux. *Un « créer un composant » sans « détacher » enferme l'utilisateur dans une décision qu'il ne peut pas défaire.* La source confirme que Lunacy livre bien les deux |
| Poser une instance | glisser depuis le panneau | 🟡 **partiel** | notre **palette** pose des composants **déclarés** (`content_browser`, `tree_view`) — mais ce sont des composants **de code**, pas des composants **de document**. Deux notions voisines à ne pas confondre |
| Surcharges acceptées par une instance | déplier l'instance | ❌ **absent** | la source les énumère : remplissages et images, contenu et propriétés de texte, styles, agencements, teintes, zones actives. ⚠️ **La note du 01/09 avait tranché « extraction par PROPRIÉTÉ, pas en bloc » — c'est exactement ce modèle-là** |
| Détacher | `Ctrl+Alt+D` → devient un groupe | ❌ **absent** | la moitié qui rend le reste acceptable |
| Réinitialiser les surcharges | bouton | ❌ **absent** |  |
| Le composant se propage à ses instances **sauf** là où une surcharge existe | — | ❌ **absent** | ⚠️ **c'est LA règle de fond** : une surcharge gagne sur la mise à jour. Sans elle, mettre à jour un composant écraserait le travail fait sur chaque instance |
| États d'un composant (Défaut / Survol / Pressé) | `Ctrl+Alt+P` | ❌ **absent** | ⚠️ **et nous avons déjà la notion, ailleurs** : nos composants déclarés portent des **états d'apparence** (§8ter du doc 3). Deux mécanismes voisins à réconcilier avant d'en écrire un troisième |
| Aller au composant principal / revenir | `Ctrl+Alt+E` | ❌ **absent** |  |
| Échanger le composant d'une instance | panneau ou icône | ❌ **absent** |  |
| Supprimer un composant → ses instances deviennent des cadres | menu | ❌ **absent** | une règle de dégradation à décider |

⚠️ **LE PROBLÈME QUE LA NOTE DU 01/09 AVAIT NOMMÉ ET QUE LA SOURCE NE RÉSOUT PAS** :
nos sommets sont **unitaires**, donc une instance redimensionnée **déforme** son
tracé. Juste pour une flèche, **faux pour un bouton à coins arrondis**. C'est le
9-slice, et il est réel dès la première instance étirée. Lunacy le contourne
avec ses *resizing constraints* ; nous avons l'ancrage, qui répond à la même
question. **À rapprocher le jour du chantier** plutôt qu'à réinventer.

---

## 10. Raccourcis clavier — notre keymap, et pourquoi ce n'est pas une copie

⚠️ **CE CHAPITRE NE RECOPIE PAS LEUR TABLE, ET C'EST UNE DÉCISION MOTIVÉE, pas
une prudence.** Trois raisons, dans l'ordre de force :

1. **leur table n'est pas cohérente avec elle-même** (§1bis) — recopier
   reviendrait à importer leurs contradictions ;
2. **une bonne moitié de leurs raccourcis désigne des outils que nous n'avons
   pas** (icônes, photos, illustrations, kits, greffons, autocollants,
   commentaires) : les réserver « au cas où » stérilise des touches ;
3. **notre application a des gestes qu'ils n'ont pas** — rôles, langues, cibles,
   ancrage, pont IA — et ceux-là auront besoin de touches.

**La règle qu'on se donne** : *on adopte le raccourci de Lunacy quand le geste
existe chez nous ET que la source est cohérente ; sinon on choisit, et on écrit
pourquoi.*

### 10.1 État des lieux, mesuré

🔴 **Aujourd'hui, l'application ne lit AUCUNE touche de lettre.** L'inventaire du
code ne trouve que quatre touches : `Échap`, `Entrée`, `Suppr`, `Retour arrière`.
Tous les raccourcis annoncés dans les menus (`Ctrl+G`, `Ctrl+D`, `Ctrl+Maj+G`…)
sont **affichés mais pas branchés** — ils décrivent le geste, ils ne le
déclenchent pas.

⚠️ **Et c'est un défaut de la famille que ce chantier chasse** : un menu qui
affiche « Ctrl+G » à côté d'une entrée promet un raccourci qui n'existe pas.
*Un libellé qui annonce une capacité absente est un libellé qui ment.* Deux
issues, et il faut en choisir une : brancher les touches, ou retirer les
libellés. **Brancher est évidemment la bonne** — c'est la première vague (§12).

### 10.2 Les raccourcis à brancher, par ordre d'utilité

| geste | touche retenue | pourquoi cette valeur |
|---|---|---|
| Déplacer de 1 px / 10 px | flèches / `Maj`+flèches | source cohérente, geste le plus fréquent qui manque |
| Redimensionner de 1 px / 10 px | `Ctrl`+flèches / `Ctrl+Maj`+flèches | idem |
| Outils : Sélection, Cadre, Rectangle, Ovale, Ligne, Texte | `V`, `F`, `R`, `O`, `L`, `T` | source cohérente ; **le cyclage par répétition** (`R,R`) vient après |
| Grouper / dégrouper / dupliquer / annuler | `Ctrl+G`, `Ctrl+Maj+G`, `Ctrl+D`, `Ctrl+Z` | déjà affichés dans nos menus, **déjà implémentés**, il ne manque que la touche |
| Zoom 100 % / ajuster / sélection | `Ctrl+0` / `Ctrl+1` / `Ctrl+2` | source cohérente |
| Pan | `Espace` maintenu | source cohérente |
| Désactiver l'aimant pendant un glisser | `Ctrl` maintenu | source cohérente, et bon marché |
| Entrer / sortir de l'édition de forme | `Entrée` | source cohérente ; `Échap` sort déjà |
| Ordre de profondeur | `Ctrl+]`, `Ctrl+[`, `Ctrl+Maj+[` | ⚠️ « au premier plan » **manque dans leur table** ; on prendra `Ctrl+Maj+]` par symétrie, et on l'écrit |
| Aligner | **à choisir** | ⚠️ **deux jeux contradictoires** dans la source (`Alt+A/D/H/W/S/V` contre `Ctrl+Maj`+flèches). À trancher par Rodolf, ou par l'usage |
| Miroir H / V | **à choisir** | ⚠️ même contradiction (`Maj+H`/`Maj+V` contre `Ctrl`+flèches) |

---

## 12. Les vagues d'implémentation — ce que la colonne d'état donne comme ordre

*Du plus utilisé au moins utilisé, en mettant devant ce qui coûte peu et se voit
beaucoup. Rodolf tranche l'ordre s'il le veut ; on n'attend pas pour les
évidences.*

**Vague 1 — brancher les touches.** Aucun modèle en jeu, du geste pur, et ça
supprime le mensonge des libellés de menu (§10.1). Déplacement au clavier,
outils par lettre, zoom, `Entrée` pour l'édition de forme, `Ctrl` qui suspend
l'aimant.

**Vague 2 — finir les gestes de la sélection.** Modificateurs au
redimensionnement (`Maj`, `Alt`), contrainte d'axe au déplacement, dupliquer par
`Alt`+glisser, ordre de profondeur, verrouiller / masquer.

**Vague 3 — finir l'édition de forme.** Élastique sur les sommets (avec la
mesure qui protège la porte de sortie), suppression d'un sommet par `Suppr`,
double-clic sur le **tracé** pour un point courbe, et la poignée d'arrondi sur
la toile.

**Vague 4 — le maillon de §1.5.** Faire descendre le triangulateur sous le
peintre. Il débloque **d'un coup** : la vectorisation du texte, les opérations
booléennes, les tracés à plusieurs contours et la règle de remplissage
pair-impair. C'est le meilleur rapport entre un travail et ce qu'il ouvre.

**Vague 5 — les tracés ouverts et la plume.** Ils vont ensemble : la plume est
l'outil qui les produit.

**Vague 6 — les propriétés qui manquent le plus en maquette** : troncature du
texte, ombre interne au peintre, position de bordure honorée, dégradés.

**Vague 7 — les composants de document**, quand extraction et détachement sont
prêts ensemble (§9).


---

<!-- LES CHAPITRES DE COMPORTEMENT SUIVENT — remplis au fil de la lecture de la
     source. Toute nouvelle ligne s'ajoute dans SON chapitre, jamais en vrac. -->
