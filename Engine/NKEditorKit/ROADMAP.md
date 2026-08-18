# NKEditorKit — ROADMAP / journal des décisions

> Créé le 2026-08-17 par l'agent Noge, à l'occasion du correctif d'occlusion de
> la palette de commandes. Le kit n'avait pas de ROADMAP : `ARCHITECTURE.md`
> décrit la structure, `README.md` l'usage, et rien ne consignait les **décisions
> et leurs conséquences pour les applications consommatrices**.

## 🔎 Les consommateurs de ce module (à connaître avant d'y toucher)

`NkEditorShell` est **partagé**. Includes actifs mesurés le 2026-08-17 :

| application | fichiers incluant NKEditorKit |
|---|---|
| **NKCode** | 25 |
| **ConquerorLab** | 8 |
| **NK3DModeler** | 7 |
| **NkAnimaEditor** | 2 |
| NKEditorKitDemo | 2 |
| NKEditMeshHarness | 1 |
| **Nogee** | chemin optionnel `--ui=rhi` (depuis le 2026-08-17) |

**Toute modification du shell arrive chez ces applications sans qu'elles l'aient
demandée.** C'est la raison d'être de ce fichier.

---

## 2026-08-17 — La palette Ctrl+P déclare enfin son occlusion (couche 50)

### Ce qui était faux

`DrawCommandPalette` peignait un **voile plein écran** (`kBackdrop`, l. 2093)
sans jamais appeler `PushOcclusion` ni `NkInputLayerScope`. Le voile était donc
**purement visuel** : un widget d'un panneau ancré (couche 0) restait
**survolable et cliquable dessous**, parce que `ItemHoverable` consulte
`PointReachable` et que rien ne lui avait déclaré cette surface.

**L'asymétrie qui l'a fait voir** — dans le **même kit** :

| surface | déclaration |
|---|---|
| `NkEditorContextMenu.h:165` | `PushOcclusion(box, 50)` ✅ |
| `NkEditorModal.h:192` | `PushOcclusion(box, 100)` ✅ |
| `NkFilePicker.h:539` | `PushOcclusion(plein écran, 100)` ✅ |
| **palette de commandes** | **rien** ❌ |

### Le correctif

Deux lignes, purement additives, sur le patron exact des trois voisines :

```cpp
mUI.PushOcclusion({0.f, 0.f, W, H}, 50);
NkGuiContext::NkInputLayerScope _paletteLayer(mUI, 50);
```

Couche **50** = « menus/palettes/popovers » selon la légende de
`NkGuiContext.h`.

### La mesure, et son témoin

Banc : `Nogee --ui=rhi --occlusion-test` (sonde interrogeant `ItemHoverable`
depuis un **panneau ancré réel**).

```
                        AVANT        APRÈS
palette FERMÉE            1            1     <- le témoin sait toujours dire OUI
palette OUVERTE           1            0     <- le clic ne traverse plus
occlCount, palette ouverte 0            1  (layer=50)
```

⚠️ **Le témoin est ce qui rend ce tableau lisible.** Une première sonde, posée
dans le hook `SetOverlay`, rendait 0 sous le voile — « ça bloque bien ». Rejouée
**palette fermée**, elle rendait 0 **aussi** : elle mesurait le clip de
l'overlay, pas la palette. Il a fallu la refaire depuis un panneau ancré pour
qu'un 0 veuille dire quelque chose.

### ⚠️ CE QUE CE CORRECTIF CHANGE POUR LES QUATRE APPLICATIONS

**Un clic qui passait à travers le voile ne passe plus.** C'est la correction
d'un défaut, mais c'est **un changement de comportement** : une application qui
s'appuyait, sciemment ou non, sur ce clic traversant verra un widget cesser de
répondre pendant que la palette est ouverte.

**Le sens de la panne justifie de l'avoir livré** : le défaut corrigé était
**silencieux** (un clic atteint un widget censé être couvert, personne n'est
prévenu) ; la panne éventuelle est **bruyante** (un clic ne répond pas, visible
au premier essai). On échange un défaut muet contre une panne qui se signale.

⚠️ **Mesuré dans Nogee UNIQUEMENT.** NKCode, ConquerorLab, NK3DModeler et
NkAnimaEditor **n'ont pas été relancés**. Si l'une d'elles se comporte
autrement, ce paragraphe est le point de départ de l'enquête, pas une surprise.

### 🔬 Ce dont le correctif NE dépend PAS — **exécuté**, pas seulement lu

Il a été suggéré que ce correctif serait sans effet dans **ConquerorLab**, parce
que `ConquerorLab/main.cpp:219` appelle `SetMaskBodyOnPopup(false)` (appel
vérifié à cette ligne exacte). **C'est faux, et ça a été mesuré.**

**La course** — condition de ConquerorLab reproduite dans le banc Nogee
(`--occlusion-test --no-mask-body` pose `SetMaskBodyOnPopup(false)` sur le shell
avant `Run()`), sonde interrogeant `ItemHoverable` depuis un panneau ancré :

| | témoin (palette fermée) | palette ouverte | `occlCount` | verdict |
|---|---|---|---|---|
| masquage **actif** (défaut) | 1 | 0 | 1 | le voile bloque |
| masquage **coupé** (ConquerorLab) | 1 | 0 | 1 | **le voile bloque** |

**Le drapeau ne change rien au correctif.** Le témoin vaut 1 dans les deux cas —
la sonde n'est pas devenue aveugle — et `PushOcclusion` s'exécute dans les deux.

⚠️ **Portée de cette course, et elle est étroite** : c'est la **condition** de
ConquerorLab reproduite dans Nogee, **pas ConquerorLab lui-même**, qui n'a pas
été relancé. Est mesurée la proposition causale *« `SetMaskBodyOnPopup(false)`
neutralise l'occlusion »* — fausse. N'est pas mesuré le comportement global de
ConquerorLab, qui diffère par ses panneaux et sa disposition.

**Et voici pourquoi le drapeau ne pouvait pas le neutraliser** — le shell a
**deux mécanismes d'étanchéité distincts** :

| mécanisme | où | désactivable par l'app ? |
|---|---|---|
| **blanchiment de l'entrée du corps** — `modal` ⇒ `mUI.input.mousePos = {-100000,-100000}` + boutons effacés | `NkEditorShell.cpp:693-707` | **oui, partiellement** : `mMaskBodyOnPopup` ne neutralise que le terme `overPopup` |
| **routeur d'occlusion** — `PushOcclusion` ⇒ `PointReachable`, **première porte** de `ItemHoverable` | `NkGuiContext.cpp:491-497` | **non** |

`SetMaskBodyOnPopup(false)` n'agit que sur `overPopup`, donc uniquement sur le
**premier** mécanisme. **Le correctif de la palette passe par le second, que
rien côté application ne désactive.** Il fonctionne donc dans ConquerorLab.

> 🎯 **À retenir avant de retirer cette ligne un jour** : si elle semble sans
> effet quelque part, ce n'est pas `SetMaskBodyOnPopup` qu'il faut regarder,
> c'est `curInputLayer` — `PointReachable` ne bloque que les couches
> **strictement supérieures** à celle en cours de dessin.

---

## 2026-08-17 — La fenêtre Préférences n'a PAS ce défaut (rétractation)

**Écrit parce qu'une conclusion fausse a failli produire un correctif inutile
dans un module partagé.**

`DrawPreferences` peint elle aussi un voile plein écran sans `PushOcclusion` —
la ressemblance avec la palette est frappante, et une première mesure a bien
affiché « le clic traverse le voile ».

**Elle était fausse, et c'est l'instrument qui mentait.** L'expression de
`NkEditorShell.cpp:693` :

```cpp
const bool modal = mShowPrefs || mUI.appModal || overPopup || mCtxOpen;
```

**`mShowPrefs` y figure ; la palette, non.** Quand les Préférences sont
ouvertes, le shell **blanchit déjà l'entrée du corps** (`mousePos` à
`-100000`) : les panneaux sont étanches. La sonde, elle, **forçait**
`input.mousePos` avant d'appeler `ItemHoverable` — elle défaisait donc
exactement la protection qu'elle prétendait mesurer.

**Contrôle qui a tranché** : relever la souris **reçue** par le panneau *avant*
tout forçage.

