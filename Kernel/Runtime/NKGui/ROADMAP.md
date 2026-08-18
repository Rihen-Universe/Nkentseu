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

> ⚠️ **CORRECTION (2026-08-18, même jour).** J'avais écrit ici « **129 appels**
> répartis sur Pong, Pong2, Mou, Nkoung, Songoo, NkImageDemo ». **C'est faux, et
> l'erreur est de méthode** : mon `grep "CircleOutline"` capturait aussi
> `DrawCircleOutline`, qui appartient à `GLRenderer2D.h` — **un autre renderer,
> une autre famille d'émulation**, sans rapport avec `NkGuiDrawList`. Pong,
> Pong2, Songoo et NkImageDemo n'ont jamais appelé `MouDraw`/`NkoungDraw`.
>
> **Le chiffre exact, ré-mesuré sur une expression bornée** (`[^A-Za-z_]` devant
> le nom) : **17 appels**, dans **8 fichiers**, sur **2 applications** (Mou et
> Nkoung). Le doublon de 155 lignes, lui, reste exact au caractère près — et
> c'est lui l'argument, pas le nombre d'appels.

Portée réelle de ces émulations : **17 appels** de `CircleOutline`/`RectOutline`,
8 fichiers, 2 applications (Mou, Nkoung) — liste exacte dans
`echanges/nkgui.questions.md` (Q2), avec la ligne de remplacement de chacun.

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

---

# LOT 2 — le placement, les icônes, le séparateur (2026-08-18, même jour)

## Le manque qui primait sur les huit autres : **aucun placement explicite**

Mesure de l'agent NK3DModeler, re-vérifiée ici sur `NkGuiWidgets.h` :

| | |
|---|---|
| fonctions déclarées | **116** |
| qui **acceptent** un `NkRect` | **12** (`Button`, `ButtonEx`, `RepeatButton`, `BeginChild`, `BeginPanel`, `BeginListBox`, `BeginMenuBar`, `InputTextMultiline`, `DockSpace`, `Splitter`, `PanelBackground`, `BeginDropTarget`) |
| qui **se placent elles-mêmes** | **104** |

Une interface intégralement pilotée par rectangles — c'est le cas de NK3DModeler
— ne pouvait donc en appeler que **12**. Son fameux « 11 appels de widget » n'était
pas de l'indiscipline : **c'était le plafond de ce qui lui était offert.**

**Chercher avant d'ajouter** (leçon `Footer`, en français *et* en anglais) :
`SetCursorPos`, `SetNextItemRect`, `SetItemRect`, `ForceRect`, `OverrideRect`,
« curseur posé », « placement explicite » → **rien**. `NextItemRect` est
**l'entonnoir unique** par lequel passent les 104, et rien ne permettait de
l'outrepasser. C'est donc là, et nulle part ailleurs, que l'ajout devait se faire.

### `SetNextItemRect` — additif, un seul appel

```cpp
ctx.SetNextItemRect({50.f, 60.f, 120.f, 30.f});
Button(ctx, "pose");   // prend EXACTEMENT ce rectangle
Button(ctx, "suivant"); // replacement automatique, comme avant
```

- **104 fonctions débloquées d'un coup** — c'est un ordre de grandeur au-dessus
  d'un manque de dessin, qui n'en débloque qu'un.
- Le **curseur ne bouge pas** : qui pose un rectangle place lui-même. En revanche
  `prevItem` et l'étendue du contenu (`maxX`/`maxY`) sont mis à jour, pour que les
  conteneurs défilables voient ce qu'on y a posé et que `SameLine` reste cohérent.
- **Un rectangle posé ne survit pas à la frame** : `BeginFrame` le vide. Un widget
  conditionnel non atteint ne peut pas décaler le suivant à la frame d'après.

## Les icônes : le **mécanisme**, jamais le vocabulaire

`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiIcons.h`

**Ce qui manquait** : rien ne permettait de dessiner un pictogramme. Résultat
mesuré — NK3DModeler tient **102 glyphes** (`NkIcon`, **une texture par icône**),
NKCode en tient **91 autres**, soit **193 glyphes définis deux fois** ; et
`Nogee/Panels/AssetBrowser.cpp:157` dessine `AddRectFilled(iconRect, iconCol, 4.f)`
là où la planche montre un pictogramme. C'est le motif `MouDraw`/`NkoungDraw`, une
couche plus haut.

**Ce qui est livré est le mécanisme seul.** Aucune énumération d'icônes n'entre
dans NKGui, et n'y entrera : ce vocabulaire devra survivre à la déclaration de
**NkUIDesign**, qui portera le **rôle** et l'**arbre de sous-éléments**, et qui
**dessinera** les icônes (Rodolf, 18/08). Figer un `enum` ici, c'était écrire du
code à défaire.

- **Poignée opaque** : sa valeur n'a aucun sens hors du jeu qui l'a produite. Elle
  embarque l'identifiant du jeu, donc une poignée présentée au mauvais jeu est
  **rejetée** au lieu de dessiner silencieusement le mauvais glyphe.
