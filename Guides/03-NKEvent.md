# 3. NKEvent — clavier, souris, tactile, manette, fenêtre

> Module : `NKEvent` (couche **Runtime**).
> Pré-requis : [NKWindow](02-NKWindow.md) (la fenêtre est la source des événements) et,
> en filigrane, la règle mémoire du [guide NKMemory](01-NKMemory.md).
> Retour au sommaire : [README](README.md).

Une fenêtre qui ne réagit à rien n'est pas une application. **NKEvent** est la couche qui
transforme les actions de l'utilisateur — frappe au clavier, clic, glissement du doigt,
bouton de manette, redimensionnement ou fermeture de la fenêtre — en objets C++ typés que
ton code peut lire. C'est l'équivalent de `sf::Event` chez SFML, en plus riche (multi-touch,
manette, multi-fenêtre, DPI).

Ce guide part de zéro et ajoute une brique à la fois. À la fin, tu sauras lire n'importe
quelle entrée, dans l'un des deux styles offerts par le moteur.

---

## 3.1 Introduction : deux modèles, un seul système

Tous les événements transitent par **un système global** que l'on récupère avec la fonction
`NkEvents()` (elle renvoie une référence vers le `NkEventSystem` détenu par le runtime — pas
de singleton à construire toi-même). Ce système t'offre **deux façons** de consommer les
événements, et tu peux mélanger les deux :

1. **Le polling (`PollEvent`)** — tu vides toi-même la file des événements à chaque frame,
   dans ta boucle principale. C'est le modèle « SFML » : explicite, séquentiel, idéal pour
   un jeu.
2. **Les callbacks (`AddEventCallback<T>`)** — tu enregistres une fonction qui sera appelée
   automatiquement quand un événement du type `T` survient. C'est le modèle « observateur »,
   pratique pour brancher des réactions ponctuelles (fermeture, perte de focus, mise en
   arrière-plan) sans encombrer la boucle.

> **Le point commun** : que tu poll ou que tu reçoives un callback, tu manipules toujours un
> `NkEvent*` (pointeur vers la classe de base polymorphe) et tu l'identifies avec les mêmes
> outils : `Is<T>()` et `As<T>()` (section 3.3).

Tous les types vivent dans le namespace `nkentseu`. Dans la suite on suppose
`using namespace nkentseu;` pour alléger.

---

## 3.2 La boucle de polling (le squelette)

Le modèle direct, celui des jeux du dépôt (Pong, Mú). À chaque tour de boucle, on **vide
entièrement** la file avant de mettre à jour la logique et de dessiner :

```cpp
#include "NKEvent/NkEventSystem.h"   // NkEvents(), PollEvent, AddEventCallback
#include "NKEvent/NkKeyboardEvent.h" // NkKeyPressEvent, NkKey
#include "NKEvent/NkMouseEvent.h"    // NkMouseMoveEvent, NkMouseButtonPressEvent...
#include "NKEvent/NkWindowEvent.h"   // NkWindowCloseEvent, NkWindowResizeEvent...

using namespace nkentseu;

while (window.IsOpen()) {
    // 1) Vider TOUTE la file d'événements de la frame.
    while (NkEvent* ev = NkEvents().PollEvent()) {
        // ev pointe vers l'événement courant. On l'identifie (section 3.3).
        if (ev->Is<NkWindowCloseEvent>()) {
            window.Close();          // l'utilisateur veut fermer
        }
        else if (auto* kp = ev->As<NkKeyPressEvent>()) {
            if (kp->GetKey() == NkKey::NK_ESCAPE) window.Close();
        }
    }

    // 2) Mettre à jour la logique (avec le delta time)
    // 3) Dessiner (voir guide NKCanvas)
}
```

`PollEvent()` renvoie un `NkEvent*` valide tant qu'il reste des événements, puis `nullptr`
quand la file est vide — ce qui termine le `while`. C'est exactement le rôle de
`window.pollEvent(event)` en SFML.