```
palette ouverte      : souris reçue = normale   -> la fuite était RÉELLE
Préférences ouvertes : souris reçue = -100000   -> VERDICT NUL
```

**Aucune ligne n'a été ajoutée à `DrawPreferences`.** Les deux surfaces se
ressemblent à l'œil et sont protégées par deux mécanismes différents : la
palette par aucun (jusqu'à aujourd'hui), les Préférences par le blanchiment
d'entrée.

> 🎯 **La leçon, pour la prochaine surface flottante ajoutée au shell** :
> demander **par lequel des deux mécanismes** elle est étanche. Ne pas déclarer
> d'occlusion est légitime *si* la surface figure dans l'expression `modal` ;
> sinon, il faut la déclarer. **Ce qu'il ne faut pas, c'est ni l'un ni l'autre —
> c'était le cas de la palette.**

---

# 2026-08-18 — DEVIS D'ARCHITECTURE : la bibliothèque de composants

> **Commandé par Rodolf** (règle du corpus « UNE BIBLIOTHÈQUE DE COMPOSANTS
> RÉUTILISABLES, THÉMABLES ET EXTENSIBLES », plus la directive « PLUSIEURS
> REPRÉSENTATIONS D'UN MÊME COMPOSANT, ET L'ÉDITEUR QUI VIENT » du même jour).
>
> ⚠️ **CECI EST UN DEVIS, PAS UNE IMPLÉMENTATION. Rien n'est monté, rien n'est
> retiré, aucune application n'est touchée.** Les deux seuls fichiers écrits
> (`Components/NkComponentDecl.h`, `Components/NkContentBrowserModel.h`) ne sont
> inclus par personne et ne figurent dans aucune cible de build : ils existent
> pour que la forme proposée soit **lisible et vérifiable**, et le §5 donne la
> commande qui les vérifie.
>
> ⚠️ **Séance sans GPU** (campagne d'entraînement d'Ilyana, 6 824 / 8 192 Mio).
> **Aucun témoin visuel n'a été pris et aucun n'est revendiqué.** Tout ce qui
> suit est de la lecture, de la mesure statique et une compilation. Ce qui
> exigerait de voir à l'écran est nommé comme différé au §9.

---

## 1. L'inventaire — ce qui existe en double, et de combien

Trois piles de rendu coexistent, et c'est la cause mécanique des copies :
**NKGui** (Nogee, PV3DE, NkAnimaEditor), **NKUI** (4 panneaux Nogee encore
vivants, en cours d'extinction), et **`NkModelerPainter` / `NkHitRegistry`**,
réimplémentation maison propre à NK3DModeler.

| Famille | Copies | Lignes cumulées | Foyer existant | Ce qui est déjà mutualisé |
|---|---|---|---|---|
| **Navigateur de contenu** | **3** | ~1 700 | aucun | modèle neutre, côté Nogee seulement |
| **Panneau de propriétés** | **4** | ~8 300 (dont ~1 900 vraiment générique) | `NkEditorInspector.h`, **inclus par 0 application** | rien |
| **Arbre de scène / outliner** | **3** | ~2 200 | aucun | `NkSceneTreeModel.h`, Nogee seulement |
| **Sélecteur de dossier** | **3 + 1 orphelin** | ~1 400 | `NkFilePicker.h` **et** `NkDirBrowser.h` (deux !) | `NkFilePickerState` dérivé 2 fois, correctement |
| **Menu / onglets / barre d'outils / barre d'état** | 5 / 3 / 3 / 2 | ~1 500 | `NkEditorShell.cpp` | partiellement |
| **Widgets atomiques réimplémentés** | 2 à 5 chacun | ~2 400 | NKGui + 6 en-têtes du kit | rien |
| **Console / journal** | **3** | ~740 | aucun | `NkConsoleModel.h`, Nogee seulement |
| **Viewport (surcouches)** | 4 | ~2 200 | aucun | `NkIEditorRenderer` ✅ |
| **Ligne de temps** | **1** | 0 | aucun | — |

### Les trois faits qui commandent le reste

**(a) Le foyer est ignoré, pas absent.** Sept en-têtes du kit
(`NkDirBrowser.h`, `NkEditorCombo.h`, `NkEditorContextMenu.h`,
`NkEditorInspector.h`, `NkEditorCommand.h`, `NkFontPrefs.h`,
`NkEditorCanvasRenderer.h`) sont inclus par **zéro** des quatre applications —
alors qu'ils couvrent exactement des familles dupliquées ci-dessus. Le problème
n'est donc pas « il manque un foyer » mais « le foyer ne prend pas ».

**(b) Le troisième sélecteur de dossier n'est pas un oubli, et c'est plus
instructif.** `NK3DModeler/Shell/NkModelerFileDialog.h` (355 l.) n'inclut aucun
en-tête du kit — mais la **même application** utilise `NkFilePickerState` dans
quatre autres fichiers (`main.cpp:1781`, `NkModelerBrowser.h:126`,
`NkModelerHierarchy.h:828`, `NkModelerProperties.h:6824`). Le kit était connu,
employé à côté, et un dialogue local a quand même été écrit. **Une bibliothèque
qu'on peut contourner sans s'en apercevoir se fait contourner.**

**(c) L'étage NKGui est court-circuité, mesuré.** NK3DModeler n'appelle que
**11** fonctions de widget NKGui dans tout son `src/` (8 `BeginDropTarget`,
3 `BeginDragSource`) — **zéro** `Button`, `Text`, `TreeNode`, `TabBar`,
`Image` — et réimplémente 22 000 lignes d'interface directement sur la draw
list. NKCode n'en appelle **aucune**. Le kit lui-même : **5**.

> 🎯 **Conséquence pour le devis, et elle en change la forme.** Le partage à
> trois étages voulu par Rodolf (NKGui primitives / NKEditorKit composants /
> application spécialisation) suppose que l'étage 1 porte. Aujourd'hui il ne
> porte pas : **l'étage réellement partagé sera le PEINTRE**, pas le widget.
> C'est pourquoi le palier 1 de l'ordre de montée (§6) est le peintre, et non un
> panneau.

### Le navigateur de contenu, en détail (le composant du palier 3)

| | NK3DModeler `NkModelerBrowser.h` (1 213 l.) | Nogee `ContentBrowserPanel` (309 l.) | Nogee `AssetBrowser` NKUI (178 l.) |
|---|---|---|---|
| arbre de dossiers récursif | **oui** | non | non |
| curseur de taille de vignettes | **non** (`tw = 96` en dur, l. 428) | **oui** | oui |
| cache / budget de vignettes par image | non | **oui** | oui |
| **deux états de sélection (actif ≠ choisi)** | **oui, seul** (l. 461-475) | non | non |
| historique arrière/avant (64 entrées) | **oui, seul** | non | non |
| barre d'outils Créer / Importer | **oui, seul** | non | non |
| menu contextuel complet | **oui, seul** | non | non |
| **rôles de thème employés** | **10** (+ 2 couleurs en dur) | **0** (**4 couleurs en dur**) | n/a |
| **littéraux de pixels nus** | **249** | constantes locales | — |

**Les deux copies vivantes sont complémentaires, pas redondantes** : chacune a
ce qui manque à l'autre. C'est la meilleure raison de monter — la fusion **ajoute
des fonctions aux deux** au lieu d'en retirer.

Et la mesure d'écart contre les planches (ROADMAP de NK3DModeler, 18
divergences) conclut que **6 d'entre elles (n° 3, 6, 8, 10, 11, 18) seront
écrites deux fois** si le composant ne monte pas.

---

## 2. Ce qui bloque — les manques mesurés de NKGui

⚠️ **Ces manques ont désormais un propriétaire** : l'agent NKGui
(`Nkentseu-nkgui`, branche `feat/nkgui-complet`). Ils sont listés ici **comme
dépendances de ce devis**, et repris au canal à son intention. **Je n'ai pas
touché `Kernel/Runtime/NKGui/`.**

