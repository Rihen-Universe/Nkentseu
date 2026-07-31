# Barre d'outils de la vue 3D — inventaire et comportements

Référence : les captures Unreal Engine 5 fournies par Rihen le 31/07/2026.
Ce document décrit **chaque bouton**, ce qu'il fait, et ce qui reste à implémenter.
Il sert de contrat : le jour où on branche les interactions, on coche ici.

État au 31/07/2026 : **tout est dessiné, rien ne réagit**. Les colonnes « état »
disent donc où en est le *comportement*, pas le dessin.

---

## Groupe de GAUCHE — ce qu'on regarde

| bouton | rôle | comportement attendu | état |
|---|---|---|---|
| ☰ **Menu de vue** | tout ce qui ne mérite pas un bouton permanent | ouvre un menu : disposition des vues (1/2/4 panneaux), plein écran, caméras enregistrées, réinitialiser la vue | ⬜ |
| 📷 **Perspective** | projection | déroulant : Perspective · Dessus · Dessous · Gauche · Droite · Avant · Arrière. Les six vues orthographiques sont les **mêmes** que celles du gizmo de navigation — un seul état, deux façons d'y arriver | ⬜ |
| 💡 **Éclairé** | mode d'ombrage | déroulant : Éclairé · Non éclairé · Fil de fer · Détaillé (matériaux complets). C'est le pendant du `Lit / Unlit / Wireframe` d'Unreal et du `Solid / Material / Rendered` de Blender | ⬜ |
| 👁 **Affichage** | surimpressions | menu à **cases à cocher**, pas un déroulant à choix unique : grille · gizmos · contours de sélection · normales · statistiques · repère d'axes. Plusieurs peuvent être actifs ensemble, d'où les cases | ⬜ |

> **Pourquoi « Affichage » est un menu à cases et non un déroulant.** Un déroulant
> impose un choix unique. Or on veut couramment « grille **et** contours, sans les
> normales ». Le confondre avec les deux boutons précédents obligerait à rouvrir le
> menu trois fois pour trois réglages indépendants.

---

## Groupe de DROITE — ce qu'on fait

### Sous-modes de sélection *(mode Édition uniquement)*

| bouton | rôle | état |
|---|---|---|
| **•** Sommet | la sélection porte sur les sommets | ⬜ |
| **╱** Arête | sur les arêtes | ⬜ |
| **■** Face | sur les faces | ⬜ |

Ils **disparaissent** en mode Objet — non pas grisés mais absents : en mode Objet
il n'y a pas de sous-élément à sélectionner, la notion n'existe pas.
C'est l'inverse du bouton « Mode de sélection » de la barre principale, qui lui
est **grisé** : la commande existe toujours, elle est seulement sans effet ici.

### Outils — « que fait mon clic ? »

Un seul actif à la fois. C'est pourquoi ils sont **collés** : ils répondent tous à
la même question, et les séparer forcerait un aller-retour du regard entre deux
coins de l'écran pour un choix unique.

| bouton | rôle | comportement attendu | état |
|---|---|---|---|
| ⌖ **Sélection** ▪ | sélectionner | le **petit point en bas à droite** annonce un sous-menu : rectangle · cercle · lasso. Sans ce point, rien ne dit que le bouton cache un choix — convention d'Unreal comme de Blender | ⬜ |
| ✛ **Curseur 3D** | poser le point de pivot / d'insertion | clic = déplacer le curseur ; Maj+clic = le remettre à l'origine | ⬜ |
| ✥ **Déplacer** | translation | gizmo à trois flèches ; raccourci `G` | 🟡 gizmo existant dans NKRenderer |
| ⟳ **Tourner** | rotation | gizmo à trois anneaux ; raccourci `R` | 🟡 idem |
| ⤢ **Redimensionner** | échelle | gizmo à trois poignées ; raccourci `S` | 🟡 idem |
| 🌐 **Repère** | monde / local / normal / vue | déroulant. Change **l'orientation du gizmo**, pas la transformation elle-même — erreur classique à ne pas commettre | ⬜ |
| 📷 **Vitesse caméra** | sensibilité de navigation | déroulant 1→8, comme Unreal. Sans lui, une scène de 200 m se parcourt au pas | ⬜ |

### Aimantation — **une bascule PAR transformation**

C'est le point que Rihen a explicitement demandé, et Unreal fait pareil.

| bascule | valeur | ce qu'elle aimante |
|---|---|---|
| ⊞ **Grille** | `10` | les translations, sur un pas de grille |
| ∠ **Angle** | `10°` | les rotations |
| ⤢ **Échelle** | `0,25` | les facteurs d'échelle |

> **Pourquoi trois interrupteurs et non un seul.** Un interrupteur global obligerait
> à le couper pour tourner librement, alors qu'on veut garder la grille active en
> déplacement. Le besoin est réellement indépendant par type de transformation.

**Détail d'affichage retenu** : quand une aimantation est coupée, sa valeur reste
**affichée mais atténuée**. On veut savoir sur quel pas on retombera en la
rallumant ; la masquer obligerait à l'activer pour lire son réglage.

Le socle existe déjà côté moteur : `NkGizmo::SetSnapEnabled / SetSnapAbsolute /
SnapToGrid`, avec l'inversion par `Ctrl` et l'accumulateur `mDragFree` qui corrige
le blocage sur la première cellule. Il reste à exposer **trois** états au lieu d'un.

---

## Coin BAS-GAUCHE — se repérer et se déplacer

| élément | rôle | état |
|---|---|---|
| **Gizmo de navigation** | six boules, une par demi-axe. Positives pleines et lettrées, négatives creuses et muettes — c'est cette dissymétrie qui dit de quel côté on regarde. Clic sur une boule = vue orthographique alignée sur cet axe | 🟡 dessiné, clic ⬜ |
| 🔍 **Zoom** | glisser vertical = avancer/reculer | ⬜ |
| ✋ **Déplacer la vue** | glisser = translation latérale | ⬜ |
| 📷 **Vue caméra** | bascule vers la caméra de rendu active | ⬜ |
| ⊞ **Ortho / Perspective** | bascule rapide, sans passer par le déroulant | ⬜ |

Ils sont **au-dessus** du gizmo, comme chez Blender : on descend du plus abstrait
(l'orientation) vers le plus direct (zoom, déplacement). Les mettre dessous les
collait au bord de la fenêtre.

Et ils sont **séparés des outils** : les boutons de navigation déplacent la **vue**,
les outils changent ce que fait le **clic**. Les mélanger ferait changer d'outil en
croyant bouger la caméra.

---

## Coin BAS-GAUCHE (flottant) — panneau de dernière opération

Il n'apparaît **qu'après** une opération, et porte ses paramètres réglables au
chiffre près : on extrude d'abord, on ajuste ensuite. Il flotte au-dessus de la
scène et n'est pas encastré dans un bord — il appartient à la vue, pas au cadre.

Il se branchera sur `NkModParam` : mêmes paramètres nommés, mêmes bornes, même
rendu générique que le panneau Modificateurs. C'est le troisième consommateur de
ce mécanisme, après le panneau Modificateurs et les add-ons.

---

## Ce que ce document ne fixe pas

Les **raccourcis** : ils vivent dans `NkShortcutTable` et nulle part ailleurs.
Cette page cite `G`, `R`, `S` à titre indicatif ; la vérité est dans la table, et
c'est elle que l'interface affiche. Recopier un raccourci ici serait créer une
seconde source qui finirait par mentir.
