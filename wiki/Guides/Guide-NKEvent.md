# Les événements avec NKEvent

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
**Écrit le 26/09/2026. Public : quelqu'un qui sait déjà ouvrir une fenêtre et veut la rendre vivante.**

> ⚠️ **Tout ce qui suit a été relevé dans le code.** Chaque nom, chaque champ vient d'un
> fichier que vous pouvez ouvrir. Si le code dit autre chose, **c'est le code qui a raison**.

👉 **À lire avant** : `Guide-NKWindow.md`, à côté de ce fichier.

---

## 1. Les DEUX façons de lire l'entrée — et il faut choisir la bonne

C'est le point le plus important du module, et celui qui coûte le plus cher quand on se
trompe. NKEvent propose deux mécanismes **qui ne servent pas à la même chose**.

| | **L'interrogation** (`NkInput`) | **Les événements** (`PollEvent`) |
|---|---|---|
| la question posée | « **est-elle** enfoncée ? » | « **vient-elle d'être** enfoncée ? » |
| ce que ça donne | l'état à l'instant de l'appel | chaque transition, une par une |
| bon pour | avancer, viser, maintenir | tirer, taper du texte, un bouton |
| si vous manquez un tour | rien de grave | **vous perdez l'événement** |

**La règle qui évite 90 % des erreurs :**

> **Ce qui dure** (le personnage avance tant que `Z` est tenue) → **`NkInput`**.
> **Ce qui arrive** (le personnage saute quand `Espace` est pressée) → **événement**.

⚠️ **Le piège classique** : gérer le saut avec `IsKeyDown(NK_SPACE)` dans la boucle. La
touche est enfoncée pendant **plusieurs images**, donc le personnage saute plusieurs fois.
`IsKeyDown` ne sait pas dire « c'est nouveau » — ce n'est pas son travail.

---

## 2. L'interrogation — `NkInput`

Fichier : `Kernel/Runtime/NKEvent/src/NKEvent/NkEventDispatcher.h`.
`NkInput` est une variable globale déclarée ligne 376 : **rien à construire, rien à passer.**

```cpp
#include "NKEvent/NkEventDispatcher.h"

if (NkInput.IsKeyDown(NkKey::NK_SPACE))
    Joueur::Avancer();
```

### Clavier
```cpp
bool       IsKeyDown(NkKey key) const noexcept;
bool       IsKeyRepeated(NkKey key) const noexcept;
bool       IsCtrlDown()  const noexcept;
bool       IsAltDown()   const noexcept;
bool       IsShiftDown() const noexcept;
bool       IsSuperDown() const noexcept;   // touche Windows / Commande
NkKey      LastKey()      const noexcept;
NkScancode LastScancode() const noexcept;
```

⚠️ **`NkKey` et `NkScancode` ne sont pas la même chose**, et le choix compte :
- **`NkKey`** = le **symbole** imprimé sur la touche. Dépend de la disposition du clavier.
- **`NkScancode`** = la **position physique** de la touche. Ne dépend de rien.

👉 **Pour un raccourci de texte** (`Ctrl+S`), employez `NkKey` : l'utilisateur veut la
touche « S ». **Pour des commandes de jeu** (ZQSD), employez `NkScancode` : sur un clavier
QWERTY, vos ZQSD deviennent WASD **au bon endroit sous les doigts**. C'est exactement ce
que font les jeux qui « marchent quel que soit le clavier ».

### Souris — position et boutons
```cpp
int32 MouseX() const noexcept;
int32 MouseY() const noexcept;

bool IsMouseDown(NkMouseButton btn) const noexcept;
bool IsLeftDown()    const noexcept;
bool IsRightDown()   const noexcept;
bool IsMiddleDown()  const noexcept;
bool IsAnyMouseDown() const noexcept;
bool IsMouseInside()  const noexcept;
```

📌 **`MouseX()`/`MouseY()` est la réponse à une question posée le 26/09** : *« n'y a-t-il pas
un getter pour récupérer la position courante de la souris ? »* Il existe — **mais il est
ici, dans NKEvent, alors que le setter est dans NKWindow**.