> **Important — durée de vie du pointeur** : le `NkEvent*` renvoyé par `PollEvent()` n'est
> valide **que jusqu'au prochain appel** de `PollEvent()`. Ne le stocke jamais pour
> l'utiliser à la frame suivante. Si tu as besoin d'une copie qui te survit (file de travail
> asynchrone, traitement différé), utilise `PollEventCopy()` qui te rend un `NkEventPtr`
> (un `NkUniquePtr` dont tu contrôles la durée de vie). Voir la section 3.10 (Pièges).

Dans les vrais jeux, on délègue souvent le traitement à une fonction selon la scène
courante (extrait réel de Mú) :

```cpp
while (NkEvent* ev = NkEvents().PollEvent()) {
    if (mCurrentScene == AppScene::GameScene && mCurrentGame) HandleGameEvent(ev);
    else                                                       HandleMainMenuEvent(ev);
}
```

---

## 3.3 Identifier un événement : `Is<T>()` et `As<T>()`

`PollEvent()` te donne un `NkEvent*` générique. Pour savoir **de quoi il s'agit** et accéder
à ses données spécifiques, NKEvent fournit un RTTI léger (plus rapide et plus sûr qu'un
`dynamic_cast`) :

- `ev->Is<T>()` → `bool` : vrai si l'événement est exactement du type `T`.
- `ev->As<T>()` → `T*` : renvoie un pointeur typé si c'est bien un `T`, sinon `nullptr`.

Le pattern idiomatique combine les deux : on caste, et on agit seulement si le cast réussit.

```cpp
void HandleEvent(NkEvent* ev) {
    // Forme « test pur » : on veut juste savoir si c'est ce type.
    if (ev->Is<NkWindowCloseEvent>()) {
        RequestQuit();
        return;
    }

    // Forme « cast + accès aux membres » : la plus courante.
    if (auto* kp = ev->As<NkKeyPressEvent>()) {        // kp == nullptr si ce n'est pas une touche
        DoSomethingWith(kp->GetKey());
        return;
    }

    if (auto* move = ev->As<NkMouseMoveEvent>()) {
        UpdateCursor(move->GetX(), move->GetY());
    }
}
```

Quelques outils complémentaires sur tout `NkEvent` :

| Méthode | Rôle |
|---------|------|
| `GetType()` | renvoie l'`NkEventType::Value` (ex. `NK_KEY_PRESSED`) |
| `GetWindowId()` | identifiant de la fenêtre source (utile en multi-fenêtre) |
| `GetTimestamp()` | horodatage en millisecondes |
| `HasCategory(cat)` | l'événement appartient-il à une catégorie (`NK_CAT_KEYBOARD`, `NK_CAT_MOUSE`…) ? |
| `MarkHandled()` / `IsHandled()` | marquer l'événement comme consommé pour stopper sa propagation |
| `ToString()` | une chaîne lisible (débogage / logs) |

> **Filtrage rapide par catégorie** : avant de tester plein de types, tu peux d'abord trier
> par grande famille. Ex. `if (ev->HasCategory(NkEventCategory::NK_CAT_INPUT)) { ... }`.

---

## 3.4 Le clavier

### Les trois (quatre) événements clavier

| Classe | Type | Quand |
|--------|------|-------|
| `NkKeyPressEvent` | `NK_KEY_PRESSED` | une touche est enfoncée (une seule fois) |
| `NkKeyRepeatEvent` | `NK_KEY_REPEATED` | auto-repeat de l'OS quand la touche reste enfoncée |
| `NkKeyReleaseEvent` | `NK_KEY_RELEASED` | la touche est relâchée |
| `NkTextInputEvent` | `NK_TEXT_INPUT` | un **caractère** Unicode a été produit (saisie de texte) |

Les trois premiers dérivent de `NkKeyboardEvent` et partagent ses accesseurs. Le quatrième
est à part : il sert à la **saisie de texte** (voir plus bas).

### `NkKey` : la position, pas le caractère

