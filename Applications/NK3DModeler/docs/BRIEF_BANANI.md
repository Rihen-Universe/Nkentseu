# Brief de conception d'interface — NK3DModeler

> Document **autonome**, à donner tel quel à Banani. Il ne suppose la lecture
> d'aucun autre fichier.
> Version du **2026-07-31**.

---

## 1. Le produit en un paragraphe

**NK3DModeler** est une application de **modélisation 3D** pour ordinateur
(Windows d'abord). L'utilisateur y crée et modifie des objets en trois
dimensions : il part d'une forme simple — cube, sphère, plan — puis la sculpte en
déplaçant des points, des arêtes et des faces, et en empilant des « modificateurs »
qui la transforment sans la détruire (arrondir, épaissir, dupliquer en série,
mettre en miroir…). Il organise son travail dans un projet contenant des
maillages, des animations, des matériaux et des textures.

**Public** : un créateur seul, qui connaît déjà ce type d'outil. Ce n'est pas un
logiciel grand public, mais ce n'est pas non plus un outil de laboratoire.

---

## 2. Direction visuelle — et la règle qui la limite

**Référence d'apparence : Unreal Engine 5.** Sombre, neutre, dense, professionnel.

> ⚠️ **Règle d'arbitrage, prioritaire sur tout le reste de ce document :**
> l'apparence d'Unreal Engine 5 est **purement cosmétique**. Unreal n'est pas un
> logiciel simple à prendre en main — il est réputé pour l'inverse. **Notre
> produit doit être facile à apprendre.** Chaque fois que la ressemblance à Unreal
> et la facilité d'usage se contredisent, **c'est la facilité qui gagne.**

### Ce que « facile » veut dire concrètement, en cinq règles vérifiables

1. **Tout icône a un mot.** Une barre d'outils d'icônes seules oblige à survoler
   pour deviner. Icône **+ libellé**, ou icône seule uniquement pour les cinq
   actions les plus évidentes (enregistrer, annuler, rétablir).
2. **Le rare est replié.** Les réglages avancés vivent dans une section fermée par
   défaut. La première ouverture ne doit pas montrer cinquante champs.
3. **Les états vides parlent.** Un panneau sans contenu ne montre pas du vide : il
   dit quoi faire (« Sélectionnez un objet pour voir ses propriétés »,
   « Aucun modificateur — cliquez sur Ajouter »).
4. **Cinq actions sans menu.** Ajouter un objet, changer de mode, ajouter un
   modificateur, enregistrer, changer l'affichage : atteignables en un clic
   visible, jamais enfouies.
5. **Une information, un endroit.** La même valeur ne s'affiche pas à deux
   endroits différents — sinon l'utilisateur se demande laquelle fait foi.

---

## 3. Écrans à produire

Trois maquettes, dans cet ordre de priorité :

| # | écran | ce qu'il doit montrer |
|---|---|---|
| **A** | **Mode objet** | disposition complète, un objet sélectionné, panneaux remplis |
| **B** | **Mode édition** | même disposition, l'objet ouvert, des faces sélectionnées |
| **C** | **Panneau de modificateurs** | zoom sur le panneau de droite, avec deux modificateurs empilés |

---

## 4. Disposition (contrainte forte)

Les **positions relatives** sont imposées. Les proportions, les marges et
l'ornement sont libres.

```
┌────────────────────────────────────────────────────────────────────────────┐
│ ①  logo   Fichier  Édition  Fenêtre  Outils  Sélection  Objet  Aide        │
│    ┌ Scène_01 ✕ ┐                                          MonProjet       │
├────────────────────────────────────────────────────────────────────────────┤
│ ②  [Enregistrer] │ [Mode ▾] │ [＋ Ajouter ▾] [＋ Modificateur ▾] │  [⚙]    │
├─────────────┬────────────────────────────────────┬─────────────────────────┤
│             │ ③  ┌ ☰ │ Perspective ▾ │ Éclairé ▾ │ ⑤  Propriétés        ✕  │
│ ④ HIÉRARCHIE│    └ Affichage ▾ ┘  [↖][✥][⟳][⤢]  │  [🔍           ] [⚙][🔒]│
│             │                     [⊞ 0,5][∠ 15°] │  Général  Objet  Tout   │
│ [🔍       ] │                                    │  ▾ Transformation       │
│ Nom    Type │                                    │    Position  X  Y  Z  ↩ │
│ ▾ Scène     │            VUE 3D                  │    Rotation  X  Y  Z    │
│   Cube  Mail│                                    │    Échelle   X  Y  Z  🔒│
│   Sphère Mai│                                    ├─────────────────────────┤
│ ▾ 📁 Groupe │                                    │ ⑥  DÉTAILS              │
│     Roue    │                                    │  ▾ Maillage             │
│             │                                    │  ▾ Modificateurs    [＋]│
│ 12 objets   │                                    │  ▾ Matériau             │
├─────────────┴────────────────────────────────────┴─────────────────────────┤
│ ⑦  NAVIGATEUR DE PROJET                                                 ✕  │
│  [＋ Ajouter] [Importer] [Tout enregistrer] │ ← → Tout ▸ Contenu ▸ Perso   │
│  ┌───────────┬──────────────────────────────────────────────────────────┐  │
│  │ ▾ MonProjet│ [🔍      ]  ●Maillage ●Animation ●Matériau ●Texture      │  │
│  │  Maillages │  ┌─────┐ ┌─────┐ ┌─────┐                                │  │
│  │  Animations│  │  ▣  │ │  ▣  │ │  ▣  │                                │  │
│  │  Matériaux │  │Cube │ │Tête │ │Bois │                                │  │
│  │  Textures  │  └━━━━━┘ └━━━━━┘ └━━━━━┘  ← barre colorée = TYPE        │  │
│  └───────────┴──────────────────────────────────────────────────────────┘  │
├────────────────────────────────────────────────────────────────────────────┤
│ ⑧  Sommets 8 · Arêtes 12 · Faces 6 · sél. 4 · actif : Cube · 60 ips         │
└────────────────────────────────────────────────────────────────────────────┘
```

### Ce qui est imposé

1. **④ Hiérarchie à GAUCHE**, et **seule** de ce côté.
2. **⑤ Propriétés au-dessus de ⑥ Détails**, à droite. Leur différence est une
   règle :
   - **Propriétés** = ce que possède **tout** objet (position, rotation, échelle,
     nom, visibilité). Contenu **fixe** — toujours au même endroit à l'œil.
   - **Détails** = ce qui est propre à **cet** objet-là. **Cette zone s'allonge**
     quand on ajoute un modificateur, un matériau, une propriété.
3. **③ La barre de la vue est DANS la vue**, en surimpression : navigation à
   gauche, outils de transformation et aimantation à droite.
4. **⑦ Navigateur en bas**, toute la largeur : arborescence à gauche, vignettes à
   droite.
5. **⑧ Barre d'état** toujours visible, avec les compteurs.
6. Les panneaux ont un **onglet avec une croix** et sont **déplaçables**.

---

## 5. Catalogue de composants

C'est la partie la plus utile : ces composants se répètent partout.

### 5.1 Ligne de propriété — **le composant central**

Deux colonnes. Libellé à gauche, valeur à droite, alignées verticalement d'une
ligne à l'autre. C'est ce qui rend un panneau lisible en balayage vertical.

```
  Largeur              │  0,050                          │ ↩
  Segments             │  1                              │
  Fermer le bord       │  ☑                              │
```

- La flèche `↩` n'apparaît **que si la valeur a été modifiée** par rapport au
  défaut. Elle la remet à zéro.
- Un cadenas `🔒` sur certaines lignes (échelle) lie les composantes entre elles.

### 5.2 Champ vectoriel X / Y / Z

Trois champs côte à côte. Chacun porte un **fin trait de couleur sur son bord
gauche** : **X rouge, Y vert, Z bleu**. Le fond du champ reste neutre — ne pas
colorer le fond, ce serait illisible et trop bruyant.

### 5.3 Section repliable

Triangle `▾` + titre en gras, empilées sans cadre. Une section peut porter un
bouton à droite de son titre (ex. `＋` pour ajouter un modificateur).

### 5.4 Puces de filtre

Rangée de petits boutons arrondis au-dessus d'une liste. L'actif est **bleu
plein**. Exemple dans Propriétés : `Général` `Objet` `Rendu` `Tout`.

### 5.5 Segmenté

Un seul choix parmi trois, collés : `Statique │ Stationnaire │ Mobile`.
L'actif est mis en avant.

### 5.6 Arborescence

Triangles de dépliage, **dossiers en jaune**, colonne « Type » atténuée à droite,
et un **pied de liste** qui compte (« 12 objets, 2 sélectionnés »).

### 5.7 Vignette d'élément (navigateur)

Carré avec un aperçu, le nom en dessous, et une **barre de couleur** en bas
indiquant le **type**. Même couleur que la puce de filtre correspondante.

### 5.8 Outil de la barre de vue

Petit bouton carré. L'actif est **bleu plein**. Groupés par famille, séparés par
un trait fin.

---

## 6. Couleurs

### 6.1 Base

Fond très sombre et **neutre** (gris presque noir, sans teinte). Panneaux à peine
plus clairs que le fond. Séparations = traits fins de 1 px, **pas d'ombres
portées**. Texte gris clair ; texte secondaire nettement plus sombre.

### 6.2 Deux couleurs, deux sens — **ne jamais les confondre**

| couleur | sens | où |
|---|---|---|
| **BLEU** | **état de l'interface** | outil actif, ligne sélectionnée dans une liste, filtre actif |
| **ORANGE** | **sélection dans la 3D** | contour de l'objet sélectionné dans la vue, éléments sélectionnés |

### 6.3 Trois états de sélection dans la 3D

| état | couleur |
|---|---|
| non sélectionné | noir |
| **sélectionné** | **orange** |
| **actif** (le dernier cliqué, celui qui sert de référence) | **blanc** |

Cette distinction vaut aussi dans les **listes** : la ligne active se distingue
des lignes seulement sélectionnées.

### 6.4 Axes

X rouge · Y vert · Z bleu. Partout : champs vectoriels, gizmo dans la vue,
repère en bas à gauche de la vue.

### 6.5 Types d'éléments du projet

Une couleur par type, **constante** entre la puce de filtre et la barre de la
vignette : Maillage · Animation · Matériau · Texture. Choisis-les distinctes et
lisibles sur fond sombre.

---

## 7. Le panneau de modificateurs (écran C)

C'est le panneau le plus important du produit. Il est **générique** : son contenu
est produit automatiquement à partir d'une liste de paramètres. **La maquette doit
donner cette impression** — pas de contrôle dessiné sur mesure pour tel ou tel
modificateur.

```
▾ Modificateurs                                       [＋ Ajouter ▾]
┌──────────────────────────────────────────────────────────────────┐
│ ▾  ◉  Subdivision de surface                   n° 1     ⋮    ✕   │
│      Niveaux                │  2                                 │
│      Simple (linéaire)      │  ☐                                 │
├──────────────────────────────────────────────────────────────────┤
│ ▾  ◉  Chanfrein                                n° 2     ⋮    ✕   │
│      Largeur                │  0,050  ────────                   │
│      Segments               │  1                                 │
└──────────────────────────────────────────────────────────────────┘
```

- **L'ordre compte** : les modificateurs s'appliquent de haut en bas. On doit
  pouvoir les **déplacer** (glisser, ou flèches ↑ ↓).
- **`◉`** = activé. Éteint, le modificateur reste **visible mais grisé** — le
  masquer ferait croire qu'il a été supprimé.
- **`⋮`** ouvre un petit menu (dupliquer, appliquer, copier).
- **`✕`** supprime.
- Un modificateur peut être **replié** sur son seul titre.

---

## 8. Textes

Interface en **français**. Les textes seront traduits plus tard : prévoir que
certains libellés seront **plus longs** dans d'autres langues (l'allemand ajoute
facilement 30 %). Ne pas dessiner des boutons calés au caractère près.

---

## 9. À éviter — ce sont des problèmes déjà rencontrés

1. **Fenêtres flottantes par défaut.** Elles se perdent derrière la vue.
2. **Information disponible seulement au survol.** Ce qui compte est visible.
3. **Un panneau de modificateurs dessiné sur mesure** par type de modificateur —
   il est générique, la maquette doit le montrer.
4. **Des icônes sans texte** dans les barres d'outils (voir § 2).
5. **Trop de choses ouvertes à la première ouverture.** Replier l'avancé.
6. **Confondre le bleu et l'orange** (voir § 6.2).

---

## 10. Ce qui est libre

Typographie · jeu d'icônes · densité exacte · rayons d'arrondi · traitement des
séparateurs · déclinaison **claire** du thème (l'application aura plusieurs
thèmes, dont un clair) · animation et transitions.

**Une maquette qui contredit ce document est une question à poser, pas une faute.**
Chaque règle ci-dessus répond à un problème constaté ; si l'une gêne visiblement,
c'est utile de le dire.