**Ce n'est pas un oubli.** `NKWindow` dépend de `NKEvent` ; mettre un setter ici
inverserait le graphe de dépendances. Depuis le 26/09, **les deux en-têtes se renvoient
l'un à l'autre** pour que les deux moitiés du geste soient nommées des deux côtés.

### 🔴 Souris — les déplacements : TROIS paires, et elles ne sont pas interchangeables

C'est le passage à lire deux fois.

```cpp
// (1) LE DELTA DU DERNIER EVENEMENT — IL PERSISTE
int32 MouseDeltaX() const noexcept;
int32 MouseDeltaY() const noexcept;
```
> ⚠️ **Elle ne retombe pas à zéro quand la souris s'arrête.** Elle garde la valeur du
> dernier mouvement reçu. **Ce n'est pas un défaut, c'est son contrat** : c'est la bonne
> fonction quand on veut la dernière **direction connue** même à l'arrêt — inertie, élan,
> relance d'un geste interrompu.

```cpp
// (2) LE DELTA DE CETTE IMAGE — IL SE REMET A ZERO
int32 MouseDeltaThisFrameX() const noexcept;
int32 MouseDeltaThisFrameY() const noexcept;
void  NewFrame() noexcept;
```
> Somme des mouvements reçus **depuis le dernier `NewFrame()`**. Souris immobile → **0**.
> C'est la bonne fonction pour **faire tourner une caméra, tirer un gizmo, faire glisser un
> panneau** : tout ce qui doit **s'arrêter quand la main s'arrête**.

```cpp
// (3) LE MOUVEMENT BRUT — sans accélération ni bords d'écran
int32 MouseRawDeltaX() const noexcept;
int32 MouseRawDeltaY() const noexcept;
```
> Le mouvement tel que le matériel le rapporte, **sans l'accélération du système** et sans
> s'arrêter au bord de l'écran. C'est ce qu'il faut pour une **caméra FPS**.

**Les trois existent et aucune ne remplace les autres.** Le choix :

| ce que vous faites | la fonction |
|---|---|
| tourner une caméra FPS | `MouseRawDeltaX/Y()` |
| glisser un panneau, tirer un gizmo | `MouseDeltaThisFrameX/Y()` |
| prolonger un geste après l'arrêt (inertie) | `MouseDeltaX/Y()` |

⚠️ **`MouseDeltaThisFrameX/Y()` exige `NewFrame()`** — une fois par tour de boucle, **avant
de dépiler les événements**. Et **son oubli ne se tait pas** : sans appel, les deux
fonctions rendent `0` en permanence **et le journal le dit une fois**. *Un zéro franc et une
ligne de journal valent mieux qu'un delta silencieusement périmé.*

### Manettes
```cpp
bool    IsGamepadDown(uint32 idx, NkGamepadButton btn) const noexcept;
float32 GamepadAxis(uint32 idx, NkGamepadAxis ax) const noexcept;
bool    IsGamepadConnected(uint32 idx) const noexcept;
void    GamepadRumble(uint32 idx, float32 motorLow = 0.f, float32 motorHigh = 0.f,
                      float32 triggerLeft = 0.f, float32 triggerRight = 0.f,
                      uint32 durationMs = 100) const;
```
`idx` est le **numéro de joueur** : `0` = joueur 1.

---

## 3. Les événements — la boucle

```cpp
while (window.IsOpen()) {
    NkInput.NewFrame();                              // 1. ouvrir l'image

    while (NkEvent *ev = NkEvents().PollEvent()) {   // 2. dépiler TOUT
        // traiter ev
    }

    MettreAJour();                                   // 3. logique
    Dessiner();                                      // 4. rendu
}
```

`NkEvents()` est l'accesseur du système d'événements —
`Kernel/Runtime/NKWindow/src/NKWindow/Core/NkWESystem.h`, ligne 191.