`GetKey()` renvoie un `NkKey` : c'est la **position physique** de la touche sur un clavier de
référence US-QWERTY, indépendante du layout (AZERTY, QWERTY…). C'est ce qu'il faut pour les
raccourcis et les contrôles de jeu (WASD, flèches, F1–F12…).

```cpp
if (auto* kp = ev->As<NkKeyPressEvent>()) {
    switch (kp->GetKey()) {
        case NkKey::NK_ESCAPE: window.Close();       break;
        case NkKey::NK_SPACE:  player.Jump();        break;
        case NkKey::NK_W:      player.MoveForward();  break;  // « W » = position, pas le caractère
        case NkKey::NK_LEFT:   camera.PanLeft();      break;
        default: break;
    }
}
```

> **Attention** : `NkKey::NK_W` désigne la touche *à la position du W en QWERTY*. Sur un
> clavier AZERTY, cette même touche produit le caractère « Z ». Pour les contrôles de jeu
> c'est exactement ce qu'on veut (la position physique reste cohérente). Pour saisir du
> **texte**, n'utilise jamais `NkKey` → utilise `NkTextInputEvent`.

### Les modificateurs (Ctrl, Shift, Alt, Super, AltGr)

Tout événement clavier transporte l'état des modificateurs au moment de la frappe. Deux
accès possibles : les raccourcis pratiques (`HasCtrl()`, `HasShift()`…) ou la structure
complète via `GetModifiers()`.

```cpp
if (auto* kp = ev->As<NkKeyPressEvent>()) {
    // Ctrl+S (ou Cmd+S sur macOS) : sauvegarder
    if (kp->GetKey() == NkKey::NK_S && (kp->HasCtrl() || kp->HasSuper())) {
        Document::Save();
        ev->MarkHandled();   // consommé : on stoppe la propagation
    }
    // Ctrl+Shift+Z : refaire
    if (kp->GetKey() == NkKey::NK_Z && kp->HasCtrl() && kp->HasShift()) {
        Document::Redo();
    }
}
```

Raccourcis disponibles : `HasCtrl()`, `HasAlt()`, `HasShift()`, `HasSuper()`, `HasAltGr()`.
La structure `NkModifierState` (renvoyée par `GetModifiers()`) expose en plus
`numLock`, `capLock`, `scrLock` et la méthode `ToString()`.

### Saisir du texte : `NkTextInputEvent`

Pour un champ de saisie, un chat, une barre de recherche, c'est l'événement à écouter : il
porte le **caractère réellement produit** après application du layout, de Shift, d'AltGr et
des IME (langues asiatiques). Le caractère est disponible en UTF-8 (`GetUtf8()`) et en
codepoint brut (`GetCodepoint()`).

```cpp
if (auto* txt = ev->As<NkTextInputEvent>()) {
    if (txt->IsPrintable()) {              // ignore les caractères de contrôle
        mSearchBuffer += txt->GetUtf8();   // ajoute le caractère encodé UTF-8
    }
}
```

> **Règle d'or clavier** : `NkKeyPressEvent` pour les **raccourcis et le gameplay** (position),
> `NkTextInputEvent` pour la **saisie de texte** (caractère Unicode). Le second peut être
> émis sans le premier (compositions IME).

---

## 3.5 La souris

### Mouvement — `NkMouseMoveEvent`

```cpp
if (auto* move = ev->As<NkMouseMoveEvent>()) {
    int32 x = move->GetX();          // position dans la zone client de la fenêtre [pixels]
    int32 y = move->GetY();
    int32 dx = move->GetDeltaX();    // déplacement depuis le dernier événement
    int32 dy = move->GetDeltaY();

    // Drag avec le bouton droit (ex. rotation de caméra)
    if (move->IsButtonDown(NkMouseButton::NK_MB_RIGHT)) {
        camera.Rotate(dx, dy);
    }
}
```

`GetScreenX()/GetScreenY()` donnent la position absolue à l'écran. `GetButtons()` renvoie le
masque des boutons enfoncés (avec `IsButtonDown(b)`), `GetModifiers()` les modificateurs
clavier actifs pendant le mouvement.

