# NKGui — Roadmap

État : **Phase 1 — squelette** (voir `README.md` et `ARCHITECTURE.md`). Réécriture
destinée à remplacer NKUI, deux paradigmes (immédiat et retenu).

---

# MANDAT — Le socle d'interaction qui manque (2026-08-04)

> Rédigé depuis NK3DModeler, à la demande de Rihen, pour un agent qui n'y
> travaille pas. **Objet** : trois défauts structurels d'interface, rencontrés et
> corrigés à la main dans NK3DModeler, qui se reposeront dans toute application
> tant que le socle ne les traite pas. À vérifier **aussi dans NKCode**.
>
> NKGui étant encore un squelette, ces leçons ont vocation à entrer dans sa
> **conception**, pas à être rattrapées après coup.

## L'état des lieux, sans complaisance

Le socle existe déjà — en double, et les applications l'ignorent.

| Couche | Ce qu'elle porte | Qui s'en sert |
|---|---|---|
| **NKGui** | `NkGuiContext::NkInputLayerScope` (**priorité par couche**), `TextWrapped`, `Button`, `Checkbox`, `NkGuiDrawList`, `NkGuiInput` | **NKCode** |
| **NKEditorKit** (`Engine/`) | `NkEditorCombo`, `NkEditorContextMenu`, `NkEditorModal`, `NkEditorPanel`, `NkEditorTooltip`, `NkEditorTextField`, `NkEditorScrollbar` | partiellement |
| **NK3DModeler** | `NkModelerWidgets.h` (Combo, DragFloat, EditableText), `NkModelerUI.h` (painter), `NkHitRegistry` (**LayerScope réécrit**), `TextWrap` (**réécrit**) | lui seul |

**Le constat, vérifié le 4 août 2026 :**

- **NKCode utilise NKGui** — `NkGuiContext::NkInputLayerScope(ctx, 50)` dans son
  panneau IA, `(ctx, 100)` dans ses commandes. Il consomme le socle.
- **NK3DModeler a tout réécrit** — `NkHitRegistry::LayerScope`, son propre
  painter, ses propres widgets. Il ne consomme rien.

Les deux briques que le modeleur a réécrites **existaient déjà dans NKGui** :
la priorité par couche et le texte qui va à la ligne. C'est donc bien un
**contournement**, pas une lacune du socle.