| Manque | État mesuré | Empêche |
|---|---|---|
| **Contour arrondi** | `AddRect` trace 4 rectangles droits (`NkGuiDrawList.cpp:204-214`) — ne sait pas arrondir | l'encodage de sélection de la planche (**écart n° 3**) ; contourné par 2 rects superposés dans `NkModelerUI.h:216` |
| **Cercle creux** | pas d'`AddCircle`, pas d'arc, pas de bézier | le bouton icône rond de la planche (**écart n° 10**) ; émulé par 2 disques (`Ring`, `NkModelerUI.h:239`) |
| **Atlas d'icônes** | **aucune notion d'icône dans NKGui.** 193 glyphes définis **deux fois** (NK3DModeler 102 en SVG, NKCode 91 en PNG), **une texture GPU par glyphe**, pas d'atlas | tout composant du kit qui doit dessiner une icône — donc **presque tous** |
| **Texte tronqué avec ellipse** | `AddText(maxWidth)` **coupe au glyphe, sans points de suite** | le pied de carte à 2 lignes (écarts n° 11, 16) ; existe seulement dans `TextClipped` (`NkModelerUI.h:366`, tampon 64 o, ASCII) |
| **Texte centré** | helper **interne au `.cpp`**, non exposé | le pied de carte centré de la planche (**écart n° 11**) |
| **Jetons de métrique** | `NkGuiTheme` = 16 couleurs + **3** métriques ; `NkTheme` = 30 rôles + **4** rayons. **Aucun** jeton d'espacement, de hauteur de ligne, d'épaisseur de trait | l'exigence n° 2 de Rodolf : sans métriques thémables, « changer le thème » ne change que les couleurs — la moitié de l'exigence. C'est la source des **249 littéraux** du navigateur |
| **Les deux thèmes ne se parlent pas** | `NkGuiTheme` (16 couleurs) et `NkTheme` (30 rôles) sont **deux systèmes sans lien de code** ; la seule conversion est `NkModelerPainter::Unpack` (`NkModelerUI.h:512`). Rien ne pousse un `NkTheme` dans `ctx.theme` | conséquence visible : les rares widgets NKGui employés (le `SliderFloat` du kit) **ne changent pas de couleur** en thème clair |
| **Sélection à deux états** | `Selectable` ne porte **qu'un bit**. `NkGuiStyleItem` porte pourtant les 4 drapeaux — **aucun widget ne les expose** | la distinction actif / choisi que Rodolf veut voir monter |
| **Ombre portée** | aucune primitive, aucun flou possible | écart n° 15 (la planche montre des cartes plates — donc **ce manque ne bloque pas**, il rend l'écart gratuit) |

**Ce qui existait déjà et que les applications ont ignoré** — à ne pas
redemander : `Splitter` (`NkGuiWidgets.h:234`, écart n° 18),
`CollapsingHeader` (l. 246), `TabBar`/`TabBarEx`/`TabBarEditable` (l. 284-294),
`BeginGrid` (l. 205), `Image`/`ImageButton` (l. 154-158). Le défaut n'est pas
leur absence : c'est qu'ils **exigent de l'appelant** qu'il calcule lui-même
rectangles et redistribution, ce qui rend l'écriture locale moins chère que
l'adoption. **C'est ce rapport de coût qu'il faut renverser, pas la liste de
fonctions.**

---

## 3. La forme d'un composant — écrite sur un composant réel

Démonstration : **le navigateur de contenu**, dans
`src/NKEditorKit/Components/NkContentBrowserModel.h`.

### La signature type, et pourquoi cinq arguments

> ⚠️ **CORRIGÉ LE 18/08 (seconde passe) : c'est SIX, pas cinq.** Il manquait
> l'**entrée**. Un composant signé ainsi dessine et **n'entend jamais rien** —
> ses crochets `onSelect` / `onDoubleClick` / `onContextMenu` seraient des
> pointeurs que rien n'appelle, c'est-à-dire « un paramètre qui n'est pas
> honoré » sous sa forme la plus coûteuse : l'application les remplit et attend.
> C'est la condition « les événements dès maintenant » posée par Rodolf qui a
> rendu le trou visible ; sans elle la forme se figeait à cinq et le mouvement
> se refaisait plus tard. Forme réelle, écrite et compilée :
>
> ```
> resultat  Dessiner( PEINTRE, ENTREE, RECTANGLE, MODELE, STYLE, GREFFES )
> ```
>
> Raisonnement complet dans `NkComponentPaint.h`, bloc `NkComponentInput`.

```
resultat  Dessiner( PEINTRE, RECTANGLE, MODELE, STYLE, GREFFES )
```

| argument | rôle | ce qu'il remplace |
|---|---|---|
| **peintre** | thème + primitives | les **66 champs** de `NkModelerState` que `PaintBrowser` prend aujourd'hui |
| **rectangle** | l'hôte place, le composant ne se place pas | `NkLayout::Compute` propre à NK3DModeler |
| **modèle** | données, **neutre**, propriété de l'application | `st.browser*` (26 champs éparpillés dans l'état global) |
| **style** | **variante** + jetons — aucune couleur, aucun pixel | les 249 littéraux et les 2 couleurs en dur |
| **greffes** | ce que l'application ajoute **sans modifier le kit** | les branches `if (application == ...)` qu'on n'a pas encore écrites |

### Les trois exigences de Rodolf, rendues vérifiables

**1. « Prendre un composant qui lui plaît »** — le modèle est autonome : il
n'inclut ni l'outliner, ni le kit, ni NKGui. Le test est le §5.

**2. « En changeant le thème »** — `NkContentBrowserStyle` ne contient **que**
des `uint16` d'identifiants de rôle et des surcharges optionnelles. La règle est
mécanique et donc revuable : *si un `uint32` de couleur ou un `float` de pixel
apparaît dans le style, c'est un défaut*. Les nombres vivent dans la
**déclaration** (§4), et le dessin écrit `decl.Metric("card_gap")`, jamais
`12.f`.

⚠️ **La nature d'asset n'est pas une énumération du composant** — c'est la
décision qui le rend réutilisable par quatre éditeurs. NK3DModeler a
« procédural » et « dataset », Nogee a « font », PV3DE aura ses natures
médicales : une énumération commune obligerait **chaque nouvelle nature à
modifier le kit**. On stocke donc un **rôle de thème résolu** via le
`NkRoleRegistry` qui existe déjà (`NkTheme.h:139`) : l'application enregistre
`nogee.type_font`, reçoit un identifiant, le pose dans l'entrée. **Le composant
sait le peindre sans savoir ce que c'est.**

**3. « En y intégrant d'autres graphiques »** — `NkContentBrowserHooks` : dessin
surajouté par carte, colonnes supplémentaires, filtre propre, et les décisions
rendues à l'application (`onActivate`, `onContextMenu`, `onDropInto`). **Le
composant ne charge rien, n'ouvre rien, ne supprime rien : il signale.**

Des **pointeurs de fonction** plutôt que des méthodes virtuelles, et c'est un
choix tourné vers NKUIEditor : une déclaration chargée depuis un fichier pourra
les remplir sans qu'une classe existe à la compilation. Le dépôt a déjà fait ce
choix deux fois, et les deux fonctionnent : `NkEditorAppMenuFn`
(`NkEditorShell.h`) et `ctx.styleFn` (NKGui).

### Les deux variantes — la démonstration demandée

`NkBrowserVariant { Grid, DenseList, Columns }` est **un champ du style**, pas
un second composant. `Grid` est la planche du 18/08 ; `DenseList` est une ligne
par entrée à vignette de 16 px ; `Columns` ajoute des colonnes triables.

**Ce que la forme garantit, et c'est le vrai test** : les trois valeurs ne
changent **que** la mise en page et le trait. Elles ne touchent ni `entries`, ni
`active`, ni `chosen`, ni le filtre. Le modèle et la logique de sélection sont
écrits **une fois**.

> ⚠️ **Le critère d'échec, écrit d'avance.** Le jour où une variante réclame un
> champ à elle dans le modèle, ce n'est pas une variante : c'est un second
> composant déguisé en option, et la duplication qu'on chasse vient de rentrer
> par la fenêtre. **Il faudra le dire, pas l'ajouter.**

### La règle qui rend tout ça sûr

Le modèle ne porte **aucun type d'interface**. Le peintre est **déclaré en
avant** (`class NkComponentPaint;`) et les greffes le prennent par référence :
assez pour le compilateur, impossible pour une couleur de fuir. C'est le motif
que le dépôt a déjà écrit **trois fois séparément** — `NkTheme.h` (« en-tête
pur, testable sans lier NKGui »), `NkDirBrowser.h` (`NkDirBrowserState`, dont
`NkOpenWsState` dérive dans NKCode), `Nogee/Panels/Model/*.h` — et qu'on ne fait
ici que **nommer une bonne fois**.