> Pour un contrôle FPS sans accélération OS, il existe aussi `NkMouseRawEvent`
> (`GetDeltaX/Y/Z` en comptes de capteur).

### Boutons — press, release, double-clic

| Classe | Type |
|--------|------|
| `NkMouseButtonPressEvent` | `NK_MOUSE_BUTTON_PRESSED` |
| `NkMouseButtonReleaseEvent` | `NK_MOUSE_BUTTON_RELEASED` |
| `NkMouseDoubleClickEvent` | `NK_MOUSE_DOUBLE_CLICK` |

Les boutons sont des `NkMouseButton` : `NK_MB_LEFT`, `NK_MB_RIGHT`, `NK_MB_MIDDLE`,
`NK_MB_BACK`, `NK_MB_FORWARD`… Accesseurs : `GetButton()`, raccourcis `IsLeft()`,
`IsRight()`, `IsMiddle()`, position `GetX()/GetY()`, et `GetClickCount()`.

```cpp
if (auto* press = ev->As<NkMouseButtonPressEvent>()) {
    if (press->IsLeft()) {
        Select(press->GetX(), press->GetY());
    }
}

if (auto* dbl = ev->As<NkMouseDoubleClickEvent>()) {
    if (dbl->IsLeft()) {
        OpenItemAt(dbl->GetX(), dbl->GetY());   // GetClickCount() == 2
        ev->MarkHandled();
    }
}
```

### Molette — `NkMouseScrollEvent`

Le plus simple et le plus polyvalent est le scroll unifié 2D (gère molette et trackpad) :

```cpp
if (auto* scroll = ev->As<NkMouseScrollEvent>()) {
    float64 dy = scroll->GetDeltaY();   // > 0 vers le haut, < 0 vers le bas (clics de molette)
    float64 dx = scroll->GetDeltaX();   // scroll horizontal (trackpad / molette latérale)
    Zoom(dy);

    // Raccourcis de lecture du sens :
    if (scroll->ScrollsUp())   { /* ... */ }
    if (scroll->ScrollsDown()) { /* ... */ }
}
```

Pour un défilement fluide sur trackpad, teste `scroll->IsHighPrecision()` et privilégie
alors `GetPixelDeltaX/Y` (en pixels). Il existe aussi des variantes à un seul axe
(`NkMouseWheelVerticalEvent`, `NkMouseWheelHorizontalEvent`) si tu préfères les séparer.

---

## 3.6 Le tactile (multi-touch)

Les écrans tactiles produisent des `NkTouchEvent` (catégorie `NK_CAT_TOUCH`). Quatre types :
`NkTouchBeginEvent` (doigt posé), `NkTouchMoveEvent` (doigt glisse), `NkTouchEndEvent`
(doigt levé), `NkTouchCancelEvent` (suivi interrompu par le système).

Un même événement peut porter **plusieurs contacts** (multi-touch). On itère dessus :

```cpp
if (auto* tb = ev->As<NkTouchBeginEvent>()) {
    for (uint32 i = 0; i < tb->GetNumTouches(); ++i) {
        const NkTouchPoint& t = tb->GetTouch(i);
        float x = t.clientX;     // position dans la zone client
        float y = t.clientY;
        // t.id : identifiant stable du doigt (le suivre entre Begin/Move/End)
        // t.normalX / t.normalY : coordonnées normalisées [0,1] (UI responsive)
        // t.pressure : pression [0,1] si supportée
        OnFingerDown(t.id, x, y);
    }
}
```

`NkTouchEvent` calcule aussi le **centroïde** des doigts (`GetCentroidX/Y`), pratique comme
pivot pour les gestes (pinch, rotate). En pratique, pour une UI au doigt, on traite souvent
juste le premier contact comme un « pointeur ». Extrait réel de Mú, qui mappe le tactile sur
le même pointeur que la souris :

```cpp
events.AddEventCallback<NkTouchBeginEvent>([this](NkTouchBeginEvent* e) {
    if (e->GetNumTouches() == 0) return;
    const auto& t = e->GetTouch(0);
    mInput.pointerPos       = { (float32)t.clientX, (float32)t.clientY };
    mInput.pressedThisFrame = true;
    mInput.pressed          = true;
});
```

