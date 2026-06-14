# Layout, docking et animations

> Couche **Runtime** · NKUI · Placer les widgets (`NkUILayout`), persister et habiller
> l'interface (`NkUILayout2`), ancrer des fenêtres en panneaux (`NkUIDock`), et animer le tout
> (`NkUIAnimation`).

Une interface immédiate, c'est un flot d'appels par frame : on déclare un bouton, puis un autre, puis
un panneau — et **personne ne dit où chaque chose va**. La question centrale de toute cette page est
donc : *qui décide de la position et de la taille de chaque widget ?* La réponse de NKUI tient en
quatre briques empilées. Le **layout** calcule, widget après widget, le rectangle suivant à partir
d'un curseur et de contraintes. La **persistance** sauve et recharge la disposition des fenêtres
entre deux sessions. Le **docking** transforme des fenêtres flottantes en panneaux ancrés,
fractionnables et à onglets, comme dans un éditeur. Et les **animations** font bouger, fondre et
rebondir tout cela en douceur.

Tout est **zéro-STL** et sans allocation cachée : les piles, les pools de nœuds et de tweens sont des
**tableaux fixes** (`NkUILayoutStack::MAX_DEPTH = 32`, `NkUIDockManager::MAX_NODES = 128`,
`NkUIAnimator::MAX_TWEENS = 256`). Quand un tableau est plein, l'opération échoue silencieusement
plutôt que d'allouer — c'est un choix assumé du temps réel. Aucune méthode de ces headers n'est
`[[nodiscard]]` : les valeurs de retour (un `bool` cliqué, une valeur de tween) sont à lire
sciemment.

- **Namespace** : `nkentseu::nkui`
- **Headers** : `NkUILayout.h`, `NkUILayout2.h`, `NkUIDock.h`, `NkUIAnimation.h`
- **Types externes** (déclarés ailleurs dans NKUI) : `NkUIContext`, `NkUIDrawList`, `NkUIFont`,
  `NkUIFontAtlas`, `NkUIWindowManager`. Types Foundation : `NkRect`, `NkVec2`, `NkColor`, `NkUIID`.

---

## Placer les widgets : `NkUILayout`

Le problème de base d'une UI immédiate : on dessine des widgets l'un après l'autre, et chacun a
besoin d'un rectangle. Le plus naïf serait de donner des coordonnées absolues à chaque appel — mais
alors ajouter un bouton oblige à repousser tous les suivants à la main. Le **layout** automatise ce
calcul. À chaque frame, NKUI tient une **pile** de layouts actifs (`NkUILayoutStack`) ; le sommet
porte un **curseur** qui avance à mesure qu'on place des widgets. Demander le rectangle suivant, c'est
appeler `NkUILayout::NextItemRect`, qui consulte le sommet de la pile et la contrainte du prochain
item ; signaler qu'on l'a occupé, c'est `AdvanceItem`, qui fait avancer le curseur.

Le **type de layout** (`NkUILayoutType`) décide *comment* le curseur avance. En `NK_FREE` (le défaut)
on positionne librement ; en `NK_ROW` les widgets se suivent horizontalement de gauche à droite ; en
`NK_COLUMN` verticalement de haut en bas ; en `NK_GRID` ils remplissent une grille à N colonnes ;
`NK_SPLIT` partage la zone en deux panneaux séparés par un *splitter* déplaçable ; `NK_TAB` produit
une barre d'onglets ; `NK_SCROLL` une région défilable ; `NK_GROUP` un groupe encadré. La valeur
`NK_NONE` n'est pas un mode utile : c'est l'absence de type.

Ce n'est **pas** un moteur de layout web qui résout des contraintes en plusieurs passes. C'est un
calcul **en une seule passe, dans l'ordre de déclaration** : le rectangle du widget courant ne dépend
que du curseur actuel et de sa propre contrainte, jamais de ce qui sera déclaré après. Cela impose une
limite — on ne peut pas, par exemple, centrer un widget en fonction de la largeur totale qu'on ne
connaît pas encore — mais c'est ce qui rend le système trivialement rapide et prévisible.

> **En résumé.** `NkUILayout` (statique, sans état) calcule le rectangle de chaque widget à partir du
> sommet de `NkUILayoutStack` : `NextItemRect` pour obtenir, `AdvanceItem` pour avancer. Le
> `NkUILayoutType` du sommet (row, column, grid, split, tab, scroll, group, free) dicte la marche du
> curseur. Une seule passe, dans l'ordre de déclaration.

### Les contraintes de taille : `NkUIConstraint`

Comment dire « ce bouton fait 80 px » ou « cette colonne prend tout l'espace restant » ? Par une
**contrainte** posée *avant* le widget, stockée dans `nextW`/`nextH` du nœud de layout. `NkUIConstraint`
est une petite struct *flex-like* : un `type`, une `value`, et des bornes `minSize`/`maxSize`. Les
quatre types (`NkUIConstraint::NK_AUTO`, `NK_FIXED`, `NK_PERCENT`, `NK_GROW`) couvrent les besoins :
laisser le widget prendre sa taille naturelle, lui imposer un nombre de pixels, lui donner un
pourcentage `[0,1]` du parent, ou le faire **grandir** pour absorber l'espace restant (avec un poids
de *flex*). On ne construit pas la struct à la main : on passe par les fabriques `Auto_()`,
`Fixed_(px)`, `Percent_(p)`, `Grow_(flex)`.

Attention au **piège du underscore final** : ces fabriques s'appellent `Auto_`, `Fixed_`, `Percent_`,
`Grow_` — le tiret bas est là pour ne pas heurter le mot-clé `auto`. C'est `NkUIConstraint::Auto_()`,
jamais `Auto()`.

La résolution se fait via `NkUILayout::ResolveWidth` / `ResolveHeight`, qui prennent le nœud parent, la
contrainte et la taille *naturelle* souhaitée (`want`) ; en `NK_AUTO`, c'est `want` qui l'emporte.

> **En résumé.** Une contrainte décrit la taille d'un item : `Auto_` (naturelle), `Fixed_(px)`,
> `Percent_(p)`, `Grow_(flex)`. **Les fabriques portent un underscore final.** `ResolveWidth/Height`
> les traduisent en pixels selon le layout parent.

### La pile et les nœuds : `NkUILayoutStack`, `NkUILayoutNode`

