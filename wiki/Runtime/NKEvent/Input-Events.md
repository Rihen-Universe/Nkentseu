# Les événements d'entrée

> Couche **Runtime** · NKEvent · Tout ce que l'utilisateur **fait** : frapper une touche,
> bouger la souris, cliquer, faire défiler, toucher l'écran, pincer pour zoomer — et les
> tables qui traduisent les codes natifs de chaque OS en un langage commun.

Une application interactive ne fait au fond qu'**écouter ce que l'utilisateur fait** puis y
réagir : déplacer un personnage, valider un menu, faire défiler une liste, zoomer une carte.
Le problème, c'est que chaque système d'exploitation décrit ces gestes dans son propre
dialecte — un `VK_*` sous Windows, un `KeySym` sous X11, un `keyCode` Carbon sous macOS, une
chaîne `"KeyA"` dans le navigateur. NKEvent répond à ce problème par deux idées qui se
complètent : d'abord un **vocabulaire commun** d'événements typés (clavier, souris, tactile,
gestes), ensuite des **tables de conversion** qui ramènent tous ces dialectes natifs vers un
seul ensemble de codes — la position physique d'une touche sur un clavier US-QWERTY.

Tout part d'une classe de base, `NkEvent`, dont héritent tous les événements. Elle apporte le
**dispatch type-safe** (`event.Is<T>()` qui teste le type, `event.As<T>()` qui renvoie un
`T*` ou `nullptr`), le clonage (`Clone()`), la conversion texte (`ToString()`), la catégorie
(`GetCategoryFlags()`), l'identifiant de fenêtre (`GetWindowId()`) et le marquage « traité »
(`MarkHandled()`). Le code utilisateur ne fait, en général, que tester le type et lire les
accesseurs :

```cpp
if (auto* k = event.As<NkKeyPressEvent>()) {
    if (k->GetKey() == NkKey::NK_SPACE) Jump();
}
```

Ce n'est **pas** un système de signaux : un événement est un objet qu'on inspecte, pas un
callback qu'on branche. Et attention à la mémoire — `Clone()` est l'un des rares endroits du
moteur qui utilise `new`/`delete` du CRT (la couche événementielle), à distinguer du pattern
NKMemory du reste. Le pointeur renvoyé par `Clone()` vous appartient : libérez-le avec
`delete`.

- **Namespace** : `nkentseu` (tout est directement ici — **pas** de sous-namespace `event`)
- **Headers** : `NKEvent/NkKeyboardEvent.h` · `NKEvent/NkMouseEvent.h` ·
  `NKEvent/NkTouchEvent.h` · `NKEvent/NkKeycodeMap.h`
- **Base commune** : `NKEvent/NkEvent.h`

---

## Le clavier : touches, modificateurs, saisie texte

Le clavier porte deux besoins **radicalement différents**, et NKEvent les sépare nettement.
D'un côté, les **raccourcis** et les **commandes de jeu** : « la touche à la position du W est
enfoncée » — peu importe que le clavier soit AZERTY ou QWERTY, on veut la **position
physique**. De l'autre, la **saisie de texte** : « l'utilisateur a tapé le caractère é » —
là, au contraire, on veut le caractère final, après application du layout et de l'IME.

Pour le premier besoin, `NkKey` désigne une touche par sa **position** sur un clavier de
référence US-QWERTY. `NK_W` est toujours la touche en haut à gauche de la zone de
déplacement, qu'elle produise un `w`, un `z` (AZERTY) ou autre chose. C'est exactement ce
qu'il faut pour des commandes WASD qui marchent partout sans reconfiguration. À côté,
`NkScancode` décrit la touche par son **code physique USB HID** (HID 1.11) — la couche la plus
basse, indépendante de l'OS, à partir de laquelle `NkScancodeToKey` reconstruit la `NkKey`.

```cpp
if (auto* k = event.As<NkKeyPressEvent>()) {
    switch (k->GetKey()) {              // position physique, pas le caractère
        case NkKey::NK_W: MoveForward(); break;
        case NkKey::NK_S: MoveBack();    break;
    }
    if (k->HasCtrl() && k->GetKey() == NkKey::NK_S) Save();   // Ctrl+S
}
```

Les **modificateurs** (Ctrl, Alt, Shift, Super, AltGr) et les **verrous** (Num/Caps/Scroll
Lock) sont regroupés dans une petite structure `NkModifierState`, accessible sur chaque
événement clavier via `GetModifiers()`, avec des raccourcis directs `HasCtrl()`, `HasAlt()`,
`HasShift()`, `HasSuper()`, `HasAltGr()`.

Pour le second besoin — taper du texte dans un champ — il y a `NkTextInputEvent`. Il ne porte
**pas** une `NkKey` mais un **codepoint Unicode** déjà résolu par le layout et l'IME, encodé
en UTF-8. C'est lui qu'on lit pour remplir une zone de texte ; les `NkKeyPressEvent` servent,
eux, aux raccourcis et aux commandes. Ne confondez pas les deux : `NkKeyPressEvent` pour
« quelle touche », `NkTextInputEvent` pour « quel caractère ».

> **En résumé.** `NkKey` = position physique US-QWERTY (commandes de jeu, raccourcis,
> indépendant du layout). `NkScancode` = code USB HID brut (couche la plus basse).
> `NkModifierState` regroupe Ctrl/Alt/Shift/Super/AltGr + les verrous. Trois événements de
> frappe : `NkKeyPressEvent` (1ʳᵉ frappe), `NkKeyRepeatEvent` (auto-repeat OS, avec compteur),
> `NkKeyReleaseEvent` (relâchement). Et `NkTextInputEvent`, **distinct**, pour la saisie de
> caractères Unicode.