---

## 3.7 La manette / gamepad

Les manettes émettent des `NkGamepadEvent` (catégorie `NK_CAT_GAMEPAD`). Chaque événement
porte l'index du joueur via `GetGamepadIndex()` (0 = joueur 1, 1 = joueur 2…).

| Classe | Type | Données clés |
|--------|------|--------------|
| `NkGamepadConnectEvent` | `NK_GAMEPAD_CONNECT` | branchement (avec `NkGamepadInfo`) |
| `NkGamepadDisconnectEvent` | `NK_GAMEPAD_DISCONNECT` | débranchement |
| `NkGamepadButtonPressEvent` | `NK_GAMEPAD_BUTTON_PRESSED` | `GetButton()`, `GetAnalogValue()` |
| `NkGamepadButtonReleaseEvent` | `NK_GAMEPAD_BUTTON_RELEASED` | `GetButton()` |
| `NkGamepadAxisEvent` | `NK_GAMEPAD_AXIS_MOTION` | `GetAxis()`, `GetValue()` |

Les boutons (`NkGamepadButton`) suivent un layout universel compatible Xbox :
`NK_GP_SOUTH` (A / Croix), `NK_GP_EAST` (B / Rond), `NK_GP_WEST` (X / Carré),
`NK_GP_NORTH` (Y / Triangle), `NK_GP_LB`/`NK_GP_RB`, `NK_GP_START`, `NK_GP_DPAD_UP`…
Les axes (`NkGamepadAxis`) : sticks `NK_GP_AXIS_LX/LY/RX/RY` (dans `[-1, +1]`), gâchettes
`NK_GP_AXIS_LT/RT` (dans `[0, +1]`).

```cpp
if (auto* gp = ev->As<NkGamepadButtonPressEvent>()) {
    if (gp->GetButton() == NkGamepadButton::NK_GP_SOUTH) {  // A / Croix
        player.Jump();
    }
}

if (auto* ax = ev->As<NkGamepadAxisEvent>()) {
    if (ax->GetAxis() == NkGamepadAxis::NK_GP_AXIS_LX) {
        player.SetMoveX(ax->GetValue());   // déjà normalisé, deadzone appliquée en amont
    }
}

if (auto* conn = ev->As<NkGamepadConnectEvent>()) {
    AssignPlayerSlot(conn->GetGamepadIndex());
}
```

> La zone morte (deadzone) et la normalisation sont appliquées par la couche d'entrée avant
> émission : les valeurs reçues sont prêtes à l'emploi.

---

## 3.8 Les événements de fenêtre

Catégorie `NK_CAT_WINDOW`. Ce sont eux qui pilotent le cycle de vie et l'état de l'affichage.
Les plus utilisés :

| Classe | Type | À quoi ça sert |
|--------|------|----------------|
| `NkWindowCloseEvent` | `NK_WINDOW_CLOSE` | l'utilisateur demande la fermeture (croix, Alt+F4) |
| `NkWindowResizeEvent` | `NK_WINDOW_RESIZE` | la fenêtre a changé de taille |
| `NkWindowFocusGainedEvent` | `NK_WINDOW_FOCUS_GAINED` | la fenêtre redevient active |
| `NkWindowFocusLostEvent` | `NK_WINDOW_FOCUS_LOST` | la fenêtre perd le focus |
| `NkWindowShownEvent` | `NK_WINDOW_SHOWN` | la fenêtre (re)devient visible |
| `NkWindowHiddenEvent` | `NK_WINDOW_HIDDEN` | la fenêtre est masquée (arrière-plan) |
| `NkWindowDpiEvent` | `NK_WINDOW_DPI_CHANGE` | changement d'échelle DPI (HiDPI, multi-écran) |

### Fermeture

`NkWindowCloseEvent` est une **requête**, pas une fermeture forcée. Tu peux l'annuler en
appelant `MarkHandled()` (ex. pour afficher « Enregistrer avant de quitter ? »). Si
`IsForced()` est vrai (shutdown OS), elle ne peut pas être annulée.