`NkUILayoutStack` est la pile elle-même : un tableau de 32 `NkUILayoutNode`, plus une profondeur.
`Top()` rend le sommet (ou `nullptr` si vide), `Push()` empile un nœud (ou `nullptr` si plein),
`Pop()` dépile, `Empty()`/`Depth()` renseignent. Tout est `O(1)`. **Piège majeur** : `Push()` ne
réinitialise *pas* le nœud réutilisé — il renvoie l'élément du tableau tel qu'il était, et c'est à
l'appelant de le configurer (type, bounds, curseur…) avant de s'en servir.

Un `NkUILayoutNode` porte tout l'état d'un niveau de layout : son `type`, sa zone `bounds`, le
`cursor` courant, la `contentSize` accumulée (pour le scroll), la hauteur de ligne (`lineH`) en mode
row et la largeur de colonne (`colW`) en mode column, les contraintes du prochain item
(`nextW`/`nextH`), le défilement (`scrollX`/`scrollY` et leurs pointeurs externes), un `id`, un drapeau
`clipped`, un compteur `itemCount`. Les modes spécialisés ajoutent leurs champs : la grille
(`gridCols`, `gridCol`, `gridCellW`), le split (`splitRatio` + son pointeur externe, `splitVertical`,
`splitSecondPane`), les onglets (`activeTab` pointeur, `tabCount`).

> **En résumé.** `NkUILayoutStack` = pile fixe de 32 nœuds (`Top/Push/Pop/Empty/Depth`, tout `O(1)`).
> **`Push()` ne nettoie pas le nœud** : à vous de le configurer. `NkUILayoutNode` agrège l'état d'un
> niveau (curseur, bounds, contraintes, scroll, et les champs propres à grid/split/tab).

### Scrollbars et splitters

Deux helpers de `NkUILayout` produisent les éléments **interactifs** d'un layout. `DrawScrollbar`
dessine une barre de défilement (verticale ou horizontale) dans une piste donnée, en fonction de la
taille du contenu et de la fenêtre visible ; elle met à jour la valeur `scroll` passée par référence et
renvoie `true` si on a cliqué dessus. `DrawSplitter` dessine la poignée déplaçable entre deux
panneaux : elle met à jour le `ratio` (in/out) et renvoie `true` tant que le glissement est actif.
Notez le motif récurrent ici comme partout dans NKUI : la **valeur d'état vit chez l'appelant** et
transite par référence ; le widget ne fait que la lire et la modifier.

L'en-tête de `NkUILayout.h` documente un idiome `NkUI::BeginRow/EndRow/SetNextWidth/...` — ces
fonctions ne sont **pas déclarées dans ce header** ; c'est une aspiration de conception, pas l'API
disponible ici.

> **En résumé.** `DrawScrollbar` et `DrawSplitter` rendent les contrôles d'un layout défilable ou
> fractionné ; ils modifient `scroll`/`ratio` par référence et renvoient l'état cliqué/glissé. L'état
> appartient à l'appelant.

---

## Persister et habiller : `NkUILayout2`

Une disposition qu'on doit recomposer à chaque lancement n'est pas une vraie disposition. `NkUILayout2`
ajoute trois utilitaires haut-niveau (et un backend GPU que cette page laisse de côté). Le premier,
`NkUILayoutSerializer`, **sauve et recharge** la configuration des fenêtres et du dock en JSON :
`Save`/`Load` vers un fichier, `SaveToMemory`/`LoadFromMemory` vers un tampon. Le JSON est lisible —
un objet `windows` (nom, position, taille, ouvert/replié, scroll) et un objet `dock` (arbre de nœuds
+ racine). **Piège** : `SaveToMemory` *alloue* le tampon `outJson`, et son commentaire dit de le
libérer avec `free()` — ce qui détonne avec la règle dure NKMemory du projet ; à vérifier côté
implémentation.

Le deuxième, `NkUIColorPickerFull`, est un sélecteur de couleur complet (carré saturation/valeur,
bande de teinte, bande alpha à damier, sliders RGBA 0-255, saisie hexadécimale `#RRGGBBAA`, aperçu
avant/après). Son entrée publique est `Draw`, qui prend la couleur en référence et renvoie `true`
quand elle change ; deux helpers de conversion `RGBtoHSV`/`HSVtoRGB` complètent l'outillage. Limite à
connaître : son état interne est plafonné à **16 pickers simultanés**.

Le troisième, `NkUIFontNKFont`, est le **pont vers le module NKFont** (sous `#define NKUI_WITH_NKFONT`) :
il rastérise une police TTF/OTF/WOFF/WOFF2 dans un atlas 512×512 Gray8. `Load` charge depuis la
mémoire, `LoadFile` depuis un chemin, `AddGlyphRange` ajoute une plage de codes (par ex. l'ASCII
32-127) à l'atlas.

> **En résumé.** `NkUILayout2` outille la durée de vie d'une UI : `NkUILayoutSerializer` (JSON,
> fichier ou mémoire — `SaveToMemory` alloue), `NkUIColorPickerFull` (picker complet, 16 états max),
> `NkUIFontNKFont` (pont NKFont, atlas 512² Gray8). Le renderer OpenGL associé est un backend hors
> périmètre de cette page.

---

## Ancrer des fenêtres : `NkUIDock`

C'est la pièce qui donne à une UI son allure d'**éditeur**. Au lieu de fenêtres flottantes qui se
chevauchent, le docking range les fenêtres dans un **arbre** de panneaux : on en glisse une sur le
bord d'une autre et elle s'y ancre, partageant l'espace ; on en glisse une au centre et elle devient un
**onglet** du même panneau. Le moteur de tout cela est `NkUIDockManager`, et il opère sur un tableau
fixe de `NkUIDockNode` (`MAX_NODES = 128`) — pas d'allocation dynamique pour la structure.

