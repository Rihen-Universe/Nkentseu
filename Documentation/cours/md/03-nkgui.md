# NKGui : l'interface immédiate

Voici le cœur du cours. NKGui est le framework qui vous donnera des boutons, des
cases à cocher, des champs de saisie, des arbres, des tables, des menus et des
fenêtres ancrables. Mais avant la liste des widgets, il faut comprendre
*comment il pense* — parce qu'il ne pense pas comme la plupart des
frameworks d'interface, et que tout le reste en découle.

## L'idée : on ne crée rien, on redéclare tout

### Le mode immédiat

Dans un framework classique, un bouton est un *objet*. On le crée une fois,
on lui donne un texte, une position, on lui branche un gestionnaire de clic, et
il vit jusqu'à ce qu'on le détruise. L'interface est un arbre d'objets qu'on
modifie.

Dans NKGui, il n'y a **aucun objet bouton**. Il y a un *appel de
fonction*, qu'on refait à chaque image :

**`Forme canonique d'une frame NKGui`**

```cpp
ctx.BeginFrame(dt);
    Text(ctx, "Bonjour");
    if (Button(ctx, "Cliquez-moi")) {
        /* reaction au clic */
    }
ctx.EndFrame();
```

Le bouton n'existe que le temps de l'appel. À l'image suivante, on le
redéclare. S'il ne faut plus l'afficher, on ne l'appelle pas — il n'y a rien à
détruire, rien à cacher, rien à retirer d'un parent.

> **✅ Ce qu'il faut retenir**
>
> **NKGui est une interface immédiate.** Les widgets ne sont pas des objets
> qu'on crée, mais des appels qu'on refait à chaque image. Ce qui survit d'une
> image à l'autre, ce n'est pas le widget : c'est un *état*, rangé dans le
> contexte et retrouvé par *identifiant*. Toute la mécanique du chapitre
> découle de cette phrase.

### Ce que ce choix vous épargne

Le gain principal est qu'il n'y a **aucune synchronisation à écrire**.
Considérez une case à cocher qui reflète un booléen de votre application. Dans
un modèle à objets, il faut : écrire le booléen vers la case quand il change,
écrire la case vers le booléen quand l'utilisateur clique, et se souvenir de ne
pas boucler. Dans NKGui :

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:45`**

```cpp
NKENTSEU_NKGUI_API bool Checkbox(NkGuiContext &ctx, const char *label, bool &value) noexcept;
```

La valeur est passée *par référence*. Le widget la lit pour se dessiner et
l'écrit si l'utilisateur clique. Il n'y a plus qu'une seule copie de la vérité,
et c'est la vôtre. Ce point est fondamental et mérite d'être formulé
explicitement :

> **✅ Qui possède quoi**
>
> Les **valeurs métier** — le booléen d'une case, le flottant d'un
> *slider*, le tampon de caractères d'un champ de saisie — appartiennent à
> **votre application**. NKGui ne les stocke pas.
>
> NKGui ne stocke que l'**état d'interaction** : ce nœud d'arbre est-il
> ouvert ? où est le curseur de saisie ? quelle est la largeur des colonnes de
> cette table ? quel onglet est actif ? Ces informations-là n'ont pas de sens pour
> votre modèle de données, et c'est pour cela que le framework s'en charge.

### Une note d'honnêteté sur la documentation du module

La documentation interne de NKGui annonce deux paradigmes :

**`Kernel/Runtime/NKGui/ARCHITECTURE.md:5-6`**

```
Deux paradigmes : immédiat (façon ImGui) et retenu (façon Qt/Unity).
```

**Le mode retenu n'existe pas dans le code.** Il est placé en « Phase 6 »
(`ARCHITECTURE.md:191`) et l'arborescence réelle du module ne contient ni
dossier `Retained/`, ni `Layout/`, ni `Window/`, ni
`Dock/` : tout ce qui est implémenté tient dans `Core/` et
`Widgets/`.

Symétriquement, `README.md:14` et `ARCHITECTURE.md:185` annoncent
une « Phase 1 — squelette », alors que `NkGuiWidgets.cpp` fait
4 973 lignes et couvre les tables, le docking, le sélecteur de couleur et les
menus imbriqués. **Les fichiers d'état sont en retard sur le code. Fiez-vous
au code.** Ce cours ne documente que ce qui existe vraiment.

## L'identité d'un widget

Puisqu'un widget n'est pas un objet, comment le framework sait-il, d'une image à
l'autre, qu'il s'agit du *même* bouton ? Par son identifiant.

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiTypes.h:26-46 (abrégé)`**

```cpp
using NkGuiId = uint32;
static constexpr NkGuiId NKGUI_ID_NONE = 0;

NKENTSEU_NKGUI_API_INLINE NkGuiId NkGuiHashStr(const char *s, NkGuiId seed = 2166136261u) noexcept {
    NkGuiId h = seed;
    while (s && *s) { h ^= static_cast<uint8>(*s++); h *= 16777619u; }
    return h ? h : 1u;                                  // jamais 0 (0 = « aucun »)
}
NKENTSEU_NKGUI_API_INLINE NkGuiId NkGuiHashPtr(const void *p, NkGuiId seed = 2166136261u) noexcept { … }
```

C'est un **hachage FNV-1a sur 32 bits** du libellé. Deux constantes, une
boucle : le décalage initial `2166136261` et le multiplicateur
`16777619`, qui sont les valeurs canoniques de FNV-1a 32 bits.

Notez le détail final : `return h ? h : 1u`. La valeur zéro est
*réservée* — elle signifie « aucun widget » (`NKGUI_ID_NONE`). Si
le hachage tombe sur zéro, on renvoie 1. Un identifiant valide n'est donc jamais
nul, ce qui permet d'utiliser zéro comme sentinelle sans ambiguïté.

### La pile d'identifiants

Deux boutons « Supprimer » dans deux panneaux différents auraient le même
libellé, donc le même hachage, donc la même identité — et cliquer sur l'un
activerait l'autre. La parade est une *pile de portées* :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.cpp:144-164`**

```cpp
void NkGuiContext::PushId(const char *s) noexcept {
    const NkGuiId seed = idDepth > 0 ? idStack[idDepth - 1] : 2166136261u;
    if (idDepth < 32) idStack[idDepth++] = NkGuiHashStr(s, seed);
}
void NkGuiContext::PopId() noexcept { if (idDepth > 0) --idDepth; }
NkGuiId NkGuiContext::GetId(const char *s) const noexcept {
    const NkGuiId seed = idDepth > 0 ? idStack[idDepth - 1] : 2166136261u;
    return NkGuiHashStr(s, seed);
}
```

Le mécanisme est élégant : **la graine de chaque niveau est l'identifiant
du niveau précédent**. L'identifiant final dépend donc du *chemin complet*,
pas du seul libellé. Il existe une surcharge `PushId(const void *)` qui
hache une adresse — pratique pour donner une identité distincte à chaque élément
d'une liste.

**`Idiome — écrit pour ce cours, d'après NkGuiContext.h:448-451`**

```cpp
for (int32 i = 0; i < itemCount; ++i) {
    ctx.PushId(&items[i]);            // ou ctx.PushId(items[i].name.CStr())
    if (Button(ctx, "Supprimer")) { /* supprime items[i] */ }
    ctx.PopId();
}
```

> **⚠️ Un identifiant qui change pendant l'édition**
>
> Plusieurs widgets acceptent un libellé *modifiable* — renommage d'un fichier
> dans un arbre, d'un onglet, d'une cellule. Si l'identité était calculée sur ce
> libellé, elle changerait à chaque frappe, et le framework croirait à chaque
> caractère qu'il s'agit d'un widget différent : le curseur de saisie sauterait, le
> focus se perdrait.
>
> C'est pourquoi toutes les variantes `*Editable` exigent un
> `idStr` **stable**, distinct du libellé affiché
> (`NkGuiWidgets.h:198`, `:209-210`, `:221-222`). La règle
> générale : **l'identité ne se calcule jamais sur une donnée qui bouge**.

## Le contexte : `NkGuiContext`

Tout l'état de l'interface vit dans une seule structure,
`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:131`. C'est un
`struct` volumineux — plus de cent champs. Voici la carte, par famille :

| **Famille** | **Champs clés** | **Lignes** |
|---|---|---|
| Vue / échelle | `viewW`, `viewH`, `scale` (DPI) | 132-134 |
| Thème | `theme`, `syntax` | 135-136 |
| Entrée | `input` | 137 |
| Dessin | `dl`, `dlOverlay` | 138-139 |
| Mise en page | `layout` | 140 |
| Polices | `font`, `codeFont` (**non possédés**) | 141-142 |
| Popups | `popupStack[8]`, `popupRects[8]`, `popupDepth` | 147-152 |
| Occlusion | `occlRects`, `occlLayers`, `curInputLayer` | 175-182 |
| Fenêtres | `winDL[32]`, `winMeta[32]`, `winZ[32]` | 228-245 |
| Docking | `dockNodes`, `dockRoot`, `dockSpaceId` | 248-259 |
| Presse-papiers | `clipboardGetFn`, `clipboardSetFn` | 278-292 |
| Interaction | `hotId`, `hotIdPrev`, `activeId` | 317-321 |
| Piles | `idStack[32]`, `disabledStack[16]` | 324-388 |
| Saisie | `inputId`, `inputCaret`, `inputAnchor` | 333-338 |
| Stockage | `openNodes`, `tabBarSel`, `scrollVals`… | 358-425 |
| Hook de style | `styleFn`, `styleUser` | 429-431 |

### Cycle de vie

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:437-443`**

```cpp
                // ── Cycle de vie ──────────────────────────────────────────────────
                bool Init(int32 width, int32 height) noexcept;
                void Shutdown() noexcept;

                // ── Cycle de frame ────────────────────────────────────────────────
                // BeginFrame : APRÈS que l'app ait posé l'input brut (events). Calcule
                // les transitions, réinitialise hotId + la draw list.
                void BeginFrame(float32 dt) noexcept;
```

### Le contexte courant

Le contexte est **explicite** : toutes les fonctions de widget le prennent
en premier paramètre. Il n'y a pas de singleton. Mais un « contexte courant »
propre à chaque fil d'exécution existe, pour le code qui n'a pas le contexte sous
la main :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.cpp:14-25`**

```cpp
namespace {
    // Contexte courant per-thread (non-singleton, comme NkUIContext).
    thread_local NkGuiContext *gCurrentContext = nullptr;
}
void SetCurrentContext(NkGuiContext *ctx) noexcept { gCurrentContext = ctx; }
NkGuiContext *GetCurrentContext() noexcept { return gCurrentContext; }
```

Une fois `ctx.Init(…)` passé, appelez
`SetCurrentContext(&ctx)` ; avant de rendre la main, appelez
`SetCurrentContext(nullptr)`. Rien de plus.