---

## 4. La forme de déclaration — ce qui rend NKUIEditor possible plus tard

`src/NKEditorKit/Components/NkComponentDecl.h`.

> **Un éditeur ne peut composer que ce qui est décrit par des données.** Un
> composant qui n'existe qu'en C++ compilé ne peut être ni assemblé, ni
> paramétré, ni sauvé.

Cinq tables, toutes en `const char*` et en tableaux statiques — une déclaration
est une **constante de compilation** : elle ne s'alloue pas, elle peut vivre en
données en lecture seule, et le jour où NKUIEditor la chargera depuis un
fichier il construira **les mêmes structures**. La forme ne change pas, seule la
provenance.

| table | ce qu'elle déclare | pour le navigateur |
|---|---|---|
| `NkParamDecl` | paramètres, avec type, défaut et bornes | 4 (`thumb_size`, `show_tree`, `show_footer`, `tree_width`) |
| `NkVariantDecl` | les représentations | 3 (`grid`, `dense_list`, `columns`) |
| `NkTokenDecl` | jetons de couleur → rôle de thème hérité | 10 |
| `NkMetricDecl` | jetons de métrique, en pixels **logiques** | 7 |
| `NkHookDecl` | points de greffe, nommés et signés en clair | 6 |

**Le point qui fait tout le travail** : la déclaration n'est **pas de la
documentation**, c'est la **source des nombres**. `decl.Metric("card_gap")`
plutôt que `12.f`. Il devient alors **impossible** que la description et le
dessin divergent — un éditeur qui lit la déclaration lit ce que le dessin
applique.

**Le patron est copié délibérément de `NkTheme.h`** : clé stable + énumération,
tables append-only, et un **registre** pour énumérer ce qui existe
(`NkComponentRegistry`, même forme que `NkRoleRegistry`). L'éditeur a besoin
d'énumérer ; une liste écrite en dur dans son code ne le lui donnerait pas.

⚠️ **Ce qui n'est PAS tranché ici, et ne doit pas l'être** : le **format de
fichier** est une décision de Rodolf. Cette structure se sérialise dans
n'importe lequel — c'est justement pourquoi elle peut être écrite avant que le
format soit choisi. ~~`NkComponentRegistry` est **déclaré et défini nulle part**~~ — ⚠️ **plus vrai
depuis le 18/08 (seconde passe)** : il est défini dans
`Components/NkComponentRegistry.cpp`. Il a fallu le définir dès que NkUIDesign
a dû **lister** les composants : une liste écrite en dur dans l'éditeur aurait
affiché les bons noms sans qu'une seule déclaration soit lue — elle aurait
« marché » en ne prouvant rien.

### ⚠️ RECTIFICATIF — j'ai proposé une déclaration en ignorant que le dépôt en a déjà deux et demie

**C'est la partie la plus importante de ce devis, et elle me contredit.** Une
mesure faite après coup change la donne, et la taire aurait produit exactement
le défaut que ce document existe pour combattre.

**(a) Le langage de description existe déjà, écrit.**
`Applications/NKUIDesign/` porte **1 053 lignes de spécification**, dont
`2_NkUIDesign_Langage_Description_NodeBlueprint.md` (**342 l.**) : langage
`.nkgui` **version 0.2**, quatre sections (`geometry` / `widgets` / `behavior` /
`controller`+`callback`), lexique complet (`Identifier`, `String`, `Number`,
`Color #RGB|#RGBA`, `Vec2`), mots-clés réservés, directive `include`, et une
phrase qui vise juste : *parseur « partagé entre NkUIDesign (auteur) et le
runtime NKGui (exécution) »*. **Zéro `.h`, zéro `.cpp`** — c'est de la spec
pure.

> 📌 **Et le nom** : « NKUIEditor » **n'apparaît nulle part dans le dépôt**. Le
> nom déjà écrit est **NkUIDesign**. À trancher par Rodolf — deux noms pour une
> chose, c'est le motif « une liste et son compte séparés » appliqué à un projet.

**(b) Le descripteur de paramètre existe déjà, et il est meilleur que le mien.**
`NKReflection` (9 621 l.) porte `NkEditableProperty` (`NkInspector.h:63`) avec
`name`, `displayName`, `type`, `category`, `value`, `metaFlags`,
`hasRange/rangeMin/rangeMax`, `tooltip`, `group`, `readOnly`, `hidden`,
`isContainer`, `isObject` — plus **25 drapeaux de métadonnées**
(`NkReflectMeta.h:46`) dont `RANGE`, `COLOR_PICKER`, `MULTILINE`, `READONLY`,
`ADVANCED`. Il sait aussi **instancier par nom** (`NkClass::CreateInstance`,
`NkClass.h:443`). **Mon `NkParamDecl` en est un sous-ensemble appauvri.**

**(c) Le panneau de propriétés générique existe déjà, écrit et complet.**
`NkEditorInspector.h:30` — `DrawInspector(ctx, obj, cls)` génère, pour toute
classe réfléchie, une grille à deux colonnes avec `Checkbox`, `SliderFloat` si
`hasRange` sinon `DragFloat`, `DragInt`, `InputText`, combo d'enum aux **noms
symboliques**, `CollapsingHeader` récursif, grisage `readOnly`, écriture live.
**136 lignes, header-only, utilisées par zéro application.** C'est l'inspecteur
de NkUIDesign, déjà fait.

**Et il y a déjà DEUX réflexions concurrentes** : `NKReflection` (runtime) et
une réflexion par macros `NK_REFLECT_BEGIN` **dupliquée** dans NKECS
(`NkReflect.h:262`) et NKSerialization (`Native/NkReflect.h:391`) — **le même
nom de macro dans deux modules**.

> 🎯 **Ce que ça oblige à écrire.** Un devis qui prétend supprimer la
> duplication ne peut pas ajouter **une quatrième** description sans le dire.
> `NkComponentDecl` **ne doit pas devenir un système de plus**. Trois issues,
> et le choix appartient à Rodolf :
>
> 1. **Réconcilier** — `NkComponentDecl` devient une *vue* sur `NKReflection`
>    (le composant se déclare `NK_CLASS`, la déclaration se génère). Le plus
>    propre ; le plus cher ; et il bute sur le fait que **NKReflection a 16
>    types enregistrés, tous en démos/tests, aucun dans une bibliothèque du
>    moteur** — l'adoption est le vrai coût, pas l'écriture.
> 2. **Garder les deux, avec une frontière écrite** : `NKReflection` décrit des
>    *objets de données* (à l'exécution, avec allocation et enregistrement),
>    `NkComponentDecl` décrit des *composants d'interface* (constante de
>    compilation, zéro allocation, zéro enregistrement, lisible sans lier quoi
>    que ce soit — la propriété qui a rendu le banc du §5 possible).
> 3. **Abandonner `NkComponentDecl`** et partir de la spec `.nkgui` v0.2, qui
>    est plus ambitieuse (elle couvre aussi le comportement et les blueprints).
>
> ✅ **TRANCHÉ PAR RODOLF LE 18/08 : issue (2), avec deux conditions** — la
> frontière écrite noir sur blanc, et **les événements dans la forme dès
> maintenant**. `NKReflection` n'est ni dépréciée ni concurrencée : elle a un
> autre domaine, et elle servira ailleurs. La cible reste `.nkgui` v0.2 :
> l'issue (2) est le chemin, pas la destination. **Livré** — voir la section
> « LA TRANCHE VERTICALE » en fin de fichier.
>
> **Je ne tranchais pas** : les trois sont défendables et c'est une décision
> d'architecture qui engage des années. **Mon avis, dit comme un avis** : (2)
> pour les six composants des paliers 3-6, parce que la propriété « constante de
> compilation, vérifiable sans rien lier » est ce qui rend la montée sûre — puis
> (1) quand NkUIDesign démarrera pour de bon. Mais l'option (3) est la seule qui
> parte de ce que Rodolf a **déjà fait écrire**, et ça compte.

### ⚠️ Pourquoi les trois existants n'ont AUCUN utilisateur — la mesure qui doit précéder l'arbitrage

*Ajouté le 18/08 sur demande du coordinateur : un système sans utilisateur a une
raison de ne pas en avoir, et cette raison se reproduira sur le quatrième.*

