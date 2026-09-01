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

### 0.3 La colonne d'état, et ce que chaque mot veut dire

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

Sources : `lunacy.docs.icons8.com/editing_shapes/`, `/tools/#types-of-points`,
`/shortcuts/`, et les notes de version `/rn_before_v10/` pour les modificateurs.

### 1.1 Entrer et sortir du mode

| comportement | geste | état | ce qui manque |
|---|---|---|---|
| Entrer en édition de forme | double-clic sur la forme, ou `Entrée` sur une forme sélectionnée | 🟡 **partiel** | le double-clic est livré et tenu (recettes points 1, 13, 33 ; document 2, 5) ; **`Entrée` n'ouvre pas encore le mode** — un raccourci à brancher, aucun modèle en jeu |
| Sortir | `Échap`, `Entrée` à nouveau, ou clic dans le vide | 🟡 **partiel** | `Échap` et le clic dans le vide sont livrés et tenus (cas 24, le correctif du blocage) ; **`Entrée` ne referme pas** — même raccourci manquant |
| Sortir par un bouton du panneau | « Finish », en accent dans `EDIT SHAPE` | ✅ **livré** | notre bouton **« Terminer »**. ⚠️ Il n'est pas en accent : `designkit::Button` n'a pas de variante accentuée, et écrire un second peintre de bouton à côté de celui du kit coûterait plus que ça ne rapporte |
| Le curseur devient la plume en mode édition | automatique | ❌ **absent** | `NkGuiCursor` n'a que 5 formes (flèche, texte, main, deux redimensionnements) — il faudrait une forme de plus **dans le socle** |

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
| Arrondir un coin | champ de rayon, actif **seulement** sur un point droit | ✅ **livré** | le champ « R » de notre section, **plus** un cycle 0/8/16/32 au double-clic sur la poignée (cas 21, 33) — une voie rapide que Lunacy n'a pas |

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
| Ouvrir / fermer un tracé | « Open Path » dans le panneau | ❌ **absent** | **le modèle ne porte pas la notion** : nos tracés sont fermés par construction (`NkContourDe` relie le dernier au premier). Il faudrait un booléen `ferme` par nœud, honoré au peintre et au round-trip. L'entrée existe, grisée, et le dit |
| Union / Soustraction / Intersection / Différence | `Ctrl+Maj+U` / `P` / `I` / `X` | ❌ **absent** | demande **deux choses** : un modèle de tracé à contours multiples, et un remplissage non convexe. Voir §1.5 — c'est le même maillon que la vectorisation |
| Vectoriser un texte (« Convert to path ») | menu contextuel | ❌ **absent, entrée posée et grisée** | voir §1.5 |

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

<!-- LES CHAPITRES DE COMPORTEMENT SUIVENT — remplis au fil de la lecture de la
     source. Toute nouvelle ligne s'ajoute dans SON chapitre, jamais en vrac. -->
