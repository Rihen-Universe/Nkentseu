# NkWindowConfig : ce que chaque dorsal tient vraiment

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**

> Couche **Runtime** · NKWindow · La table de vérité **propriété × dorsal**.
>
> Elle existe parce qu'un utilisateur a posé `resizable = false`, vu sa fenêtre se redimensionner
> quand même, et cherché l'erreur chez lui pendant que le défaut était chez nous. **Une propriété
> qui s'accepte sans agir est pire qu'une propriété absente** : l'absence se voit à la compilation,
> le silence se paie en heures perdues.

## Comment lire la table

| Marque | Sens |
|---|---|
| **agit** | le dorsal lit le champ et produit l'effet promis |
| **silence** | le champ est accepté, jamais lu : aucun effet, **aucun message** — c'est le défaut que ce document existe pour tuer |
| **refus** | non tenu, mais **dit une fois au journal**, avec le nom de la propriété et de la plateforme |
| **s/o** | sans objet sur cette plateforme (il n'y a pas de barre de titre sur un téléphone) |
| **hors NKWindow** | le champ voyage dans `NkWindowConfig` mais c'est un autre module qui l'honore |

Règle du **refus** : il ne se déclenche **que si l'utilisateur a demandé une valeur différente du
défaut**. Une application qui ne touche à rien ne doit pas voir une page d'avertissements au
démarrage ; le journal ne parle que de ce qui a été *demandé* et ne sera pas *livré*.

---

## Table — état au **25/09/2026, AVANT le correctif Win32**

Mesure : comptage des lectures du champ dans `Kernel/Runtime/NKWindow/src/NKWindow/Platform/<dorsal>/`
(fichiers `.cpp`, `.h`, `.mm`), puis lecture du site de chaque lecture.

### Comportement

| Propriété | Win32 | Cocoa | XLib | XCB | Wayland | Android | UIKit | Emscripten | UWP | HarmonyOS | Xbox | Noop |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `resizable` | **silence** | agit | agit | agit | agit | s/o | s/o | silence | s/o | s/o | s/o | s/o |
| `movable` | **silence** | **silence** | **silence** | **silence** | **silence** | s/o | s/o | s/o | s/o | s/o | s/o | s/o |
| `closable` | **silence** | partiel¹ | **silence** | **silence** | **silence** | s/o | s/o | s/o | s/o | s/o | s/o | s/o |
| `minimizable` | **silence** | agit | **silence** | **silence** | **silence** | s/o | s/o | s/o | s/o | s/o | s/o | s/o |
| `maximizable` | **silence** | **silence** | **silence** | **silence** | **silence** | s/o | s/o | s/o | s/o | s/o | s/o | s/o |
| `canFullscreen` | **silence** | **silence** | **silence** | **silence** | **silence** | **silence** | **silence** | **silence** | **silence** | **silence** | s/o | s/o |
| `fullscreen` | agit | agit | agit | agit | agit | agit | agit | agit | agit | agit | agit | agit |
| `modal` | **silence** | **silence** | **silence** | **silence** | **silence** | s/o | s/o | s/o | s/o | s/o | s/o | s/o |
| `centered` | agit | agit | agit | agit | s/o² | s/o | s/o | s/o | s/o | s/o | s/o | s/o |
| `vsync` | hors NKWindow³ | hors NKWindow³ | hors NKWindow³ | hors NKWindow³ | hors NKWindow³ | hors NKWindow³ | hors NKWindow³ | hors NKWindow³ | hors NKWindow³ | hors NKWindow³ | hors NKWindow³ | hors NKWindow³ |
| `dropEnabled` | agit | **silence**⁴ | **silence**⁴ | **silence**⁴ | agit | s/o | s/o | **silence**⁴ | s/o | s/o | s/o | s/o |

¹ Cocoa pose `NSWindowStyleMaskClosable` **inconditionnellement** quand `frame` est vrai : le champ
`closable` n'est jamais lu, la fenêtre est toujours fermable.
² `xdg-shell` n'expose aucune requête de positionnement client : le compositeur place la fenêtre.
³ `vsync` traverse `NkWindowConfig` mais c'est `NKCanvas`/`NKRHI`/`NKRenderer` qui l'appliquent
   (`eglSwapInterval`, `IDXGISwapChain::Present`, mode de présentation Vulkan). NKWindow ne le lit
   qu'au passage EGL de `NkContext.cpp`.
⁴ La cible de dépôt est construite **sans condition** sur ces dorsaux : `dropEnabled = false`
   n'empêche rien.

### Taille

| Propriété | Win32 | Cocoa | XLib | XCB | Wayland | mobiles / consoles |
|---|---|---|---|---|---|---|
| `minWidth` / `minHeight` | agit **à tort**⁵ | **silence** | agit | agit | agit | s/o |
| `maxWidth` / `maxHeight` | **silence** | **silence** | **silence** | **silence** | agit | s/o |

⁵ `WM_GETMINMAXINFO` pose `ptMinTrackSize` **avec les valeurs client, sans `AdjustWindowRect`**.
   `ptMinTrackSize` est en coordonnées **fenêtre** : bordure et barre de titre comprises. Demander
   un client minimum de 160×90 laisse donc la fenêtre descendre à un client de 160−16 × 90−39,
   c'est-à-dire **plus petit que ce qui a été demandé**. `ptMaxTrackSize`, lui, n'est pas touché.

### Apparence

| Propriété | Win32 | Cocoa | XLib | XCB | Wayland | Android | UIKit | Emscripten | UWP | HarmonyOS | Xbox | Noop |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `frame` | agit | agit | agit | agit | agit | s/o | s/o | s/o | s/o | s/o | s/o | s/o |
| `hasShadow` | agit | agit | **silence** | **silence** | **silence** | s/o | s/o | s/o | s/o | s/o | s/o | s/o |
| `transparent` | agit | agit | agit | agit | agit | **silence** | agit | agit | **silence** | **silence** | agit | s/o |
| `visible` | agit | agit | agit | agit | agit | s/o | s/o | agit | agit | agit | agit | agit |
| `bgColor` | **silence** | **silence** | agit | agit | **silence** | **silence** | **silence** | **silence** | **silence** | **silence** | agit | s/o |
| `title` / `name` / `iconPath` | agit | agit | agit | agit | agit | s/o | s/o | agit (titre) | agit | s/o | s/o | s/o |

### Fenêtre discrète

| Propriété | Win32 | Cocoa | XLib | XCB | Wayland | Android | UIKit | Emscripten | UWP | HarmonyOS | Xbox | Noop |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `alwaysOnTop` | agit | agit | agit | agit | **silence**⁶ | **silence**⁷ | **silence**⁷ | **silence**⁷ | **silence**⁷ | **silence**⁷ | s/o | s/o |
| `clickThrough` | agit | agit | agit | agit | agit | **silence**⁷ | **silence**⁷ | **silence**⁷ | **silence**⁷ | **silence**⁷ | s/o | s/o |
| `opacity` | agit | agit | agit | agit | **silence**⁶ | **silence**⁷ | **silence**⁷ | **silence**⁷ | **silence**⁷ | **silence**⁷ | s/o | s/o |
| `noActivate` | **agit** (25/09) | **silence** | **silence** | **silence** | **silence** | **silence** | **silence** | **silence** | **silence** | **silence** | **silence** | **silence** |

⁶ `xdg-shell` n'a ni « toujours au-dessus » ni opacité par fenêtre : ce sont des protocoles
   d'extension (`wlr-layer-shell`) que nous n'implémentons pas.
⁷ **Le cas le plus grave de la table** : le dorsal **stocke la valeur dans `mConfig` et la
   rend par le getter**. `SetAlwaysOnTop(true)` suivi de `IsAlwaysOnTop()` rend `true` sur Android,
   alors que **rien n'a été fait**. Le code appelant n'a aucun moyen de savoir qu'il a été ignoré :
   il interroge notre variable, pas le système.

> Le commentaire de `NkWindowConfig::noActivate` annonce que « le dorsal écrit un refus nommé dans
> le journal » sur les plateformes qui ne le tiennent pas. **Ce refus n'existait nulle part** au
> moment où cette table a été écrite : la promesse du commentaire était elle-même un silence.

### Mobile

| Propriété | Win32 / Cocoa / X / Wayland | Android | UIKit | Emscripten | UWP | HarmonyOS | Xbox | Noop |
|---|---|---|---|---|---|---|---|---|
| `screenOrientation` | **silence** (bureau : s/o) | agit | agit | agit | **silence** | agit | s/o | s/o |
| `respectSafeArea` | s/o | agit | **silence** | **silence** | s/o | **silence** | s/o | s/o |
| `hideSystemUI` | s/o | agit | **silence** | **silence** | s/o | agit | s/o | s/o |
| `lockOrientation` | s/o | agit | **silence** | **silence** | s/o | agit | s/o | s/o |

---

## Les réglages à l'exécution

`NkWindow` n'expose **aucun** `SetResizable`, `SetMovable`, `SetClosable`, `SetMinimizable`,
`SetMaximizable`, `SetMinSize` ni `SetMaxSize` : ces cinq comportements et les bornes de taille ne
se règlent **qu'à la création**. C'est une limite à connaître, pas un défaut à corriger dans ce lot.

Les réglages qui existent après coup — et ce qu'ils valent sur Win32 :

| Méthode | Win32 | Remarque |
|---|---|---|
| `SetVisible`, `SetSize`, `SetPosition`, `SetTitle` | agit | |
| `Minimize` / `Maximize` / `Restore` | agit | agissent **même si** `minimizable`/`maximizable` sont faux : ce sont des ordres du programme, pas de l'utilisateur |
| `SetFullscreen` | agit | ne consulte pas `canFullscreen` |
| `SetDecorated` | **agit, et défaisait le correctif**⁸ | |
| `SetOpacity`, `SetAlwaysOnTop`, `SetClickThrough` | agit | et leurs getters interrogent le **système**, pas `mConfig` — c'est la bonne façon |
| `SetScreenOrientation`, `SetAutoRotateEnabled`, `SetWebInputOptions` | **silence** | corps vide sur Win32 |
| `BeginResize` | agit | ne consultait pas `resizable` |

⁸ `SetDecorated(true)` remettait `WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX` **sans
condition**. Une fenêtre créée `resizable = false` redevenait redimensionnable dès le premier
passage par une barre de titre personnalisée. Corriger la création sans corriger ce setter
n'aurait tenu que jusqu'au premier appel.

---

## Après le correctif du 25/09 — ce qui change

Les cases modifiées, et elles seules :

| Propriété | Win32 avant | Win32 après |
|---|---|---|
| `resizable` | silence | **agit** (`WS_THICKFRAME` + `WS_MAXIMIZEBOX` retirés ; `BeginResize` refuse) |
| `minimizable` | silence | **agit** (`WS_MINIMIZEBOX` retiré, `SC_MINIMIZE` grisé) |
| `maximizable` | silence | **agit** (`WS_MAXIMIZEBOX` retiré, `SC_MAXIMIZE` grisé) |
| `closable` | silence | **agit** (`SC_CLOSE` grisé, `WM_SYSCOMMAND` filtré → le bouton X s'éteint) |
| `movable` | silence | **agit** (`SC_MOVE` grisé, `WM_NCLBUTTONDOWN`/`HTCAPTION` filtré, `BeginDragMove` refuse) |
| `minWidth`/`minHeight` | agit à tort | **agit** (converties en coordonnées **fenêtre** par `AdjustWindowRectEx`) |
| `maxWidth`/`maxHeight` | silence | **agit** (`ptMaxTrackSize`, mêmes coordonnées) |
| `modal`, `canFullscreen`, `bgColor`, `screenOrientation`, `hideSystemUI`, `lockOrientation`, `respectSafeArea` | silence | **refus** nommé au journal |

Sur tous les autres dorsaux, les **silence** du tableau deviennent des **refus**. L'audit
(`NkWindowAuditerDorsalCourant`, dans `NkWindowAudit.cpp`) est appelé depuis
**`NkWESystem::RegisterWindow`** — le seul point que les douze `NkWindow::Create` traversent tous —
et parle une fois par couple (propriété, plateforme).

**Le choix est assumé, et il a un prix.** Un appel écrit dans chacun des douze `Create` aurait été
plus lisible, mais onze de ces fichiers ne sont compilés par aucune construction faite ici, et *un
refus qui ne compile pas est un silence de plus*. En contrepartie, les masques des onze dorsaux
non-Windows sont **lus dans le code, pas mesurés à l'exécution** : cette colonne du tableau est de
la lecture de source, pas de la mesure. La colonne Win32, elle, est mesurée par `NkWindowSonde`.

Mesure du 25/09 sur Win32, `NkWindowSonde` : **19 essais, 0 échec**, et le négatif
`--ancien-style` fait rougir les six critères de l'essai A pendant que le témoin de non-régression
reste vert. Deux chiffres qui résument le lot : `GWL_STYLE` passe de `0x04CF0000` (défaut) à
`0x04C80000` avec les trois interdits — exactement `WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX`
en moins ; et `ptMinTrackSize` rend `516×439` pour un client demandé de `500×400`, là où l'ancien
code rendait `500×400` et laissait donc la fenêtre descendre **sous** le minimum demandé.

### Le cas de la fenêtre sans cadre (`frame = false`)

`frame = false` gardait `WS_THICKFRAME` **exprès** : la bordure invisible donne l'accrochage Aero et
le redimensionnement natif que `BeginResize` relaie depuis notre décoration à nous.

**Décision : `resizable = false` gagne sur cette astuce.** `WS_THICKFRAME` et `WS_MAXIMIZEBOX`
tombent aussi sur une fenêtre sans cadre, `BeginResize` refuse au journal, et l'accrochage Aero est
perdu — c'est le prix, et il est juste : une fenêtre déclarée non redimensionnable n'a aucun bord à
accrocher. `WS_CAPTION | WS_SYSMENU` restent (ils portent l'animation de réduction et le menu
système, pas de pixels : `WM_NCCALCSIZE` rend toute la fenêtre cliente).

---

## Le banc

`Applications/NkWindowSonde` — il ne lit **aucune** de nos variables. Il crée des fenêtres
**invisibles** (`visible = false`, titrées `*** SONDE DE MESURE ***`) et interroge le système :

- `GetWindowLongW(GWL_STYLE)` → `WS_THICKFRAME`, `WS_MINIMIZEBOX`, `WS_MAXIMIZEBOX` ;
- `GetWindowLongW(GWL_EXSTYLE)` → `WS_EX_TOPMOST`, `WS_EX_TRANSPARENT`, `WS_EX_LAYERED`, `WS_EX_NOACTIVATE` ;
- `GetMenuState(GetSystemMenu(...))` → `SC_CLOSE`, `SC_MINIMIZE`, `SC_MAXIMIZE`, `SC_MOVE`, `SC_SIZE` ;
- `GetLayeredWindowAttributes` → l'octet alpha réellement posé ;
- `SendMessageW(WM_GETMINMAXINFO)` → les bornes que Windows appliquera, en coordonnées fenêtre.

Il porte son **négatif** : `--ancien-style` reconstruit le style d'avant le correctif et le banc
doit **rougir**. Un banc qui ne sait dire que « oui » ne mesure rien.

Deux critères **ne s'inversent pas** en mode négatif, et c'est voulu : le **témoin de
non-régression** (configuration par défaut == `WS_OVERLAPPEDWINDOW`, la garde des cinq applications
qui partagent NKWindow), et le **négatif de l'audit lui-même** — `resizable` *est* tenu, donc il ne
doit produire **aucun** refus. Un audit qui crie sur tout ne vaut pas mieux qu'un audit muet : il
apprend à l'utilisateur à ne plus lire son journal.