---

## La souris : position, boutons, molette, capture

La souris est plus riche qu'il n'y paraît, parce qu'elle mêle plusieurs flux : la
**position** (où est le curseur), les **boutons** (lesquels sont enfoncés, simple ou double
clic), le **défilement** (molette ou trackpad), et les transitions de **focus et de capture**
(le curseur entre/sort de la fenêtre, une fenêtre capture la souris pendant un drag).

Tous ces événements partagent une base, `NkMouseEvent`. Le mouvement vient en deux saveurs
qu'il ne faut **pas** confondre : `NkMouseMoveEvent` donne la position en coordonnées client et
écran *plus* un delta — c'est le mouvement « tel que vu par l'OS », avec son accélération ;
`NkMouseRawEvent` donne au contraire les **comptes bruts du capteur**, sans accélération ni
filtrage, indispensables pour viser dans un FPS où l'on veut un contrôle 1:1.

```cpp
if (auto* m = event.As<NkMouseMoveEvent>()) {
    cursor = { m->GetX(), m->GetY() };        // position client
}
if (auto* b = event.As<NkMouseButtonPressEvent>()) {
    if (b->IsLeft()) Select(b->GetX(), b->GetY());
}
```

Les boutons donnent `NkMouseButtonPressEvent` / `NkMouseButtonReleaseEvent`, et — *en plus*
des press/release individuels — un `NkMouseDoubleClickEvent` quand l'OS détecte un double
clic. Chaque event bouton sait quel bouton (`GetButton()`, plus les raccourcis `IsLeft()` /
`IsRight()` / `IsMiddle()`), à quelle position, et son `GetClickCount()`.

Le défilement, lui aussi, vient en plusieurs formes : `NkMouseScrollEvent` unifie les deux
axes (idéal pour les trackpads qui défilent en diagonale), tandis que
`NkMouseWheelVerticalEvent` et `NkMouseWheelHorizontalEvent` traitent un seul axe à la fois
(une molette classique). Tous distinguent les **clics logiques** (un cran de molette) des
**pixels** (défilement lisse haute précision d'un trackpad), via `IsHighPrecision()`.

Enfin une famille d'événements **sans données** signale les transitions : la souris entre ou
sort de la zone client (`NkMouseEnter`/`Leave`), entre ou sort de la fenêtre cadre compris
(`NkMouseWindowEnter`/`Leave`), et la capture exclusive commence ou finit
(`NkMouseCaptureBegin`/`End` — la fin pouvant être imposée par l'OS).

> **En résumé.** Base `NkMouseEvent`. Mouvement : `NkMouseMoveEvent` (avec accélération OS)
> contre `NkMouseRawEvent` (capteur brut, FPS). Boutons : Press/Release + un DoubleClick
> *additionnel*. Défilement : `NkMouseScrollEvent` (2D unifié) contre Wheel Vertical/Horizontal
> (1 axe), `IsHighPrecision()` pour le trackpad. Et 6 events de focus/capture sans payload.

---

## Le tactile et les gestes

Sur écran tactile, un événement ne porte plus *un* point mais **plusieurs doigts à la fois** —
c'est le multi-touch. NKEvent modélise chaque doigt par un `NkTouchPoint` (identifiant stable,
phase, positions client/écran/normalisées, delta, pression, rayon, angle), et chaque
événement tactile transporte un **tableau** de ces points plus leur **centroïde** (le centre
de gravité du contact, déjà calculé).

Les quatre événements bruts suivent le cycle de vie d'un toucher : `NkTouchBeginEvent` (un
doigt se pose), `NkTouchMoveEvent` (il glisse), `NkTouchEndEvent` (il se lève),
`NkTouchCancelEvent` (le système annule, par exemple un appel entrant). Deux pièges importants
ici : `GetTouch(i)` ne fait **aucun contrôle de bornes** (comportement indéfini si
`i >= GetNumTouches()`), et le constructeur **clampe silencieusement** le nombre de points à
32 (`NK_MAX_TOUCH_POINTS`), en copiant dans un buffer interne fixe — un event tactile est donc
relativement lourd.

```cpp
if (auto* t = event.As<NkTouchMoveEvent>()) {
    for (uint32 i = 0; i < t->GetNumTouches(); ++i) {
        const NkTouchPoint& p = t->GetTouch(i);   // i borné par GetNumTouches() !
        Drag(p.id, p.clientX, p.clientY);
    }
}
```

Au-dessus des touchers bruts viennent les **gestes** reconnus : pincer pour zoomer
(`NkGesturePinchEvent`), pivoter (`NkGestureRotateEvent`), glisser à plusieurs doigts
(`NkGesturePanEvent`), balayer (`NkGestureSwipeEvent`), tapoter (`NkGestureTapEvent`), et
appuyer longuement (`NkGestureLongPressEvent`). Subtilité d'architecture à connaître : ces
gestes dérivent **directement de `NkEvent`**, pas de `NkTouchEvent` — ils ne portent donc
**pas** la catégorie `NK_CAT_TOUCH`. Ils représentent une interprétation de haut niveau, pas
un contact brut.

> **En résumé.** Un `NkTouchPoint` par doigt ; les events bruts (Begin/Move/End/Cancel)
> portent un tableau (clampé à 32) + le centroïde. `GetTouch(i)` n'est **pas** vérifié. Les
> gestes (Pinch/Rotate/Pan/Swipe/Tap/LongPress) sont des `NkEvent` directs, sans catégorie
> tactile, et exposent des helpers parlants (`IsZoomIn`, `IsClockwise`, `IsDoubleTap`…).

---

## Le mapping des codes natifs : `NkKeycodeMap`