- **Deux sources, une seule poignée** :
  - `AddBitmap` — découpe d'un atlas rasterisé, **et une texture par région est
    permise** : le modèle actuel de NK3DModeler passe sans rien changer, sinon
    rien ne migre ;
  - `AddPath` + `AddContour` — **contours vectoriels** dans une boîte unité
    `[0,1]²`, mis à l'échelle du rect au dessin. **Le vectoriel est la source, la
    rasterisation une étape de sortie.** Un glyphe vectoriel se **recolore** par
    jeton (pas d'atlas par couleur), suit le **DPI sans flou** (aucun
    échantillonnage : la géométrie est émise à la taille finale), et **reste
    éditable**. Les deux primitives qui le rendent possible — `AddConvexPolyFilled`
    et `AddPolyline` — venaient d'être ajoutées au lot 1.
- **Un échec se constate** : `AddIcon` rend `Glyph`, `Fallback` ou `None`. Le
  glyphe de secours est choisi par l'**application**, et quand il sert, **la
  fonction le dit**. Un carré muet à la place d'une icône est exactement le défaut
  qu'on corrige.
- **Le jeu s'énumère** (`GlyphAt`, `ContourAt`, `Points`) — un futur NkUIDesign
  doit pouvoir **lister et éditer** ce que l'application a déclaré.

## Séparateur : deux propriétés récupérées de NKUI avant son extinction

`NkUILayout::DrawSplitter` avait deux choses que `nkgui::Splitter` n'avait pas, et
**l'extinction de NKUI allait supprimer le fichier** :

1. **un ratio** plutôt qu'une position en pixels — un ratio survit au
   redimensionnement de la fenêtre, une position en pixels non ;
2. **une zone de préhension élargie** — sans elle, on attrape la barre d'onglets
   voisine par erreur, et *un clic manqué devient un désancrage au lieu d'un
   redimensionnement*.

Livré : `SplitterRatio(...)`, plus un paramètre `grabPx = 0.f` sur le `Splitter`
existant (0 = comportement historique exact). La géométrie est sortie du widget
dans `SplitterRects(...)` — **fonction pure**, donc vérifiable sans GPU.

## README : la moitié non technique de « rendre l'existant trouvable »

Le README de NKGui, dernier commit le **26/06**, annonçait encore « **Phase 1 —
squelette, widgets ⏳** ». `Splitter` existait depuis ce même 26/06. **La
vérification de deux minutes que le corpus prescrit répondait donc que la chose
n'existait pas** — et un agent l'a réécrite de son côté.

> Une documentation périmée ne ralentit pas l'adoption : **elle la refuse.**