| système | ce qu'il couvre vraiment | ce qui lui manque | **pourquoi 0 utilisateur** |
|---|---|---|---|
| **`NkEditorInspector.h`** (136 l.) | grille complète : slider si `hasRange`, combo d'enum aux **noms symboliques**, sections récursives, `readOnly`, écriture live | conteneurs (`NkVector` → affiche `"(liste)"`), pointeurs. **Ni variantes, ni jetons, ni greffes** — il décrit un objet de données, pas un composant | **il n'a rien à inspecter** : *zéro* type réfléchi dans `Kernel/` + `Engine/` hors NKReflection et hors tests |
| **`NKReflection`** (9 621 l.) | `NkEditableProperty` + **25 drapeaux** (`RANGE`, `COLOR_PICKER`…), `CreateInstance` par nom. **Superset de `NkParamDecl`** | `NkVector`/pointeurs non gérés par le pont ; enums sérialisés **en valeur, pas en nom** (fragile au réordonnancement — le défaut que `NkTheme` évite) | enregistrement **manuel type par type**, coût à l'entrée, bénéfice à la sortie, **et rien ne casse si on ne le fait pas**. Hors NKReflection : **7 fichiers**, dont 4 de tests |
| **spec `.nkgui` v0.2** (342 l.) | EBNF complète, 4 sections, node blueprint, `include`, table rôle → API | **structurel** : sa table §8 mappe 1:1 vers les **primitives**. **Aucune entrée composite** — ni `ContentBrowser`, ni `PropertyRow`. Elle sait assembler l'étage 1, pas nommer l'étage 2 | elle vise `Splitter`, `TabBar`, `Table`, `CollapsingHeader` — **les fonctions mêmes que les trois applications ont contournées** |

**Quatre contournements mesurés, un seul motif :**

| brique partagée | existait | réécrite localement |
|---|---|---|
| `Splitter` (`NkGuiWidgets.h:234`) | oui | NK3DModeler — c'est l'écart n° 18 |
| `NkFilePicker` — utilisé dans **4** fichiers de NK3DModeler | oui, **et connu** | `NkModelerFileDialog.h`, 355 l. |
| `NkEditorInspector.h::DrawInspector` | oui | **`NKGuiDemo/main.cpp:83` — 58 l., même nom, sans inclure le kit** |
| `NkDirBrowser.h` | oui | orphelin, 0 utilisateur |

La troisième ligne est la preuve : **celui qui avait exactement besoin de la
fonction partagée l'a réécrite.** Ce n'est pas de la négligence — c'est un calcul
de coût, et il a eu raison à son échelle.

> 🎯 **La loi**, et elle vaut pour le quatrième système : *dans ce dépôt, une
> brique partagée n'est pas ignorée par oubli — elle est écartée par un calcul de
> coût que l'auteur refait à chaque fois, et qu'il gagne.* Elle exige une
> conversion (se déclarer, calculer ses rectangles, ajouter une dépendance au
> `.jenga`) plus chère que la réécriture, **et rien ne signale qu'on la
> contourne**.
>
> Le critère d'adoption n'est donc **pas la richesse de la description** —
> `NKReflection` est le plus riche et le moins adopté — mais **le coût du premier
> pas**. D'où la seule chose à défendre quelle que soit l'issue choisie : **la
> description doit être un sous-produit de l'écriture du composant, jamais un
> préalable.** C'est pourquoi les nombres vivent *dans* la déclaration et que le
> dessin *la lit* : l'auteur ne déclare pas en plus, il écrit ses métriques
> ailleurs, et il y gagne tout de suite (un seul point de vérité). Le jour où
> déclarer coûte une étape de plus que ne pas déclarer, le quatrième système
> rejoindra les trois autres.

### Ce que le dépôt a déjà, et qui sert — mesuré

| brique | état | réutilisable pour décrire des composants ? |
|---|---|---|
| **NKSerialization** — 21 451 l., 6 formats (JSON, XML, YAML, binaire, **NKS1** magic `0x314B534E`, auto-détection) | mûr, versionnement de schéma sur 1 581 l. | **oui** — le format n'est pas un obstacle, il y en a six |
| **`NkReflectSerializer`** (pont réflexion ↔ archive, 729 l.) | fonctionne : primitifs, string, enum, objets imbriqués, héritage | **partiel** — `NkVector` et pointeurs **non gérés** ; enums écrits en **valeur numérique, pas en nom** (fragile au réordonnancement) |
| **`NKEvent`** — 29 104 l., ~118 classes, **367 abonnements sur 76 fichiers** | très mûr et **réellement adopté** | **oui pour les actions** |
| **`NkActionManager` / `NkAxisManager`** (`NkEventDispatcher.h:709`, `:832`) | actions adressées **par chaîne** | **oui — le meilleur candidat** pour un `on_click: "SaveDocument"` lu dans un fichier. Le câblage nom → handler reste à écrire |
| **`NkCustomStringEvent`** (`NkCustomEvent.h:450`) | existe | **oui** — émettre une action nommée sans nouveau type C++ |
| **Glisser-déposer NKGui** | **générique, charge opaque**, type = chaîne libre ; 20 sites, 2 applications | **oui tel quel** — ⚠️ **plafond dur de 256 octets** : un descripteur devra passer par **poignée/index**, jamais par valeur |
| **`NkTheme::Save/Load`** | fonctionne | **le modèle est le bon** : héritage depuis une base complète, `outUnknown` tolère l'inconnu au lieu d'échouer, enum append-only + nom. **Trois principes à reprendre tels quels** |
| **`NkEditorShell::SaveUiState/LoadUiState`** | **fonctionne vraiment** — arbre de dock en DFS, plafond 256 nœuds | preuve de faisabilité, mauvais véhicule (texte maison au `snprintf`, hors NKSerialization) |
| **`NkEditorShell::SaveLayout/LoadLayout`** | ⚠️ **STUBS VIDES** (`NkEditorShell.h:352-360`, `return false`), et le seul appelant est une démo | **non** — à budgéter |
| **`NkShortcutTable`** | Bind/Rebind/conflits, drapeau `userDefined` | ⚠️ **aucune persistance** : un raccourci remappé est perdu à la fermeture. À budgéter |
| **`NkGuiInput`** (`NkGuiInput.h:30`) | struct plate, NKGui **ne connaît pas NKEvent** (0 include, délibéré) | **atout à préserver** : NKGui est pilotable par n'importe quelle source, donc **rejouable** par un éditeur |

⚠️ **Le vrai trou, et il n'est pas où je le cherchais** : **NKGui est en mode
immédiat**. Aucun de ses 116 points d'API n'est descriptible par une donnée, il
n'inclut ni NKSerialization ni NKReflection, et **il n'existe aucun arbre de
widgets retenu**. « Composer et sauver une interface » suppose une **couche
retenue** — arbre de nœuds + fabrique « nom → widget » + registre de propriétés.
**Aucune ligne n'existe.** C'est le cœur de NkUIDesign, et c'est hors de ce
devis.

⚠️ **NkUIDesign n'est pas engagé.** La rentrée de septembre passe avant, et le
calendrier est à Rodolf. Ce paragraphe ne demande qu'une chose : **que les
composants montent sous une forme qui ne l'interdise pas.**

---

## 5. Le banc de neutralité — la preuve, sans ouvrir de fenêtre

La question « ce fichier compile-t-il sans NKGui ? » est **exécutable**. Depuis
la racine du dépôt :

```sh
g++ -std=c++17 -Wall -fsyntax-only \
  -I Engine/NKEditorKit/src \
  -I Kernel/Foundation/NKCore/src -I Kernel/Foundation/NKContainers/src \
  -I Kernel/Foundation/NKMemory/src -I Kernel/Foundation/NKMath/src \
  -I Kernel/Foundation/NKPlatform/src  sonde.cpp
```

Aucun chemin NKGui n'est fourni. La sonde inclut les deux en-têtes, construit un
modèle, choisit la variante `DenseList` et lit une métrique dans la déclaration.

| ce qui est compilé | résultat |
|---|---|
| `NkComponentDecl.h` + `NkContentBrowserModel.h` | **passe (0)** |
| `NkTheme.h` seul (contrôle : en-tête neutre déjà existant) | **passe (0)** |
| **`NkEditorInspector.h`** — en-tête du **même kit**, qui dépend de NKGui | **échoue (1)** : `fatal error: NKGui/NKGui.h: No such file or directory` |