Pour qu'un `NkKeyPressEvent` arrive avec la bonne `NkKey` quel que soit l'OS, il faut traduire
le code natif que la fenêtre reçoit. C'est le rôle des fonctions de conversion (libres, dans
`NkKeyboardEvent.h`) et de l'utilitaire **100 % statique** `NkKeycodeMap`. On donne un code
Windows VK, un KeySym X11, un keyCode macOS, une chaîne DOM ou un keycode Android, et on
récupère une `NkKey` normalisée. Ces conversions sont **déterministes** et sans état — ce sont
des tables.

> **En résumé.** `NkKeycodeMap` (méthodes statiques) traduit tout dialecte natif → `NkKey`.
> Une seule fonction par OS (`NkKeyFromWin32VK`, `NkKeyFromX11KeySym`, `NkKeyFromMacKeyCode`,
> `NkKeyFromDomCode`, `NkKeyFromAndroid`), plus `Normalize`/`SameKey` pour fusionner les
> variantes gauche/droite. Attention : `NkKeyToScancode` est un **stub** (renvoie toujours
> `NK_SC_UNKNOWN`).

---

## Aperçu de l'API

Tous les éléments publics, par fichier. Chacun est détaillé en « Référence complète ».

### Clavier — `NkKeyboardEvent.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Touches | `enum class NkKey : uint32` | Touche par **position physique** US-QWERTY ; `NK_UNKNOWN=0`…`NK_KEY_MAX`. |
| Touches | alias `NK_ADD/SUBTRACT/MULTIPLY/DIVIDE/DECIMAL` | Alias constexpr vers les touches numpad (hors enum, ≠ `NK_EQUALS`/`NK_MINUS`/`NK_PERIOD`). |
| Touches | `NkKeyToString` `[noexcept]` | `NkKey` → littéral statique (« NK_A »). Ne pas libérer. |
| Touches | `NkKeyIsModifier` / `NkKeyIsNumpad` / `NkKeyIsFunctionKey` `[noexcept]` | Tests de famille (modificateur / numpad / F1–F24). |
| Scancodes | `enum class NkScancode : uint32` | Code physique **USB HID** ; `NK_SC_UNKNOWN=0`…`NK_SC_MAX=0x100`. |
| Scancodes | `NkScancodeToString` `[noexcept]` | `NkScancode` → littéral statique (« SC_A »). |
| Scancodes | `NkScancodeToKey` `[noexcept]` | **Cœur** du mapping : HID → `NkKey`. |
| Scancodes | `NkScancodeFromWin32/Linux/XKeycode/Mac/DOMCode` `[noexcept]` | Code natif d'un OS → `NkScancode`. |
| Modificateurs | `struct NkModifierState` | `ctrl/alt/shift/super/altGr/numLock/capLock/scrLock`. |
| Modificateurs | `Any` / `IsNone` / `operator==` / `!=` / `ToString` | État actif ? ; comparaison (**ignore les verrous**) ; texte. |
| Base | `class NkKeyboardEvent : NkEvent` (abstraite) | Catégorie `KEYBOARD\|INPUT`. |
| Base | `GetKey/GetScancode/GetModifiers/GetNativeKey/IsExtended` `[noexcept]` | Accesseurs de la frappe. |
| Base | `HasCtrl/HasAlt/HasShift/HasSuper/HasAltGr` `[noexcept]` | Raccourcis modificateurs. |
| Frappe | `NkKeyPressEvent` (`NK_KEY_PRESSED`) | 1ʳᵉ frappe (non répétée). |
| Frappe | `NkKeyRepeatEvent` (`NK_KEY_REPEATED`) | Auto-repeat OS ; `GetRepeatCount()`. |
| Frappe | `NkKeyReleaseEvent` (`NK_KEY_RELEASED`) | Relâchement. |
| Texte | `class NkTextInputEvent : NkEvent` (`NK_TEXT_INPUT`) | Caractère Unicode saisi (après layout/IME). |
| Texte | `GetCodepoint/GetUtf8/IsPrintable/IsAscii` `[noexcept]` | Codepoint UTF-32 / UTF-8 (≤5 o) / imprimable ? / ASCII ? |