### Le thème

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:28-49`**

```cpp
struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiTheme {
        NkColor bgPrimary   = {26, 29, 36, 255};
        NkColor panel       = {38, 42, 52, 255};
        NkColor header      = {46, 51, 63, 255};
        NkColor button      = {58, 64, 80, 255};
        NkColor buttonHover = {78, 92, 120, 255};
        NkColor buttonActive= {96, 150, 230, 255};
        NkColor border      = {86, 94, 112, 255};
        NkColor text        = {230, 232, 240, 255};
        NkColor textDisabled= {120, 124, 134, 255};
        NkColor selection   = {64, 110, 200, 235};
        NkColor accent      = {96, 165, 250, 255};
        NkColor track       = {30, 34, 43, 255};
        NkColor tabBar      = {22, 25, 31, 255};
        NkColor tab         = {40, 45, 56, 255};
        NkColor tabHover    = {58, 64, 80, 255};
        NkColor tabActive   = {52, 58, 72, 255};
        float32 rounding  = 5.f;
        float32 framePadX = 10.f;
        float32 framePadY = 6.f;
};
```

Ce sont des couleurs *nommées par usage*, lues directement
(`ctx.theme.button`). Il n'y a pas de rôles sémantiques abstraits. Pour
changer l'apparence, on écrase les champs après `Init` — c'est ce que fait
le shell de l'éditeur (`Engine/NKEditorKit/src/NKEditorKit/NkEditorShell.cpp:155-179`,
qui y écrit une palette « GitHub Dark »).

Un second thème, `NkGuiSyntax` (`NkGuiContext.h:53-66`), porte les
couleurs de coloration syntaxique : `keyword`, `type`,
`string`, `comment`, `number`, `preproc`,
`heading`, `mdcode`, `function`, `constant`,
`oper`. Il sert à l'éditeur de code, et rien ne vous empêche de vous en
servir pour autre chose.

Un idiome courant dans le dépôt, pour savoir si l'on est en thème clair ou
sombre sans le stocker :

**`Engine/NKEditorKit/src/NKEditorKit/NkEditorScrollbar.h:29`**

```cpp
const bool light = ((int32)th.bgPrimary.r + th.bgPrimary.g + th.bgPrimary.b) > 384;
```

## L'entrée : `NkGuiInput`

`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiInput.h` — 145 lignes,
entièrement dans l'en-tête, tout `inline`. C'est la structure que
*votre application remplit*, et c'est la frontière entre le monde des
événements et le monde des widgets.

### Ce que l'application pose

| **Champ** | **Sens** |
|---|---|
| `mousePos` | position du pointeur |
| `mouseDown[3]` | 0 = gauche, 1 = droit, 2 = milieu |
| `wheel`, `wheelH` | molette verticale et horizontale (cumulées) |
| `ctrlDown`, `shiftDown`, `altDown` | modificateurs |
| `PushChar(cp)` | un caractère saisi (*codepoint* Unicode) |
| `SetKey(NkGuiKey, bool)` | une touche enfoncée ou relâchée |
| `SetDoubleClick(button)` | double-clic détecté par le système |
| `wantCopy`, `wantCut`, `wantPaste`, `wantSelectAll` | raccourcis d'édition |

### Ce que NKGui calcule

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiInput.h:103-135 (abrégé)`**

```cpp
void NewFrame() noexcept {
    for (int32 i = 0; i < 3; ++i) {
        mouseClicked[i]  =  mouseDown[i] && !mousePrev[i];
        mouseReleased[i] = !mouseDown[i] &&  mousePrev[i];
        …
        // Double-clic : (a) détection interne (2e clic < 0.40 s) OU
        // (b) injection OS via SetDoubleClick (consommée puis remise à 0).
        …
    }
    for (int32 i = 0; i < KeyCount; ++i) {
        keyInit[i] = keyDown[i] && !keyPrev[i];
        …
    }
}
```

Les champs calculés — `mouseClicked`, `mouseReleased`,
`mouseDoubleClicked`, `mouseDownDur`, `keyInit`,
`keyDur` — sont ce que les widgets consultent. Vous ne les écrivez
jamais ; vous ne posez que l'état *brut*.

### La répétition au maintien

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiInput.h:87-94 (signature)`**

```cpp
int32 KeyPressedRepeat(NkGuiKey k, float32 delay = 0.30f, float32 rate = 0.04f) const noexcept;
```

Cette fonction renvoie le *nombre* de déclenchements franchis entre deux
durées d'appui. C'est ce qui fait qu'une flèche maintenue déplace le curseur en
rafale après un court délai, comme dans n'importe quel éditeur de texte. Elle
sert aussi aux boutons à rafale.

### Les touches suivies

NKGui ne suit pas tout le clavier. Il maintient une liste explicite
(`NkGuiTypes.h:75-129`) : `Left`, `Right`, `Up`,
`Down`, `Home`, `End`, `Backspace`,
`Delete`, `Enter`, `Escape`, `Tab`, `F2`,
`F5`, plusieurs lettres, `Space`, `F8`, `F12`,
etc.

> **✅ Ce qu'il faut retenir**
>
> La **saisie de texte** passe par `chars[]` — des *codepoints*
> Unicode poussés par `PushChar` — et **jamais** par les touches. Les
> touches servent à la navigation et aux commandes. Confondre les deux donne une
> application où l'on ne peut pas taper d'accents, ou bien où les flèches écrivent
> des caractères.

## Le cycle d'une image

### `BeginFrame`

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.cpp:39-75`**

```cpp
void NkGuiContext::BeginFrame(float32 dt) noexcept {
    input.dt = dt;
    input.NewFrame();      // transitions clic/relâche
    time += dt;            // blink du caret
    hotIdPrev = hotId;     // le survol résolu de la frame précédente
    hotId = NKGUI_ID_NONE; // re-calculé par les widgets (greedy)
    interact = NkGuiInteract::None;
    lastItemHovered = false;
    wantCursor = NkGuiCursor::Arrow;
    idDepth = 0;
    disabledDepth = 0;
    inputClickConsumed = false;
    curPopupLevel = -1;    // le dessin reprend sur la couche principale
    // Routeur d'occlusion : la liste ecrite la frame PRECEDENTE devient la
    // liste LUE (stable toute la frame, comme hotIdPrev) ; on repart a zero
    // pour l'ecriture de cette frame.
    occlCount = occlCountNew;
    for (int32 i = 0; i < occlCountNew; ++i) { occlRects[i] = occlRectsNew[i]; occlLayers[i] = occlLayersNew[i]; }
    occlCountNew = 0;
    curInputLayer = 0;
    winCount = 0;          // pool de fenêtres ré-attribué cette frame
    curWindow = -1; curWindowId = NKGUI_ID_NONE; curWindowDocked = false;
    containerDepth = 0;    // pile de conteneurs ré-attribuée
    overlayDepth = 0;
    for (uint32 i = 0; i < windowMeta.Size(); ++i) {
        windowMeta[i].hostRendered = false;
        windowMeta[i].frameDL = -1;
        windowMeta[i].dockDL = -2;
    }
    dl.Reset();
    dlOverlay.Reset();
}
```