⚠️ **La boucle intérieure est un `while`, pas un `if`.** Plusieurs événements arrivent dans
la même image — un déplacement de souris en produit beaucoup. Avec un `if`, vous en traitez
un par image, la file grandit, et **votre application prend du retard sur l'utilisateur**.
Le symptôme est une souris « en caoutchouc » qui traîne derrière le curseur.

### Les variantes de `PollEvent`
```cpp
NkEvent   *PollEvent();                 // le plus simple
bool       PollEvent(NkEvent *&event);  // la même, en forme de condition
NkEventPtr PollEventCopy();             // durée de vie contrôlée par l'appelant
void       PollEvents();                // dépile tout vers les callbacks
```

🔴 **Le pointeur rendu par `PollEvent()` ne vous appartient pas.** Il est valable **le temps
du tour de boucle**. Ne le stockez pas pour plus tard — si vous devez garder un événement,
c'est `PollEventCopy()` qui est là pour ça.

---

## 4. Reconnaître un événement — `NkEventDispatcher`

Comparer des types à la main devient vite illisible. Le répartiteur le fait pour vous.

```cpp
#include "NKEvent/NkEventDispatcher.h"

while (NkEvent *ev = NkEvents().PollEvent()) {
    NkEventDispatcher d(*ev);

    NK_DISPATCH_FREE(d, NkKeyPressEvent, [&](NkKeyPressEvent &e) {
        if (e.GetKey() == NkKey::NK_ESCAPE) running = false;
        return true;                      // true = consommé, on s'arrête là
    });

    NK_DISPATCH_FREE(d, NkWindowCloseEvent, [&](NkWindowCloseEvent &e) {
        running = false;
        return true;
    });
}
```

Dans une **classe**, la macro `NK_DISPATCH` branche directement une méthode :
```cpp
NK_DISPATCH(d, NkKeyPressEvent, OnKeyPress);   // appelle this->OnKeyPress(e)
```

⚠️ **Le `return` de votre lambda n'est pas décoratif** : `true` = « je l'ai consommé »,
`false` = « qu'il continue son chemin ». C'est ainsi qu'un panneau au premier plan empêche
un clic d'atteindre la scène derrière lui. *Un `true` posé par distraction fait disparaître
des clics ailleurs dans l'application* — c'est un défaut pénible à retrouver.

### L'autre voie : s'abonner une fois pour toutes
```cpp
NkEvents().AddEventCallback<NkKeyPressEvent>(
    [](NkKeyPressEvent &e) { /* ... */ return true; });

NkEvents().SetWindowCallback(window.GetId(), cb);   // pour UNE fenêtre
NkEvents().SetGlobalCallback(cb);                   // pour tout
```
Utile quand un morceau de code éloigné de la boucle doit réagir.
⚠️ **Un abonnement doit se défaire** — voyez `ClearEventCallbacks<T>()` et
`ClearAllCallbacks()`. Un callback qui survit à l'objet qu'il capture plante.

---

## 5. Le catalogue — ce que vous pouvez écouter