⚠️ **La troisième ligne est ce qui rend les deux premières lisibles.** Un banc
qui ne sait dire que « oui » ne mesure rien. Même commande, mêmes chemins : un
en-tête du kit qui a réellement besoin de NKGui **échoue**, les deux nouveaux
**passent**. Le `0` veut donc dire quelque chose.

*(Première tentative de témoin, écartée : un fichier incluant explicitement
`NkGuiContext.h`. Il échouait — mais parce que le chemin n'était pas fourni, pas
parce que le modèle le refusait. Il aurait « prouvé » exactement la même chose
avec n'importe quel en-tête inexistant. Remplacé par le témoin ci-dessus, qui
discrimine.)*

⚠️ **Portée de ce banc, et elle est étroite** : il compile avec **g++ en
`-fsyntax-only`**, pas avec le compilateur de production, et il ne lie rien. Il
répond à *« un type d'interface a-t-il fui dans le modèle ? »* — et à rien
d'autre. Il ne dit rien du rendu, qui demanderait une fenêtre.

---

## 6. L'ordre de montée — du meilleur rendement au moindre

| # | palier | pourquoi ici | coût | dépend de |
|---|---|---|---|---|
| **0** | **Jetons de métrique dans `NkTheme`** | sans eux, « changer le thème » ne change que les couleurs. 249 littéraux dans le seul navigateur. **Le moins cher du lot** : une table à côté des rayons, qui existent déjà | ~150 l. | rien |
| **1** | **Le peintre partagé** — `NkModelerPainter` (`NkModelerUI.h`, 571 l.) monte en `NkComponentPaint` | **rien d'autre ne peut monter avant.** L'étage NKGui est court-circuité (11 appels dans NK3DModeler, 0 dans NKCode) : le substrat réellement partagé est le peintre. Il est **déjà écrit, déjà thémé par rôles, déjà aligné au pixel** | ~600 l. déplacées | palier 0 ; l'atlas d'icônes (agent NKGui) pour `Icon()` |
| **2** | **Fusionner les 3 sélecteurs de dossier en 1** | **le geste le moins cher à effet immédiat**, et le seul qui ne demande aucune brique neuve : `NkModelerFileDialog.h` (355 l.) dérive de `NkFilePickerState`, `NkDirBrowser.h` (orphelin) fusionne dans `NkFilePicker.h`. Le mécanisme d'extension **est déjà prouvé** — `NkModelerPicker` et `NkCodeDialogs` en dérivent correctement | ~400 l. retirées | rien |
| **3** | **Le navigateur de contenu** | **déjà écrit 3 fois** ; maquette la plus complète (2 captures + contrat de props §4.2) ; **6 des 18 écarts seraient écrits deux fois** ; les deux copies vivantes sont **complémentaires** — la fusion ajoute des fonctions aux deux | ~900 l. | paliers 0-1 ; ellipse + contour arrondi (agent NKGui) |
| **4** | **L'arbre / `TreeView`** | **un composant, quatre usages** : outliner, arbre de dossiers du navigateur, navigateur de projet, visionneuse de classes. Contrat de props §4.3 déjà écrit, `extraColumns` y est **déjà** un point de greffe. Chaque copie a ce qui manque aux autres (Maj+clic chez NK3DModeler, garde anti-cycle et colonne Layer chez Nogee) | ~700 l. | palier 3 (mêmes briques) |
| **5** | **La console** | **la famille où le spécifique est quasi nul** — le rapport ne lui trouve « rien de substantiel ». Petite, sûre, et elle valide la forme sur un cas facile | ~250 l. | palier 1 |
| **6** | **`PropertyRow` + section repliable** (pas le panneau) | 8 300 l. dont **~1 900 seulement sont génériques**. On monte la **ligne** (vecteur3, cadenas de liaison, code couleur d'axes, champ à glissement) et l'**en-tête de section**, jamais les 6 400 l. de sémantique 3D. Contrat de props §4.1 écrit | ~600 l. | palier 1 |
| **7** | **Surcouches de viewport** | le cadre « texture externe + surcouches », le cube de navigation, les combos d'affichage. La 3ᵉ capture du 18/08 en est la planche — **non chiffrée** | ? | mesure d'écart du viseur, non faite |
| **8** | **La ligne de temps** | ⚠️ **le seul composant PAS ENCORE dupliqué** (1 copie, `NkAnimaEditor/Panels.h:23-142`). Rendement immédiat nul — mais c'est **le seul qu'on puisse écrire générique du premier coup**, sans dette à défaire. À faire quand NkAnima y retouchera, pas avant | — | forme validée aux paliers 3-5 |

**Le fil qui relie les paliers 0 à 3** : chacun est une dépendance dure du
suivant, et le palier 2 est glissé au milieu parce qu'il **ne dépend de rien** —
il rapporte pendant que le peintre se déplace.

---

## 7. Le coût de l'écart — les deux nombres

### Descriptible maintenant, contre descriptible après coup

| | maintenant | après coup |
|---|---|---|
| **quoi** | écrire la déclaration **à côté** du composant, pendant qu'on le monte | **rouvrir** chaque surface de dessin, en extraire les nombres et les couleurs, réécrire les appels |
| **combien** | **~40-60 l. de déclaration par composant** + ~200 l. d'infrastructure (registre + chargeur). Pour les 6 composants des paliers 3-6 : **~500 l.** | le périmètre des surfaces de panneau à rouvrir, mesuré fichier par fichier : **15 344 l.** |
| **rapport** | **1** | **~30** |

Le second nombre n'est pas une estimation d'effort mais **une mesure de
périmètre** : c'est le total réel des 16 fichiers de panneaux des quatre
applications (`NkModelerProperties.h` 7 909, `NkModelerHierarchy.h` 1 642,
`NkModelerBrowser.h` 1 213, `NkModelerWidgets.h` 870, `NkModelerUI.h` 571, les
9 panneaux de Nogee, `NkAnimaEditor/Panels.h`). Toutes ces lignes ne seraient pas
réécrites — mais toutes devraient être **relues** pour savoir lesquelles.

⚠️ **Ce que ces nombres ne disent pas**, et il faut le dire avant que quelqu'un
les relaie sans :

- les ~500 l. sont une **projection** à partir d'une seule déclaration
  réellement écrite (le navigateur : 30 entrées, ~90 l. avec ses commentaires).
  Les cinq autres composants n'ont pas été déclarés. Le premier nombre est donc
  **du même ordre**, pas exact. Le second, lui, est un **décompte**.
- ils supposent l'issue (2) du rectificatif du §4. **L'issue (1)** —
  réconcilier avec `NKReflection` — remplacerait ces ~500 l. par un coût
  d'**adoption** que je n'ai pas chiffré (16 types réfléchis aujourd'hui, aucun
  dans une bibliothèque du moteur), et qui est probablement plus élevé à court
  terme et plus bas à long terme.
- **le rapport ~30 tient quelle que soit l'issue** : il oppose « écrire la
  description pendant qu'on monte » à « rouvrir 15 344 lignes après ». C'est
  cette asymétrie-là que Rodolf a nommée, et elle ne dépend pas du mécanisme
  choisi.

### Monter, contre recopier — l'estimation qui existait déjà

Le dépôt avait déjà chiffré la version « composants » de la même question
(`Engine/Noge/CARNET.private.md:420-432`) : **~11 000 l. génériques par
éditeur** ; copier = ~44 000 l. dont **~33 000 dupliquées** (à corriger quatre
fois) ; extraire = **~11 000 une fois**. **Facteur 4 sur la dette, pour le même
résultat visuel.** Ce devis ne la remplace pas, il la confirme par une autre
route.

---

## 8. Ce que ça change pour les applications consommatrices

**Rien, aujourd'hui.** Aucun fichier d'application n'est touché ; les deux
en-têtes ajoutés ne sont inclus par personne.

**Le jour où le palier 1 partira**, ce sera un déplacement de `NkModelerPainter`
hors de NK3DModeler : cette application est **la seule** concernée, et c'est un
changement de chemin d'include, pas de comportement. À écrire ici à ce
moment-là, comme l'exige l'en-tête de ce fichier.

---

## 9. Ce qui n'a pas été fait, et les arbitrages qui restent à Rodolf

**Non fait, et nommé comme tel :**

- **Aucun témoin visuel.** Séance sans GPU. Rien de ce qui suit n'a été vu à
  l'écran, et **aucune conformité aux planches n'est revendiquée** — le devis dit
  ce qu'il faudrait pour y être conforme, pas qu'on y est.
- **Le peintre n'est toujours pas monté** — et il ne le sera pas par moi : son
  extraction appartient à l'agent NK3DModeler, je la **reçois**. ⚠️ Mais la
  fonction de dessin, elle, **est définie** (`NkContentBrowserDraw.cpp`) : elle
  ne dépend pas de son peintre, elle dépend de l'**interface**
  `NkComponentPaint` que ce peintre satisfera. C'est ce qui a permis de livrer
  sans lui prendre son travail.
- ~~**`NkComponentRegistry` est déclaré, défini nulle part.**~~ **Défini.**
- **Cinq des six composants n'ont pas de déclaration écrite** — seul le
  navigateur en a une.
- **Aucune mesure d'écart du viseur** contre la 3ᵉ capture (palier 7 non
  chiffré).