README réécrit sur des **chiffres datés** (9 051 lignes, 116 fonctions, 15
dépendants) plutôt qu'un état d'avancement, avec ce qui n'existe pas dit
explicitement (le mode **retenu** n'existe pas) et les deux règles de conception
qu'un contributeur doit connaître avant d'ajouter quoi que ce soit.

## Le témoin, toujours sans GPU : **108/108**

Les nouveaux blocs suivent la même méthode que l'anneau du lot 1 — mesurer la
géométrie produite, et doubler chaque zéro d'un contrôle positif :

- **icônes bitmap** : les UV émises couvrent **exactement** la région déclarée
  (`(0,0,16,16)` dans un atlas `128×64` → `u ∈ [0, 0.125]`, `v ∈ [0, 0.25]`) ;
  **contre-épreuve** — la région voisine donne `u0 = 16/128`, sans quoi le test
  passerait même si le code renvoyait toujours le même quad ;
- **icônes vectorielles** : le même glyphe dessiné à `16 px` puis à `64 px`
  produit une géométrie **exactement ×4** — c'est la preuve mesurable qu'il n'y a
  aucun échantillonnage, donc rien qui puisse flouter au DPI ;
- **poignée étrangère** : rejetée, `None`, **pas un mauvais glyphe** ;
- **placement** : la géométrie tombe **entièrement dans le rectangle posé** ;
  contre-épreuve, sans rectangle posé elle tombe ailleurs ; et le widget suivant
  reprend le placement automatique ;
- **séparateur** : préhension strictement plus large que le visuel, **même
  centre**, ratio borné, `grabPx = 0` → préhension = visuel.

**Témoin du sens inverse, trois fois** — chaque sabotage tombe sur exactement les
assertions concernées, puis est retiré :

| sabotage | résultat |
|---|---|
| UV toujours `[0,1]` + contrôle de jeu retiré | **84/88**, sur les 2 assertions d'UV et les 2 de poignée étrangère |
| rectangle posé rendu permanent | **96/97**, sur « un seul appel » |
| anneau remplacé par un disque (lot 1) | **45/46**, sur « le centre n'est PAS couvert » |

## Ce qui reste

1. **Migrer les émulations** — `MouDraw.h`/`NkoungDraw.h` (17 sites, liste exacte
   et ligne de remplacement au canal Q2), `NkcDraw.h`. Hors périmètre.
2. **Faire adopter les icônes** — Nogee remplace son `AddRectFilled(iconRect, …)`
   par un `AddIcon` ; NK3DModeler et NKCode versent leurs 193 glyphes dans **un
   jeu unique**. Hors périmètre, et à cadrer avec NkUIDesign pour le vocabulaire.
3. **Témoin visuel différé** (le GPU se libère dans ~9 h) : superposer un
   `AddRect` arrondi et un `AddRectFilled` de même rayon — les contours doivent
   coïncider ; puis `AddCircle` sur `AddCircleFilled` ; puis vérifier à l'œil les
   25 contours de widgets passés au paramètre `rounding`.
4. **Second étage de la description** — décrire les **composants** (paramètres,
   variantes, points de greffe), format commun avec NKEditorKit et NkUIDesign.

---

# LOT 3 — la fusion avec `main`, et ce que NkUIDesign révèle (2026-08-18)

## Le témoin passe à **115/115**

| étape | banc |
|---|---|
| fin du lot 2 | 108/108 |
| garde d'atlas (`ae26f773`) | **111/111** |
| troncature de poignée (`262aa4f7`) | **115/115** |

## Une garde qui se désarmait elle-même

`AddBitmap` vérifiait les bornes de la région **à l'intérieur** d'un
`if (mAtlasW > 0 && mAtlasH > 0)`, censé épargner les jeux purement vectoriels.
Cette enveloppe désactivait le contrôle **exactement quand il servait** : un
bitmap déclaré sur un jeu sans atlas passait, et le dessin normalisait ensuite
les UV par `1.f` — des coordonnées en **pixels**, soit seize fois hors de la
texture pour une région de 16 px, **en silence**.

Ce n'était donc pas une garde manquante mais **une garde neutralisée par sa
propre condition** — la famille « une protection qui ne protège rien ». Les deux
causes se séparent, et c'est ce qui a tranché : retirer la garde explicite seule
ne change rien (111/111), remettre l'enveloppe d'origine casse (110/111).

La déclaration échoue désormais **au chargement**, là où c'est vérifiable, et
non à l'écran.

## ⚠️ Ce que NkUIDesign attend de NKGui — et la fente qui ne passe pas

La tranche verticale (#83) reçoit les icônes par
`NkComponentPaint::Icon(const NkPaintRect&, uint16 iconHandle, uint16 role)`.

Or `NkGuiIconHandle` vaut `(identifiant de jeu << 16) | (indice + 1)` :
l'identifiant occupe les **16 bits de poids fort**, précisément ceux qu'un
`uint16` supprime. Le contrôle d'appartenance — celui qui interdit de dessiner
le glyphe **d'un autre jeu** — est le premier à tomber dans ce transport.

**Mesuré, pas déduit** (4 assertions, dont un contrôle positif qui prouve que la
troncature a bien lieu) : le refus est **franc**. Le jeu émetteur rejette la
poignée amputée, `AddIcon` rend `None` sans rien émettre, **jamais le glyphe
voisin d'indice égal**. La conséquence côté NKEditorKit est donc « aucune icône
ne se dessine » — visible — et non « une icône fausse se dessine », qui aurait
coûté des jours de diagnostic.

> **Remède, hors périmètre NKGui : élargir la fente à `uint32`.**

## Trois affirmations de `main` que la fusion n'a pas signalées

Une fusion sans conflit ne garantit pas la cohérence. Ces trois-là **compilent**,
et sont pourtant périmées par ce que les lots 1 et 2 ont livré :

| où (`Engine/NKEditorKit/`) | ce qui est écrit | l'état réel |
|---|---|---|
| `NkGuiComponentPaint.h:47`, `:89` | « `AddRect` ne sait pas arrondir » | faux depuis le lot 1, **déjà dans `main`** quand #83 a été écrit |
| `NkComponentPaint.h:84` | « le cercle creux… `Ring` = 2 disques » | `AddCircle` existe (rayon = ligne médiane) |
| `NkGuiComponentPaint.h:40` | « aucune notion d'icône dans NKGui » | `NkGuiIcons.h` : atlas **et** vectoriel |

`Outline` peut aujourd'hui s'écrire `AddRectFilled(q, inner, rounding)` +
`AddRect(q, border, th, rounding)` — un contour d'épaisseur réelle, au lieu de
deux rectangles pleins superposés. **Hors périmètre** : signalé au canal (Q4),
pas corrigé ici.

## Les trois horizons

- **court** — le témoin visuel des 25 contours arrondis, dès que le GPU se libère
  (il est le seul point encore différé, et il est nommé) ;
- **moyen** — les 193 glyphes de NK3DModeler et NKCode versés dans **un jeu
  unique**, une fois le vocabulaire cadré par NkUIDesign ;
- **long** — NKGui comme socle unique de dessin du dépôt : plus une application
  ne réécrit une primitive. Les émulations restantes (`MouDraw.h`,
  `NkoungDraw.h`, `NkcDraw.h`) en sont la mesure, et elle est décroissante.