Nuance à ne pas perdre : la version NKGui n'a peut-être pas tout ce dont le
modeleur a besoin (le report d'une image pour les surcouches peintes *après* le
panneau qu'elles couvrent, notamment — voir défaut 1). **La première tâche est
donc de comparer les deux implémentations**, pas de supposer que l'une remplace
l'autre. NKCode, qui vit déjà sur NKGui, est le bon témoin : s'il ne souffre pas
des quatre bugs listés plus bas, la version NKGui suffit et le modeleur n'a qu'à
migrer. S'il en souffre en silence, le socle est à compléter.

## Défaut 1 — La priorité d'interaction n'est pas dérivée de l'ordre de peinture

**Le plus coûteux des trois.** Aujourd'hui, trois mécanismes complémentaires
doivent être câblés **à la main dans chaque panneau** :

- `NkHitRegistry::LayerScope(hit, 50)` — monter les surcouches d'une couche ;
- `hit.SetBlock(rect, on)` — ignorer les clics d'une emprise ;
- `st.UiBlockAdd(rect)` / `st.UiBlocks(mx, my)` — emprise mémorisée d'une image
  sur l'autre, pour les surcouches peintes *après* le panneau qu'elles couvrent.

Quatre bugs le 4 août 2026, tous de la même famille :

1. le menu d'en-tête de groupe peint sur la couche du panneau : clics inopérants
   et menu qui ne se refermait pas ;
2. le panneau de l'édition proportionnelle laissait passer ses clics ;
3. la liste déroulante du format de sortie : « je ne peux choisir aucun format » —
   le panneau Propriétés ne consultait pas `UiBlocks`, alors qu'il porte le plus
   de listes de toute l'application ;
4. **ma propre correction du point 3** : en armant le blocage, j'ai neutralisé la
   liste elle-même, peinte après. D'où la règle qui manquait, à inscrire dans le
   socle : **le blocage protège ce qui est dessous, jamais ce qui est au-dessus.**

**Directive.** Dans un socle correct, l'ordre de peinture définit la priorité
d'interaction, sans rien à armer. Ce qui est peint plus tard capte le clic en
premier ; le reste en découle. Si le paradigme immédiat impose de connaître les
emprises avant de les peindre, alors le report d'une image doit être **interne au
socle** — jamais une responsabilité de l'appelant.

## Défaut 2 — Les surcouches différées gardent des pointeurs vers la pile

`NkComboPending` (NK3DModeler) conserve `const char *const *items` **et**
`int32 *selected` pour peindre la liste plus tard, hors de la portée qui l'a
créée. Deux bugs distincts en sont sortis le même jour :

1. **Pointeurs morts** — un tableau de libellés ou une variable de sélection
   *locale* est détruit avant que la liste ne soit peinte. Symptôme : le combo
   s'ouvre, choisir ne change rien (au mieux) ou l'application plante (au pire —
   le code du modeleur documente déjà ce second cas pour les items).
2. **Deux sources concurrentes** — comparer la sélection mémorisée à la vérité du
   moteur ne suffit pas : quand c'est le *moteur* qui a changé la valeur, l'écart
   se lit comme un choix de l'utilisateur et l'ancienne valeur est réappliquée.
   Il faut mémoriser la dernière valeur **vue** du moteur, et lui donner la
   priorité. Symptôme observé : un échange principale/miniature s'appliquait puis
   s'annulait à l'image suivante.

**Directive.** Une surcouche différée ne doit **jamais** détenir de pointeur vers
la pile de l'appelant. Deux voies acceptables : copier les libellés et la valeur
dans le socle, ou fonctionner par **identifiant** (le socle rend le choix,
l'appelant l'interroge). La seconde est préférable en mode immédiat.

Corollaire, à documenter dans le contrat : quand une valeur peut changer des deux
côtés (interface et modèle), le socle doit rendre explicite **qui a la priorité**.
Un simple `if (a != b)` ne peut pas le savoir.

## Défaut 3 — Le texte n'est pas contraint par son conteneur

Le système de groupes de NK3DModeler cadre les **widgets** — leurs rectangles se
calculent depuis la largeur du groupe — mais pas les **chaînes**, qui se peignent
où on leur dit. Un libellé plus large que la colonne débordait tel quel.

Ce n'était pas un défaut du système de groupes : c'était une brique manquante.
`TextWrap` (retenu : coupe aux espaces, place seul un mot trop large, renvoie la
**hauteur consommée** pour que l'appelant avance son curseur sans compter les
lignes) a été ajouté au painter du modeleur — alors que **`NKGui::TextWrapped`
existait déjà**.

**Directive.** Vérifier que `TextWrapped` couvre ces cas, et que le socle expose
une notion de **conteneur** dont le texte hérite — pas seulement un `wrapWidth`
que chaque appelant doit calculer.

## Ce qui ne relève PAS de ce mandat

À dire explicitement, pour ne pas élargir le périmètre : plusieurs bugs de la même
session **ne sont pas** des défauts d'interface, et aucun socle UI ne les aurait
évités.

- `NkImage::Free()` libère l'objet (`nkFree(this)`) et non ses pixels : appelée
  sur un objet statique, elle fermait l'application. `Unload()` est la méthode qui
  vide. **Nommage**, pas interface.
- Un identifiant de **nœud** passé à une fonction qui attend un index d'**objet** :
  deux espaces d'indices, aucune erreur de compilation, un nom pris au hasard
  (« mur gi » pour une caméra). **Typage métier.**
- Une table de libellés indexée par entier restée à 6 entrées quand une 7ᵉ section
  est apparue : l'en-tête n'affichait plus de nom. **Une table unique décrivant
  les sections** l'aurait évité — pas un socle UI.

## Ordre proposé

1. **Comparer `NkGuiContext::NkInputLayerScope` (NKGui, utilisé par NKCode) et
   `NkHitRegistry::LayerScope` (réécrit par NK3DModeler).** Le second gère en
   plus `SetBlock` et le report d'une image (`UiBlockAdd`/`UiBlocks`) : vérifier
   si le premier en a l'équivalent, et **tester NKCode sur les quatre cas du
   défaut 1** — un panneau dont une liste déroulante recouvre d'autres widgets
   cliquables. S'il y résiste, le modeleur n'a qu'à migrer.
2. **Défaut 1 dans NKGui** — priorité dérivée de l'ordre de peinture. Le plus
   structurant, et celui qui doit être décidé avant que l'API se fige.
3. **Défaut 2** — contrat des surcouches différées, sans pointeur vers la pile.
4. **Défaut 3** — vérifier `TextWrapped`, exposer la notion de conteneur.
5. **Migration progressive**, jamais en bloc : NK3DModeler et NKCode fonctionnent.
   Faire remonter une brique à la fois, en commençant par celle qui coûte le plus.

## Pour l'agent qui reprend

Les cas réels sont consignés, avec leur cause et leur correction, dans
`CARNET.private.md` à la racine (vagues 40 à 42) et dans
`Applications/NK3DModeler/ROADMAP.md`. Ils valent mieux qu'une description
abstraite : chacun a un symptôme observable et une cause identifiée.

Contrainte de travail du dépôt : **zéro STL**, français dans les commentaires,
build Debug **et** Release vérifiés à 28/28.

---

# CHANTIER « COMPLÉTER NKGui » — inventaire mesuré (2026-08-18)

> Agent NKGui, branche `feat/nkgui-complet`. **Périmètre déclaré :
> `Kernel/Runtime/NKGui/` uniquement** (+ le banc témoin dans
> `Applications/NKGuiDrawTest/`). NKEditorKit et les applications ne sont **pas**
> touchés : un autre agent y travaille au même moment.
>
> **Contrainte de séance : aucun GPU** (campagne d'entraînement en cours). Tout
> ce qui suit est donc vérifié par un **témoin non visuel** — un banc qui appelle
> l'API et mesure la géométrie produite. Le rendu à l'écran reste **à confirmer
> par un témoin visuel, différé et nommé** (voir « Ce qui reste » ci-dessous).

## La méthode : ce que les applications émulent = ce qui manque

On n'a pas listé les widgets qu'une bibliothèque « devrait » avoir. On a cherché
**ce que les applications réécrivent localement faute de l'avoir**. Un helper de
dessin local est une absence de NKGui **qui a déjà coûté deux fois**.

### Mesure 1 — les émulations dupliquées

| fichier | lignes | ce qu'il émule |
|---|---|---|
| `Applications/Mou/src/Mou/UI/MouDraw.h` | 155 | `CircleOutline`, `RectOutline` (arrondi) |
| `Applications/Nkoung/src/Nkoung/UI/NkoungDraw.h` | 155 | **le même fichier** |
| `Applications/ConquerorLab/src/ConquerorLab/NkcDraw.h` | 145 | `NkcRing`, `NkcPolyFilled`, `NkcPolyOutline(Inset)` |
| `Applications/NKCode/src/NKCode/Editor/NkTextDraw.h` | 400 | glyphes de repli, réparation mojibake (**spécifique NKCode, reste local**) |

**MouDraw.h et NkoungDraw.h sont identiques à 5 lignes près** — le nom du
fichier, le namespace, la garde d'inclusion. 150 lignes écrites deux fois : c'est
la mesure la plus nette de l'absence.

Portée de ces émulations : **129 appels** de `CircleOutline`/`RectOutline`
répartis sur Pong, Pong2, Mou, Nkoung, Songoo, NkImageDemo.

### Mesure 2 — le contour arrondi, absence la plus coûteuse

`AddRectFilled` sait arrondir depuis toujours ; `AddRect`, son contour, ne le
pouvait pas. Conséquence mesurée sur tout le dépôt :

- **252 appels** de `.AddRect` dans 40 fichiers ;
- **185** d'entre eux sont **voisins immédiats d'un `AddRectFilled` arrondi** —
  c'est-à-dire un fond aux coins ronds cerclé d'un cadre carré ;
- dont **29 des 41 appels de NKGui lui-même** (`NkGuiWidgets.cpp`). **La
  bibliothèque souffrait de sa propre absence** — c'est ce qui a fixé la priorité.

### Mesure 3 — couverture des jetons de thème

Question posée : le pied de page du kit (5 couleurs tirées du thème) est-il
l'exception ou la règle ? **Réponse : ni l'un ni l'autre — c'est la moitié.**

- **426 couleurs écrites en dur** chez les consommateurs NKGui ;
- **555 lectures de jetons** ;
- soit **56,6 % de couverture**.

Les couleurs en dur les plus répétées **nomment les jetons manquants** :

| valeur | occurrences | applications | rôle réel |
|---|---|---|---|
| `255,255,255` | 48 | 6 | texte/icône **posé sur** l'accent → `onAccent` |
| `0,0,0` | 33 | 3 | voile de modale, ombre → `scrim`, `shadow` |
| `40,46,54` · `54,60,70` | 25 | 2 | fond de carte, survol de ligne → `card`, `rowHover` |
| `130,138,148` · `168,176,185` | 8 | 2 | texte secondaire → `textMuted` |
| `80,88,98` · `120,130,142` | 8 | 2 | barre de défilement → `scrollbar(Hover)` |
| `232,106,106` · `240,120,120` | 12 | 1 | erreur/suppression → `danger` |
| `88,166,255` | 7 | 2 | information → `info` |

Un cas extrême : `Applications/NKCode/src/NKCode/Shell/Dialogs.h` — **75 couleurs
en dur, 0 jeton**. NKCode s'est reconstruit une palette parallèle complète
(`cCard`, `cBorder`, `cSelBg`, `cRowHov`, `cSide`, `cFaint`, `cAccent`…) parce
que le thème n'avait pas les rôles.

### Mesure 4 — les icônes

Il n'existe **aucun atlas d'icônes**. Chaque site charge sa propre texture et
appelle `AddImage` (`NkEditorShell.cpp` : `mActTexR[]`, `mTitleLogoTex` ;
`NkEditorContextMenu.h` : `icons[i]`). Là où la planche montre un pictogramme,
`Applications/Nogee/src/Nogee/Panels/AssetBrowser.cpp:157` dessine
`dl.AddRectFilled(iconRect, iconCol, 4.f)` — **un rectangle coloré à la place de
l'icône**. Bloquant nommé depuis deux jours, **non comblé ici** (voir plus bas).

## Ce qui est comblé

Tout est **strictement additif** : aucun appelant existant ne change (paramètres
nouveaux avec valeur par défaut, champs et fonctions nouveaux).

### `NkGuiDrawList` — 4 primitives

| ajout | comble | convention |
|---|---|---|
| `AddRect(r, col, thickness, rounding = 0.f)` | mesure 2 (185 sites) | trait **strictement à l'intérieur** de `r`, comme le cas droit ; `rounding = 0` emprunte le chemin historique **inchangé** |
| `AddCircle(center, r, col, thickness, segs)` | `CircleOutline` ×2 + `NkcRing` | `r` = **ligne médiane** — même convention que les émulations remplacées, donc leur migration est un simple renommage |
| `AddConvexPolyFilled(pts, n, col)` | `NkcPolyFilled` | éventail depuis `pts[0]` ; non convexe non vérifié (coût) |
| `AddPolyline(pts, n, col, th, closed)` | `NkcPolyOutline` | un quad par segment, sans raccord d'onglet — comme `AddLine`, dont c'est la généralisation |

Le rayon d'arc suit **la même règle de segments que `AddCircleFilled`**
(`NkGuiArcSegs`), volontairement déterministe : c'est ce qui rend la géométrie
vérifiable au banc.

### `NkGuiTheme` — 13 couleurs + 3 tailles

`onAccent`, `card`, `rowHover`, `textMuted`, `separator`, `scrollbar`,
`scrollbarHover`, `scrim`, `shadow`, `success`, `warning`, `danger`, `info` ;
`roundingSmall`, `roundingLarge`, `borderThickness`. Chacun est justifié par un
compte d'occurrences en dur (mesure 3), pas par une intuition.

### Le thème s'ÉNUMÈRE — premier étage de la description

La règle du dépôt dit qu'un composant doit pouvoir être **décrit**, pas seulement
appelé, sinon aucun futur NKUIEditor ne peut le composer ni le sauver. Premier
étage, livré ici et de coût quasi nul :

```cpp
const NkGuiTokenDesc *NkGuiThemeTokens(int32 *count) noexcept; // nom, groupe, type, offset
NkColor  *NkGuiThemeColor (NkGuiTheme &, const char *name) noexcept;
float32  *NkGuiThemeScalar(NkGuiTheme &, const char *name) noexcept;
```

**35 jetons décrits.** Un sélecteur de thème, un sérialiseur ou un éditeur peut
désormais parcourir le thème sans connaître un seul nom de champ à la
compilation. Ajouter un jeton = un champ + une ligne de table ; le banc vérifie
que les deux restent en phase.

## Le témoin — non visuel, parce qu'il n'y a pas de GPU

`Applications/NKGuiDrawTest/` — application **console**, aucune fenêtre, aucun
device. Elle appelle les primitives et **recalcule des propriétés géométriques
depuis les triangles émis**.

    jenga build --target NKGuiDrawTest --config Debug
    ./Build/Bin/Debug-Windows/NKGuiDrawTest/NKGuiDrawTest.exe   ->  46/46, code 0

La mesure qui porte : un prédicat « ce point est-il couvert par un triangle ? »
distingue un **anneau** d'un **disque** sans regarder une image. D'où :

- `AddRect` arrondi : le centre n'est **pas** couvert, les quatre bords le sont ;
- `AddCircle` : idem, et tous les sommets tombent dans `[r-th/2, r+th/2]` ;
- contre-épreuve obligatoire : `AddCircleFilled`, lui, **couvre** son centre —
  sinon les tests ci-dessus passeraient pour la mauvaise raison.

**Chaque zéro est doublé d'un contrôle positif** (rect nul → rien, *puis* appel
valide → non vide). Non-régression du chemin historique : `AddRect` sans arrondi
produit toujours exactement 16 sommets / 24 indices / 1 commande.

**Témoin du sens inverse** : en remplaçant volontairement l'anneau par un disque
plein, le banc tombe à **45/46** en signalant exactement
`le centre n'est PAS couvert (anneau, pas disque)`. Le sabotage a été retiré.

**Ce que le banc ne prouve pas** : que le backend dessine ce flux correctement à
l'écran. Il prouve que le flux **est celui qu'on croit**.

> **Note sur la forme** : ce banc est une application console et non un
> `unittest()`, parce que **l'exécution des tests unitaires est désactivée par la
> politique du workspace** (`disableunittestexecution` — `jenga test` le refuse
> explicitement). Un test Unitest compilerait sans jamais tourner. NKGui n'a
> d'ailleurs **aucun dossier `tests/`**, contrairement à 21 autres modules.

## Non-régression des consommateurs

Build Debug vert pour NKGui **et** ses dépendants : NKEditorKit, NKGuiDemo,
NKCode, Nogee, NK3DModeler, NkAnimaEditor, ConquerorLab, Mou, Nkoung, PV3DE,
NKPA, NKEditorKitDemo, NKViewportDemo.

## Ce qui reste — par ordre de blocage

1. **Atlas d'icônes** (mesure 4). **Le vrai bloquant restant.** Nogee dessine des
   rectangles là où la planche montre des pictogrammes. Demande une décision qui
   dépasse le dessin : format de la planche, nommage stable des icônes, chemin de
   chargement, et un jeu d'icônes. À cadrer avec Rodolf avant d'écrire.
2. **Migrer les émulations** vers les nouvelles primitives — `MouDraw.h` et
   `NkoungDraw.h` deviennent des alias puis disparaissent (≈300 lignes en moins),
   `NkcDraw.h` perd `NkcRing`/`NkcPoly*`. **Hors périmètre de cet agent**
   (touche les applications) : une ligne par site, à faire par leurs agents ou
   sur autorisation.
3. **Passer les 29 `AddRect` de NKGui lui-même** au paramètre `rounding` —
   dans le périmètre, mais **change l'apparence** : à faire quand un témoin
   visuel est possible, donc **après la campagne**. C'est un changement qu'on ne
   valide pas à l'aveugle.
4. **Remonter les couleurs en dur vers les nouveaux jetons** (426 sites, dont 75
   dans le seul `Dialogs.h`). Hors périmètre ; les jetons existent désormais.
5. **Deuxième étage de la description** : décrire non plus seulement le thème
   mais les **composants** (paramètres, variantes, points de greffe). Reste à
   trancher avec le chantier NKEditorKit — le format de description est commun.

## Témoin visuel différé — à faire quand le GPU se libère

Nommément : lancer **NKGuiDemo**, dessiner un `AddRect` arrondi par-dessus un
`AddRectFilled` de même rayon, et vérifier que **les deux contours coïncident**
(pas de dépassement, pas de liseré). Puis un `AddCircle` sur un `AddCircleFilled`
de même rayon. C'est la seule chose que le banc ne peut pas dire.