- **La spec `.nkgui` v0.2 (342 l.) n'a pas été dépouillée ligne à ligne** — je
  l'ai mesurée et située, pas lue en entier. Le devis en tient compte au §4 ;
  il ne la remplace pas.

**Arbitrages qui appartiennent à Rodolf, posés et non tranchés :**

1. **`NkComponentDecl` contre `NKReflection` contre la spec `.nkgui`** — les
   trois issues du rectificatif du §4. **C'est l'arbitrage le plus lourd de ce
   devis** : il décide si le dépôt aura une description d'interface ou une
   quatrième.
2. **Le nom** : le dépôt a écrit **NkUIDesign** (1 053 l. de spec) ; la
   directive dit **NKUIEditor**. Le terme « NKUIEditor » n'existe nulle part
   dans le code.
3. **L'encodage des deux états de sélection** (écart n° 3, déjà posé par l'agent
   NK3DModeler). La planche réserve l'aplat à l'arbre et le contour aux cartes ;
   prise à la lettre pour la carte active, **les deux états deviennent des
   contours et la distinction meurt**. Le modèle proposé porte les deux états
   **sans préjuger** de leur peinture — il n'enferme aucune des deux réponses.
4. **Le format de fichier des déclarations** (§4) — six véhicules existent déjà,
   aucun n'est imposé.
5. **Français ou anglais** dans les libellés (écart n° 17) — écart de langue,
   pas de structure.

**À budgéter, découvert en chemin et sans rapport direct avec les composants :**
`SaveLayout`/`LoadLayout` du kit sont des **stubs vides** alors que l'API est
publique et appelée par la démo ; `NkShortcutTable` **n'a aucune persistance**,
donc un raccourci remappé par l'utilisateur est perdu à la fermeture. Ni l'un ni
l'autre n'a été touché.

**Repris au canal à l'intention de l'agent NKGui** : contour arrondi, cercle
creux, atlas d'icônes, ellipse, texte centré, jetons de métrique, et la jonction
`NkGuiTheme` ↔ `NkTheme` (§2).

---

# 2026-08-18 (seconde passe) — LA TRANCHE VERTICALE : NkUIDesign lit la déclaration