Lisez cette fonction comme une remise à zéro sélective : **tout ce qui est
recalculé chaque image est effacé** (les listes de dessin, les piles, le survol
courant, le *pool* de fenêtres), et **tout ce qui doit survivre est
préservé** (les états persistants indexés par identifiant, la position des
fenêtres, l'arbre de docking).

Deux lignes méritent qu'on s'y arrête, parce qu'elles portent le même mécanisme :
`hotIdPrev = hotId` et le recopiage des rectangles d'occlusion. Dans les
deux cas, **ce qui a été écrit à l'image précédente devient ce qu'on lit à
l'image courante**. Nous verrons pourquoi à la section 3.6.

### `EndFrame`

`EndFrame` (`NkGuiContext.cpp:77-142`) fait cinq choses :

1. **Fusionne les listes de dessin des fenêtres dans `dl`, dans
   l'ordre de profondeur** (tri par insertion, lignes 80-94). C'est ce qui
   donne un recouvrement correct entre fenêtres.
2. **Détermine la fenêtre survolée** pour l'image suivante
   (`hoveredWindowId`, lignes 96-106) : celle qui est le plus en
   avant sous le curseur.
3. **Ferme la chaîne de popups** : Échap ferme le niveau le plus
   profond ; un clic hors de tous les popups et hors de leur ancre ferme
   tout (lignes 113-124).
4. **Libère `activeId` si le bouton est relâché** — la garde
   anti-gel, ci-dessous.
5. **Consomme** la molette et le texte (lignes 139-141) :
   `input.wheel` et `input.wheelH` repassent à zéro,
   puis `input.ClearPerFrameText()`.

> **⚠️ L'anti-gel d'`activeId`**
>
> **`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.cpp:125-132`**
>
> ```cpp
> // ANTI-GEL : aucun glissement légitime ne conserve activeId bouton RELÂCHÉ.
> // Si le widget détenant activeId a disparu (hôte redevenu flottant, onglet
> // caché, fenêtre fermée…) il ne libère jamais activeId et l'occlusion bloque
> // TOUTE interaction. Souris haute + activeId encore posé ⇒ on libère d'office.
> if (!input.mouseDown[0] && activeId != NKGUI_ID_NONE) {
>     activeId = NKGUI_ID_NONE;
>     movingWindowId = NKGUI_ID_NONE;
> }
> ```
>
> Retenez le symptôme, car il est spectaculaire et déroutant : **plus rien ne
> répond**. Aucun bouton ne réagit, aucun survol ne s'allume, l'application semble
> figée alors qu'elle continue de tourner à soixante images par seconde. La cause
> est presque toujours un `activeId` resté posé par un widget qui a
> disparu. Cette garde le libère d'office, mais si vous écrivez votre propre
> mécanisme de glissement (comme la barre de défilement de NKEditorKit), c'est à
> vous de remettre `ctx.activeId = 0` au relâchement.

### L'invariant d'ordre

> **✅ Ce qu'il faut retenir**
>
> `événements` → `ctx.BeginFrame(dt)` →
> `widgets` → `ctx.EndFrame()` →
> `backend.Submit(…)`
>
> `BeginFrame` appelle `input.NewFrame()` en interne. Si vous pompez
> les événements *après* `BeginFrame`, les transitions clic/relâche
> portent sur l'état de l'image *précédente* : vos clics arrivent avec une
> image de retard, ou pas du tout.

> **⚠️ Ne pas transposer l'ordre de l'ancien module**
>
> L'ancien module d'interface, `NKUI`, avait l'ordre *inverse*, et il
> est documenté ainsi :
>
> **`Applications/Sandbox/src/DemoNkentseu/NkUICanvas/NkUICanvasDemo.cpp:268-270`**
>
> ```cpp
> // Ordre input CORRECT : BeginFrame (vide les deltas clic/release) PUIS les
> // events (re-remplissent pos/boutons/clics) -> les widgets voient le clic.
> ```
>
> **Ce commentaire ne s'applique pas à NKGui.** La démo NKGui pompe les
> événements *avant* `BeginFrame` (`main.cpp:438` contre
> `:468`), précisément parce que c'est `BeginFrame` qui fait le
> travail de `NewFrame()`. Si vous copiez du code de l'ancien module, c'est
> la première chose à vérifier.

## Comment un widget conserve son état

Réponse : **il ne conserve rien lui-même**. L'état vit dans le contexte,
indexé par identifiant, dans des tableaux parallèles clé/valeur.

| **État persistant** | **Stockage** | **Lignes** |
|---|---|---|
| Nœuds d'arbre ouverts | `openNodes` | 358 |
| Onglet sélectionné | `tabBarKeys` + `tabBarSel` | 359-360 |
| Défilement d'une zone | `scrollKeys` + `scrollVals` | 377-378 |
| Focus/ancre de liste | `selKeys` + `selFocusStore` | 363-365 |
| Largeurs de colonnes | `tblKeys` + `tblWidths` | 419-420 |
| Teinte du sélecteur couleur | `pickerKeys` + `pickerHSV` | 424-425 |
| Fenêtres (pos, taille, z) | `windowMeta` | 236 |
| Arbre de dock | `dockNodes` | 248 |

Le mécanisme est volontairement simple :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.cpp:336-355 (abrégé)`**

```cpp
bool NkGuiContext::IsNodeOpen(NkGuiId id) const noexcept {
    for (uint32 i = 0; i < openNodes.Size(); ++i)
        if (openNodes[i] == id) return true;
    return false;
}
void NkGuiContext::SetNodeOpen(NkGuiId id, bool open) noexcept { … /* swap-remove */ }
```

Recherche linéaire, pas de table de hachage. C'est assumé : ces listes restent
petites — quelques dizaines de nœuds ouverts au plus.

## Comment un widget reçoit les événements

Nous arrivons au mécanisme le plus subtil de NKGui. Prenez le temps.

### Le problème

En mode immédiat, quand on traite le bouton numéro trois, **on ne connaît
pas encore les rectangles des widgets qui viendront après**. Or ce sont eux qui
seront dessinés par-dessus. Comment savoir si le pointeur est réellement sur le
bouton trois, ou sur une fenêtre qui le recouvre et qu'on n'a pas encore
déclarée ?

Un framework à objets répondrait facilement : il a l'arbre complet, il fait un
test de haut en bas. Le mode immédiat n'a pas cet arbre.

### La solution : glouton, avec une image de retard

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.cpp:491-528`**

```cpp
bool NkGuiContext::ItemHoverable(const NkRect &r, NkGuiId id) noexcept {
    // Routeur d'occlusion UNIFIE : un widget n'est jamais survolable si une
    // surface flottante d'une couche SUPERIEURE (modal, palette, popover
    // declares via PushOcclusion) recouvre le pointeur — quel que soit
    // l'ordre de dessin des panneaux.
    if (!PointReachable(input.mousePos)) return false;
    // Désactivé : aucune interaction.
    if (IsDisabled()) return false;
    // Un popup ouvert capture le pointeur : un widget ne réagit pas si le
    // pointeur est au-dessus d'un popup PLUS PROFOND que le niveau courant
    // (couche principale = -1 ; un item de menu ne capture pas sous son
    // sous-menu déployé).
    for (int32 i = curPopupLevel + 1; i < popupDepth; ++i)
        if (NkGuiRectContains(popupRects[i], input.mousePos)) return false;
    // Occlusion par fenêtre : hors popup, un widget ne réagit que si SA fenêtre
    // est celle survolée au-dessus (curWindowId). Bloque le fond ET les fenêtres
    // recouvertes. (hoveredWindowId/curWindowId = NONE pour le fond hors fenêtre.)
    if (curPopupLevel < 0 && hoveredWindowId != NKGUI_ID_NONE && hoveredWindowId != curWindowId)
        return false;
    // Hors du CLIP courant (zone défilable, panneau) : pas d'interaction —
    // un item scrollé hors-vue ne doit pas capturer le pointeur.
    if (!NkGuiRectContains(DL().CurrentClip(), input.mousePos)) return false;
    // Bloqué si un AUTRE widget capture le pointeur.
    if (activeId != NKGUI_ID_NONE && activeId != id) return false;
    if (!NkGuiRectContains(r, input.mousePos)) return false;
    // Greedy : le DERNIER widget soumis sous le pointeur écrase hotId →
    // celui dessiné par-dessus gagne. On ne déclare « survolé » QUE le
    // front-most de la frame précédente (hotIdPrev) ; le widget masqué
    // dessous, lui, met à jour hotId mais retourne false → ne capture pas.
    hotId = id;
    return hotIdPrev == id;
}
```

Chaque `return false` est une leçon, et l'ordre des tests est celui du
moins cher au plus cher. Mais l'essentiel est dans les deux dernières lignes.

**Glouton (*greedy*)** : tout widget qui se trouve sous le pointeur
*écrase* `hotId`. Comme les widgets sont déclarés dans l'ordre de
dessin, le dernier qui écrit est celui qui est visuellement au-dessus. À la fin
de l'image, `hotId` contient donc bien l'identifiant du widget de premier
plan.

**Une image de retard** : mais on ne peut pas exploiter cette information
pendant l'image en cours — elle n'est complète qu'à la fin. Alors on la reporte :
`BeginFrame` copie `hotId` dans `hotIdPrev`, et
`ItemHoverable` ne déclare « survolé » que le widget dont l'identifiant
correspond à `hotIdPrev`. Le survol effectif est donc celui *résolu à
l'image précédente*.

> **✅ Glouton plus une image de retard**
>
> Chaque widget sous le pointeur écrit `hotId` (le dernier gagne, donc
> celui du dessus) ; le résultat sert à l'image *suivante*, via
> `hotIdPrev`. Un widget masqué met bien à jour `hotId` mais retourne
> `false` : il ne capture rien.
>
> Conséquence pratique et **contre-intuitive** : **ce qui est dessiné
> plus tard capte le clic**. L'ordre de peinture définit la priorité d'interaction.
> La démo l'écrit noir sur blanc :
>
> **`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:806-808`**
>
> ```cpp
> // Soumise APRÈS les boutons → au-dessus. ItemHoverable résout le z-ordre :
> // au-dessus d'un bouton, c'est la boîte qui capture, pas le bouton dessous.
> ```

Le prix à payer est un décalage d'une image sur le survol. À soixante images par
seconde, il est invisible. Mais il explique un comportement qu'on remarque
parfois : le tout premier clic sur un élément jamais survolé auparavant peut ne
pas prendre. Nous verrons à la section 3.12 que les menus contournent ce point
par un test géométrique direct.

### `ButtonBehavior` : la sémantique du clic

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:541-543`**

```cpp
bool ButtonBehavior(NkGuiId id, const NkRect &r, NkGuiButtonFlags flags = NkGuiButtonFlags::None,
                    float32 repeatDelayOverride = -1.f, float32 repeatRateOverride = -1.f,
                    bool *outHovered = nullptr, bool *outHeld = nullptr) noexcept;
```

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.cpp:530-570 (extraits)`**

```cpp
const bool hovered = ItemHoverable(r, id);   // respecte le z-ordre
…
if (hovered && input.mouseClicked[0]) {
    activeId = id;
    if (repeat) pressed = true;              // rafale : déclenche dès l'appui
}
const bool held = (activeId == id);
if (held) {
    interact = NkGuiInteract::EditWidget;
    …
    if (input.mouseReleased[0]) {
        // Sans Repeat : clic validé au relâchement DANS le rect.
        if (!repeat && NkGuiRectContains(r, input.mousePos)) pressed = true;
        activeId = NKGUI_ID_NONE;
    }
} else if (hovered) {
    interact = NkGuiInteract::HoverWidget;
}
…
lastItemHovered = hovered;   // pour IsItemHovered() / SetTooltip
```

> **✅ Ce qu'il faut retenir**
>
> **Un clic se valide au RELÂCHEMENT, à l'intérieur du rectangle.** C'est la
> convention de tous les systèmes de fenêtrage : on peut appuyer sur un bouton,
> glisser en dehors, relâcher, et rien ne se produit — l'action est annulée. Avec
> le drapeau `Repeat`, en revanche, le déclenchement se fait dès l'appui,
> puis en rafale.

### La machine à états d'interaction

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiTypes.h:52-61`**

```cpp
// ── Machine à états d'interaction (le coeur du fix UX) ───────────────
// À chaque frame, UN SEUL mode actif et explicite. Zones de préhension
// disjointes et priorisées (resize > move > contenu), chacune avec son
// curseur et son affordance. Voir ARCHITECTURE.md §4.
enum class NkGuiInteract : uint8 {
    None = 0,
    HoverWidget,  ///< survol d'un widget
    EditWidget,   ///< édition active (slider/input…)
    MoveWindow,   ///< déplacement d'une fenêtre (barre de titre / onglet)
    ResizeWindow, ///< redimensionnement (bord/coin — cf. edge)
    DragSplitter, ///< glissement d'un séparateur de dock
    DragTab,      ///< glissement d'un onglet (réordre / détache)
    DockTarget    ///< visée d'une cible de dock (boussole)
};
```

La règle de conception associée mérite d'être citée, parce qu'elle décrit un
défaut d'ergonomie que beaucoup d'interfaces ont :

**`Kernel/Runtime/NKGui/ARCHITECTURE.md:121-124`**

```
les zones de préhension sont disjointes et priorisées (bord/coin de resize >
barre de titre/onglet pour move > contenu), chacune avec son curseur
(NkWindow::SetCursor, déjà en place) et son affordance visuelle.
→ Plus jamais « je voulais redimensionner et ça a docké ».
```

## Le routeur d'occlusion : gérer vos propres surfaces flottantes

NKGui sait gérer ses propres popups. Mais dès que votre application dessine
*elle-même* une surface flottante — une boîte de dialogue modale, une
palette de commandes, un aperçu au survol — le framework ne peut pas deviner
qu'elle est là. Sans précaution, les clics la traversent.

Le problème est décrit précisément dans l'en-tête :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:163-182`**

```cpp
// ── ROUTEUR D'OCCLUSION UNIFIÉ (surfaces flottantes « maison ») ─────
// PROBLÈME DE FOND résolu ici : les overlays applicatifs (modals, palettes,
// popovers, peek…) faisaient des hit-tests BRUTS (rect + mouseClicked) qui
// ignoraient ce qui est dessiné AU-DESSUS d'eux → traversées de clics,
// boutons inertes, consommations manuelles fragiles dupliquées partout.
// PRINCIPE : chaque surface flottante déclare son rect + sa couche CHAQUE
// frame (PushOcclusion) pendant son dessin ; tous les hit-tests passent par
// InputHits/ClickIn qui refusent un point recouvert par une couche PLUS
// HAUTE que celle en cours de dessin (curInputLayer). Comme hotIdPrev, la
// liste lue est celle de la FRAME PRÉCÉDENTE (stable pour toute la frame,
// indépendante de l'ordre de dessin des panneaux). Couches conseillées :
// 0 fond/panneaux · 50 menus/palettes/popovers · 100 modals · 200 debug.
```

### Les quatre couches

| **Couche** | **Contenu** |
|---|---|
| 0 | fond, panneaux, contenu ordinaire |
| 50 | menus, palettes, *popovers* |
| 100 | boîtes de dialogue modales |
| 200 | superpositions de débogage |

Ce ne sont que des entiers : rien n'interdit d'en intercaler d'autres. Ce qui
compte est la relation d'ordre.

### L'API

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:185-223 (abrégé)`**

```cpp
void PushOcclusion(const NkRect &r, int32 layer) noexcept;   // declarer une surface
bool PointReachable(const NkVec2 &p) const noexcept;          // point atteignable ?
bool InputHits(const NkRect &r) const noexcept;               // hit-test UNIFIE
bool ClickIn(const NkRect &r) const noexcept;                 // clic gauche unifie

// RAII : fixe la couche d'input pendant le dessin d'une surface flottante.
struct NkInputLayerScope {
        NkGuiContext *c; int32 saved;
        NkInputLayerScope(NkGuiContext &ctx, int32 layer) : c(&ctx), saved(ctx.curInputLayer) {
            ctx.curInputLayer = layer;
        }
        ~NkInputLayerScope() { c->curInputLayer = saved; }
};
```

Et la directive, écrite dans le fichier même (`:202-203`) :

> **✅ Ce qu'il faut retenir**
>
> « Hit-test UNIFIÉ : souris dans `r` ET non recouverte par une couche
> supérieure. À utiliser PARTOUT à la place de
> `NkGuiRectContains(mousePos)`. »
>
> Donc : `ctx.InputHits(r)` et `ctx.ClickIn(r)`, jamais un test
> géométrique brut, sauf pour du chrome de très bas niveau.

### L'usage type

**`Applications/NKCode/src/NKCode/Shell/NkAppCommands.h:137-164 (condensé, idiome des modales)`**

```cpp
const NkRect box = { (W-pw)*0.5f, (Vh-ph)*0.5f, pw, ph };

ctx.PushOcclusion(box, 100);                                  // routeur d'occlusion, couche 100
NkGuiContext::NkInputLayerScope _layer(ctx, 100);             // RAII : mes hit-tests passent

// MODALITE GLOBALE : popupDepth > 0 est LE signal verifie par les points
// d'interaction des panneaux — sans lui les clics TRAVERSENT vers l'editeur.
if (ctx.popupDepth == 0) ctx.popupDepth = 1;
ctx.popupRects[0]  = box;
ctx.popupAnchor    = box;
```

> **⚠️ Le blocage protège ce qui est dessous, jamais ce qui est au-dessus**
>
> Cette phrase vient d'un rapport de défaut du dépôt
> (`Kernel/Runtime/NKGui/ROADMAP.md`, « Défaut 1 ») et c'est la meilleure
> formulation du principe :
>
> « ma propre correction du point 3 : en armant le blocage, j'ai neutralisé la
> liste elle-même, peinte après. D'où la règle qui manquait, à inscrire dans le
> socle : **le blocage protège ce qui est dessous, jamais ce qui est
> au-dessus.** »
>
> et la directive qui suit : « Dans un socle correct, l'ordre de peinture définit
> la priorité d'interaction, sans rien à armer. Ce qui est peint plus tard capte le
> clic en premier ; le reste en découle. »
>
> Quatre bugs de la même famille sont recensés dans ce document : un menu d'en-tête
> dont les clics étaient inopérants et qui ne se refermait pas, un panneau qui
> laissait passer ses clics, une liste déroulante dans laquelle « je ne peux
> choisir aucun format ». Tous avaient la même cause : un test de survol brut qui
> ignorait ce qui était peint au-dessus.

> **⚠️ Les tests négatifs**
>
> **`Applications/NKCode/src/NKCode/Shell/Panels.h:3461-3465`**
>
> ```cpp
> // « Clic en dehors du champ -> valider » : test NEGATIF, donc on
> // exige d'abord que le clic ATTEIGNE notre couche
> // (PointReachable). Sinon un clic destine a une surface
> // flottante posee au-dessus (modale, menu) validait le
> // renommage au passage.
> else if (mRenameArmed && ctx.input.mouseClicked[0] && !ctx.input.mouseDoubleClicked[0] &&
>          ctx.PointReachable(ctx.input.mousePos) &&
>          !NkGuiRectContains(mRenameRect, ctx.input.mousePos)) {
> ```
>
> Un test *positif* (« le clic est-il dans mon rectangle ? ») échoue
> naturellement quand quelque chose recouvre. Un test *négatif* (« le clic est-il
> *en dehors* de mon rectangle ? ») réussit au contraire dans ce cas — et
> déclenche à tort. Chaque fois que vous écrivez « clic en dehors ⇒ fermer ou
> valider », commencez par exiger `ctx.PointReachable(mousePos)`.

> **⚠️ Ne jamais écraser `ctx.input.mousePos`**
>
> C'est probablement le piège le plus coûteux du dépôt, et le commentaire qui le
> documente est le plus important sur le sujet de l'entrée :
>
> **`Applications/NKCode/src/NKCode/Shell/NkExplorer.h:89-98 (extrait)`**
>
> ```cpp
> // /!\ NE PAS toucher mousePos : il n'est mis à jour QUE sur un événement
> // de DÉPLACEMENT (jamais re-sondé) -> l'écraser le GÈLE pour toute
> // l'app à la frame suivante (clic sans bouger = position figée). On
> // neutralise donc SEULEMENT les clics/molette/frappes.
> ```
>
> La neutralisation correcte est *sélective* :
>
> **`Applications/NKCode/src/NKCode/Shell/NkExplorer.h:99-107`**
>
> ```cpp
> if (mPeekOpen || mDelMenu.open) {
>     for (int32 b = 0; b < 3; ++b) {
>         ctx.input.mouseClicked[b] = false;
>         ctx.input.mouseDown[b] = false;
>         ctx.input.mouseDoubleClicked[b] = false;
>     }
>     ctx.input.wheel = ctx.input.wheelH = 0.f;
>     ctx.input.charCount = 0;
> }
> ```
>
> Un shell peut se permettre de téléporter la souris hors de l'écran, parce qu'il
> restaure l'intégralité de `input` quelques lignes plus loin. Un panneau,
> non : ce qu'il écrase reste écrasé.

## La liste de dessin

### Choisir la bonne couche : `ctx.DL()`

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:299-303`**

```cpp
NkGuiDrawList &DL() noexcept {
    if (curPopupLevel >= 0 || overlayDepth > 0)
        return dlOverlay;                  // popup / couche overlay forcée
    return curWindow >= 0 ? winDL[curWindow] : dl;
}
```

Trois destinations possibles : la liste de superposition (si l'on dessine un
popup), la liste propre à la fenêtre courante, ou la liste principale.

> **✅ Ce qu'il faut retenir**
>
> **On écrit toujours dans `ctx.DL()`, jamais dans `ctx.dl` en
> dur.** Un panneau qui écrirait directement dans `ctx.dl` se dessinerait
> *sous* tout le reste et *sans* découpe — parce qu'il aurait
> court-circuité à la fois le *pool* de fenêtres et la pile de clip.

Chaque fenêtre a sa propre liste, ce qui rend le tri par profondeur possible :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:225-232`**

```cpp
// ── Fenêtres flottantes (Begin/End) ───────────────────────────────
// Chaque fenêtre dessine dans SA draw-list (pool `winDL`), fusionnées dans
// `dl` triées par z-order à EndFrame → recouvrement correct + passage devant.
static constexpr int32 WinMax = 32;
NkGuiDrawList winDL[WinMax];
```

### Les primitives

Toutes sont implémentées dans `NkGuiDrawList.cpp`.

| **Primitive** | **Décl.** | **Remarque** |
|---|---|---|
| `AddRectFilled(r, col, rounding)` | .h:66 | **coins arrondis inclus** |
| `AddRect(r, col, thickness)` | .h:67 | contour |
| `AddRectFilledMultiColor(r, tl, tr, br, bl)` | .h:69 | **dégradé bilinéaire** |
| `AddImage(texId, r, uv0, uv1, tint)` | .h:73 | quad texturé |
| `AddLine(a, b, col, thickness)` | .h:75 | |
| `AddTriangleFilled(a, b, c, col)` | .h:76 | |
| `AddTriangleMultiColor(a, b, c, ca, cb, cc)` | .h:78 | dégradé barycentrique |
| `AddCircleFilled(center, r, col, segs)` | .h:80 | `segs = 0` → auto (12…128) |
| `AddText(face, texId, baseline, s, col, maxWidth, skew)` | .h:87 | |
| `AddTextRange(face, texId, baseline, begin, end, col)` | .h:91 | sous-chaîne |

**Il n'y a pas** de `AddPolyline`, `AddBezier`,
`AddArc`, ni `AddCircle` (contour non plein), contrairement à ce
qu'annonce `ARCHITECTURE.md:101-102`.

Rappelez-vous du chapitre 2 : NkCanvas n'a ni dégradé ni coins arrondis. Voici
donc où ils sont fabriqués. Le rectangle arrondi est quatre arcs de coin, quatre
segments chacun, triangulés en éventail depuis le centre du rectangle
(`NkGuiDrawList.cpp:114-141`). Le dégradé est simplement quatre couleurs
de sommets différentes.

> **⚠️ Une bordure n'est pas quatre lignes**
>
> **`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.cpp:205-208`**
>
> ```cpp
> // Bordure = 4 rectangles pleins tracés STRICTEMENT à l'intérieur du rect.
> // (AddLine centrerait l'épaisseur sur l'arête → la moitié extérieure sort
> // du rect et est rognée par le clip — scissor exclusif à droite/bas → bord
> // droit invisible/aminci.) Ici chaque bord tient dans [r.x, r.x+r.w] etc.
> ```
>
> Symptôme : les bordures droite et basse de vos panneaux sont plus fines que les
> autres, ou disparaissent complètement. Cause : une épaisseur centrée sur l'arête,
> dont la moitié extérieure tombe hors de la zone de découpe.

### Le découpage

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.cpp:34-55 (abrégé)`**

```cpp
NkRect NkGuiDrawList::CurrentClip() const noexcept {
    return clipDepth > 0 ? clipStack[clipDepth - 1] : NkRect{0.f, 0.f, 1.0e9f, 1.0e9f};
}
void NkGuiDrawList::PushClipRect(const NkRect &r, bool intersect) noexcept { … }
void NkGuiDrawList::PopClipRect() noexcept { if (clipDepth > 0) --clipDepth; }
```

Pile de 32 niveaux, intersection avec le parent par défaut. Et la règle de
découpage des commandes :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.cpp:57-75 (extrait)`**

```cpp
if (need) {
    NkGuiDrawCmd c;
    c.texId = texId;
    c.clipRect = clip;
    c.idxOffset = static_cast<uint32>(idx.Size());
    c.idxCount = 0;
    c.type = texId ? NkGuiDrawCmdType::TexturedTriangles : NkGuiDrawCmdType::Triangles;
    cmds.PushBack(c);
}
```

Une nouvelle commande est ouverte **dès que la texture ou le
rectangle de découpe change**. Puisque, côté NkCanvas, un changement de découpe
force un vidage (chapitre 2), on voit la chaîne de causalité complète :
*beaucoup de `PushClipRect` → beaucoup de commandes
→ beaucoup d'appels de dessin*.

> **✅ Le double filtrage**
>
> Le découpage garantit la *correction* visuelle ; il ne dispense pas
> d'éviter le travail inutile. L'idiome systématique du dépôt combine les deux :
>
> **`Applications/NKCode/src/NKCode/Shell/NkAppCommands.h:344-354 (abrégé)`**
>
> ```cpp
> u.dl->PushClipRect(body, true);
> float32 y = body.y - d->helpScroll;
> for (int32 i = 0; i < N; ++i) {
>     if (y + rowH >= body.y && y <= body.y + body.h && SC[i].k[0]) {   // culling manuel
>         u.Text(body.x,        y + u.s(4), SC[i].k, nkcode::NkCol::primary);
>         u.Text(body.x + keyW, y + u.s(4), SC[i].a, nkcode::NkCol::foreground);
>     }
>     y += SC[i].k[0] ? rowH : u.s(10.f);
> }
> u.dl->PopClipRect();
> ```

### Le texte, et le piège du flou

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.cpp:226-235`**

```cpp
// ── ALIGNEMENT D'UN GLYPHE SUR LA GRILLE DE PIXELS ──────────────────────
// Le curseur de texte accumule des avances FRACTIONNAIRES. Sans arrondi,
// seul le PREMIER glyphe d'une chaîne tombe sur un pixel entier ; tous les
// suivants dérivent et échantillonnent l'atlas ENTRE deux texels, ce qui
// rend le texte uniformément flou. Le défaut est d'autant plus marqué que le
// corps est petit : l'erreur vaut un demi-texel CONSTANT, soit 4 % de la
// hauteur d'un caractère à 13 px et 3,3 % à 15 px.
```

La solution tient en une fonction d'arrondi, mais sa *subtilité* est dans
ce qu'elle n'arrondit pas :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.cpp:258-266`**

```cpp
// On arrondit la POSITION du quad, jamais l'AVANCE : `x` continue
// d'accumuler la valeur exacte, donc la largeur totale de la chaîne
// est inchangée et MeasureWidth reste d'accord avec le rendu. […]
// La LARGEUR est reportée telle quelle : arrondir les deux bords
// séparément étirerait le glyphe d'un pixel et le rééchantillonnerait,
// ce qu'on cherche précisément à éviter.
```

Trois décisions, trois raisons : on arrondit la position (sinon flou), on
n'arrondit pas l'avance (sinon la mesure ne correspond plus au rendu), on ne
touche pas à la largeur (sinon on rééchantillonne, donc on refloute). C'est un
excellent exemple de ce qu'est une correction *bien* faite.

### La formule de la ligne de base

`AddText` prend une **ligne de base**, pas un coin haut-gauche. Les
deux conversions canoniques sont :

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.cpp:111-118`**

```cpp
float32 TextAt(NkGuiContext &ctx, const NkVec2 &topLeft, const char *s, const NkColor &col) noexcept {
    if (!ctx.font || !ctx.font->Valid() || !s) return 0.f;
    // topLeft = coin haut-gauche → ligne de base = y + ascender.
    const float32 baseY = topLeft.y + ctx.font->Ascent();
    ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), {topLeft.x, baseY}, s, col);
    return ctx.font->MeasureWidth(s);
}
```

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.cpp:125-133`**

```cpp
static void DrawCenteredLabel(NkGuiContext &ctx, const NkRect &r, const char *label, const NkColor &col) {
    const float32 tw = ctx.font->MeasureWidth(label);
    const float32 tx = r.x + (r.w - tw) * 0.5f;
    const float32 baseY = r.y + (r.h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent();
    ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), {tx, baseY}, label, col, r.w - 6.f);
}
```

> **✅ Deux formules à mémoriser**
>
> - Aligné en haut : `baseline.y = y + font->Ascent();`
> - Centré dans un rectangle :
>   `baseline.y = r.y + (r.h - font->LineHeight()) * 0.5f + font->Ascent();`
>
> Et la garde qui doit précéder tout appel à `AddText` ou
> `MeasureWidth` : `if (!ctx.font || !ctx.font->Valid()) return;`.

Attention : `NkGuiFont::LineHeight()` renvoie `face->lineAdvance`,
et **non** `ascent + descent`.

## La mise en page

### Le modèle : un curseur qui descend

`NkGuiLayout` (`NkGuiContext.h:71-95`) est un curseur, comme celui
d'une machine à écrire : `region` (la zone disponible), `cursor`
(où l'on est), `lineStartX`, `curLineH`, `prevItem`,
`maxX`/`maxY`, plus les espacements
(`padding = 10`, `itemSpacingX = 8`,
`itemSpacingY = 6`).

Le mode le plus simple, le mode vertical, tient en douze lignes :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.cpp:303-317`**

```cpp
if (w <= 0.f) w = ContentWidth();
const NkRect rect = {layout.cursor.x, layout.cursor.y, w, h};
layout.prevItem = rect;
if (rect.x + rect.w > layout.maxX) layout.maxX = rect.x + rect.w;  // largeur contenu
if (h > layout.curLineH) layout.curLineH = h;
layout.cursor.x = layout.lineStartX;
layout.cursor.y += layout.curLineH + layout.itemSpacingY;
if (layout.cursor.y > layout.maxY) layout.maxY = layout.cursor.y;
layout.curLineH = 0.f;
return rect;
```

### Les sept modes de flux

Le champ `layout.flow` sélectionne le comportement de
`NextItemRect` (`NkGuiContext.cpp:194-318`) :

| **flow** | **Mode** | **Comportement** |
|---|---|---|
| 0 | Vertical (défaut) | un élément par ligne, le curseur descend ; `w <= 0` = remplir la largeur |
| 1 | HBox | le curseur avance en X, pas de retour à la ligne ; `w <= 0` → 120 px |
| 2 | Grid | colonnes régulières, retour à la ligne tous les `gridCols` |
| 3 | Stack | enfants superposés, ancrés dans une boîte (9 ancres) |
| 4 | Flow | comme HBox mais passe à la ligne quand ça déborde (étiquettes, barres d'outils) |
| 5 | Row (flex) | rangée horizontale, largeurs pré-calculées, hauteur étirée |
| 6 | Column (flex) | colonne verticale, hauteurs pré-calculées, largeur étirée |

On ne règle pas `flow` à la main : on ouvre un conteneur.

### Les helpers de curseur

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:453-466`**

```cpp
void    BeginLayout(const NkRect &region) noexcept;   // ouvre une region de contenu
NkRect  NextItemRect(float32 w, float32 h) noexcept;  // w<=0 = remplir la largeur
void    SameLine(float32 spacingX = -1.f) noexcept;   // item suivant a droite du precedent
void    Spacing(float32 px = -1.f) noexcept;          // saut vertical
float32 ContentWidth() const noexcept;                // largeur restante au curseur
float32 AvailHeight() const noexcept;                 // hauteur restante sous le curseur
float32 ItemHeight() const noexcept;                  // hauteur standard d'un widget
float32 S(float32 px) const noexcept;                 // px logiques -> px ecran (DPI)
void    Indent(float32 w) noexcept;                   // decale le debut de ligne (arbres)
```

`ItemHeight()` vaut simplement `lineHeight + 2 * theme.framePadY`,
avec un repli de 16 px si aucune police n'est chargée
(`NkGuiContext.cpp:189-192`).

> **⚠️ `AvailHeight()` vaut un milliard dans une zone défilable**
>
> **`Applications/NKCode/src/NKCode/Shell/Panels.h:676-685 (extrait)`**
>
> ```cpp
> // AvailHeight()/ContentWidth() = taille du CONTENU (scrollable, ~1e9), PAS
> // la taille visible -> on borne par le rect de CLIP (zone visible du dock)
> // sinon viewH gigantesque (pas de scrollbar, barre H hors ecran).
> const NkRect clip = ctx.DL().CurrentClip();
> NkRect r = {ctx.layout.cursor.x, ctx.layout.cursor.y, ctx.ContentWidth(), ctx.AvailHeight()};
> if (r.x + r.w > clip.x + clip.w) r.w = clip.x + clip.w - r.x;
> if (r.y + r.h > clip.y + clip.h) r.h = clip.y + clip.h - r.y;
> ```
>
> **Règle : `CurrentClip()` donne le VISIBLE ;
> `ContentWidth()` et `AvailHeight()` donnent le CONTENU** — qui,
> dans une zone défilable, est volontairement gigantesque. Il faut toujours
> intersecter les deux. Symptôme si on l'oublie : plus de barre de défilement, et
> une barre horizontale placée à un million de pixels du bord.

### Les conteneurs

Ils sauvegardent le layout parent, le remplacent, puis le restaurent en avançant
le parent du bloc consommé (`NkGuiWidgets.cpp:1322-1365`). Ils sont
**composables** — un HBox dans une cellule de grille, par exemple.

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:146-189 (signatures)`**

```cpp
void BeginVBox(NkGuiContext &ctx, float32 gap = -1.f) noexcept;   void EndVBox(NkGuiContext &) noexcept;
void BeginHBox(NkGuiContext &ctx, float32 gap = -1.f) noexcept;   void EndHBox(NkGuiContext &) noexcept;
void BeginGrid(NkGuiContext &ctx, int32 columns, float32 gap = -1.f) noexcept; void EndGrid(NkGuiContext &) noexcept;
void BeginGroup(NkGuiContext &ctx) noexcept;                      void EndGroup(NkGuiContext &) noexcept;
void BeginFlow(NkGuiContext &ctx, float32 gap = -1.f) noexcept;   void EndFlow(NkGuiContext &) noexcept;
void BeginRow(NkGuiContext &ctx, float32 height, const float32 *sizes, int32 count, float32 gap = -1.f) noexcept;
void EndRow(NkGuiContext &ctx) noexcept;
void BeginColumn(NkGuiContext &ctx, float32 width, const float32 *sizes, int32 count,
                 float32 totalHeight = -1.f, float32 gap = -1.f) noexcept;
void EndColumn(NkGuiContext &ctx) noexcept;
void BeginStack(NkGuiContext &ctx, float32 width, float32 height) noexcept;
void StackAnchor(NkGuiContext &ctx, int32 anchor) noexcept; // 0=HG 1=HC 2=HD 3=CG 4=C 5=CD 6=BG 7=BC 8=BD
void EndStack(NkGuiContext &ctx) noexcept;
void Spacer(NkGuiContext &ctx, float32 w, float32 h) noexcept;
void SpringRight(NkGuiContext &ctx, float32 width) noexcept;
bool Splitter(NkGuiContext &ctx, const char *idStr, const NkRect &handle, bool vertical,
              float32 *value, float32 minV, float32 maxV) noexcept;
void SetUiScale(NkGuiContext &ctx, float32 s) noexcept;
float32 Scaled(NkGuiContext &ctx, float32 px) noexcept;
void PushOverlay(NkGuiContext &ctx) noexcept;  void PopOverlay(NkGuiContext &) noexcept;
```

Le flex à poids mérite une note :

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:159-160`**

```
`sizes[i] > 0` = px fixes ; `sizes[i] < 0` = poids (-1 = poids 1) qui se
partagent l'espace RESTANT. Single-pass exact (total connu).
```

### Le DPI

`SetUiScale(ctx, s)` multiplie les marges, l'arrondi et les espacements.
Mais attention : **l'application doit recharger la police** à
`tailleBase * scale`, faute de quoi la mise en page grossit et le texte
reste petit. La démo le fait ainsi :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:461-466`**

```cpp
        if (pendingScale > 0.f) { // F9 : appliquer la nouvelle echelle DPI
            SetUiScale(ctx, pendingScale);
            if (fontPtr->LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f * pendingScale))
                backend.UploadFontGray8(fontPtr->TexId(), fontPtr->pixels, fontPtr->atlasW, fontPtr->atlasH);
            pendingScale = 0.f;
        }
```

Notez où ce code est placé : **avant** `BeginFrame`. Recharger un
atlas au milieu d'une image laisserait des commandes de dessin pointant vers
l'ancienne texture.

## Le catalogue des widgets

Voici l'inventaire de ce qui existe réellement. Toutes les signatures viennent de
`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h` ; la macro
`NKENTSEU_NKGUI_API` est omise. **Toutes** ont une définition dans
le `.cpp` — cela a été vérifié fonction par fonction.

### Texte et boutons

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:16-44, 140`**

```cpp
void    PanelBackground(NkGuiContext &ctx, const NkRect &r) noexcept;                      // .cpp:106
float32 TextAt(NkGuiContext &ctx, const NkVec2 &topLeft, const char *s) noexcept;          // .cpp:120
float32 TextAt(NkGuiContext &ctx, const NkVec2 &topLeft, const char *s, const NkColor &col) noexcept;
void    Text(NkGuiContext &ctx, const char *s) noexcept;                                   // .cpp:166
void    TextWrapped(NkGuiContext &ctx, const char *text, float32 wrapWidth = -1.f) noexcept;
bool    Button(NkGuiContext &ctx, const char *label, const NkRect &r) noexcept;            // rect explicite
bool    Button(NkGuiContext &ctx, const char *label) noexcept;                             // rect auto
bool    ButtonEx(NkGuiContext &ctx, const char *label, const NkRect &r, NkGuiButtonFlags flags,
                 float32 repeatDelay = -1.f, float32 repeatRate = -1.f) noexcept;
bool    RepeatButton(NkGuiContext &ctx, const char *label, const NkRect &r,
                     float32 repeatDelay = -1.f, float32 repeatRate = -1.f) noexcept;
void    Separator(NkGuiContext &ctx) noexcept;
```

Le widget le plus simple du framework, en entier — il résume tout ce que nous
avons vu :

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.cpp:166-172`**

```cpp
void Text(NkGuiContext &ctx, const char *s) noexcept {
    const float32 lh = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
    const float32 w  = (ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(s) : 0.f;
    const NkRect r = ctx.NextItemRect(w, lh);
    TextAt(ctx, {r.x, r.y}, s, ctx.IsDisabled() ? ctx.theme.textDisabled : ctx.theme.text);
}
```

Mesurer, demander un rectangle au layout, dessiner. Quatre lignes, et la garde
sur la police est présente deux fois avec son repli chiffré.

### Cases à cocher

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:45-64`**

```cpp
bool Checkbox(NkGuiContext &ctx, const char *label, bool &value) noexcept;
bool CheckboxTristate(NkGuiContext &ctx, const char *label, NkGuiCheck &state) noexcept;
bool CheckBox3(NkGuiContext &ctx, const char *idStr, NkGuiCheck &state) noexcept;   // compacte, sans libelle
void NkGuiTreeCascade(NkGuiCheck *states, const int32 *parent, int32 count, int32 node,
                      NkGuiCheck value) noexcept;
void NkGuiTreeRecomputeMixed(NkGuiCheck *states, const int32 *parent, int32 count) noexcept;
```

Les deux dernières fonctions illustrent un choix de conception intéressant :

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:55-58`**

```
NKGui ne stocke PAS la hiérarchie (façon ImGui) : l'app possède `states[]`
(1 NkGuiCheck par nœud) + `parent[]` (index du parent, -1 = racine). Ces
utilitaires agissent sur ces tableaux quand l'app le décide (ex. Alt+clic).
```

### Valeurs numériques

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:65, 112-122`**

```cpp
bool SliderFloat(NkGuiContext &ctx, const char *label, float32 &value, float32 vmin, float32 vmax) noexcept;
bool DragFloat  (NkGuiContext &ctx, const char *label, float32 &v, float32 speed = 0.1f,
                 float32 vmin = -1.0e30f, float32 vmax = 1.0e30f,
                 NkGuiDragDir dir = NkGuiDragDir::Horizontal) noexcept;
bool DragInt    (NkGuiContext &ctx, const char *label, int32 &v, float32 speed = 0.25f,
                 int32 vmin = -2147483640, int32 vmax = 2147483640,
                 NkGuiDragDir dir = NkGuiDragDir::Horizontal) noexcept;
bool InputFloat (NkGuiContext &ctx, const char *label, float32 &v, float32 step = 1.f, …) noexcept;
bool InputInt   (NkGuiContext &ctx, const char *label, int32 &v, int32 step = 1, …) noexcept;
```

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:106-111`**

```
DragFloat/DragInt : GLISSER sur le champ pour changer la valeur (vitesse
`speed`/pixel) + DOUBLE-CLIC pour saisir au clavier. InputFloat/InputInt : idem
+ boutons -/+ (pas `step`). […] Pendant le survol/glisser, le widget pose
`ctx.wantCursor` (↔ ou ↕) que l'app peut appliquer.
```

Ce dernier point est à retenir : le widget *demande* un curseur, il ne le
pose pas. C'est à votre boucle d'appliquer `ctx.wantCursor` en appelant
`window.SetCursor(…)`. Nous le ferons au chapitre 4.

### Saisie de texte

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:126-138`**

```cpp
bool InputText(NkGuiContext &ctx, const char *label, char *buf, int32 bufSize) noexcept;
bool InputTextEx(NkGuiContext &ctx, const char *label, char *buf, int32 bufSize,
                 NkGuiInputFlags flags, int32 maxChars = -1) noexcept;
bool InputTextMultiline(NkGuiContext &ctx, const char *idStr, char *buf, int32 bufSize,
                        const NkRect &rect, NkGuiInputFlags flags = NkGuiInputFlags::None,
                        int32 maxChars = -1, bool wrap = false) noexcept;
```

Drapeaux disponibles (`NkGuiTypes.h:212-220`) : `Password`,
`CharsDecimal`, `CharsHex`, `Uppercase`, `NoBlank`,
`ReadOnly`.

> **✅ Deux distinctions à ne pas rater**
>
> - `maxChars` compte en **codepoints**, `bufSize` borne
>   les **octets**. Un caractère accentué occupe deux octets.
> - `InputText` renvoie `true` **à la pression
>   d'Entrée** ; `InputTextMultiline` renvoie `true`
>   **si le texte a changé**. Ce n'est pas la même sémantique, et
>   confondre les deux donne soit une validation qui n'arrive jamais, soit
>   une action déclenchée à chaque frappe.

### Graphiques et couleur

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:71-93`**

```cpp
void ProgressBar(NkGuiContext &ctx, float32 fraction, const char *overlay = nullptr) noexcept;
void PlotLines(NkGuiContext &ctx, const char *label, const float32 *values, int32 count,
               float32 minV = 0.f, float32 maxV = 0.f, float32 height = 0.f) noexcept;
void PlotHistogram(NkGuiContext &ctx, const char *label, const float32 *values, int32 count,
                   float32 minV = 0.f, float32 maxV = 0.f, float32 height = 0.f) noexcept;

bool ColorButton(NkGuiContext &ctx, const char *idStr, const float32 *col, float32 w = 0.f,
                 float32 h = 0.f, NkGuiColorFlags flags = NkGuiColorFlags::None) noexcept;
bool ColorPicker4(NkGuiContext &ctx, const char *label, float32 *col,
                  NkGuiColorFlags flags = NkGuiColorFlags::None) noexcept;
bool ColorEdit4(NkGuiContext &ctx, const char *label, float32 *col,
                NkGuiColorFlags flags = NkGuiColorFlags::None) noexcept;
```

`NkGuiColorFlags` (`NkGuiTypes.h:151-159`) : `NoAlpha`,
`Wheel` (roue et triangle SV), `NoInputs`, `Disc`,
`Hexagon`, `Honeycomb`. Détail de conception noté dans l'en-tête
(`:87-88`) : « La TEINTE est préservée même au noir/blanc (HSV mémorisé par
id) » — d'où les tableaux `pickerKeys`/`pickerHSV` du contexte.

### Images

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:99-105`**

```cpp
void Image(NkGuiContext &ctx, uint32 texId, float32 w, float32 h,
           NkColor tint = NkColor{255,255,255,255},
           NkVec2 uv0 = NkVec2{0.f,0.f}, NkVec2 uv1 = NkVec2{1.f,1.f}) noexcept;
bool ImageButton(NkGuiContext &ctx, const char *idStr, uint32 texId, float32 w, float32 h,
                 NkColor tint = NkColor{255,255,255,255},
                 NkVec2 uv0 = NkVec2{0.f,0.f}, NkVec2 uv1 = NkVec2{1.f,1.f}) noexcept;
```

Le `texId` est l'identifiant *côté backend* : au préalable, votre
application doit avoir enregistré l'image sous cet identifiant par
`backend.UploadImageRGBA(texId, …)` (chapitre 4). La teinte multiplie l'échantillon : blanc donne l'image telle
quelle, une couleur donne une icône monochrome recolorée.

### Arbres, sélection, onglets

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:191-239`**

```cpp
bool CollapsingHeader(NkGuiContext &ctx, const char *label) noexcept;
bool TreeNode(NkGuiContext &ctx, const char *label) noexcept;
void TreePop(NkGuiContext &ctx) noexcept;
bool TreeNodeEditable(NkGuiContext &ctx, const char *idStr, char *label, int32 bufSize,
                      bool allowRename = true) noexcept;
bool Selectable(NkGuiContext &ctx, const char *label, bool selected) noexcept;
bool SelectableEditable(NkGuiContext &ctx, const char *idStr, char *label, int32 bufSize,
                        bool selected, bool allowRename = true) noexcept;
bool SelectItem(NkGuiContext &ctx, const char *label) noexcept;
bool SelectItemEditable(NkGuiContext &ctx, const char *idStr, char *label, int32 bufSize,
                        bool allowRename = true) noexcept;
int32 TabBar(NkGuiContext &ctx, const char *id, const char *const *labels, int32 count) noexcept;
int32 TabBarEx(NkGuiContext &ctx, const char *id, const char *const *labels, int32 count,
               const bool *enabled) noexcept;
int32 TabBarEditable(NkGuiContext &ctx, const char *id, char *const *labels, int32 count,
                     int32 labelBufSize, const bool *enabled = nullptr,
                     const bool *allowRename = nullptr) noexcept;
```

Les variantes `*Editable` sont une signature de NKGui : le renommage en
place — par double-clic ou Maj+Entrée — est intégré au socle, pas laissé à
l'application. Elles prennent toutes un `idStr` stable, pour la raison
vue à la section 3.2.

### Zones défilables et tables

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:315-364`**

```cpp
bool BeginChild(NkGuiContext &ctx, const char *idStr, const NkRect &rect, bool border = true,
                bool horizontal = false) noexcept;
void EndChild(NkGuiContext &ctx) noexcept;
bool BeginListBox(NkGuiContext &ctx, const char *idStr, const NkRect &rect) noexcept;
void EndListBox(NkGuiContext &ctx) noexcept;

bool BeginTable(NkGuiContext &ctx, const char *idStr, int32 columns,
                NkGuiTableFlags flags = NkGuiTableFlags::Borders) noexcept;
void TableSetupColumn(NkGuiContext &ctx, const char *label, float32 width = 0.f) noexcept;
void TableHeadersRow(NkGuiContext &ctx) noexcept;
void TableNextRow(NkGuiContext &ctx, float32 minHeight = 0.f) noexcept;
bool TableNextColumn(NkGuiContext &ctx) noexcept;
bool TableSetColumnIndex(NkGuiContext &ctx, int32 n) noexcept;
void EndTable(NkGuiContext &ctx) noexcept;
bool TableGetSortColumn(NkGuiContext &ctx, int32 *outCol, bool *outAscending) noexcept;
bool TableCellText(NkGuiContext &ctx, const char *idStr, char *buf, int32 bufSize,
                   bool editable = false) noexcept;
```

L'idiome complet est donné en commentaire dans l'en-tête :

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:323-334`**

```cpp
if (BeginTable(ctx,"t",3, Borders|RowBg|Resizable)) {
    TableSetupColumn(ctx,"Nom",   0);     // 0 = colonne étirable
    TableSetupColumn(ctx,"Type", 90);     // largeur fixe px
    TableSetupColumn(ctx,"Taille",70);
    TableHeadersRow(ctx);
    for (…) { TableNextRow(ctx);
              TableNextColumn(ctx); Text(ctx,nom);
              TableNextColumn(ctx); Text(ctx,type);
              TableNextColumn(ctx); Text(ctx,taille); }
    EndTable(ctx);
}
```

`NkGuiTableFlags` (`NkGuiTypes.h:132-140`) : `Borders`,
`RowBg`, `Resizable`, `Header`, `ResizableOuter`,
`Sortable`. Limites : `NkGuiTableMaxCols = 16`, et **une
seule table active à la fois** — « pas d'imbrication v1 »
(`NkGuiContext.h:397`).

### Popups, combos, menus, infobulles

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:373-402`**

```cpp
bool BeginCombo(NkGuiContext &ctx, const char *label, const char *preview, int32 itemCount,
                float32 heightOverride = 0.f) noexcept;
void EndCombo(NkGuiContext &ctx) noexcept;
void EndPopup(NkGuiContext &ctx) noexcept;

bool BeginMenuBar(NkGuiContext &ctx, const NkRect &rect) noexcept;
void EndMenuBar(NkGuiContext &ctx) noexcept;
bool BeginMenu(NkGuiContext &ctx, const char *label) noexcept;
void EndMenu(NkGuiContext &ctx) noexcept;
bool MenuItem(NkGuiContext &ctx, const char *label, const char *shortcut = nullptr,
              bool enabled = true) noexcept;
bool BeginPopupMenu(NkGuiContext &ctx, const char *idStr) noexcept;
void EndPopupMenu(NkGuiContext &ctx) noexcept;

void SetTooltip(NkGuiContext &ctx, const char *text) noexcept;
```

Usage de l'infobulle, documenté en `:400-401` :
`if (ctx.IsItemHovered()) SetTooltip(ctx, "…");`. Pour un menu
contextuel, l'application appelle
`ctx.OpenPopupAt(ctx.GetId(idStr), mousePos)` au clic droit, puis dessine
entre `BeginPopupMenu` et `EndPopupMenu`.

Voici, en passant, comment un menu contourne le décalage d'une image :

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.cpp:4806-4812`**

```cpp
// Ouverture au PRESS via test geometrique direct : ne depend PAS du
// survol resolu (hotIdPrev) de la frame precedente. Sinon un clic sur
// un titre jamais survole avant n'ouvrait pas le menu au 1er coup.
// On N'utilise PAS `clicked` (relachement) ici : sinon press ouvrirait
// et release refermerait aussitot (double bascule).
```

### Le contexte utilisé comme un widget

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:448-543 (sélection)`**

```cpp
void  PushId(const char *s);  void PushId(const void *p);  void PopId();
NkGuiId GetId(const char *s) const;
void  BeginDisabled(bool disabled = true);  void EndDisabled();  bool IsDisabled() const;
void  BeginSelectList(const char *id, bool *mask, int32 count, NkGuiSelectFlags flags);
void  ApplySelectClick(int32 idx);  void EndSelectList();
void  OpenPopup(NkGuiId);  void OpenPopupAt(NkGuiId, NkVec2);  void ClosePopup();
bool  IsPopupOpen(NkGuiId) const;
bool  IsHovered(const NkRect &r) const;  bool IsItemHovered() const;
bool  ItemHoverable(const NkRect &r, NkGuiId id);
NkString GetClipboard() const;  void SetClipboard(const char *t);
```

## Panneaux, fenêtres, docking

### `BeginPanel` / `EndPanel`

Le conteneur le plus simple. Son code entier tient sur une page et vaut d'être lu
en entier — il utilise presque tout ce que nous avons vu :

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.cpp:2047-2078`**

```cpp
bool BeginPanel(NkGuiContext &ctx, const char *title, const NkRect &r) noexcept {
    ctx.DL().AddRectFilled(r, ctx.theme.panel, ctx.theme.rounding); // fond
    float32 top = r.y;
    if (title && *title) {
        const float32 th = ctx.ItemHeight();
        ctx.DL().AddRectFilled({r.x, r.y, r.w, th}, ctx.theme.header, ctx.theme.rounding);
        ctx.DL().AddRectFilled({r.x, r.y + th - 1.f, r.w, 1.f}, ctx.theme.border); // séparateur
        if (ctx.font && ctx.font->Valid()) {
            ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                             {r.x + 10.f, r.y + (th - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent()},
                             title, ctx.theme.text);
        }
        top = r.y + th;
    }
    // Bordure EN DERNIER → entoure tout le panneau (en-tête inclus), n'est
    // plus recouverte par le fond de la barre de titre.
    ctx.DL().AddRect(r, ctx.theme.border, 1.f);
    // Le contenu (sous l'en-tête) est une ZONE DÉFILABLE : si les widgets
    // dépassent la hauteur visible, une scrollbar apparaît automatiquement…
    const NkRect content = {r.x, top, r.w, r.y + r.h - top};
    const NkGuiId id = ctx.GetId((title && *title) ? title : "##panel");
    return BeginScrollFrame(ctx, id, content, false);
}

void EndPanel(NkGuiContext &ctx) noexcept { EndScrollFrame(ctx); }
```

Trois choses à en retirer : la formule de la ligne de base est exactement celle
de la section 3.7 ; la bordure est dessinée *en dernier* pour ne pas être
recouverte ; et le contenu est automatiquement une zone défilable.

### `Begin` / `EndWindow`

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:253-263`**

```cpp
// Appeler End UNIQUEMENT si Begin retourne true (convention NKGui, comme
// BeginPanel).
//   if (Begin(ctx,"Inspecteur",&open)) { …widgets… EndWindow(ctx); }
void SetNextWindowPos(NkGuiContext &ctx, float32 x, float32 y) noexcept;
void SetNextWindowSize(NkGuiContext &ctx, float32 w, float32 h) noexcept;
bool Begin(NkGuiContext &ctx, const char *title, bool *open = nullptr,
           NkGuiWindowFlags flags = NkGuiWindowFlags::None) noexcept;
void EndWindow(NkGuiContext &ctx) noexcept;
```

> **✅ La convention `Begin`/`End` de NKGui**
>
> **On appelle `End*` uniquement si `Begin*` a retourné
> `true`.** C'est vrai pour `Begin`, `BeginPanel`,
> `BeginCombo`, `BeginTable`, `BeginChild`,
> `BeginMenu`, `BeginPopupMenu`. Les conteneurs de layout
> (`BeginVBox`, `BeginHBox`…) ne retournent rien : ceux-là
> s'appairent toujours.

`NkGuiWindowFlags` (`NkGuiTypes.h:245-252`) : `NoResize`,
`NoMove`, `NoCollapse`, `NoTitleBar`, `NoClose`.

À la création, une fenêtre reçoit un décalage en cascade pour ne pas se
superposer aux précédentes :

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.cpp:2286-2288`**

```cpp
const float32 off = static_cast<float32>(ctx.windowMeta.Size() % 8) * 28.f;
nm.rect = {90.f + off, 80.f + off, 320.f, 210.f};
```

`SetNextWindowPos` et `SetNextWindowSize` ne s'appliquent qu'*à
la création* — c'est la sémantique « la première fois seulement » — et sont
consommés aussitôt (`ctx.hasNextPos = ctx.hasNextSize = false;`,
`cpp:2293`). Vous ne pouvez donc pas vous en servir pour forcer une
position à chaque image ; c'est délibéré, sans quoi l'utilisateur ne pourrait
jamais déplacer la fenêtre.

> **⚠️ Interactions avant dessin**
>
> **`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.cpp:2396-2397`**
>
> ```cpp
> // ── PHASE 1 : INTERACTIONS — mettent à jour `wr` AVANT le dessin (sinon le
> //    contenu, dessiné à la nouvelle position, « court après » le chrome). ──
> ```
>
> Si vous écrivez vous-même une surface déplaçable, traitez le glissement
> *avant* de dessiner, sinon le contenu accuse une image de retard sur le
> cadre et l'ensemble semble élastique.

### Le docking

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:271-306`**

```cpp
void  DockSpace(NkGuiContext &ctx, const char *idStr, const NkRect &rect) noexcept;
void  DockBuilderDock(NkGuiContext &ctx, const char *windowTitle, int32 zone) noexcept;
void  DockBuilderDockTab(NkGuiContext &ctx, const char *windowTitle, const char *targetTitle) noexcept;
bool  DockFocusWindow(NkGuiContext &ctx, const char *windowTitle) noexcept;
bool  DockIsWindowDocked(NkGuiContext &ctx, const char *windowTitle) noexcept;
int32 DockWindowNode(NkGuiContext &ctx, const char *windowTitle) noexcept;
void  DockDetachWindow(NkGuiContext &ctx, const char *windowTitle) noexcept;
void  DockWindowHideSingleTab(NkGuiContext &ctx, const char *windowTitle, bool hide) noexcept;
void  DockSpaceOverViewport(NkGuiContext &ctx, float32 topMargin = 0.f) noexcept;
void  DockWindowIntoWindow(NkGuiContext &ctx, const char *hostTitle, const char *winTitle) noexcept;
int32 DockTabAddRequest(NkGuiContext &ctx) noexcept;
void  DockAddTab(NkGuiContext &ctx, const char *windowTitle, int32 node) noexcept;
```

Les zones de `DockBuilderDock` (`NkGuiWidgets.h:273`) : « 0 =
onglet, 1 = gauche, 2 = droite, 3 = haut, 4 = bas (sur la 1re feuille) ».

> **⚠️ `DockSpace` avant les `Begin`**
>
> « À appeler AVANT les Begin des fenêtres dockables »
> (`NkGuiWidgets.h:266-267`). L'espace de docking calcule les rectangles
> que les fenêtres ancrées viendront occuper ; si les fenêtres sont déclarées
> d'abord, elles utilisent les rectangles de l'image précédente.

L'arbre de dock est un arbre binaire de nœuds
(`NkGuiDockNode`, `NkGuiTypes.h:288-299`) : `kind` (0 =
vide, 1 = division, 2 = feuille), `vertical`, `ratio`,
`child0`/`child1`/`parent`, `windows[8]`,
`activeTab`, `rect`. Huit onglets au maximum par feuille.

## Le hook de style

Dernier mécanisme, et le plus élégant : re-styler entièrement l'interface sans
toucher à la logique des widgets.

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiTypes.h:301-325`**

```cpp
// ── Hook de style : override de DESSIN par widget ─────────────────────
// L'app enregistre `ctx.styleFn` ; pour chaque élément visuel, NKGui l'appelle
// AVANT le rendu par défaut. Si le callback retourne true, il a dessiné lui-même
// (le défaut est sauté) → re-skin TOTAL sans toucher la logique du widget.
enum class NkGuiStyleKind : uint8 {
    Button = 0, FrameBg, CheckMark, Header, Selectable, DockTarget, Count
};

struct NkGuiStyleItem {
        NkGuiStyleKind kind = NkGuiStyleKind::Button;
        NkRect rect = {0.f, 0.f, 0.f, 0.f};
        NkGuiId id = NKGUI_ID_NONE;
        const char *label = nullptr;
        bool hovered = false;
        bool active = false;   ///< pressé / en cours d'édition / cible visée
        bool selected = false;
        bool disabled = false;
        int32 value = 0;       ///< donnée par type (DockTarget : 0=onglet,1=G,2=D,3=H,4=B,5=bord)
};
```

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiContext.h:429-431`**

```cpp
using NkGuiStyleFn = bool (*)(NkGuiContext &, const NkGuiStyleItem &, void *);
NkGuiStyleFn styleFn = nullptr;
void *styleUser = nullptr;
```

Le contrat est simple : votre fonction reçoit la description de l'élément à
dessiner ; si elle retourne `true`, elle a dessiné et NKGui saute son
rendu par défaut ; si elle retourne `false`, le rendu par défaut a lieu.
Vous pouvez donc ne surcharger que les boutons et laisser le reste inchangé. La
démo en donne un exemple complet, `DemoStyle`
(`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:193-238`, branché ligne
271).

> **✅ Ce qu'il faut retenir**
>
> Pour changer les *couleurs*, on écrase `ctx.theme`. Pour changer la
> *forme* — un bouton en biseau, une case à cocher dessinée comme un
> interrupteur — on branche `ctx.styleFn`. On ne modifie jamais
> `NkGuiWidgets.cpp`.

## Les limites dures

Toutes ces constantes sont des tailles de tableaux fixes, décidées à la
compilation. Les dépasser ne plante pas : le framework ignore silencieusement
ce qui excède.

| **Limite** | **Valeur** | **Fichier:ligne** |
|---|---|---|
| Popups imbriqués | 8 | `NkGuiContext.h:147` |
| Surfaces d'occlusion | 16 | `NkGuiContext.h:175` |
| Fenêtres | 32 | `NkGuiContext.h:228` |
| Zones défilables imbriquées | 8 | `NkGuiContext.h:379` |
| Conteneurs de layout | 32 | `NkGuiContext.h:385` |
| Pile d'identifiants | 32 | `NkGuiContext.h:324` |
| Pile « désactivé » | 16 | `NkGuiContext.h:328` |
| Colonnes de table | 16 | `NkGuiContext.h:120` |
| Onglets par feuille de dock | 8 | `NkGuiTypes.h:295` |
| Caractères saisis par image | 32 | `NkGuiInput.h:53` |
| Pile de découpe (draw-list) | 32 | `NkGuiDrawList.h:52` |

## Récapitulatif des idiomes

1. Récupérer la liste de dessin par `ctx.DL()`, jamais
   `ctx.dl` en dur.
2. Convertir un coin haut-gauche en ligne de base par
   `y + font->Ascent()` ; centrer par
   `r.y + (r.h - font->LineHeight()) * 0.5f + font->Ascent()`.
3. Toujours vérifier `if (!ctx.font || !ctx.font->Valid())` avant
   `AddText` ou `MeasureWidth`, avec un repli chiffré.
4. `PushClipRect(zone, true)` … `PopClipRect()` —
   toujours appariés, toujours avec intersection.
5. Ajouter un filtrage manuel du hors-vue en plus de la découpe.
6. Molette : `if (Hit(zone) && ctx.input.wheel != 0.f) { scroll -= wheel * pas; ctx.input.wheel = 0.f; }` puis bornage.
7. Glissement : poser `ctx.activeId = monId` à l'appui, agir tant que
   `ctx.activeId == monId && mouseDown[0]`, remettre
   `ctx.activeId = 0` au relâchement.
8. Préférer `ctx.InputHits(r)` et `ctx.ClickIn(r)` à un test
   géométrique brut.
9. Surface flottante : `ctx.PushOcclusion(rect, couche)` +
   `NkInputLayerScope`.
10. Toute taille en dur passe par `ctx.S(px)`.
11. Identifiant stable, distinct du libellé affiché ;
    `PushId`/`PopId` autour des éléments d'une liste.
12. Lire les couleurs dans `ctx.theme` et `ctx.syntax`, jamais
    de littéral RGBA dans le contenu.
13. Ne jamais recharger un atlas de police pendant une image : le différer
    avant `BeginFrame`.
14. Différer toute mutation d'état lourde (ouverture de fichier,
    reconstruction d'arbre, accès disque) hors du rendu : lever un drapeau,
    agir à l'image suivante.

## Exercices

> **✏️ 1 — Le compteur**
>
> Écrivez, dans la boucle de la démo, un petit panneau contenant : un
> `Text` affichant une valeur entière, un bouton « + » et un bouton
> « − ». La valeur est une variable locale de votre boucle. Vérifiez que le
> compteur ne s'incrémente qu'au *relâchement* du bouton, et qu'appuyer puis
> glisser en dehors avant de relâcher n'incrémente pas. Remplacez ensuite
> `Button` par `RepeatButton` et observez la différence.

> **✏️ 2 — Le piège des identifiants**
>
> Affichez une liste de cinq éléments, chacun suivi d'un bouton « Supprimer ».
> Version A : sans `PushId`. Version B : avec `PushId(&items[i])`
> autour de chaque ligne. Observez ce qui se passe en version A quand vous
> survolez et cliquez. Expliquez le comportement à partir de `GetId` et de
> `ItemHoverable`.

> **✏️ 3 — Comprendre le z-ordre**
>
> Dessinez deux boutons qui se chevauchent partiellement, le second déclaré après
> le premier. Cliquez dans la zone de recouvrement : lequel réagit ? Inversez
> l'ordre de déclaration et recommencez. Ajoutez ensuite un `logger.Info`
> qui affiche `ctx.hotId` et `ctx.hotIdPrev` à chaque image, et
> observez le décalage d'une image en déplaçant lentement la souris d'un bouton à
> l'autre.

> **✏️ 4 — Une modale correcte**
>
> Écrivez une boîte de dialogue modale maison : un rectangle centré dessiné dans
> `ctx.dlOverlay` (via `PushOverlay`/`PopOverlay`), avec un
> fond assombri sur toute la fenêtre et deux boutons « Valider » et « Annuler ».
> Appliquez les gestes de la section 3.6 : `PushOcclusion(box, 100)`,
> `NkInputLayerScope`, et la fermeture au clic en dehors — en n'oubliant
> pas d'exiger `PointReachable` pour ce test négatif. Vérifiez qu'un clic
> sur la modale n'atteint pas les widgets qui sont dessous.