```cpp
if (ev->Is<NkWindowCloseEvent>()) {
    window.Close();   // cas simple : on accepte de quitter
}
```

### Redimensionnement

```cpp
if (auto* rz = ev->As<NkWindowResizeEvent>()) {
    uint32 w = rz->GetWidth();
    uint32 h = rz->GetHeight();
    renderer.OnResize(w, h);          // adapter viewport + projection
    if (rz->GotLarger()) { /* ... */ } // GotLarger() / GotSmaller() : sens du changement
}
```

> **Piège classique** : à la création de la fenêtre, l'OS (Windows) envoie déjà un événement
> de resize. Un handler naïf déclenche alors un `OnResize` inutile avant la première frame
> (crash possible sur certains backends). La parade est de **suivre la taille courante** et
> de ne réagir que si elle a réellement changé — c'est exactement ce que fait Mú dans sa
> boucle (`mLastWindowWidth`/`mLastWindowHeight`).

### Focus et arrière-plan

Très utile pour **mettre en pause** le jeu et l'audio quand la fenêtre n'est plus au premier
plan, et reprendre au retour (voir l'exemple complet en 3.9).

---

## 3.9 Le modèle callbacks (`AddEventCallback`)

Au lieu de tout traiter dans la boucle, tu peux **enregistrer des réactions** par type
d'événement. La signature : `AddEventCallback<T>(callback, windowId = toutes)`, où `callback`
est un appelable prenant un `T*`.

```cpp
auto& events = NkEvents();

// Fermeture : on arrête la boucle.
events.AddEventCallback<NkWindowCloseEvent>([this](NkWindowCloseEvent*) {
    mRunning = false;
});

// Souris : on met à jour la position du pointeur.
events.AddEventCallback<NkMouseMoveEvent>([this](NkMouseMoveEvent* e) {
    mPointer = { (float32)e->GetX(), (float32)e->GetY() };
});

// Clic gauche.
events.AddEventCallback<NkMouseButtonPressEvent>([this](NkMouseButtonPressEvent* e) {
    if (e->GetButton() == NkMouseButton::NK_MB_LEFT) mClicked = true;
});
```

Extrait réel de Mú — pause propre quand l'app perd le focus ou passe en arrière-plan
(Android), reprise au retour :

```cpp
events.AddEventCallback<NkWindowFocusLostEvent>([this](NkWindowFocusLostEvent*) {
    mPaused = true;  mAudio.Pause();
});
events.AddEventCallback<NkWindowFocusGainedEvent>([this](NkWindowFocusGainedEvent*) {
    mPaused = false; mAudio.Resume();
});

#if defined(NKENTSEU_PLATFORM_ANDROID)
events.AddEventCallback<NkWindowHiddenEvent>([this](NkWindowHiddenEvent*) {
    mActive = false; mAudio.Pause();             // la surface native est détruite
});
events.AddEventCallback<NkWindowShownEvent>([this](NkWindowShownEvent*) {
    if (mRenderTarget) mRenderTarget->RecreateSurface();  // recrée la surface au resume
    mActive = true;  mAudio.Resume();
});
#endif
```

> **Filtrer par fenêtre** : en multi-fenêtre, passe un `windowId` en second argument pour ne
> recevoir que les événements de cette fenêtre :
> `events.AddEventCallback<NkMouseMoveEvent>(cb, monWindowId);`

### Se désinscrire

Deux mécanismes :

- **Manuel global** : `events.ClearEventCallbacks<T>()` retire tous les callbacks d'un type ;
  `events.ClearAllCallbacks()` vide tout.
- **RAII (recommandé)** : `AddEventCallbackGuard<T>(cb)` renvoie un `NkCallbackGuard`. Tant
  que le guard vit, le callback est actif ; à sa destruction, il est **automatiquement
  désinscrit**. Pratique pour lier la durée de vie d'un callback à celle d'un objet (un menu,
  un panneau…).

```cpp
class PauseMenu {
    NkCallbackGuard mGuard;
public:
    PauseMenu() {
        mGuard = NkEvents().AddEventCallbackGuard<NkKeyPressEvent>(
            [this](NkKeyPressEvent* e) {
                if (e->GetKey() == NkKey::NK_ESCAPE) Toggle();
            });
    }
    // À la destruction de PauseMenu, mGuard se détruit → désinscription auto.
};
```

> Polling et callbacks coexistent : beaucoup d'apps utilisent les callbacks pour le cycle de
> vie (close/focus/background) et le polling pour le gameplay frame par frame.

---

## 3.10 Pièges à connaître

1. **Durée de vie du pointeur `PollEvent()`** — le `NkEvent*` n'est valide que jusqu'au
   prochain `PollEvent()`. Ne le conserve pas entre frames. Pour une copie persistante,
   `PollEventCopy()` → `NkEventPtr` (un `NkUniquePtr`).
2. **`NkKey` ≠ caractère** — `NkKey` est la position physique (US-QWERTY). Pour saisir du
   texte, écoute `NkTextInputEvent` (UTF-8 / codepoint), jamais `NkKey`.
3. **Resize au démarrage** — l'OS émet un resize à la création de la fenêtre. Suis la taille
   courante et ne réagis qu'en cas de vrai changement (sinon crash possible sur certains
   backends GPU).