> **Commandé par Rodolf le 18/08**, en tranchant l'issue (2) du §4 : la frontière
> écrite noir sur blanc, **les événements dans la forme dès maintenant**, et
> NkUIDesign livré **maintenant** — en tranche verticale, pas « la totale ».
>
> ⚠️ **Séance sans GPU** (campagne d'Ilyana). **Aucune fenêtre n'a été ouverte,
> aucun témoin visuel n'a été pris, et aucune conformité aux planches n'est
> revendiquée.** L'éditeur compile ; il n'a jamais été vu. Ce qui est prouvé
> l'est par un banc headless, et ce banc ne juge pas un pixel.

## 1. Pourquoi une tranche, et pas la déclaration seule

C'est ma propre loi qui l'impose. La mesure du §4 dit qu'une brique partagée
n'est pas ignorée par oubli mais **écartée par un calcul de coût**. Une
déclaration **sans consommateur** serait donc le **quatrième système dormant**, à
côté de `NKReflection`, de l'interpréteur blueprint et de `NkEditorInspector.h`.
**Le consommateur construit en même temps est ce qui rend la déclaration
rentable tout de suite** — et c'est la seule protection contre le motif que ce
chantier existe pour arrêter.

## 2. Ce qui est livré

| pièce | fichier | ce qu'elle apporte |
|---|---|---|
| **la frontière** | `NkComponentDecl.h`, bloc en tête | `NKReflection` = objets de données (exécution, allocation) ; `NkComponentDecl` = composants d'interface (constante de compilation). **Règle opératoire** : *si ça peut être INSTANCIÉ c'est `NKReflection` ; si ça peut être DESSINÉ c'est `NkComponentDecl`* |
| **les événements** | `NkArgKind` / `NkArgDecl` / `NkEventDecl` | 5 événements déclarés **avec leur charge** sur le navigateur |
| **la convergence `.nkgui`** | `NkWriteControllerBlock()` | **produit** le bloc `controller` de la spec §10 — pas « compatible en principe » |
| **l'instance** | `NkComponentInstance.h` | les écarts, mutables et sauvés, **séparés** de la déclaration qui reste constante |
| **le peintre (interface)** | `NkComponentPaint.h` | le contrat de réception du peintre de NK3DModeler |
| **le dessin** | `NkContentBrowserDraw.cpp` | **pas un seul nombre de pixels écrit** : tout passe par `M("...")` |
| **l'éditeur** | `Applications/NKUIDesign/` | charge, affiche, édite en direct, sauve |
| **le banc** | `NKUIDesign --probe` | **21/21**, `Build/Bin/Debug-Windows/NKUIDesign/nkuidesign_probe.txt` |

**Build : 201/201, SUCCESS.**

## 3. Le témoin, et ce qu'il ne dit pas

Les trois contrôles qui rendent les autres lisibles : **témoin de bruit** (deux
passes identiques → 0 différence sur 88 commandes) ; **contrôle positif**
(`card_gap` 12 → 40 → 57 commandes changent) ; **contrôle négatif** (une clé
inconnue de la déclaration → **0 écrasement retenu, 0 différence**).

Le cœur du « sans recompiler » est l'essai 6 : **écrit → texte → relu → même
dessin** (0 différence avec l'écrit, 57 avec la référence). Le dessin suit un
**fichier texte**, pas un littéral C++.

⚠️ **Deux défauts de la sonde trouvés par la sonde elle-même**, et le second est
le plus instructif : le point de clic en dur (60, 300) tombait **dans la colonne
d'arbre**, donc aucun événement ne partait. Les essais 10 et 11 échouaient — mais
**10b PASSAIT** : « la charge `path` n'est pas vide » était vrai *parce qu'aucune
charge n'existait*. **Un succès à vide**, face n° 2 de la grille. Corrigé des deux
côtés : le point de clic se **calcule depuis la déclaration**, et l'assertion sur
la charge exige d'abord qu'une charge existe.

⚠️ **Portée, écrite avec le résultat.** Régime couvert : 12 entrées, panneau
900×600, échelle 1.0, variantes `grid` et `dense_list`. **Non couverts** : liste
vide, filtre actif, échelle ≠ 1 en simultané, panneau plus étroit qu'une carte,
variante `columns` (déclarée, rendue comme `dense_list`). Les métriques de texte
du peintre enregistreur sont **fictives** : le banc ne peut rien dire de
l'ellipse, de la troncature ni du centrage.

## 4. Ce qui est HORS de la tranche — nommé, pas oublié

Les **blueprints** (les événements sont déclarés, rien ne s'y branche) ·
l'**IA spécialisée** · la **création de composants ex nihilo** · le **canevas
libre** · les **variantes multiples** au-delà de ce qui prouve la chaîne.

Différé et nommé, en plus : le **premier lancement fenêtré** · la conformité aux
planches · le **témoin DPI simultané** (deux fenêtres à DPI différents en même
temps — un témoin séquentiel ne discrimine pas, une globale le passerait aussi) ·
un **widget de sélection de rôle** pour rebrancher un jeton (le mécanisme est en
place et testé, c'est le widget qui manque — et l'écrire ici en ferait une copie
de plus).

## 5. Précision de Rodolf : la bibliothèque n'est pas une prison

*« On n'interdit pas la réécriture, mais nos applications utilisent ce qui existe
sauf spécificité propre non couverte. »* Ce devis disait « la règle mord : toute
interface neuve hors bibliothèque exige une justification écrite ». **Reformulé :
la bibliothèque est un CHOIX PAR DÉFAUT.** Ce qui est proscrit, c'est de réécrire
**sans raison** ce qui existe ; une spécificité propre à une application, non
couverte, reste légitimement locale et **se justifie en une ligne**. Pour un
utilisateur tiers du moteur, le choix reste entier.

## 6. L'échelle appartient à la surface — correction reçue

J'avais écrit `Scale()` comme **méthode du peintre**, pour tuer la globale de
processus `gUiScale`. **Diagnostic bon, remède faux** : l'arbitrage du 18/08 l'a
montré sur une mesure que je n'avais pas faite — `S(px)` est appelée dans
`NkLayout::Compute` et `NkModelerTables.h`, **du code qui ne peint pas**. L'échelle
appartient à la **surface**, elle vit dans `NkComponentInput::surfaceScale`, une
instance par fenêtre. Le peintre ne la connaît plus du tout.

## 7. Les trois horizons

- **court** — recevoir le peintre de NK3DModeler et remplacer l'adaptateur NKGui
  provisoire ; ouvrir la fenêtre dès qu'un GPU est libre.
- **moyen** — le second composant déclaré (`TreeView`, palier 4) : c'est lui qui
  dira si la forme tient sur autre chose que celui pour lequel elle a été écrite.
  Une forme validée sur un seul cas n'est pas validée.
- **long** — la convergence vers `.nkgui` v0.2 et ses blueprints. **Aucun des
  deux gestes courts n'y mène**, et c'est exactement ce que l'horizon rend
  visible : le bloc `controller` produit aujourd'hui est le seul fil qui y monte.

## 8. Ce que ça change pour les applications consommatrices

**Rien.** Aucun fichier d'application n'est touché ; les trois navigateurs
existants sont intacts. NKEditorKit gagne trois `.cpp` et cinq en-têtes ;
`NKUIDesign` est une cible neuve. Le seul changement partagé est l'ajout de
`Applications/NKUIDesign/NKUIDesign.jenga` au workspace.

⚠️ **Et un fait vérifiable qui vaut mieux qu'une affirmation sur la frontière** :
`NKUIDesign.jenga` **ne déclare pas `NKReflection`**. L'éditeur de composants
n'en lie pas une ligne — la déclaration ne lui doit rien.

---

## 9. Les quatre ajouts de Rodolf, et le CRITÈRE D'ÉCHEC écrit d'avance (2026-08-19)

> ⚠️ **Cette section est écrite AVANT la déclaration du second composant, et
> elle est commitée séparément.** Un critère d'échec rédigé après le résultat ne
> discrimine rien : on l'ajuste, sans même le vouloir, à ce qu'on vient de
> trouver. L'ordre des commits est la seule preuve d'antériorité qui ne se
> raconte pas.

### 9.1 Ce que la forme porte désormais

Aux quatre acquis de la tranche (paramètres · jetons · variantes · greffes ·
événements avec charge · l'entrée) s'ajoutent les quatre demandes de Rodolf du
18/08 au soir :

| ajout | où il vit | ce qu'il change |
|---|---|---|
| **le rôle** | `NkComponentRole.h` + champ `role` | on dessine une apparence, on lui **attribue une capacité**. Une poignée de rôles sert des milliers d'apparences |
| **l'arbre** | `NkComponentLayout.h` + `elements` | une apparence est un arbre ; une interface complète est un composant qui en contient d'autres — **même mécanisme aux deux échelles** |
| **taille et agencement** | `NkComponentLayout.h` + `NkLayoutSolve.h` | l'enfant déclare figé/contenu/fraction/poids/extensible + bornes, le parent déclare ligne/colonne/grille/ancrage. **La position devient un résultat** |
| **la provenance** | `NkProvenance`, 3 champs | qui l'a produite, si elle a été vérifiée par rejeu, si elle a été corrigée ensuite |

### 9.2 Pourquoi un second composant, et pourquoi un ARBRE

Mon prédécesseur l'a écrit lui-même en fermant la tranche : *« la forme n'est
validée que sur UN composant, celui pour lequel elle a été écrite. »* Une forme
validée sur un seul cas décrit ce cas, pas une famille.

`TreeView` (palier 4) est le meilleur second cas parce qu'il **stresse ce que le
navigateur ne stressait pas** : un état d'ouverture par nœud, une structure
récursive de profondeur inconnue, une sélection **multiple**, et un
glisser-déposer qui **réordonne** au lieu de simplement déposer.

### 9.3 LE CRITÈRE D'ÉCHEC — six questions, et ce qui compte comme échec

**Écrire ce qui prouverait qu'on a tort est la seule façon de ne pas se donner
raison.** Si aucune réponse ne peut être « la forme ne tient pas », le test ne
discrimine pas.

| # | ce qui serait un ÉCHEC de la forme | ce qui serait acceptable |
|---|---|---|
| **1** | déclarer `TreeView` **oblige à ajouter un champ** à `NkComponentDecl` / `NkElementDecl` qui ne sert qu'aux arbres | tout ce qui est propre à l'arbre atterrit dans `params` / `metrics` / `tokens` / `events` / `elements`, **sans nouveau champ** |
| **2** | une charge d'événement de `TreeView` **ne s'exprime pas** dans `NkArgKind`, et il faut **ajouter un type** — ce qui casserait l'invariant « exactement la table §11 de `.nkgui`, sans un type de plus » | toutes les charges s'écrivent avec les huit types existants |
| **3** | le **rôle** doit connaître quelque chose du **dessin** (l'indentation, la vignette) pour être utilisable — la séparation apparence/comportement serait alors fausse | le rôle ne porte que des **faits** et des **états** ; `content_browser` et `TreeView` partagent la famille de rôles sans qu'aucun ne soit taillé pour un seul |
| **4** | l'arbre de sous-éléments **ne sait pas exprimer la récursion** des lignes, et il faut un mécanisme réservé aux arbres | le besoin se règle par un ajout **général** — qui sert aussi les cartes du navigateur — ou par une frontière écrite entre partie statique et partie répétée par les données |
| **5** | l'indentation force une **coordonnée** dans la déclaration | l'indentation est une **métrique déclarée**, consommée là où la donnée est connue ; aucun `x` n'apparaît |
| **6** | la **provenance** ne sait pas décrire l'origine d'une déclaration d'arbre | rien de spécifique n'est attendu ici : **ce critère ne discrimine pas**, et il est écrit pour qu'on ne le compte pas comme une réussite |

**Prédictions, posées avant l'écriture** — elles valent d'être fausses :

1. **le n°2 va mordre.** Une sélection multiple veut porter *l'ensemble des
   entrées choisies* ; aucun des huit types n'est une collection. Je m'attends à
   ne pas pouvoir l'exprimer sans inventer un type — et **inventer un type de la
   spec `.nkgui` n'est pas à ma main** ;
2. **le n°4 va mordre aussi**, mais je m'attends à ce que la réparation soit
   **générale** : les cartes du navigateur sont, elles aussi, un gabarit répété
   par la donnée. Si la réparation n'est générale que pour les arbres, c'est un
   échec au sens du n°1 ;
3. les n°1, 3 et 5 devraient passer. S'ils passent **tous les trois sans la
   moindre gêne**, il faudra se demander si le second composant a été choisi
   assez loin du premier — un test qui ne coûte rien n'a rien mesuré.

### 9.4 Comment le résultat se mesure

Trois bancs, et aucun ne remplace les autres :

1. **`Engine/NKEditorKit/tests/NkFormProbe.cpp`** — banc de la **forme**,
   compilé *et exécuté* avec `g++` seul, **sans lier quoi que ce soit**. Il fait
   passer les deux déclarations par `NkCheckComponent`, résout des dispositions
   et vérifie que les positions **bougent quand une métrique bouge** ;
2. **le banc de neutralité** (§5), rejoué avec son témoin qui échoue ;
3. **`NKUIDesign --probe`**, qui doit **rester à 21/21** : les ajouts ne valent
   rien s'ils cassent la tranche déjà livrée.