Chaque `NkUIDockNode` est soit une **feuille** qui contient des fenêtres (`NkUIDockNodeType::NK_LEAF`,
jusqu'à `MAX_WINDOWS = 16`), soit un **split** horizontal ou vertical (`NK_SPLIT_H`/`NK_SPLIT_V`) avec
deux enfants et un `splitRatio`, soit la racine (`NK_ROOT`). Un nœud connaît son `parent`, ses
`children[2]`, son `rect`, l'onglet actif (`activeTab`), un défilement vertical, et — nouveauté v2 —
le `parentWindowId` quand il s'agit d'un dock **enfant** logé dans une fenêtre.

La direction d'ancrage s'exprime par `NkUIDockDrop` : `NK_LEFT`, `NK_RIGHT`, `NK_TOP`, `NK_BOTTOM`
créent un split de ce côté ; `NK_CENTER` et `NK_TAB` ajoutent la fenêtre comme onglet ; `NK_NONE` est
l'absence de cible. Ce n'est **pas** un système où chaque fenêtre a des coordonnées indépendantes : une
fenêtre ancrée n'a plus de position propre, c'est l'arbre qui répartit l'espace et `RecalcRects`
recalcule tous les rectangles depuis la racine.

> **En résumé.** `NkUIDockManager` (128 nœuds fixes) gère un arbre de `NkUIDockNode` — feuilles (16
> fenêtres max), splits H/V, racine. On ancre via `NkUIDockDrop` (left/right/top/bottom = split ;
> center/tab = onglet). Une fenêtre ancrée perd sa position : l'arbre répartit, `RecalcRects` recalcule.

### Piloter le docking au fil des frames

Le cycle de vie suit la règle NKMemory Create/Destroy : `Init(viewport)` au départ, `Destroy()` à la
fin. Chaque frame, avant de dessiner les fenêtres, on appelle `BeginFrame` (qui traite les inputs de
docking : drags, dépôts, onglets) ; puis `Render` dessine tout l'arbre — barres d'onglets et contenus.
Pour dessiner manuellement le contenu d'un panneau ancré, on encadre par `BeginDocked(name)` /
`EndDocked`. Le redimensionnement de la zone se fait par `SetViewport`, et la synchronisation des
rectangles de fenêtres ancrées par `SyncDockedWindowRects`.

Les opérations d'arbre sont directes : `AllocNode` réserve un nœud, `DockWindow` ancre une fenêtre dans
un nœud selon une direction, `UndockWindow` la détache, `FindNodeAt(pos)` retrouve la feuille sous un
point, `Root()` rend la racine. `DockWindow` respecte les **flags de fenêtre** (déclarés dans
NkUIWindow) : `NK_NO_DOCK` refuse l'ancrage, et `NK_NO_TABS` interdit les onglets — un dépôt CENTER/TAB
sur un nœud sans onglets est alors **dégradé en `NK_BOTTOM`**. `NodeAllowsTabs` répond à la question
« ce nœud accepte-t-il un onglet de plus ? ».

Le retour visuel du drag passe par `DrawDropZones` (dessine les zones de dépôt d'un nœud et renvoie
celle survolée, en filtrant le centre si les onglets sont interdits) et `DrawDirectionalHighlight`
(surligne le bord cible). La persistance dédiée du dock est `SaveLayout`/`LoadLayout`.

> **En résumé.** Cycle : `Init` → chaque frame `BeginFrame` (inputs) puis `Render` ; `Destroy` à la
> fin. Contenu manuel via `BeginDocked/EndDocked`. `DockWindow` honore `NK_NO_DOCK` (refus) et
> `NK_NO_TABS` (CENTER/TAB dégradé en BOTTOM). Retour visuel : `DrawDropZones`, `DrawDirectionalHighlight`.

### Docking enfant et décorations d'onglets

La v2 apporte le **docking imbriqué** : un sous-arbre de dock logé dans la zone cliente d'une fenêtre,
encadré par `BeginChildDock(..., parentWindowId, childViewport)` / `EndChildDock`. La fenêtre parente
doit porter le flag `NK_ALLOW_DOCK_CHILD`. Cela permet, par exemple, un panneau « contenu » qui a
lui-même ses propres onglets ancrables.

On peut aussi **décorer la barre d'onglets** avec des boutons personnalisés (un bouton de menu, une
icône d'épingle…) via `AddTabDecoration(fn, userData, width, id)`, `RemoveTabDecoration(id)`,
`EnableTabDecoration(id, enabled)` ; et activer/désactiver le bouton ▶ de débordement avec
`SetDockOverflowButton`. La fonction de décoration est un `NkTabDecoFn` — un pointeur de fonction
appelé de droite à gauche, recevant le contexte, la *draw list*, la police, l'index de nœud et un rect.

**Piège des doublons** à connaître absolument : il existe **deux** types de callback et **deux** structs
de décoration aux noms presque identiques. Au niveau du namespace, `NkTabDecoFn` et `NkTabDecoration`
sont ceux que l'API publique et le champ `tabDecos[]` utilisent réellement. À l'intérieur de la classe,
`NkUIDockManager::TabDecoFn` et `NkUIDockManager::TabDecoration` sont des doublons imbriqués, *définis
mais non branchés* sur l'API publique (`AddTabDecoration` prend bien le `NkTabDecoFn` du namespace), et
dont la sémantique diffère (le doublon imbriqué dit « retour < 0 → décoration ignorée »). **Utilisez
toujours `NkTabDecoFn`/`NkTabDecoration` du namespace.**

> **En résumé.** `BeginChildDock/EndChildDock` imbrique un dock dans une fenêtre (flag
> `NK_ALLOW_DOCK_CHILD`). `AddTabDecoration/RemoveTabDecoration/EnableTabDecoration` et
> `SetDockOverflowButton` habillent la barre d'onglets via `NkTabDecoFn`. **Doublon piège** : préférez
> toujours `NkTabDecoFn`/`NkTabDecoration` du namespace, pas leurs jumeaux imbriqués.

---

## Animer : `NkUIAnimation`

Une interface qui apparaît net, se ferme net et ne réagit jamais paraît morte. Les animations donnent la
sensation de vivant : un panneau qui glisse, un bouton qui fond, une fenêtre qui tremble pour signaler
une erreur. Au cœur, une seule idée : interpoler une valeur de `start` à `end` sur une `duration`, en
courbant le temps par une **fonction d'easing**.

### Les courbes : `NkEase` et `NkUIEasing`

`NkEase` énumère les courbes classiques : linéaire (`NK_LINEAR`), et les familles *quad / cubic / quart
/ sine / expo / elastic / bounce / back*, chacune en variante `IN`, `OUT` et `IN_OUT` (par ex.
`NK_OUT_CUBIC`, `NK_IN_OUT_ELASTIC`). La valeur `NK_COUNT` est la **sentinelle** de fin d'énumération,
pas une courbe. Concrètement : `IN` démarre lentement, `OUT` finit lentement, `IN_OUT` fait les deux ;
*elastic* dépasse en ressort, *bounce* rebondit, *back* dépasse légèrement avant de revenir.

`NkUIEasing` applique tout cela. La seule entrée à utiliser est `Apply(ease, t)`, qui prend un `t ∈
[0,1]` et renvoie le `t` courbé selon la courbe demandée. **Piège** : seules quelques fonctions ont une
implémentation *inline* dédiée dans le header (`Linear`, les quad/cubic, `OutElastic`, `OutBounce`,
`OutBack`) ; toutes les autres valeurs de `NkEase` (quart, sine, expo, et les variantes IN/IN_OUT
elastic/bounce/back) sont résolues **à l'intérieur de `Apply`** (implémentation hors header). Passez
donc toujours par `Apply` plutôt que d'appeler une fonction inline qui pourrait ne pas exister.

> **En résumé.** `NkEase` = le catalogue de courbes (linear + quad/cubic/quart/sine/expo/elastic/
> bounce/back × in/out/in-out ; `NK_COUNT` est une sentinelle). `NkUIEasing::Apply(ease, t)` est le
> point d'entrée — n'appelez pas les fonctions inline individuelles, toutes ne sont pas définies.

### Une animation : `NkUITween`

Un `NkUITween` est une animation en cours, un petit POD à logique inline. Il porte son `id`, ses bornes
`start`/`end`, sa `duration`, le temps écoulé `elapsed`, sa courbe `ease`, des drapeaux `looping` /
`pingPong` / `active`, et la `value` courante calculée. On l'avance avec `Step(dt)` : il incrémente
`elapsed`, calcule `t`, gère le bouclage (`looping` via modulo) ou le va-et-vient (`pingPong`, qui
inverse le sens à chaque cycle), sinon clampe à `t = 1` et se désactive. À la fin il met à jour
`value = start + (end - start) × Apply(ease, t)`. `IsDone()` dit si l'animation est terminée et non
bouclée. En pratique, on manipule rarement un tween à la main : on passe par l'animator.

> **En résumé.** `NkUITween` = une animation (start→end, duration, ease, loop/pingPong). `Step(dt)`
> l'avance et met à jour `value` ; `IsDone()` teste la fin. Géré normalement via `NkUIAnimator`.

### Le pool : `NkUIAnimator`

`NkUIAnimator` est un **pool fixe de 256 tweens** indexés par `NkUIID` — c'est l'objet qu'on garde
vivant entre les frames et qu'on interroge. Chaque frame, `Update(dt)` avance tous les tweens actifs.
Pour lancer une animation : `Play(id, from, to, duration, ease, loop, pingPong)`, qui réutilise le
tween de cet `id` s'il existe ou en alloue un. Pour aller vers une cible depuis la valeur actuelle
(idéal pour un *hover* qui peut changer d'avis en cours de route) : `Toward(id, target, duration,
ease)`. Pour lire la valeur courante : `Get(id)` (renvoie `0.f` si l'`id` est inconnu) ou
`Get(id, def)` (renvoie `def` à la place). `Stop`/`StopAll` désactivent, `IsPlaying(id)` teste l'état.

**Pièges essentiels** :

- `Play` **renvoie `from`** — la valeur *initiale*, pas la valeur live. Pour la valeur courante,
  utilisez toujours `Get`. C'est l'erreur classique : on croit récupérer la valeur animée alors qu'on
  a la valeur de départ.
- Si le pool est **plein** (256 tweens), `Play` renvoie silencieusement `to` **sans créer de tween** —
  l'animation n'a tout simplement pas lieu.
- Les tweens stoppés **gardent leur slot** : pas de compaction ni de libération. Un `id` réutilisé
  reprend son tween ; sinon le pool se remplit.

Pour la valeur live d'un tween nommé `id` :

```cpp
anim.Play(BTN_FADE, 0.f, 1.f, 0.2f);   // lance — renvoie 0.f (from), pas la valeur live
// ... plus tard, dans le rendu :
float32 a = anim.Get(BTN_FADE);        // <- la VRAIE valeur courante
```

> **En résumé.** `NkUIAnimator` = pool de 256 tweens par `id` : `Update(dt)` chaque frame, `Play` /
> `Toward` pour lancer, `Get` pour lire (**`Play` renvoie `from`, pas la valeur live** — utilisez
> `Get`), `Stop`/`StopAll`/`IsPlaying`. Pool plein → animation muette ; les slots ne se libèrent pas.

### Les effets prêts à l'emploi

Au-dessus du pool, l'animator offre des **effets nommés** qui font un `Play` paresseux : `Shake` (un
décalage `NkVec2` oscillant amorti, pour signaler une erreur), `Pulse` (un facteur d'échelle qui
respire, en boucle — pour attirer l'œil), `FadeIn` / `FadeOut` (opacité 0→1 / 1→0), `SlideInFromTop`
(un offset Y de `-h` à 0, avec un léger dépassement *back* — pour une notification qui descend),
`Bounce` (un offset Y qui rebondit en `out-bounce`). On lit leur sortie comme une valeur ordinaire et on
l'applique à une position, une échelle ou une couleur.

**Piège du redéclenchement** : un effet ne se relance **que si `!IsPlaying(id)`**. Pour rejouer un
effet déjà terminé, son `id` doit être redevenu inactif — ou alors appelez `Play` directement avec les
mêmes bornes. Sinon l'appel à `Shake`/`Pulse`/... est ignoré tant que l'animation précédente du même
`id` tourne encore.

> **En résumé.** Effets clés en main : `Shake` (erreur), `Pulse` (en boucle, attirer l'œil), `FadeIn`/
> `FadeOut`, `SlideInFromTop`, `Bounce`. Chacun fait un `Play` paresseux et **ne se relance que si
> l'`id` n'est plus en cours** (`IsPlaying`).

---

## Aperçu de l'API

La liste de **tous** les éléments publics, par header. Chacun est détaillé dans la « Référence
complète ».

### `NkUILayout.h` — placement

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkUILayoutType` : `NK_NONE` `NK_FREE` `NK_ROW` `NK_COLUMN` `NK_GRID` `NK_SPLIT` `NK_TAB` `NK_SCROLL` `NK_GROUP` | Mode de progression du curseur. |
| Contrainte | `NkUIConstraint` (+ enum imbriqué `Type` : `NK_AUTO` `NK_FIXED` `NK_PERCENT` `NK_GROW`) | Taille flex-like d'un item. |
| Fabriques | `Auto_()` `Fixed_(px)` `Percent_(p)` `Grow_(flex)` | Construire une contrainte (**underscore final**). |
| Nœud | `NkUILayoutNode` (type, bounds, cursor, contentSize, nextW/H, scroll, grid/split/tab) | État d'un niveau de layout. |
| Pile | `NkUILayoutStack` : `Top` `Push` `Pop` `Empty` `Depth` (`MAX_DEPTH=32`) | Pile fixe des layouts (`O(1)`). |
| Calcul | `NkUILayout::NextItemRect` `AdvanceItem` | Obtenir le rect suivant / avancer le curseur. |
| Calcul | `ResolveWidth` `ResolveHeight` | Résoudre une contrainte en pixels. |
| Contrôles | `DrawScrollbar(...&scroll...)` `DrawSplitter(...&ratio...)` | Scrollbar / splitter (état par référence, retour cliqué/glissé). |

### `NkUILayout2.h` — persistance et habillage

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Persistance | `NkUILayoutSerializer` : `Save` `Load` `SaveToMemory` `LoadFromMemory` | Config fenêtres + dock en JSON (`SaveToMemory` **alloue**). |
| Couleur | `NkUIColorPickerFull` : `Draw` `RGBtoHSV` `HSVtoRGB` | Picker complet (16 états max). |
| Police | `NkUIFontNKFont` : `Load` `LoadFile` `AddGlyphRange` | Pont NKFont (atlas 512² Gray8, `NKUI_WITH_NKFONT`). |
| Backend | `NkUIOpenGLRenderer` | Backend GPU OpenGL (**hors périmètre** de cette page). |

### `NkUIDock.h` — docking

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkUIDockNodeType` : `NK_ROOT` `NK_LEAF` `NK_SPLIT_H` `NK_SPLIT_V` | Nature d'un nœud de l'arbre. |
| Enum | `NkUIDockDrop` : `NK_NONE` `NK_LEFT` `NK_RIGHT` `NK_TOP` `NK_BOTTOM` `NK_CENTER` `NK_TAB` | Direction d'ancrage. |
| Nœud | `NkUIDockNode` (`MAX_WINDOWS=16`) | Feuille / split / racine de l'arbre. |
| Décoration | `NkTabDecoFn` (typedef) · `NkTabDecoration` | Callback + struct de bouton d'onglet (**à utiliser**, ceux du namespace). |
| Cycle | `Init` `Destroy` `SetViewport` `BeginFrame` `Render` | Vie du manager + frame. |
| Arbre | `AllocNode` `RecalcRects` `RecalcRectsAll` `Root` `FindNodeAt` | Manipulation de l'arbre. |
| Ancrage | `DockWindow` `UndockWindow` `NodeAllowsTabs` | Ancrer / détacher (respecte `NK_NO_DOCK`/`NK_NO_TABS`). |
| Dessin | `BeginDocked` `EndDocked` · `DrawDropZones` `DrawDirectionalHighlight` | Contenu d'un panneau / retour visuel du drag. |
| Enfant | `BeginChildDock` `EndChildDock` | Dock imbriqué (`NK_ALLOW_DOCK_CHILD`). |
| Persistance | `SaveLayout` `LoadLayout` · `SyncDockedWindowRects` | Sauver/charger le dock ; resync rects. |
| Onglets | `AddTabDecoration` `RemoveTabDecoration` `EnableTabDecoration` `SetDockOverflowButton` | Boutons de barre d'onglets. |

### `NkUIAnimation.h` — animations

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkEase` : `NK_LINEAR` + quad/cubic/quart/sine/expo/elastic/bounce/back (`IN`/`OUT`/`IN_OUT`) + `NK_COUNT` | Catalogue de courbes (`NK_COUNT` = sentinelle). |
| Easing | `NkUIEasing::Apply(ease, t)` (+ inlines `Linear` `In/Out/InOutQuad` `In/Out/InOutCubic` `OutElastic` `OutBounce` `OutBack`) | Courber `t ∈ [0,1]` (**passer par `Apply`**). |
| Tween | `NkUITween` : `Step(dt)` `IsDone` | Une animation (start→end, ease, loop/pingPong, `value`). |
| Pool | `NkUIAnimator` (`MAX_TWEENS=256`) : `Update` `Play` `Toward` `Get` `Get(.,def)` `Stop` `StopAll` `IsPlaying` | Pool de tweens par `id` (**`Play` renvoie `from`**). |
| Effets | `Shake` `Pulse` `FadeIn` `FadeOut` `SlideInFromTop` `Bounce` | Effets prêts à l'emploi (`Play` paresseux). |

---

## Référence complète

Chaque élément est repris ici à fond, avec ses usages dans les différents domaines de l'UI temps réel —
éditeur, jeu, outil, dashboard.

### `NkUILayoutType` — la marche du curseur

Le type d'un nœud de layout décide *comment* `AdvanceItem` fait progresser le curseur, donc la forme
visuelle :

- **`NK_FREE`** (défaut) — positionnement libre, le curseur ne contraint rien. Pour un canevas, un
  éditeur de nœuds, un placement absolu.
- **`NK_ROW`** — empilement horizontal gauche→droite. Barre d'outils, ligne de boutons, en-tête.
- **`NK_COLUMN`** — empilement vertical haut→bas. Panneau de propriétés, liste de réglages, menu.
- **`NK_GRID`** — grille à `gridCols` colonnes. Palette d'icônes, galerie d'assets, sélecteur de tuiles.
- **`NK_SPLIT`** — deux panneaux séparés par un splitter déplaçable. Vue éditeur (hiérarchie | scène),
  comparateur côte à côte.
- **`NK_TAB`** — barre d'onglets. Plusieurs documents dans un même panneau.
- **`NK_SCROLL`** — région défilable. Console de logs, liste longue, inspecteur volumineux.
- **`NK_GROUP`** — groupe à bordure optionnelle. Sous-section visuellement encadrée d'un panneau.

`NK_NONE` ne désigne aucun mode : c'est un état « non défini ».

### `NkUIConstraint` — dimensionner un item

La struct porte un `type`, une `value`, et des bornes `minSize` / `maxSize` (défaut `0` à `1e9`). Les
quatre types, posés via les fabriques :

- **`NK_AUTO` / `Auto_()`** — le widget prend sa **taille naturelle** (la valeur `want` passée à
  `NextItemRect`). Le défaut, quand on ne veut rien imposer.
- **`NK_FIXED` / `Fixed_(px)`** — un nombre **exact de pixels**. Pour une colonne d'icônes de 24 px, un
  bouton calibré, une gouttière.
- **`NK_PERCENT` / `Percent_(p)`** — un **pourcentage** `[0,1]` du parent. Pour « la colonne de gauche
  fait 30 % », un panneau redimensionnable proportionnellement.
- **`NK_GROW` / `Grow_(flex)`** — le widget **grandit** pour absorber l'espace restant, pondéré par le
  *flex*. Pour un champ de recherche qui s'étire entre deux boutons fixes, une zone centrale élastique.

Rappel impératif : ce sont `Auto_`, `Fixed_`, `Percent_`, `Grow_` — **avec underscore final** (pour ne
pas heurter le mot-clé `auto`). Les valeurs de l'enum imbriqué s'écrivent `NkUIConstraint::NK_AUTO`,
etc. (non scopées à un type enum, injectées dans la struct).

### `NkUILayoutNode` et `NkUILayoutStack` — l'état du layout

Un `NkUILayoutNode` est l'état complet d'un niveau : sa zone (`bounds`), son `cursor`, la `contentSize`
cumulée pour le scroll, la hauteur de ligne (`lineH`) en row et la largeur de colonne (`colW`) en
column, les contraintes du prochain item (`nextW`/`nextH`), le défilement (`scrollX`/`scrollY` plus
leurs pointeurs externes `scrollXPtr`/`scrollYPtr` fournis par l'appelant pour persister le scroll),
un `id`, le drapeau `clipped`, un `itemCount`. Les modes spécialisés ajoutent : la grille
(`gridCols`/`gridCol`/`gridCellW`), le split (`splitRatio` + `splitRatioPtr`, `splitVertical`,
`splitSecondPane`), les onglets (`activeTab` pointeur, `tabCount`).

`NkUILayoutStack` est la pile de ces nœuds : 32 emplacements fixes, plus une `depth`. `Top()` rend le
sommet (`&nodes[depth-1]` ou `nullptr`), `Push()` empile (`nullptr` si plein), `Pop()` dépile (no-op si
vide), `Empty()`/`Depth()` renseignent — tout en `O(1)`. **Le piège à ne jamais oublier** : `Push()`
renvoie l'élément réutilisé *tel quel*, sans le réinitialiser ; l'appelant doit configurer le nœud (au
minimum son `type` et ses `bounds`) avant de l'utiliser.

### `NkUILayout` — calculer et dessiner les contrôles

Struct utilitaire **sans état**, uniquement des méthodes statiques :

- **`NextItemRect(ctx, stack, wantW, wantH)`** — le cœur du système : calcule le rectangle du prochain
  widget selon le sommet de pile et sa contrainte. `wantW`/`wantH` est la taille naturelle, retenue
  quand la contrainte est `NK_AUTO`. Renvoie le rect en espace viewport.
- **`AdvanceItem(ctx, stack, placed)`** — déclare qu'un widget a occupé `placed` et fait avancer le
  curseur en conséquence (passe à la ligne en row, descend en column, change de cellule en grid).
- **`ResolveWidth(node, c, want)` / `ResolveHeight(node, c, want)`** — résolvent une contrainte `c` en
  pixels selon le nœud parent et la taille souhaitée `want`. C'est ce que `NextItemRect` appelle en
  interne, exposé pour des calculs sur mesure.
- **`DrawScrollbar(ctx, dl, vertical, track, contentSize, viewSize, scroll, id)`** — dessine une
  scrollbar (verticale ou horizontale) dans `track`, dimensionnée par le ratio contenu/visible, met à
  jour `scroll` (in/out) et renvoie `true` si cliquée. Pour toute région `NK_SCROLL` : log, liste,
  inspecteur.
- **`DrawSplitter(ctx, dl, rect, vertical, ratio, id)`** — dessine la poignée déplaçable d'un split,
  met à jour `ratio` (in/out) et renvoie `true` pendant le glissement. Pour un `NK_SPLIT` :
  hiérarchie/scène, éditeur/aperçu.

L'idiome `NkUI::BeginRow/SetNextWidth/Button/BeginScrollRegion...` cité en tête de header est
**aspirationnel** : ces fonctions ne sont pas déclarées ici, ne comptez pas dessus.

### `NkUILayoutSerializer` — persister la disposition

Sauve et recharge la config fenêtres + dock en **JSON**. `Save`/`Load` opèrent sur un fichier,
`SaveToMemory`/`LoadFromMemory` sur un tampon (avec sa longueur). Le format est un objet `windows`
(liste de `{name,x,y,w,h,open,collapsed,scrollY}`) et un objet `dock` (`nodes` + `root`, chaque nœud
décrivant un split avec `ratio`/`children` ou une feuille avec `windows`/`active`). Cas d'usage : un
éditeur qui rouvre exactement la disposition de la dernière session, des *workspaces* nommés qu'on
charge à la demande, un layout par défaut embarqué. **Piège** : `SaveToMemory` **alloue** `outJson` et
son commentaire indique de le libérer avec `free()` — incohérent avec la règle dure NKMemory du projet,
à vérifier dans l'implémentation avant de s'y fier.

### `NkUIColorPickerFull` — choisir une couleur

Un sélecteur complet : carré saturation/valeur, bande de teinte, bande alpha à damier, sliders RGBA
0-255, saisie hexadécimale `#RRGGBBAA`, et un aperçu avant/après. La seule entrée publique est
**`Draw(ctx, dl, font, ls, id, color)`** : elle affiche le picker, prend `color` en référence (in/out)
et renvoie `true` quand l'utilisateur l'a modifiée. Deux helpers de conversion complètent l'outillage :
`RGBtoHSV` et `HSVtoRGB` (le picker travaille en HSV pour le carré SV et la teinte). Cas d'usage :
réglage d'un matériau, couleur d'une lumière, thème d'éditeur, teinte d'un calque. **Limite** : l'état
interne est plafonné à **16 pickers simultanés** (au-delà, les états sont recyclés).

### `NkUIFontNKFont` — charger une police via NKFont

Pont entre le module NKFont et NkUIFont, actif sous `#define NKUI_WITH_NKFONT`. Il rastérise une police
dans un **atlas 512×512 Gray8**. `Load(fontData, dataSize, pixelSize, atlas, outFont)` charge depuis la
mémoire un fichier TTF/OTF/WOFF/WOFF2 ; `LoadFile(path, pixelSize, atlas, outFont)` depuis un chemin
disque ; `AddGlyphRange(font, atlas, first, last)` ajoute une plage de codes Unicode à l'atlas — l'ASCII
imprimable est `AddGlyphRange(font, atlas, 32, 127)`, et on ajoute des plages pour le latin étendu, le
cyrillique, etc. Cas d'usage : charger la police d'un thème d'éditeur, supporter plusieurs langues,
fournir une police de secours.

### `NkUIOpenGLRenderer` — le backend GPU (hors périmètre)

Backend de rendu OpenGL : initialise un VAO/VBO/EBO et un shader, téléverse l'atlas en texture, soumet
les *draw lists*. C'est un **backend spécifique** (OpenGL), donc hors du périmètre layout/dock/anim de
cette page — il est mentionné pour complétude. Il respecte la règle Create/Destroy de NKMemory en
couplant `Init`/`Destroy`, et expose `BeginFrame`/`Submit`/`EndFrame`, `UploadTexture`/`FreeTexture`,
`SubmitDrawList`. Pour le rendu d'une UI sur un backend donné, voir les pages dédiées au rendu NKUI.

### `NkUIDockNodeType` et `NkUIDockDrop` — la structure et l'ancrage

`NkUIDockNodeType` qualifie un nœud : `NK_ROOT` (la racine de l'arbre), `NK_LEAF` (une feuille qui
contient des fenêtres, jusqu'à 16), `NK_SPLIT_H` / `NK_SPLIT_V` (un partage en deux enfants,
respectivement horizontal et vertical). `NkUIDockDrop` est la **direction d'un dépôt** pendant le drag :
`NK_LEFT`/`NK_RIGHT`/`NK_TOP`/`NK_BOTTOM` créent un split de ce côté (la fenêtre déposée prend une
moitié), `NK_CENTER` et `NK_TAB` ajoutent la fenêtre comme **onglet** du panneau visé, `NK_NONE`
signifie « aucune cible » (le drag retombe sans ancrer).

### `NkUIDockNode` — un nœud de l'arbre

Chaque nœud porte : son `type`, son `rect` calculé, le `splitRatio` (part du premier enfant, défaut
`0.5`), l'index de son `parent` et ses `children[2]` (index, `-1` si absent), la liste `windows[16]` +
`numWindows` (pour une feuille), l'`activeTab` courant, un `scrollY`, et — v2 — `parentWindowId`
(non-`NONE` quand le nœud appartient à un dock enfant logé dans une fenêtre). On manipule rarement un
nœud directement : le manager s'en charge, mais `Root()` et `FindNodeAt` rendent des pointeurs de nœud
pour l'inspection.

### `NkUIDockManager` — orchestrer le docking

Le manager opère sur un tableau de 128 nœuds. Son **cycle de vie** suit Create/Destroy : `Init(viewport)`
fixe la zone de travail et crée la racine, `Destroy()` libère. `SetViewport(r, wm)` recadre quand la
fenêtre principale change de taille.

La **boucle par frame** : `BeginFrame(ctx, wm, dl, font)` traite tous les inputs de docking (détecter un
drag de barre de titre ou d'onglet, calculer la zone de dépôt, ancrer au relâchement) — à appeler
**avant** de dessiner les fenêtres. Puis `Render(ctx, dl, font, wm, ls, rootOverride)` parcourt l'arbre
et dessine barres d'onglets et contenus (`rootOverride` permet de rendre un sous-arbre précis).

Les **opérations d'arbre** : `AllocNode()` réserve un nœud (index, `-1` si plein), `RecalcRects(idx)` /
`RecalcRectsAll()` recalculent les rectangles à partir d'un nœud ou de la racine après un
redimensionnement, `Root()` rend la racine (`nullptr` si `rootIdx < 0`), `FindNodeAt(pos)` retrouve la
**feuille** sous un point (pour savoir où un drag va atterrir).

L'**ancrage** : `DockWindow(wm, windowId, nodeIdx, drop)` ancre une fenêtre dans un nœud selon une
direction, et renvoie le succès. Il honore les flags de fenêtre (définis dans NkUIWindow) : `NK_NO_DOCK`
fait **refuser** l'ancrage ; `NK_NO_TABS` interdit les onglets, donc un dépôt `NK_CENTER`/`NK_TAB` sur
un tel nœud est **dégradé en `NK_BOTTOM`** (un split plutôt qu'un onglet). `UndockWindow` détache une
fenêtre. `NodeAllowsTabs(wm, nodeIdx, draggedWindowId)` renvoie `true` si aucune fenêtre du nœud (ni la
draggée) n'interdit les onglets.

Le **retour visuel** du drag : `DrawDropZones(ctx, dl, nodeIdx)` dessine les zones de dépôt d'un nœud et
renvoie celle survolée (en filtrant `NK_CENTER` si les onglets sont interdits), `DrawDirectionalHighlight`
surligne le bord cible avec une couleur (défaut bleu translucide). Pour dessiner le **contenu** d'un
panneau ancré à la main : `BeginDocked(name)` / `EndDocked`.

Cas d'usage du docking : un éditeur de scène (hiérarchie à gauche, inspecteur à droite, console en bas,
viewport au centre, tous ré-agençables), un outil d'analyse multi-panneaux, un dashboard de debug en
jeu.

### Docking enfant et décorations d'onglets

Le **docking imbriqué** loge un sous-arbre de dock dans la zone cliente d'une fenêtre, encadré par
`BeginChildDock(ctx, wm, dl, font, ls, parentWindowId, childViewport)` / `EndChildDock` ; la fenêtre
parente doit porter `NK_ALLOW_DOCK_CHILD`. Utile pour un panneau qui héberge lui-même des sous-panneaux
ancrables.

Les **décorations d'onglets** ajoutent des boutons personnalisés à droite d'une barre d'onglets, dessinés
de droite à gauche : `AddTabDecoration(fn, userData, width, id)` enregistre une décoration (jusqu'à
`MAX_TAB_DECOS = 8`), `RemoveTabDecoration(id)` la retire, `EnableTabDecoration(id, enabled)`
l'active/désactive, et `SetDockOverflowButton(show)` contrôle le bouton ▶ de débordement (affiché par
défaut). La fonction `fn` est de type `NkTabDecoFn` (reçoit contexte, draw list, police, index de nœud,
rect de décoration et `userData`). **Piège des doublons** : `NkUIDock.h` définit deux paires de noms
quasi identiques — au niveau du namespace `NkTabDecoFn`/`NkTabDecoration` (ceux que l'API et le champ
`tabDecos[]` utilisent vraiment), et à l'intérieur de la classe `NkUIDockManager::TabDecoFn`/
`TabDecoration` (doublons imbriqués, non branchés sur l'API, à la sémantique différente « retour < 0 →
ignoré »). **Employez toujours les types du namespace.**

La **persistance** dédiée : `SaveLayout(path)` / `LoadLayout(path)` sérialisent l'arbre de dock seul ;
`SyncDockedWindowRects(wm)` recopie les rectangles calculés dans les fenêtres ancrées.

### `NkEase` et `NkUIEasing` — les courbes du temps

`NkEase` est le catalogue : `NK_LINEAR`, puis les familles *quad, cubic, quart, sine, expo, elastic,
bounce, back*, chacune en `IN` (départ lent), `OUT` (arrivée lente) et `IN_OUT` (les deux). Le choix de
la courbe donne le *caractère* d'une animation : `OUT_QUAD` pour un fondu sobre, `OUT_BACK` pour une
notification qui dépasse légèrement et revient, `OUT_ELASTIC` pour un effet ressort, `OUT_BOUNCE` pour
une chute qui rebondit. `NK_COUNT` n'est pas une courbe : c'est la sentinelle de fin d'énumération.

`NkUIEasing::Apply(ease, t)` est le **point d'entrée unique** : il prend un `t ∈ [0,1]` (avancement
linéaire) et renvoie le `t` courbé. **Piège** : seules certaines fonctions ont une implémentation inline
dans le header (`Linear`, les quad/cubic, `OutElastic`, `OutBounce` à 4 segments, `OutBack` avec
dépassement `1.70158`) ; toutes les autres courbes de `NkEase` sont résolues *dans* `Apply`
(implémentation hors header). Donc passez systématiquement par `Apply` — appeler directement une
fonction inline d'une courbe non listée échouera.

### `NkUITween` — une animation

POD à logique inline représentant une animation en cours : `id`, `start`/`end`, `duration`, `elapsed`,
`ease`, drapeaux `looping`/`pingPong`/`active`, et la `value` courante. `Step(dt)` l'avance : il ajoute
`dt` à `elapsed`, calcule `t = elapsed/duration` (ou `1` si `duration ≤ 0`), gère le **bouclage**
(`looping` via `fmodf`), le **va-et-vient** (`pingPong`, qui inverse le sens selon la parité du cycle),
ou sinon clampe `t = 1` et désactive le tween, puis met à jour `value = start + (end - start) ×
Apply(ease, t)`. `IsDone()` renvoie vrai quand le tween est inactif et ni bouclé ni ping-pong. On le
manipule rarement seul — l'animator le fait pour nous.

### `NkUIAnimator` — le pool de tweens

Pool fixe de **256 tweens** indexés par `NkUIID`, conservé entre les frames :

- **`Update(dt)`** — avance tous les tweens actifs (`Step` sur chacun). À appeler une fois par frame.
- **`Play(id, from, to, duration, ease, loop, pingPong)`** — démarre ou redémarre l'animation de cet
  `id`, en réutilisant son tween ou en allouant. **Renvoie `from`** (la valeur initiale), pas la valeur
  live — c'est `Get` qui donne la valeur courante. Si le pool est plein, renvoie `to` **sans rien
  créer**.
- **`Toward(id, target, duration, ease)`** — repart de la `value` courante (ou `target` si l'`id`
  n'existe pas encore) vers `target` ; idéal pour un *hover* qui peut s'interrompre. Appelle `Play` sans
  loop ni ping-pong.
- **`Get(id)` / `Get(id, def)`** — la valeur courante du tween ; `0.f` (ou `def`) si l'`id` est inconnu.
  C'est ce qu'on lit chaque frame pour appliquer l'animation.
- **`Stop(id)` / `StopAll()`** — désactivent (le tween garde son slot). **`IsPlaying(id)`** — vrai si le
  tween existe et est actif.

Pièges déjà signalés, à graver : `Play` renvoie `from` (utilisez `Get` pour la valeur live) ; un pool
plein rend l'animation muette ; les slots stoppés ne se libèrent pas (pas de compaction).

### Les effets prêts à l'emploi

Chacun fait un `Play` **paresseux** (seulement si l'effet n'est pas déjà en cours pour cet `id`) et
renvoie une valeur à appliquer :

- **`Shake(id, intensity, duration)`** — un décalage `NkVec2` oscillant amorti (X à fréquence 20, Y à
  fréquence ×1.3 et amplitude ×0.3) ; renvoie `{}` si l'animation n'a pas commencé. Pour signaler une
  **erreur** (un champ invalide qui tremble), un impact.
- **`Pulse(id, amplitude, period)`** — un facteur d'échelle ≈ `1 + amplitude·sin(...)`, lancé **en
  boucle** (linéaire). Pour faire **respirer** un élément qui doit attirer l'œil (un bouton
  d'action requise).
- **`FadeIn(id, duration)` / `FadeOut(id, duration)`** — opacité 0→1 (`OUT_QUAD`) / 1→0 (`IN_QUAD`,
  partant de `Get(id, 1.f)`). Pour l'apparition/disparition douce d'un panneau, d'un *tooltip*.
- **`SlideInFromTop(id, h, duration)`** — un offset Y de `-h` à `0` avec un léger dépassement
  (`OUT_BACK`). Pour une notification qui descend du haut.
- **`Bounce(id, h, duration)`** — un offset Y de `-h` à `0` qui rebondit (`OUT_BOUNCE`). Pour une icône
  qui tombe et s'installe.

**Piège du redéclenchement** : ces effets ne se relancent que si `!IsPlaying(id)`. Pour rejouer un effet
terminé, attendez que son `id` soit redevenu inactif, ou appelez `Play` directement.

---

### Exemple

```cpp
#include "NKUI/NkUILayout.h"
#include "NKUI/NkUIDock.h"
#include "NKUI/NkUIAnimation.h"
using namespace nkentseu::nkui;

// --- Layout : une ligne de boutons, le champ central s'étire ---
NkUILayoutStack stack;
NkUILayoutNode* row = stack.Push();        // ATTENTION : Push() ne nettoie pas
row->type   = NkUILayoutType::NK_ROW;
row->bounds = toolbarRect;
row->cursor = { toolbarRect.x, toolbarRect.y };

row->nextW = NkUIConstraint::Fixed_(80.f);                 // bouton gauche : 80 px (underscore !)
NkRect b1  = NkUILayout::NextItemRect(ctx, stack, 80, 24);
NkUILayout::AdvanceItem(ctx, stack, b1);

row->nextW = NkUIConstraint::Grow_();                      // champ central : prend le reste
NkRect mid = NkUILayout::NextItemRect(ctx, stack, 0, 24);
NkUILayout::AdvanceItem(ctx, stack, mid);
stack.Pop();

// --- Docking : init, puis chaque frame inputs -> render ---
NkUIDockManager dock;
dock.Init(viewportRect);                   // couple Init/Destroy
// boucle :
dock.BeginFrame(ctx, wm, dl, font);        // traite les drags/dépôts
dock.Render(ctx, dl, font, wm, stack);     // dessine onglets + contenus
// fin : dock.Destroy();

// --- Animation : un fondu d'apparition ---
NkUIAnimator anim;
// boucle :
anim.Update(dt);
float32 a = anim.FadeIn(PANEL_ID, 0.25f);  // lance une fois, lit la valeur ensuite
float32 live = anim.Get(PANEL_ID);         // <- la VRAIE valeur (Play/FadeIn renvoient from)
```

---

[← Index NKUI](README.md) · [Récap NKUI](../NKUI.md) · [Couche Runtime](../README.md)