4. **`MarkHandled()`** — pour les requêtes annulables (fermeture) et pour stopper la
   propagation d'un événement consommé par un handler.
5. **Thread** — le système d'événements est conçu pour être **pompé sur le thread principal**
   (celui qui a créé la fenêtre). `PollEvent()`/`PollEvents()` ne doivent être appelés que
   depuis ce thread. Pour un traitement multi-thread, copie l'événement (`PollEventCopy()`)
   et passe la copie à ta file.
6. **Arrière-plan mobile** — sur Android/HarmonyOS, la surface native est détruite quand
   l'app passe en arrière-plan (`NkWindowHiddenEvent`) : cesse de rendre, puis recrée la
   surface sur `NkWindowShownEvent`. Voir l'exemple Mú ci-dessus.

---

## 3.11 Récapitulatif

- Tout passe par `NkEvents()` (le `NkEventSystem` global du runtime).
- Deux modèles, combinables : **polling** (`while (NkEvent* ev = NkEvents().PollEvent())`)
  et **callbacks** (`AddEventCallback<T>` / `AddEventCallbackGuard<T>`).
- On identifie un événement avec `ev->Is<T>()` et `ev->As<T>()`.
- Familles : **clavier** (`NkKeyPressEvent` + `NkKey` + modificateurs ; `NkTextInputEvent`
  pour le texte), **souris** (`NkMouseMoveEvent`, `NkMouseButton*Event`, `NkMouseScrollEvent`),
  **tactile** (`NkTouch*Event`, multi-contact), **manette** (`NkGamepad*Event`),
  **fenêtre** (`NkWindowClose/Resize/Focus*/Shown/Hidden/Dpi Event`).
- Pièges majeurs : durée de vie du pointeur poll, `NkKey` n'est pas un caractère, resize au
  démarrage, polling sur le thread principal uniquement.

### Dépendances Jenga

Pour utiliser NKEvent, ajoute-le à `nkentseudependson` (Jenga propage automatiquement les
includes des modules dont NKEvent dépend) :

```python
with project("MonJeu"):
    kind("app")
    files(["src/**.cpp"])
    nkentseudependson(
        ["NKWindow", "NKEvent", "NKCanvas",          # NKWindow fournit la fenêtre source
         "NKMemory", "NKCore", "NKMath", "NKContainers", "NKLogger"],
        extra_includes=["src"],
    )
```

Suite logique : tu sais lire les entrées et gérer la fenêtre — apprends maintenant à
**afficher** quelque chose avec [NKImage](04-NKImage.md) puis [NKCanvas](05-NKCanvas.md).
Retour au [sommaire des guides](README.md).