Les types vivent dans `NkEvent.h` (l'énumération `NkEventType`), et chaque famille a son
en-tête. **25 en-têtes au total.**

### Fenêtre — `NkWindowEvent.h`
`NkWindowCreateEvent` · `NkWindowCloseEvent` · `NkWindowDestroyEvent` · `NkWindowPaintEvent` ·
`NkWindowResizeEvent` · `NkWindowResizeBeginEvent` · `NkWindowResizeEndEvent` ·
`NkWindowMoveEvent` · `NkWindowMoveBeginEvent` · `NkWindowMoveEndEvent` ·
`NkWindowFocusGainedEvent` · `NkWindowFocusLostEvent` · `NkWindowMinimizeEvent` ·
`NkWindowMaximizeEvent`

📌 **`ResizeBegin`/`ResizeEnd` encadrent le redimensionnement.** Pendant que l'utilisateur
tire le coin, `NkWindowResizeEvent` arrive des dizaines de fois par seconde. Si vous
reconstruisez quelque chose de coûteux, **faites-le sur `ResizeEnd`**, pas à chaque pas.

### Clavier — `NkKeyboardEvent.h`
`NkKeyPressEvent` · `NkKeyRepeatEvent` · `NkKeyReleaseEvent` · **`NkTextInputEvent`**

🔴 **Pour saisir du texte, employez `NkTextInputEvent` — jamais `NkKeyPressEvent`.**
`KeyPress` vous donne une **touche** ; `TextInput` vous donne le **caractère** que
l'utilisateur voulait écrire, accents compris, majuscules comprises, et il fonctionne avec
les méthodes de saisie asiatiques (IME). Reconstruire du texte à partir des touches est une
impasse connue — vous n'y arriverez pas pour le français, encore moins pour le japonais.

### Souris — `NkMouseEvent.h`
`NkMouseMoveEvent` · `NkMouseRawEvent` · `NkMouseButtonPressEvent` ·
`NkMouseButtonReleaseEvent` · `NkMouseDoubleClickEvent` · `NkMouseScrollEvent` ·
`NkMouseWheelVerticalEvent` · `NkMouseWheelHorizontalEvent` · `NkMouseEnterEvent` ·
`NkMouseLeaveEvent` · `NkMouseWindowEnterEvent` · `NkMouseWindowLeaveEvent`

Champs d'un événement souris : `mX`, `mY` (client) · `mScreenX`, `mScreenY` (écran) ·
`mDeltaX`, `mDeltaY` · `mButtons`.

⚠️ **`Enter`/`Leave` et `WindowEnter`/`WindowLeave` sont deux paires différentes.** Lisez
l'en-tête avant de choisir — le nom ne suffit pas à les distinguer.

### Manette — `NkGamepadEvent.h`
`NkGamepadConnectEvent` · `NkGamepadDisconnectEvent` · `NkGamepadButtonPressEvent` ·
`NkGamepadButtonReleaseEvent` · `NkGamepadAxisEvent` · `NkGamepadRumbleEvent` ·
`NkGamepadBatteryEvent`

### Glisser-déposer — `NkDropEvent.h`
`NkDropEnterEvent` · `NkDropOverEvent` · `NkDropLeaveEvent` · `NkDropFileEvent` ·
`NkDropTextEvent` · `NkDropImageEvent`
⚠️ Il faut **`dropEnabled = true`** dans `NkWindowConfig`, sinon aucun n'arrive.

### Graphique — `NkGraphicsEvent.h`
`NkGraphicsContextReadyEvent` · `NkGraphicsContextLostEvent` · `NkGraphicsContextResizeEvent` ·
`NkGraphicsFrameBeginEvent` · `NkGraphicsFrameEndEvent` · `NkGraphicsGpuMemoryEvent` ·
`NkGraphicsVSyncEvent`

### Et aussi
`NkApplicationEvent.h` (`NK_APP_LAUNCH`, `NK_APP_TICK`, `NK_APP_UPDATE`, `NK_APP_RENDER`,
`NK_APP_CLOSE`) · `NkTouchEvent.h` · `NkPointerEvent.h` · `NkSystemEvent.h` ·
`NkTransferEvent.h` · `NkGenericHidEvent.h` · `NkCustomEvent.h`

### Les catégories — filtrer en gros
```cpp
NK_CAT_APPLICATION  NK_CAT_INPUT     NK_CAT_KEYBOARD  NK_CAT_MOUSE
NK_CAT_WINDOW       NK_CAT_GRAPHICS  NK_CAT_TOUCH     NK_CAT_GAMEPAD
NK_CAT_CUSTOM       NK_CAT_TRANSFER  NK_CAT_GENERIC_HID
NK_CAT_DROP         NK_CAT_SYSTEM    NK_CAT_ALL
```
Ce sont des **bits**, donc ils se combinent :
```cpp
NkEventCategory::Value filtre = NkEventCategory::NK_CAT_INPUT
                              | NkEventCategory::NK_CAT_KEYBOARD;
```

---

## 6. Un exemple complet qui tient debout

```cpp
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NkEventDispatcher.h"

using namespace nkentseu;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{}; d.appName = "Demo"; d.appVersion = "0.1.0"; return d;
})());

int nkmain(const NkEntryState &state) {
    (void)state;

    NkWindow window;
    NkWindowConfig cfg;
    cfg.title = "Evenements";
    cfg.width = 1280; cfg.height = 720;
    cfg.centered = true;
    if (!window.Create(cfg)) return -1;

    bool running = true;
    float32 camYaw = 0.f;

    while (running && window.IsOpen()) {
        NkInput.NewFrame();

        while (NkEvent *ev = NkEvents().PollEvent()) {
            NkEventDispatcher d(*ev);

            NK_DISPATCH_FREE(d, NkWindowCloseEvent, [&](NkWindowCloseEvent &) {
                running = false; return true;
            });

            NK_DISPATCH_FREE(d, NkKeyPressEvent, [&](NkKeyPressEvent &e) {
                if (e.GetKey() == NkKey::NK_ESCAPE) running = false;
                return true;                  // ce qui ARRIVE -> evenement
            });

            NK_DISPATCH_FREE(d, NkWindowResizeEndEvent, [&](NkWindowResizeEndEvent &) {
                Reconstruire();               // a la FIN, pas a chaque pas
                return true;
            });
        }

        // ce qui DURE -> interrogation
        if (NkInput.IsKeyDown(NkKey::NK_Z)) Avancer();
        if (NkInput.IsRightDown())
            camYaw -= NkInput.MouseRawDeltaX() * 0.002f;   // camera -> RAW

        Dessiner();
    }
    return 0;
}
```

---

## 6 bis. Les axes nommés — `NkAxisManager`

Plutôt que d'écrire `if (IsKeyDown(NK_LEFT)) x -= 5;` partout, on **nomme** un axe et on
déclare ce qui l'alimente. L'utilisateur peut alors reconfigurer ses touches sans que vous
touchiez au code du jeu.

```cpp
NkAxisManager axes;

axes.CreateAxis("Horizontal", [&](const NkString &, const NkInputCode &, float v) {
    joueur.vitesseX = v;          // UNE valeur, deja sommee
});

axes.AddCommand(NkAxisCommand("Horizontal", NkInputCode::Key(NkKey::NK_LEFT),  -100.f));
axes.AddCommand(NkAxisCommand("Horizontal", NkInputCode::Key(NkKey::NK_RIGHT), +100.f));
```

📌 **Le rappel reçoit UNE valeur, déjà sommée** sur toutes les commandes de l'axe. Vous
écrivez `valeur = v`, jamais `valeur += v`, et vous n'avez rien à remettre à zéro.

### 🔴 Une touche est binaire — d'où la rampe

Le résolveur clavier rend **1 ou 0**. Multiplié par `scale = -100`, un axe saute donc de
`0` à `-100` **en un seul pas**, sans aucune valeur intermédiaire. Pour un curseur c'est
parfait ; pour une accélération de véhicule, c'est inutilisable.

**Deux méthodes existent, et aucune ne remplace l'autre :**

```cpp
axes.UpdateAxes(resolveur);           // BRUT   : 0 -> -100 d'un coup
axes.UpdateAxes(resolveur, dt);       // LISSE  : 0 -> -20 -> -40 -> ... -> -100
```

La seconde exige que la commande porte une vitesse :

```cpp
NkAxisCommand("Horizontal", NkInputCode::Key(NkKey::NK_LEFT),
              -100.f,   // scale
              0.f,      // minInterval
              2.f,      // sensitivity : montee, en COURSES par seconde
              2.f);     // gravity     : retour a zero, meme unite
```

⚠️ **`sensitivity` et `gravity` sont en fractions de la course, pas en unités.** Un même
`2.f` se comporte pareil pour un axe à `-1` et pour un axe à `-100` — sinon il faudrait
re-régler la rampe chaque fois qu'on touche à `scale`.

⚠️ **`dt` vient de vous, et ce n'est pas une formalité.** Sans lui, la rampe avancerait
d'un pas **par image** : votre axe monterait deux fois plus vite sur une machine qui rend
deux fois plus d'images.

📌 **`sensitivity = 0` veut dire « instantané », et c'est le défaut.** Tout code écrit
avant le 26/09 garde exactement sa conduite, même s'il appelle la version à `dt`.

**Mesuré** (`scale = -100`, `sensitivity = 2`, `dt = 0,1 s`) :

| | valeurs successives |
|---|---|
| `sensitivity = 0` | `-100 -100 -100 …` |
| `sensitivity = 2` | `-20 -40 -60 -80 -100 …` |
| relâchée au 7ᵉ pas | `… -100 -80 -60 -40 -20 -0 0` |

📌 **La dernière ligne compte** : le **zéro final arrive**. Avec un `minInterval`, une rampe
qui redescend passerait sous le seuil et cesserait d'émettre — le consommateur garderait
pour toujours la dernière valeur non nulle, **un axe bloqué à mi-course**. La version à `dt`
émet aussi quand la valeur a changé, précisément pour livrer cette dernière marche.

---

## 7. Les pièges, en résumé

| piège | ce qu'il faut savoir |
|---|---|
| **Le saut se déclenche plusieurs fois** | `IsKeyDown` dure ; il fallait un événement |
| **La souris traîne derrière le curseur** | `if` au lieu de `while` sur `PollEvent` |
| **`MouseDeltaThisFrame` rend toujours 0** | `NewFrame()` n'est pas appelé — le journal le dit |
| **La caméra tourne encore à l'arrêt** | `MouseDeltaX/Y` persiste ; prenez `ThisFrame` ou `Raw` |
| **Les accents ne passent pas** | `NkKeyPressEvent` au lieu de `NkTextInputEvent` |
| **Les ZQSD ne marchent pas en QWERTY** | `NkKey` au lieu de `NkScancode` |
| **Des clics disparaissent ailleurs** | un `return true` posé par distraction |
| **Le glisser-déposer ne donne rien** | `dropEnabled = false` dans la config |
| **Ça plante après la fermeture d'un objet** | un callback lui a survécu |
| **L'axe saute de 0 à la limite** | une touche est binaire — `UpdateAxes(resolveur, dt)` + `sensitivity` |
| **L'axe reste bloqué à mi-course** | `minInterval` avalait le zéro final ; la version à `dt` le livre |
| **Pas de getter pour la souris** | il est ici (`NkInput`), le setter est dans NKWindow |

---

## 8. Où lire la suite

| fichier | ce qu'il contient |
|---|---|
| `NKEvent/NkEvent.h` | la classe de base, les types, les catégories |
| `NKEvent/NkEventDispatcher.h` | `NkInput` (l. 376) et le répartiteur |
| `NKEvent/NkEventSystem.h` | `PollEvent`, les abonnements, la file |
| `NKWindow/Core/NkWESystem.h` (l. 191) | `NkEvents()` |
| `NKEvent/NkKeyboardEvent.h` | `NkKey`, `NkScancode`, `NkModifierState` |
| `NKEvent/NkMouseEvent.h` | `NkMouseButton`, `NkButtonState`, `NkMouseButtons` |
| `Applications/ImGuiRef/src/ImGuiRef/main.cpp` | la boucle complète en situation |

⚠️ **Les en-têtes expliquent *pourquoi*, pas seulement *quoi*.** Le commentaire de
`MouseDeltaX` dit son contrat et **pourquoi ce contrat a été gardé** au lieu d'être
« corrigé ». Ce sont ces paragraphes-là qui font gagner du temps.

---

## 9. Si quelque chose ne marche pas

1. **Lisez `logs/app.log`.** Plusieurs manques s'y écrivent **nommément** — dont l'oubli de
   `NewFrame()`.
2. **Vérifiez que vous avez choisi le bon mécanisme.** Interrogation ou événement : c'est
   la source d'erreur numéro un, loin devant les autres.
3. **Signalez-le.** Les fonctions `MouseDeltaThisFrameX/Y()` et `NewFrame()` **existent
   parce qu'un utilisateur a posé une question le 26/09 au matin.** Une question vaut mieux
   qu'une heure perdue — et parfois elle fait naître la fonction qui manquait.