### Souris — `NkMouseEvent.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Boutons | `enum class NkMouseButton : uint32` | `NK_MB_UNKNOWN=0`, LEFT/RIGHT/MIDDLE/BACK/FORWARD/6/7/8, `NK_MOUSE_BUTTON_MAX`. |
| Boutons | `NkMouseButtonToString` `[noexcept]` | Bouton → littéral. |
| Boutons | `enum class NkButtonState : uint32` | `NK_RELEASED=0`, `NK_PRESSED=1`. |
| Boutons | `struct NkMouseButtons` | Masque de bits ; `Set/Clear/IsDown/Any/IsNone` `[O(1)]`. |
| Base | `class NkMouseEvent : NkEvent` (abstraite) | Catégorie `MOUSE\|INPUT`. |
| Mouvement | `NkMouseMoveEvent` (`NK_MOUSE_MOVE`) | Position client/écran + delta + boutons + modificateurs. |
| Mouvement | `NkMouseRawEvent` (`NK_MOUSE_RAW`) | Delta **brut** capteur (`GetDeltaX/Y/Z`). |
| Bouton (base) | `class NkMouseButtonEvent : NkMouseEvent` (abstraite) | `GetButton/GetState/GetClickCount/IsLeft/IsRight/IsMiddle`… |
| Bouton | `NkMouseButtonPressEvent` (`NK_MOUSE_BUTTON_PRESSED`) | Enfoncement. |
| Bouton | `NkMouseButtonReleaseEvent` (`NK_MOUSE_BUTTON_RELEASED`) | Relâchement. |
| Bouton | `NkMouseDoubleClickEvent` (`NK_MOUSE_DOUBLE_CLICK`) | Double clic (clickCount=2), **en plus** des press/release. |
| Molette (base) | `class NkMouseWheelEvent : NkMouseEvent` (abstraite) | Deltas logiques + pixels ; `IsHighPrecision`. |
| Molette | `NkMouseScrollEvent` (`NK_MOUSE_SCROLL`) | Scroll **2D unifié** ; `ScrollsUp/Down/Left/Right`. |
| Molette | `NkMouseWheelVerticalEvent` (`NK_MOUSE_WHEEL_VERTICAL`) | Axe vertical ; `ScrollsUp/Down`. |
| Molette | `NkMouseWheelHorizontalEvent` (`NK_MOUSE_WHEEL_HORIZONTAL`) | Axe horizontal ; `ScrollsLeft/Right`. |
| Focus | `NkMouseEnterEvent` / `NkMouseLeaveEvent` | Entrée/sortie zone **client**. |
| Focus | `NkMouseWindowEnterEvent` / `NkMouseWindowLeaveEvent` | Entrée/sortie **fenêtre** (cadre compris). |
| Capture | `NkMouseCaptureBeginEvent` / `NkMouseCaptureEndEvent` | Capture exclusive (la fin peut être forcée par l'OS). |

### Tactile & gestes — `NkTouchEvent.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Phases | `enum class NkTouchPhase : uint32` | BEGAN/MOVED/STATIONARY/ENDED/CANCELLED + MAX. |
| Direction | `enum class NkSwipeDirection : uint32` | NONE/LEFT/RIGHT/UP/DOWN ; `NkSwipeDirectionToString`. |
| Constante | `NK_MAX_TOUCH_POINTS = 32` | Capacité max d'un event tactile. |
| Point | `struct NkTouchPoint` | `id/phase/clientX-Y/screenX-Y/normalX-Y/deltaX-Y/pressure/radiusX-Y/angle` ; `HasMoved/IsActive`. |
| Base | `class NkTouchEvent : NkEvent` (abstraite) | Catégorie `TOUCH` seule ; `GetNumTouches/GetTouch/GetCentroidX/Y`. |
| Toucher | `NkTouchBeginEvent` (`NK_TOUCH_BEGIN`) | Doigt(s) posé(s). |
| Toucher | `NkTouchMoveEvent` (`NK_TOUCH_MOVE`) | Glissement. |
| Toucher | `NkTouchEndEvent` (`NK_TOUCH_END`) | Levé. |
| Toucher | `NkTouchCancelEvent` (`NK_TOUCH_CANCEL`) | Annulation système. |
| Geste | `NkGesturePinchEvent` (`NK_GESTURE_PINCH`) | Zoom ; `GetScale/GetScaleDelta/GetCenterX-Y/IsZoomIn/IsZoomOut`. |
| Geste | `NkGestureRotateEvent` (`NK_GESTURE_ROTATE`) | Rotation ; `GetAngle/GetAngleDelta/IsClockwise`. |
| Geste | `NkGesturePanEvent` (`NK_GESTURE_PAN`) | Pan ; `GetDeltaX-Y/GetVelocityX-Y/GetNumFingers`. |
| Geste | `NkGestureSwipeEvent` (`NK_GESTURE_SWIPE`) | Balayage ; `GetDirection/GetSpeed/GetNumFingers/IsLeft-Right-Up-Down`. |
| Geste | `NkGestureTapEvent` (`NK_GESTURE_TAP`) | Tape ; `GetX-Y/GetTapCount/GetNumFingers/IsDoubleTap`. |
| Geste | `NkGestureLongPressEvent` (`NK_GESTURE_LONG_PRESS`) | Appui long ; `GetX-Y/GetDurationMs/GetNumFingers`. |

### Mapping — `NkKeycodeMap.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Scancode | `ScancodeToNkKey` / `NkKeyToScancode` | HID → NkKey ; **inverse = stub** (toujours `NK_SC_UNKNOWN`). |
| Windows | `NkKeyFromWin32VK` / `NkKeyFromWin32Scancode` | `VK_*` (+ flag `extended`) → NkKey ; scancode PS/2 → NkKey. |
| Linux/X11 | `NkKeyFromX11KeySym` / `NkKeyFromX11Keycode` | KeySym X11 → NkKey ; keycode X11 (offset 8) → NkKey. |
| macOS | `NkKeyFromMacKeyCode` | keyCode Carbon (0x00–0x7E) → NkKey. |
| Web | `NkKeyFromDomCode` | Chaîne DOM `code` (« KeyA ») → NkKey ; **O(n)** (recherche linéaire). |
| Android | `NkKeyFromAndroid` | `AKEYCODE_*` → NkKey. |
| Normalisation | `Normalize` / `SameKey` | Fusionne droite→gauche ; égalité après normalisation. |

---

## Référence complète

Les éléments triviaux (enums, accesseurs) sont décrits brièvement ; les pièces importantes
(le mapping des codes, les modificateurs, le tactile, les pièges mémoire) le sont **à fond**,
avec leurs usages par domaine.

### `NkEvent`, le socle : `Is<T>` / `As<T>` / `Clone`

Tout événement hérite de `NkEvent` et reçoit son outillage : `GetType()`/`GetTypeStr()` (le
type concret, posé par la macro `NK_EVENT_TYPE_FLAGS`), `GetCategoryFlags()` (les catégories
`NkEventCategory`, posées par `NK_EVENT_CATEGORY_FLAGS`), `GetWindowId()`, `MarkHandled()`,
`ToString()` et `Clone()`. Le dispatch se fait par `event.Is<T>()` (teste le type) et
`event.As<T>()` (renvoie `T*` ou `nullptr`), ce qui évite tout `dynamic_cast` et tout
`switch` géant côté appelant. Un point capital dans tous les domaines : **`Clone()` alloue
avec `new` du CRT** — c'est la seule entorse au pattern NKMemory, justifiée par la nature
événementielle. Quand un système met un événement en file (rejouer une entrée, réseau, undo),
il en garde un `Clone()` et devient responsable du `delete`.

- **Gameplay / IA** — `As<NkKeyPressEvent>()` puis lecture de `GetKey()` pour router les
  commandes ; `MarkHandled()` pour qu'un layer supérieur (UI) consomme l'event avant le jeu.
- **UI / 2D** — un widget teste `Is<NkMouseButtonPressEvent>()` et, s'il est sous le curseur,
  appelle `MarkHandled()` pour bloquer la propagation au monde.
- **Outils / éditeur** — un système d'enregistrement clone les events bruts pour rejouer une
  session ; chaque clone est libéré par `delete` (ownership transféré à l'appelant).
- **IO / réseau** — sérialiser une intention dérivée de l'event (jamais l'objet event tel quel,
  qui contient des codes natifs spécifiques OS).

### `NkKey` — la touche par sa position

`NkKey` énumère les touches par leur **position physique** sur un clavier US-QWERTY, de
`NK_UNKNOWN = 0` jusqu'à la sentinelle `NK_KEY_MAX` (qui vaut le nombre de valeurs valides —
pratique pour dimensionner un tableau d'état clavier). Les familles couvrent les fonctions
(`NK_F1`…`NK_F24`), la ligne supérieure, les quatre rangées de lettres, la rangée inférieure
(modificateurs, espace, menu), la navigation/édition, les flèches, le pavé numérique, le
multimédia, le navigateur, l'IME (kana/kanji/hangul…) et l'OEM. Le sens « position, pas
caractère » est ce qui rend les commandes portables :

- **Gameplay** — un schéma WASD codé en `NK_W/A/S/D` reste sous les doigts quel que soit le
  layout (QWERTY, AZERTY, QWERTZ) sans aucune reconfiguration.
- **UI / éditeur** — les raccourcis (`NK_S` pour Save, `NK_Z` pour Undo) se définissent par
  position ; on affiche le **caractère** correspondant séparément via `NkTextInputEvent`.
- **Tableau d'état** — indexer un `bool down[NK_KEY_MAX]` par `static_cast<uint32>(key)` donne
  un état clavier complet en `O(1)` par touche.

Les **alias hors enum** `NK_ADD`/`NK_SUBTRACT`/`NK_MULTIPLY`/`NK_DIVIDE`/`NK_DECIMAL` pointent
sur les touches du pavé numérique (`NK_NUMPAD_ADD`…) : ils sont définis *en dehors* de l'enum
pour ne pas fausser `NK_KEY_MAX`, et sont **distincts** de leurs homonymes de la rangée
principale — `NK_ADD ≠ NK_EQUALS`, `NK_SUBTRACT ≠ NK_MINUS`, `NK_DECIMAL ≠ NK_PERIOD`. Une
calculatrice doit en tenir compte.

Les tests de famille sont des helpers `noexcept` : `NkKeyIsModifier` (true pour les huit
Ctrl/Alt/Shift/Super gauche et droite — AltGr est traité à part), `NkKeyIsNumpad` (tout
`NK_NUMPAD_*`, y compris l'Enter du pavé et le Num Lock), `NkKeyIsFunctionKey` (dans la plage
`NK_F1`–`NK_F24`). `NkKeyToString` rend un **littéral statique** (« NK_A ») — utile pour le log
et les éditeurs de raccourcis, à ne jamais libérer.

### `NkScancode` — le code physique USB HID

`NkScancode` est la couche la plus basse : un identifiant d'usage HID 1.11, indépendant et de
l'OS et du layout, de `NK_SC_UNKNOWN = 0` à `NK_SC_MAX = 0x100`. Beaucoup de valeurs sont
explicites et reconnaissables (lettres `NK_SC_A = 0x04`…, modificateurs `NK_SC_LCTRL = 0xE0`…),
ce qui en fait la **clé de voûte** du portage : chaque OS fournit une fonction qui ramène son
code natif vers un `NkScancode`, et une seule fonction, `NkScancodeToKey`, finit le travail en
produisant la `NkKey`. C'est la fonction qu'un nouveau backend de fenêtre doit câbler.

- **Portage / fenêtrage** — un `NkWin32Window` appelle `NkScancodeFromWin32(sc, extended)` puis
  `NkScancodeToKey(...)`. Les autres backends font de même avec
  `NkScancodeFromLinux` (evdev 0–255), `NkScancodeFromXKeycode` (X11, **retranche
  automatiquement l'offset 8**), `NkScancodeFromMac` (Carbon 0–127),
  `NkScancodeFromDOMCode` (chaîne `KeyboardEvent.code`, ex « KeyA »).
- **Configuration bas niveau** — un remapping clavier d'éditeur peut raisonner en scancodes
  pour rester insensible au layout courant.
- **Diagnostic** — `NkScancodeToString` (« SC_A ») pour journaliser le code physique reçu.

### `NkModifierState` — Ctrl, Alt, Shift, Super, AltGr et les verrous

Cette petite structure agrège l'état des modificateurs et des verrous au moment d'un
événement : `ctrl` (gauche|droit fusionnés), `alt` (Alt gauche, **hors** AltGr), `shift`,
`super` (Win/Cmd/Meta), `altGr`, plus les trois verrous `numLock`/`capLock`/`scrLock` (faux
par défaut). On la construit vide, ou via le constructeur léger
`NkModifierState(ctrl, alt, shift, super=false)`. `Any()` est vrai si l'un des modificateurs —
mais **pas** les verrous — est actif, `IsNone()` est son inverse. Subtilité à retenir
absolument : **`operator==` (et `!=`) ne compare que ctrl/alt/shift/super/altGr et ignore les
trois verrous** — deux états avec Caps Lock différent sont donc considérés égaux.

- **Raccourcis (UI/éditeur)** — comparer un état courant à un raccourci attendu
  (`mods == NkModifierState(true, false, true)` pour Ctrl+Shift) marche justement parce que les
  verrous sont ignorés : peu importe que Caps soit allumé.
- **Saisie de texte** — `altGr` distingue les caractères du troisième niveau (`@`, `#`…) sur
  layout européen ; il est volontairement séparé de `alt`.
- **Gameplay** — `Any()` pour savoir si un modificateur est tenu (sprint = Shift + déplacement).
- **Log / debug** — `ToString()` rend une chaîne lisible (« Ctrl+Shift », « AltGr », « None »)
  dans un ordre fixe Ctrl, Alt, Shift, Super, AltGr.

### `NkKeyboardEvent` et les frappes Press / Repeat / Release

La base abstraite `NkKeyboardEvent` (catégorie `KEYBOARD|INPUT`) porte la frappe : `GetKey()`,
`GetScancode()`, `GetModifiers()`, le code natif brut `GetNativeKey()` (VK / KeySym / keyCode
selon l'OS), et `IsExtended()` (préfixe E0 du PS/2, surtout pertinent sous Win32). Les
raccourcis `HasCtrl/HasAlt/HasShift/HasSuper/HasAltGr` lisent directement les modificateurs.
Trois sous-classes concrètes :

- **`NkKeyPressEvent`** (`NK_KEY_PRESSED`) — la **première** frappe, non répétée. C'est l'event
  des commandes ponctuelles (sauter, tirer, valider). Tous ses paramètres après `key` ont une
  valeur par défaut, donc `NkKeyPressEvent(NkKey::NK_SPACE)` suffit pour un test.
- **`NkKeyRepeatEvent`** (`NK_KEY_REPEATED`) — l'**auto-repeat** de l'OS quand on maintient une
  touche. Il ajoute `GetRepeatCount()`. À utiliser pour la répétition « naturelle » d'un champ
  de texte (effacer caractère par caractère) ; à **ignorer**, en général, pour les commandes de
  jeu (où l'on veut une seule action par appui).
- **`NkKeyReleaseEvent`** (`NK_KEY_RELEASED`) — le relâchement, indispensable pour les actions
  maintenues (lâcher la gâchette, arrêter un déplacement). Même signature que Press.

### `NkTextInputEvent` — la saisie de caractères

C'est le pendant « texte » du clavier, et il hérite **directement de `NkEvent`** (pas de
`NkKeyboardEvent`), tout en déclarant la catégorie `KEYBOARD|INPUT`. Il transporte un
**codepoint Unicode** déjà résolu (layout + IME appliqués), accessible en UTF-32 via
`GetCodepoint()` et en UTF-8 via `GetUtf8()` (buffer interne de **5 octets max** — 4 + nul — à
ne pas modifier). `IsPrintable()` filtre les caractères de contrôle (`cp >= 0x20 && cp != 0x7F`)
et `IsAscii()` teste `cp < 0x80`. Le constructeur prend le codepoint et encode automatiquement
l'UTF-8 (RFC 3629 ; il ne valide pas les codepoints au-delà de `0x10FFFF`).

- **UI / 2D** — remplir un champ de texte : on n'écoute **que** `NkTextInputEvent`, jamais les
  `NkKeyPressEvent` (qui donneraient des positions, pas des caractères, et casseraient l'IME et
  les layouts non-latins).
- **Localisation** — comme le codepoint est déjà Unicode, accents, emoji et idéogrammes
  arrivent corrects sans traitement supplémentaire.
- **Filtrage** — `IsPrintable()` pour rejeter les caractères de contrôle avant d'insérer ;
  `IsAscii()` pour un champ restreint (nom de fichier, code).

### La souris : mouvement, boutons, molette, focus

`NkMouseEvent` est la base abstraite (catégorie `MOUSE|INPUT`). Au-dessus :

- **`NkMouseMoveEvent`** (`NK_MOUSE_MOVE`) — position client (`GetX/GetY`), position écran
  (`GetScreenX/Y`), delta (`GetDeltaX/Y`), boutons tenus (`GetButtons`, `IsButtonDown`) et
  modificateurs. C'est le mouvement « OS », avec accélération du pointeur — celui qu'on veut
  pour un curseur d'UI.
- **`NkMouseRawEvent`** (`NK_MOUSE_RAW`) — les comptes **bruts** du capteur (`GetDeltaX/Y/Z`),
  sans accélération ni filtrage. Indispensable pour la **visée FPS** (contrôle 1:1) et toute
  caméra qui doit ignorer les réglages OS.
- **Boutons** : la base `NkMouseButtonEvent` porte `GetButton()`, `GetState()`, la position, le
  `GetClickCount()` et les raccourcis `IsLeft/IsRight/IsMiddle`. Les concrets sont
  `NkMouseButtonPressEvent` (`NK_MOUSE_BUTTON_PRESSED`, state forcé PRESSED),
  `NkMouseButtonReleaseEvent` (`NK_MOUSE_BUTTON_RELEASED`, RELEASED) et
  `NkMouseDoubleClickEvent` (`NK_MOUSE_DOUBLE_CLICK`, clickCount=2) — ce dernier est émis **en
  plus** des press/release individuels, donc un double clic produit aussi les clics simples.
- **Focus / capture** (events sans données, ctor `(windowId=0)`) : `NkMouseEnter`/`Leave`
  (zone client), `NkMouseWindowEnter`/`Leave` (fenêtre cadre compris),
  `NkMouseCaptureBegin`/`End` (capture exclusive d'un drag ou d'un modal, dont la fin peut être
  imposée par l'OS).

Par domaine : **UI/2D** — hover (`Move` + `Enter/Leave`), clic et drag (`Press`→`Move` sous
capture→`Release`), double clic pour ouvrir ; **gameplay/caméra** — `Raw` pour la visée, `Move`
pour un curseur libre ; **outils/éditeur** — `CaptureBegin/End` encadrent un déplacement de
poignée pour ne pas perdre l'event quand la souris sort de la fenêtre.

### Le défilement : `Scroll` contre `Wheel V/H`

La base `NkMouseWheelEvent` distingue deux unités : des **clics logiques**
(`GetDeltaX/Y`, `GetOffsetX/Y` qui en sont l'alias) et des **pixels** (`GetPixelDeltaX/Y`,
valides seulement si `IsHighPrecision()` — un trackpad qui défile en continu). Trois events
concrets, avec un piège d'héritage :

- **`NkMouseScrollEvent`** (`NK_MOUSE_SCROLL`) — scroll **2D unifié**, dX et dY ensemble (pensé
  trackpad). Il dérive de `NkMouseEvent` (**pas** de `NkMouseWheelEvent`) et offre les quatre
  helpers de sens `ScrollsUp/Down/Right/Left`.
- **`NkMouseWheelVerticalEvent`** (`NK_MOUSE_WHEEL_VERTICAL`) — un seul axe vertical (molette
  classique), `ScrollsUp/Down`. Dérive de `NkMouseWheelEvent`.
- **`NkMouseWheelHorizontalEvent`** (`NK_MOUSE_WHEEL_HORIZONTAL`) — un seul axe horizontal,
  `ScrollsLeft/Right`. Dérive de `NkMouseWheelEvent`.

Usages : **UI** — défiler une liste/un document (`ScrollsUp/Down`), défilement horizontal
d'une timeline ; **rendu/caméra** — molette = zoom (dolly) ; **2D/éditeur** — `IsHighPrecision`
pour un pan/zoom fluide au trackpad.

### `NkMouseButtons` — le masque de boutons (piège de bits)

`NkMouseButtons` est un simple masque (`uint32 mask`) avec `Set`/`Clear`/`IsDown`/`Any`/
`IsNone`, tous inline `O(1)`. **Piège important** : le bit d'un bouton est `1u << valeur_enum`,
or `NK_MB_UNKNOWN = 0` occupe le bit 0 — donc `NK_MB_LEFT` est le bit **1**, et seuls 9 bits
sont réellement utiles. On l'utilise surtout pour savoir quels boutons sont tenus pendant un
`NkMouseMoveEvent` (drag à plusieurs boutons, navigation 3D bouton du milieu).

### Le tactile : `NkTouchPoint` et les events bruts

Un `NkTouchPoint` décrit un doigt : `id` (stable d'un Begin à son End — c'est lui qui suit un
doigt dans le temps), `phase`, positions `clientX/Y`, `screenX/Y` et `normalX/Y` (dans [0,1]),
`deltaX/Y`, `pressure` (1.0 par défaut), `radiusX/Y` et `angle` (empreinte du doigt). Deux
helpers : `HasMoved()` (delta non nul), `IsActive()` (phase BEGAN/MOVED/STATIONARY).

La base abstraite `NkTouchEvent` (catégorie **`TOUCH` seule**, pas `INPUT`) expose
`GetNumTouches()`, `GetTouch(i)`, et le centroïde déjà calculé `GetCentroidX/Y()` (moyenne des
positions client). Le constructeur copie les points dans un **buffer interne fixe** de 32
(`NkTouchPoint mTouches[NK_MAX_TOUCH_POINTS]`), **clampe silencieusement** `count` à 32, et
recalcule le centroïde — il n'y a donc jamais de pointeur pendant, mais un event tactile pèse
jusqu'à 32 × `sizeof(NkTouchPoint)`. **Danger** : `GetTouch(i)` ne vérifie **pas** les bornes
(comportement indéfini si `i >= GetNumTouches()`).

Les quatre concrets suivent le cycle de vie : `NkTouchBeginEvent` (`NK_TOUCH_BEGIN`),
`NkTouchMoveEvent` (`NK_TOUCH_MOVE`), `NkTouchEndEvent` (`NK_TOUCH_END`), `NkTouchCancelEvent`
(`NK_TOUCH_CANCEL`, déclenché par le système — appel entrant, geste système).

- **UI / 2D mobile** — suivre chaque doigt par son `id` pour le drag multi-points ; le
  centroïde sert de pivot à un zoom à deux doigts.
- **Gameplay mobile** — un joystick virtuel lit le `NkTouchPoint` de phase MOVED ; `pressure`
  module l'intensité d'une action sur les écrans qui le supportent.
- **Annulation** — toujours traiter `NkTouchCancelEvent` comme un End « propre » pour ne pas
  laisser un doigt « collé » (bouton resté enfoncé).

### Les gestes : un niveau au-dessus du toucher

Les gestes reconnus dérivent **directement de `NkEvent`** (pas de `NkTouchEvent`) : ils
portent donc la catégorie par défaut de `NkEvent`, **pas** `NK_CAT_TOUCH`. Chacun expose des
accesseurs parlants :

- **`NkGesturePinchEvent`** (`NK_GESTURE_PINCH`) — `GetScale` (facteur cumulé), `GetScaleDelta`
  (variation depuis la dernière frame), centre `GetCenterX/Y`, et `IsZoomIn()` (delta>0) /
  `IsZoomOut()` (delta<0). Le **zoom** d'une carte, d'une photo, d'une scène 2D/3D ; le centre
  donne le point fixe du zoom.
- **`NkGestureRotateEvent`** (`NK_GESTURE_ROTATE`) — `GetAngle`/`GetAngleDelta` (degrés),
  `IsClockwise()` (delta>0). Pivoter une image, une pièce, une vue.
- **`NkGesturePanEvent`** (`NK_GESTURE_PAN`) — `GetDeltaX/Y`, vitesse `GetVelocityX/Y`,
  `GetNumFingers`. Le **défilement à plusieurs doigts** ; la vitesse alimente une inertie
  (fling) après le relâchement.
- **`NkGestureSwipeEvent`** (`NK_GESTURE_SWIPE`) — `GetDirection` (un `NkSwipeDirection`),
  `GetSpeed`, `GetNumFingers`, et `IsLeft/IsRight/IsUp/IsDown()`. La **navigation** par balayage
  (page suivante/précédente, fermer une carte).
- **`NkGestureTapEvent`** (`NK_GESTURE_TAP`) — `GetX/Y`, `GetTapCount`, `GetNumFingers`,
  `IsDoubleTap()` (tapCount≥2). La sélection/activation, le double-tap pour zoomer.
- **`NkGestureLongPressEvent`** (`NK_GESTURE_LONG_PRESS`) — `GetX/Y`, `GetDurationMs`,
  `GetNumFingers`. L'appui prolongé qui ouvre un menu contextuel ou entre en mode édition.

### `NkKeycodeMap` — la table de portage

Utilitaire **100 % statique** (aucun état), il centralise la traduction d'un code natif vers
une `NkKey`. C'est le code qu'un nouveau backend de fenêtre branche pour livrer des events
clavier corrects.

- **`ScancodeToNkKey(sc)`** — délègue à `NkScancodeToKey`. **`NkKeyToScancode(key)`** est en
  revanche un **stub** qui renvoie **toujours `NK_SC_UNKNOWN`** : ne pas s'en servir pour
  fabriquer des events synthétiques.
- **`NkKeyFromWin32VK(vk, extended=false)`** — mapping exhaustif des `VK_*`. Le flag `extended`
  est essentiel pour lever les ambiguïtés Windows : Enter vs Numpad Enter (0x0D), LCtrl vs RCtrl
  (0x11), LAlt vs RAlt (0x12), Insert vs Numpad0 (0x2D), Delete vs Numpad-point (0x2E).
  `NkKeyFromWin32Scancode(sc, extended)` passe, lui, par le scancode.
- **`NkKeyFromX11KeySym(keysym)`** — mappe les plages ASCII (a–z, A–Z) par arithmétique, plus
  un switch pour chiffres, contrôle, KeySym X11 (0xFFxx), numpad, modificateurs et média XF86.
  `NkKeyFromX11Keycode(keycode)` passe par le scancode (evdev = keycode − 8).
- **`NkKeyFromMacKeyCode(keyCode)`** — mapping Carbon complet (0x00–0x7E).
- **`NkKeyFromDomCode(domCode)`** — recherche **linéaire `O(n)`** dans une table (~150 entrées)
  associant la chaîne `KeyboardEvent.code` (« KeyA ») à une `NkKey` ; `nullptr` → `NK_UNKNOWN`.
- **`NkKeyFromAndroid(aKeycode)`** — mapping exhaustif des `AKEYCODE_*`.
- **`Normalize(key)`** — fusionne les variantes **droite vers gauche** (RShift→LShift,
  RCtrl→LCtrl, RAlt→LAlt, RSuper→LSuper), le reste inchangé. **`SameKey(a, b)`** compare après
  normalisation : pratique pour « Ctrl » sans distinguer le côté dans un test de raccourci.

Tous ces points de conversion vivent au **carrefour OS ↔ moteur** (backends NKWindow,
portage), pas dans le code gameplay, qui ne voit que des `NkKey` déjà normalisées.

---

### Exemple récapitulatif

```cpp
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKEvent/NkKeycodeMap.h"
using namespace nkentseu;

void OnEvent(NkEvent& event) {
    // Clavier : commande de jeu par POSITION (NkKey), raccourci par modificateur.
    if (auto* k = event.As<NkKeyPressEvent>()) {
        if (k->GetKey() == NkKey::NK_W) MoveForward();
        if (k->HasCtrl() && k->GetKey() == NkKey::NK_S) { Save(); event.MarkHandled(); }
    }
    // Texte : on remplit un champ depuis le codepoint Unicode, pas depuis NkKey.
    if (auto* t = event.As<NkTextInputEvent>()) {
        if (t->IsPrintable()) AppendUtf8(t->GetUtf8());
    }
    // Souris : visée brute (FPS) + molette = zoom.
    if (auto* raw = event.As<NkMouseRawEvent>())   AimDelta(raw->GetDeltaX(), raw->GetDeltaY());
    if (auto* w   = event.As<NkMouseWheelVerticalEvent>())
        ZoomCamera(w->ScrollsUp() ? +1 : -1);
    // Tactile : pinch pour zoomer une carte autour de son centre.
    if (auto* p = event.As<NkGesturePinchEvent>())
        ZoomMap(p->GetScaleDelta(), p->GetCenterX(), p->GetCenterY());
}

// Côté backend de fenêtre : traduire un code natif Windows en NkKey.
NkKey key = NkKeycodeMap::NkKeyFromWin32VK(wParam, /*extended=*/(lParam >> 24) & 1);
```

---

[← Index NKEvent](README.md) · [Récap NKEvent](../NKEvent.md) · [Couche Runtime](../README.md)
