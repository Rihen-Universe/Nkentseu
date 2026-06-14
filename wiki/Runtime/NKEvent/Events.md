# Les événements

> Couche **Runtime** · NKEvent · Le cœur du système d'entrées : le type de base **`NkEvent`**,
> les énumérations **`NkEventType`** / **`NkEventCategory`**, et les outils de routage —
> **`NkEventDispatcher`** (push typé), **`NkInputQuery`** (pull/polling), le **système**
> **`NkEventSystem`** et son **état** **`NkEventState`**, plus la couche **actions/axes**
> (`NkActionManager`, `NkAxisManager`) pour le rebinding.

Tout ce qui « arrive » à une application — une touche enfoncée, la fenêtre redimensionnée, une
manette branchée, un fichier lâché par glisser-déposer, le système qui change de thème — est
représenté par **un objet `NkEvent`**. La question centrale d'un module d'événements n'est pas
« comment les recevoir » (le système plateforme s'en charge), mais « **comment les router vers le
bon code, sans surcoût, et sans se tromper de type** ». NKEvent répond avec deux modèles
complémentaires : **push** (on vous appelle quand un événement du type voulu survient) et **pull**
(vous interrogez l'état d'entrée quand vous en avez besoin). Cette page vous apprend les deux et
quand prendre l'un plutôt que l'autre.

Tous les événements héritent d'une **classe de base polymorphe** unique (`NkEvent`) et portent un
**type** (`NkEventType::Value`, une énumération exhaustive) et un ou plusieurs **flags de catégorie**
(`NkEventCategory::Value`, un bitfield pour filtrer en gros). Le repérage de type est un **RTTI
léger maison** en `O(1)` (`Is<T>()` / `As<T>()`), pas le `dynamic_cast` du C++ : pas de table RTTI,
juste une comparaison d'entiers. Toute la mémoire reste cohérente avec NKMemory et le déplacement
`traits::NkMove` ; rien ne passe par `new`/`delete` côté applicatif.

- **Namespaces** : `nkentseu` (racine), `nkentseu::traits`, `nkentseu::memory`, `nkentseu::detail`
- **Headers** : `NKEvent/NkEvent.h`, `NKEvent/NkEventDispatcher.h`, `NKEvent/NkEventSystem.h`,
  `NKEvent/NkEventState.h` (les deux derniers incluent les deux premiers)

---

## Qu'est-ce qu'un événement : `NkEvent`, `NkEventType`, `NkEventCategory`

Un `NkEvent` est un petit objet qui décrit **un fait survenu**. Il connaît trois choses
fondamentales : son **type** (quoi exactement — `NK_KEY_PRESSED`, `NK_WINDOW_RESIZE`…), sa ou ses
**catégories** (à quelle famille il appartient — clavier, fenêtre, souris…), et le contexte minimal
commun : la **fenêtre** d'origine (`GetWindowId()`, `0` = global), l'**horodatage**
(`GetTimestamp()`, en millisecondes depuis l'init du système) et son **état de traitement**
(`IsHandled()`).

Le **type** est une énumération **plate et exhaustive** : `NkEventType::Value` liste *tous* les
événements possibles du moteur, de `NK_NONE = 0` à la sentinelle `NK_EVENT_COUNT`. Les valeurs sont
contiguës et **leur ordre ne change jamais** (compat binaire) — cela permet d'indexer des tableaux
par type. La **catégorie**, elle, est un **bitfield** (`BIT(n)`) : un même événement peut cumuler
plusieurs catégories (une touche est à la fois `NK_CAT_INPUT` et `NK_CAT_KEYBOARD`), ce qui rend le
**filtrage grossier** trivial — « je veux tout ce qui touche à l'entrée » = un seul `&`.

Ce n'est **pas** un système à `dynamic_cast`. Le repérage de type repose sur une convention :
chaque classe dérivée expose un `static GetStaticType()` (généré par les macros
`NK_EVENT_TYPE_FLAGS` / `NK_EVENT_STATIC_TYPE`). À partir de là, `ev.Is<NkKeyPressedEvent>()`
compare deux entiers (`GetType() == T::GetStaticType()`) et `ev.As<NkKeyPressedEvent>()` fait un
`static_cast` **seulement si** le type correspond, renvoyant `nullptr` sinon. C'est `O(1)`, sans
table RTTI — mais cela impose que `T` ait bien un `GetStaticType()`, faute de quoi le code **ne
compile pas**.

```cpp
NkEvent& ev = /* reçu du système */;
if (ev.Is<NkKeyPressedEvent>()) {                 // O(1), comparaison d'entiers
    auto* k = ev.As<NkKeyPressedEvent>();         // static_cast vérifié, jamais null ici
    // ... traiter la touche ...
    ev.MarkHandled();                             // stoppe la propagation en aval
}
```

`MarkHandled()` / `IsHandled()` / `Unmark()` portent l'**état de consommation** : un handler qui
marque l'événement signale aux suivants qu'il est traité (le dispatcher s'en sert pour stopper la
chaîne), et `Unmark()` le remet en jeu pour un re-dispatch ou un replay. `Clone()` produit une
**copie polymorphe sur le tas** — utile pour différer un événement — mais **l'appelant est
responsable de la libération** (le cycle de vie suit la convention `new`/`delete` à encapsuler).

> **En résumé.** Un `NkEvent` = un fait survenu, porteur d'un **type** (`NkEventType`, énum plate
> exhaustive, ordre figé) et de **catégories** (`NkEventCategory`, bitfield pour filtrer). Le
> repérage est un **RTTI léger `O(1)`** via `Is<T>()` / `As<T>()` (qui exigent `T::GetStaticType()`,
> fourni par les macros). `MarkHandled()` consomme l'événement ; `Clone()` copie sur le tas (à
> libérer soi-même).

---

## Le modèle push : `NkEventDispatcher`

Le **dispatcher** est le routeur type-safe du modèle push. On l'enveloppe **temporairement** autour
d'un événement reçu, puis on tente de le router vers un handler typé : `Dispatch<T>(handler)`
n'appelle le handler **que si** l'événement est exactement du type `T`, qu'il n'est pas déjà
consommé, et qu'il n'est pas nul. Si le handler renvoie `true`, l'événement est marqué traité
(propagation stoppée). C'est `O(1)` — une comparaison de type suivie d'un `static_cast`.

L'idiome typique : dans le `OnEvent` d'une couche, on crée un dispatcher et on enchaîne les
`Dispatch` (un par type qui m'intéresse). Le handler renvoie un `bool` (`true` = consommé). La macro
`NK_DISPATCH(d, EventType, method)` génère la lambda `[this]` qui délègue à `this->method(e)`,
attendue sous la forme `bool method(EventType&)` ; `NK_DISPATCH_FREE` fait de même pour une fonction
libre.

```cpp
void MyLayer::OnEvent(NkEvent& e) {
    NkEventDispatcher d(e);
    NK_DISPATCH(d, NkKeyPressedEvent,  OnKey);     // bool OnKey(NkKeyPressedEvent&)
    NK_DISPATCH(d, NkWindowResizeEvent, OnResize); // bool OnResize(NkWindowResizeEvent&)
}
```

Ce n'est **pas** un abonnement durable : le dispatcher est un **wrapper non-propriétaire** créé et
détruit par événement, il ne retient rien. Pour des abonnements persistants (callbacks enregistrés
une fois pour toutes), c'est `NkEventSystem::AddEventCallback` qu'il faut — voir plus bas. Le retour
`bool` du handler est la pièce maîtresse : il porte la sémantique « j'ai consommé, n'allez pas plus
loin », essentielle pour les couches UI au-dessus du gameplay.

> **En résumé.** `NkEventDispatcher` route **un** événement vers un handler **du bon type** en
> `O(1)` ; le handler renvoie `true` pour consommer (et stopper la propagation). C'est un wrapper
> jetable par événement, pas un abonnement — utilisez `NK_DISPATCH` pour le câblage concis.

---

## Le modèle pull : `NkInputQuery` et l'instance `NkInput`

Tout n'a pas besoin d'être un événement. Pour la question « **la touche Espace est-elle enfoncée
maintenant ?** », le push est lourd : on veut juste **interroger l'état courant** au moment du
gameplay. C'est le rôle de `NkInputQuery`, une façade de **polling** qui lit l'**état d'entrée**
agrégé (`NkEventState`) tenu à jour par le système. L'instance globale **`NkInput`** est prête à
l'emploi :

```cpp
if (NkInput.IsKeyDown(NkKey::NK_SPACE))  player.Jump();
float move = NkInput.GamepadAxis(0, NkGamepadAxis::NK_AXIS_LEFT_X);
int mx = NkInput.MouseX(), my = NkInput.MouseY();
```

`NkInputQuery` est **stateless** (toutes ses lectures sont `const`) : il ne stocke rien, il délègue
à l'état partagé. Tant que `NkSystem` ne lui a pas injecté les systèmes sous-jacents, il renvoie des
valeurs neutres via un **fallback factice** — donc il est **sûr par défaut** (jamais de
déréférencement nul). Il couvre le clavier (`IsKeyDown`, modificateurs, dernière touche), la souris
(position, deltas, deltas bruts, boutons, présence dans la fenêtre) et la manette (boutons, axes,
connexion, et le `GamepadRumble` qui, lui, **agit** au lieu de lire).

Ce n'est **pas** un journal d'événements : le polling ne dit pas « la touche vient d'être
*pressée* », seulement « elle est *enfoncée* en ce moment ». Pour réagir à une **transition**
(appui/relâché), c'est le modèle push (dispatcher ou callback). En pratique on combine : push pour
les **actions discrètes** (tirer, ouvrir un menu), pull pour les **états continus** (déplacement,
visée).

> **En résumé.** `NkInputQuery` (instance globale `NkInput`) **interroge l'état d'entrée courant**
> en lecture seule — clavier, souris, manette. Sûr par défaut (fallback factice avant injection).
> Pull pour les **états continus**, push pour les **transitions** ; on mélange les deux.

---

## Actions et axes : le rebinding logique

Lier le code à des touches physiques en dur (`if Espace…`) est rigide : impossible de reconfigurer
les commandes. La couche **action/axe** introduit une indirection. Un **`NkInputCode`** décrit une
entrée physique abstraite (un périphérique `NkInputDevice` + un code + un modificateur) ; une
**action** (`NkActionCommand`) ou un **axe** (`NkAxisCommand`) est une **commande nommée** liée à un
ou plusieurs `NkInputCode`. Le gameplay s'abonne au **nom** (« Sauter », « Avancer ») ; rebinder =
changer le `NkInputCode` derrière le nom, sans toucher au gameplay.

La différence entre les deux : une **action** est **discrète** (pressée/relâchée, éventuellement
répétée) — `NkActionManager::TriggerAction(code, isPressed)` cherche les commandes liées à ce code
et notifie les abonnés. Un **axe** est **continu** (une valeur `float`) — `NkAxisManager::UpdateAxes`
demande, **chaque frame**, à un *resolver* (`NkAxisResolver`) la valeur brute de chaque (device,
code), la multiplie par l'échelle (`scale`) et, si elle dépasse le seuil (`minInterval`, une
deadzone), notifie les abonnés.

```cpp
NkActionManager actions;
actions.CreateAction("Jump", NK_ACTION_SUBSCRIBER(OnJump));   // void OnJump(name, code, pressed, repeat)
actions.AddCommand({ "Jump", NkInputCode::Key(NkKey::NK_SPACE) });
// ... à l'appui d'une touche :
actions.TriggerAction(NkInputCode::Key(NkKey::NK_SPACE), /*isPressed*/true);

NkAxisManager axes;
axes.CreateAxis("MoveX", NK_AXIS_SUBSCRIBER(OnMoveX));        // void OnMoveX(name, code, value)
axes.AddCommand({ "MoveX", NkInputCode::GamepadAxis(NkGamepadAxis::NK_AXIS_LEFT_X), /*scale*/1.f });
axes.UpdateAxes(resolver);                                    // chaque frame
```

Ces deux managers ne sont **pas thread-safe** : appelez-les depuis un seul thread (typiquement la
boucle de jeu). Subtilité importante : l'égalité d'une `NkActionCommand` (ou `NkAxisCommand`) se
compare **uniquement sur le `NkInputCode`**, pas sur le nom — `RemoveCommand` retire donc la
commande qui partage le même code.

> **En résumé.** `NkInputCode` abstrait une entrée physique ; `NkActionManager` route les
> **actions discrètes** (pressé/relâché via `TriggerAction`), `NkAxisManager` les **axes continus**
> (valeur scalée + deadzone via `UpdateAxes(resolver)`). On s'abonne au **nom**, on rebinde le
> **code**. Non thread-safe ; l'égalité des commandes se fait sur le code seul.

---

## Le système : `NkEventSystem` et son état `NkEventState`

`NkEventSystem` est le **moteur d'événements cross-plateforme** : il pompe les messages de l'OS, met
à jour l'état d'entrée, met en file les événements et appelle les callbacks enregistrés. Il est
**possédé par `NkSystem`** (ce n'est **pas** un singleton) et n'est pas copiable. Côté applicatif on
le pilote surtout via la **pompe d'événements** (`PollEvent` / `PollEvents`) et l'**abonnement
durable** (`AddEventCallback<T>`).

La pompe a trois variantes selon la **durée de vie** voulue : `PollEvent()` renvoie un pointeur
**valide seulement jusqu'au prochain appel** (à ne jamais stocker entre frames) ; la surcharge
out-param `PollEvent(NkEvent*& e)` fait pareil avec un `bool` de présence ; `PollEventCopy()` renvoie
un `NkEventPtr` (un `unique_ptr` à deleter dédié) dont **l'appelant devient propriétaire** — c'est
celle qu'il faut pour un traitement différé ou asynchrone. Sous le capot, les événements transitent
par un **ring buffer dual-priorité** (`NkEventRingBuffer`) : la file **HIGH** (128 slots) ne perd
**jamais** d'événements (cycle de vie, clavier, boutons souris/manette…), la file **NORMAL** (512
slots) **droppe le plus ancien** sous pression. La priorité d'un type est donnée par
`NkGetEventPriority`.

```cpp
NkEvent* e;
while (sys.PollEvent(e)) {            // e invalidé au prochain PollEvent — ne pas conserver
    NkEventDispatcher d(*e);
    NK_DISPATCH(d, NkKeyPressedEvent, OnKey);
}
// Pour différer : prendre une copie possédée.
NkEventPtr keep = sys.PollEventCopy();
```

L'**abonnement durable** est typé et sûr : `AddEventCallback<T>(cb, windowId)` enregistre un
callback qui ne sera appelé **que** pour les événements de type `T` (et, si un `windowId` est fourni,
seulement ceux de cette fenêtre) — il fabrique en interne un wrapper qui filtre puis fait `As<T>()`.
Pour une désinscription **automatique**, `AddEventCallbackGuard<T>(...)` renvoie un `NkCallbackGuard`
(RAII) : quand le guard meurt, le callback est retiré. `ClearEventCallbacks<T>()` et
`ClearAllCallbacks()` nettoient en masse. Il existe aussi des callbacks plus larges :
`SetGlobalCallback` (tous les événements) et `SetWindowCallback` (tous ceux d'une fenêtre).

L'**état d'entrée** que tient le système est `NkEventState` : un **agrégat de snapshots** — clavier,
souris, tactile, manettes. C'est exactement ce que `NkInput` interroge en lecture. Le système le met
à jour automatiquement (sauf si on désactive `SetAutoUpdateInputState`) ; côté applicatif on le lit
via `GetInputState()`, l'écriture étant **réservée au système et aux backends**.

Le **threading** est strict : `PollEvent()` / la pompe OS doivent être appelés depuis le **thread
enregistré à `Init()`** (des assertions le vérifient). Le système utilise deux verrous — un pour le
dispatch direct externe, un pour la file producteur/consommateur. Le pointeur de `PollEvent()` étant
invalidé au prochain appel, **conservez via `PollEventCopy()`**, jamais le pointeur brut.

> **En résumé.** `NkEventSystem` pompe l'OS, met l'état à jour et route les callbacks ; possédé par
> `NkSystem`. `PollEvent()` (pointeur éphémère) / `PollEventCopy()` (copie possédée) pour la boucle ;
> `AddEventCallback<T>` (+ `Guard` RAII) pour l'abonnement typé et durable. File **dual-priorité**
> (HIGH no-drop, NORMAL drop-oldest). Pompe sur le thread d'`Init()` ; ne stockez pas le pointeur de
> `PollEvent`.

---

## Aperçu de l'API

Tous les éléments publics réels, par fichier. Les complexités notables et `noexcept` sont indiqués
quand ils comptent. Chaque entrée est détaillée dans la « Référence complète ».

### `NkEvent.h` — type de base, type, catégorie

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Catégories | `NkEventCategory::Value` (enum bitfield) | Familles : `NK_CAT_APPLICATION/INPUT/KEYBOARD/MOUSE/WINDOW/GRAPHICS/TOUCH/GAMEPAD/CUSTOM/TRANSFER/GENERIC_HID/DROP/SYSTEM`, plus `NK_CAT_NONE`/`NK_CAT_ALL`. |
| Catégories | `NkEventCategory::ToString` / `FromString` | Nom symbolique ↔ valeur (parsing insensible à la casse). |
| Catégories | `operator\| & ~ \|= &=` (free, `inline`) | Combinaison de flags (cast `uint32`, trivial). |
| Catégories | `NkCategoryHas` / `NkCategoryEmpty` / `NkCategoryFull` | Test d'appartenance / vide / complet. |
| Types | `NkEventType::Value` (enum, `NK_NONE`→`NK_EVENT_COUNT`) | Énumération **exhaustive** de tous les événements (ordre figé). |
| Types | `NkEventType::ToString` / `FromString` | Nom ↔ valeur (insensible à la casse). |
| Alias | `NkTimestampMs = uint64` | Millisecondes depuis l'init. |
| Macros | `NK_EVENT_STATIC_TYPE` / `NK_EVENT_TYPE_FLAGS` | Génèrent `GetStaticType()` (+ `GetType`/`GetName`/`GetTypeStr`). |
| Macros | `NK_EVENT_CATEGORY_FLAGS` | Génère l'override `GetCategoryFlags()`. |
| Macros | `NK_EVENT_BIND_HANDLER` | Lambda `[this]` compatible `NkFunction`. |
| Base | `class NkEvent` | Classe de base polymorphe de tout événement. |
| Base — virtuels | `GetType` `GetName` `GetTypeStr` `GetCategoryFlags` `Clone` `ToString` `~NkEvent` | Interface à surcharger ; `Clone` = copie tas (à libérer). |
| Base — ctors | `NkEvent(uint64 windowId)` · `NkEvent()` · copie | Lié à une fenêtre / global / copie. |
| Base — accès | `GetCategory` `GetWindowId`/`SetWindowId` `GetTimestamp` | Contexte commun (`inline`). |
| Base — état | `IsHandled` `MarkHandled` `Unmark` | Consommation / replay (`inline`). |
| Base — prédicats | `IsValid` `IsType` `HasCategory` | Validité / type / catégorie (`inline`). |
| Base — cast | `Is<T>` `As<T>` (+ const) | RTTI léger `O(1)` (exige `T::GetStaticType()`). |
| Base — statics | `TypeToString` `CategoryToString` | Délègent aux énums. |
| Callbacks | `EventObserver` `EventObserverRef` `EventHandler` `EventHandlerRef` `NkEventCallback` | Alias `NkFunction` (réf/const-réf/pointeur brut). |

### `NkEventDispatcher.h` — push typé, polling, actions/axes

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Push | `NkEventHandler<T> = NkFunction<bool(T&)>` | Callback typé ; `true` = consommé. |
| Push | `NkEventDispatcher(NkEvent*\|&)` `noexcept` | Wrapper jetable par événement. |
| Push | `Dispatch<T>(handler)` / `Dispatch<T,Fn>(fn)` | Route si type = `T` & non consommé ; marque traité si `true`. `O(1)`. |
| Push | `GetEvent` `IsHandled` `GetEventType` (`noexcept`) | Accès / état / type courant. |
| Push — macros | `NK_DISPATCH` / `NK_DISPATCH_FREE` | Câblage concis méthode / fonction libre. |
| Pull | `class NkInputQuery` | Façade de polling sur `NkEventState`. |
| Pull — clavier | `IsKeyDown` `IsKeyRepeated` `IsCtrl/Alt/Shift/SuperDown` `LastKey` `LastScancode` | État clavier (lecture `const noexcept`). |
| Pull — souris | `MouseX/Y` `MouseDeltaX/Y` `MouseRawDeltaX/Y` `IsMouseDown` `IsLeft/Right/MiddleDown` `IsAnyMouseDown` `IsMouseInside` | État souris. |
| Pull — manette | `IsGamepadDown` `GamepadAxis` `IsGamepadConnected` `GamepadRumble` | État + vibration (`Rumble` non-`noexcept`). |
| Pull — injection | `SetEventSystem` `SetGamepadSystem` (`noexcept`) | Réservé à `NkSystem`. |
| Pull | `NkInput` (instance globale `inline`) | Idiome `NkInput.IsKeyDown(...)`. |
| Mapping | `NkInputDevice` (enum) | `NK_KEYBOARD/MOUSE/MOUSEWHEEL/GAMEPAD/GAMEPAD_AXIS`. |
| Mapping | `NkInputCode` (struct) | (device, code, modifier) + fabriques `Key/Mouse/Wheel/Gamepad/GamepadAxis` ; `==`/`!=`. |
| Actions | `NkActionCommand` | Action nommée ↔ `NkInputCode` (`==` sur **code seul**). |
| Actions | `NkActionSubscriber` | `(name, code, isPressed, isRepeat)`. |
| Actions | `NkActionManager` | `CreateAction` `AddCommand` `RemoveAction/Command` `TriggerAction` + comptes. **Non thread-safe.** |
| Axes | `NkAxisCommand` | Axe nommé ↔ `NkInputCode` + `scale`/`minInterval` (`==` sur code seul). |
| Axes | `NkAxisSubscriber` | `(name, code, value)`. |
| Axes | `NkAxisResolver` | `(device, code) → float` (valeur brute). |
| Axes | `NkAxisManager` | `CreateAxis` `AddCommand` `RemoveAxis/Command` `UpdateAxes(resolver)` + comptes. **Non thread-safe.** |
| Macros | `NK_ACTION_SUBSCRIBER` / `NK_AXIS_SUBSCRIBER` | Lambdas `[this]` déléguant à une méthode. |

### `NkEventSystem.h` — système, file, callbacks

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias | `NkGlobalEventCallback` `NkTypedEventCallback` (`=NkEventCallback`) · `NkRemoverCallback` · `NkEventPtr` | Callbacks bruts / remover / pointeur possédé. |
| Deleter | `NkEventDelete` | `delete event` (deleter de `NkEventPtr`). |
| Priorité | `NkEventPriority { HIGH, NORMAL }` | HIGH jamais droppé / NORMAL droppable. |
| Priorité | `NkGetEventPriority(type)` `noexcept` | Type → priorité (`O(1)`, switch). |
| File | `NkEventRingBuffer` | Ring dual-priorité (`kHighCapacity=128` no-drop, `kNormalCapacity=512` drop-oldest) ; `Push`/`Pop`/`Empty`/`Size`/`Clear` (`O(1)`). |
| RAII | `NkCallbackGuard` | Désinscription auto (`Release`/`IsActive`), movable, non-copiable. |
| Système | `NkEventSystem` | Moteur cross-plateforme (possédé par `NkSystem`, non copiable). |
| Système — vie | `Init` `Shutdown` `IsReady` (`noexcept`) | Cycle de vie. |
| Système — callbacks | `SetWindowCallback`/`RemoveWindowCallback` · `SetGlobalCallback` · `AddEventCallback<T>` · `AddEventCallbackGuard<T>` · `ClearEventCallbacks<T>` · `ClearAllCallbacks` | Abonnements (typés, par fenêtre, globaux, RAII). |
| Système — pompe | `PollEvent()` · `PollEvent(NkEvent*&)` · `PollEventCopy()` · `PollEvents()` | Pointeur éphémère / out-param / copie possédée / pompe. |
| Système — dispatch | `DispatchEvent(NkEvent&)` · `DispatchEvent<T>(T&&)` · `Enqueue_Public` | Dispatch direct / typé / pont plateforme. |
| Système — état/info | `GetInputState` (const/non) · `GetHidMapper` · `SetGamepadSystem` · `GetPendingEventCount` · `GetTotalEventCount` · `GetPlatformName` | État, mapper HID, stats. |
| Système — toggles | `SetAuto…/Get…InputState` · `…AutoGamepadPoll` · `…QueueMode` (`noexcept`) | Comportements automatiques. |
| Système — modale | `NkSizeMoveFrameFn` · `SetSizeMoveFrameCallback` · `InvokeSizeMoveFrame` (`noexcept`) | Rendu pendant resize/move (Win32). |
| Traits | `detail::NkIsBoolTestable` / `_v` | Détecte un callback convertible en `bool`. |

### `NkEventState.h` — snapshots d'entrée

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| États sémantiques | `NkWindowState` `NkRegionState` `NkConnectionState` `NkResizeState` `NkAxisState` `NkAxisDirection` `NkStatusState` `NkDraggedState` `NkFocusState` | Petits enums `Value` + `ToString` ; `NkAxisState::Classify(value, deadzone)`. |
| Snapshot clavier | `NkKeyboardInputState` | `KEY_COUNT=256` ; `IsKeyPressed/Repeated` `IsAnyKeyPressed` modificateurs + mutateurs `On*` (réservés). |
| Snapshot souris | `NkMouseInputState` | Position/écran/deltas/raw ; `IsButtonPressed` `IsLeft/Right/MiddleDown` ; `On*`. |
| Snapshot tactile | `NkTouchInputState` | `TouchSlot[]` ; `IsTouching` `IsMultiTouch` `FindById` ; `OnBegin/Move/End/Cancel` `UpdateCentroid`. |
| Snapshot manette | `NkGamepadInputState` | `IsButtonDown` `GetAxis/PrevAxis` `GetAxisState` `IsConnected` `IsCharging` ; `On*`. |
| Ensemble manettes | `NkGamepadSetState` (`NK_MAX_GAMEPADS_STATE=8`) | `GetSlot` `IsButtonDown` `GetAxis` `OnConnect/Disconnect`. |
| Agrégat | `NkEventState` | `keyboard` `mouse` `touch` `gamepads` + `IsAnyInputActive` `Clear` ; getters const/non. |

---

## Référence complète

Chaque élément est repris ici. Les éléments triviaux (opérateurs de flags, accesseurs) sont décrits
brièvement ; les pièces de routage et d'état le sont **à fond**, avec leurs usages dans les
différents domaines du temps réel.

### `NkEventCategory` — les familles d'événements

Un **bitfield** de catégories (`BIT(n)`), pensé pour le **filtrage grossier**. Un événement cumule
souvent plusieurs flags : une touche est `NK_CAT_INPUT | NK_CAT_KEYBOARD`. Les opérateurs libres
(`|`, `&`, `~`, `|=`, `&=`) et les helpers `NkCategoryHas` / `NkCategoryEmpty` / `NkCategoryFull`
sont triviaux (de simples `uint32`). `ToString` / `FromString` font le pont symbolique (parsing
insensible à la casse, `NK_CAT_NONE` si inconnu).

- **Outils / éditeur** : un panneau de log qui ne veut afficher que l'entrée filtre sur
  `NK_CAT_INPUT` ; un inspecteur d'événements coche/décoche des familles via les opérateurs de flags.
- **UI / 2D** : une couche overlay s'abonne au `NK_CAT_MOUSE | NK_CAT_KEYBOARD` et laisse passer le
  reste.
- **IO / réseau** : `NK_CAT_TRANSFER` et `NK_CAT_DROP` isolent les événements de transfert et de
  glisser-déposer (import d'assets, fichiers lâchés).
- **Système** : `NK_CAT_SYSTEM` (alimentation, locale, affichage, mémoire) sépare le « niveau OS » du
  « niveau jeu ».

### `NkEventType` — l'énumération exhaustive

`NkEventType::Value` liste **tous** les événements possibles, de `NK_NONE = 0` à `NK_EVENT_COUNT`.
Les valeurs sont **contiguës et figées** (compat binaire), regroupées par famille : application
(`NK_APP_*`), fenêtre (`NK_WINDOW_*` — création, fermeture, paint, resize/move avec leurs phases
*begin/end*, focus, minimize/maximize/restore, fullscreen, DPI, thème, orientation, *safe area*,
surface), clavier (`NK_KEY_PRESSED/REPEATED/RELEASED`, `NK_TEXT_INPUT`, `NK_CHAR_ENTERED`), souris
(move, raw, boutons, double-clic, molette V/H, scroll, enter/leave, capture), manette
(`NK_GAMEPAD_*`), HID générique (`NK_GENERIC_*`), tactile et gestures (`NK_TOUCH_*`, `NK_GESTURE_*`),
glisser-déposer (`NK_DROP_*`), système (`NK_SYSTEM_*`), plus `NK_TRANSFER` et `NK_CUSTOM`.

Le fait que l'énum soit **plate et bornée** par `NK_EVENT_COUNT` est exploité partout : on peut
indexer un tableau de callbacks par type, ou valider qu'un `GetStaticType()` est `< NK_EVENT_COUNT`
avant d'enregistrer (ce que fait `AddEventCallback`). `ToString` / `FromString` servent au logging,
à la sérialisation lisible et aux outils.

- **Gameplay / IA** : un replay enregistre la suite des `NkEventType` + payloads pour rejouer une
  partie.
- **Outils / éditeur** : un overlay de debug affiche le `ToString` de chaque événement reçu.
- **Threading** : la priorité (`NkGetEventPriority`) est décidée par switch sur ce type — d'où
  l'intérêt d'un ordre stable.

### `NkEvent` — la classe de base

Le contrat polymorphe. Les **virtuels** (`GetType`, `GetName`, `GetTypeStr`, `GetCategoryFlags`,
`Clone`, `ToString`) sont surchargés par chaque dérivé, le plus souvent via les macros
`NK_EVENT_TYPE_FLAGS` (génère type/nom) et `NK_EVENT_CATEGORY_FLAGS` (génère les flags). Les
accesseurs non-virtuels (`GetWindowId/SetWindowId`, `GetTimestamp`, `GetCategory`) et l'état
(`IsHandled`/`MarkHandled`/`Unmark`) sont `inline`.

Le **cast type-safe** est la pièce centrale, à connaître par cœur :

- `Is<T>()` compare `GetType()` à `T::GetStaticType()` — `O(1)`, juste deux entiers. Sert de garde
  avant tout traitement spécifique.
- `As<T>()` fait le `static_cast` **si** le type correspond, sinon `nullptr` — d'où l'idiome
  `if (auto* k = ev.As<NkKeyPressedEvent>())`. C'est un `static_cast` non vérifié au-delà du test de
  type : il **repose sur l'unicité** de `GetStaticType()`.

Domaines d'usage du repérage :

- **Rendu** : réagir à `NkWindowResizeEvent` / `NK_WINDOW_DPI_CHANGE` pour recréer la swapchain.
- **UI / 2D** : une couche teste `Is<NkMouseButtonPressedEvent>()` pour le hit-test, puis
  `MarkHandled()` si elle consomme le clic (le gameplay sous-jacent ne le verra pas).
- **Gameplay / IA** : `As<NkKeyPressedEvent>()` pour mapper une touche à une action.
- **IO / réseau** : `As<NkDropFileEvent>()` (famille `NK_DROP_*`) pour importer des fichiers lâchés.

`Clone()` produit une copie **polymorphe** sur le tas — utile pour mettre un événement de côté — mais
**l'appelant doit la libérer**. `MarkHandled()`/`Unmark()` pilotent la propagation : un handler qui
consomme stoppe la chaîne ; `Unmark()` autorise un re-dispatch ou un replay.

Les **alias de callbacks** (`EventObserver`, `EventObserverRef`, `EventHandler`, `EventHandlerRef`,
`NkEventCallback`) sont de simples `NkFunction` sur `NkEvent&` / `const NkEvent&` / `NkEvent*` — le
dernier (`NkEventCallback`, pointeur brut) est le format **bas niveau** utilisé par le système.

> **Piège.** `Is<T>` / `As<T>` exigent que `T` expose `GetStaticType()` (sinon erreur de
> compilation) — fourni par `NK_EVENT_TYPE_FLAGS` / `NK_EVENT_STATIC_TYPE`. `As<T>` est un
> `static_cast` non vérifié au-delà du test de type : il dépend de l'unicité de `GetStaticType()`.

### `NkEventDispatcher` — le routeur push

Wrapper **non-propriétaire** sur un `NkEvent*`, créé temporairement par événement.
`Dispatch<T>(handler)` (avec `static_assert` que `T` dérive de `NkEvent`) renvoie `false` si
l'événement est nul, déjà traité, ou d'un autre type ; sinon il appelle `handler(static_cast<T&>(*ev))`
et, si le handler renvoie `true`, marque l'événement traité. La surcharge `Dispatch<T, Fn>(fn)`
accepte n'importe quel callable convertible. `GetEvent`, `IsHandled`, `GetEventType` (`NK_NONE` si
nul) renseignent sur l'état courant. `O(1)` partout.

- **UI / 2D** : la pile de couches descend ; chaque couche `Dispatch` ses types et consomme (`true`)
  ce qu'elle gère, stoppant la propagation vers le gameplay dessous.
- **Outils / éditeur** : raccourcis clavier d'un viewport routés vers des méthodes dédiées via
  `NK_DISPATCH`.
- **Gameplay** : un état (menu, pause, jeu) installe ses propres handlers et ignore le reste.

### `NkInputQuery` et `NkInput` — le polling

Façade `const` de lecture d'état, déléguant à `NkEventState`. Tant que `NkSystem` n'a pas injecté
les systèmes (`SetEventSystem` / `SetGamepadSystem`, réservés), un **fallback factice** renvoie des
valeurs neutres — donc l'appel est toujours sûr. Couverture : clavier (`IsKeyDown`, `IsKeyRepeated`,
modificateurs `IsCtrl/Alt/Shift/SuperDown`, `LastKey`, `LastScancode`), souris (`MouseX/Y`, deltas
et deltas bruts, `IsMouseDown` + raccourcis gauche/droite/milieu, `IsAnyMouseDown`, `IsMouseInside`),
manette (`IsGamepadDown`, `GamepadAxis`, `IsGamepadConnected`, et `GamepadRumble` qui **déclenche**
une vibration — seul membre non-`noexcept`).

- **Gameplay** : déplacement continu (`GamepadAxis` / `IsKeyDown` chaque frame), visée, tir maintenu.
- **Animation** : moduler une vitesse d'anim par l'amplitude d'un stick (`GamepadAxis`).
- **UI / 2D** : connaître la position souris (`MouseX/Y`) pour le survol, `IsMouseInside` pour
  ignorer les entrées hors fenêtre.
- **Audio / retour haptique** : `GamepadRumble` pour un feedback (impact, moteur).

L'instance globale **`NkInput`** rend tout cela immédiat (`NkInput.IsKeyDown(NkKey::NK_SPACE)`) ;
étant stateless, elle est partageable sans souci.

### `NkInputDevice`, `NkInputCode` — l'entrée abstraite

`NkInputDevice` énumère les sources (`NK_KEYBOARD`, `NK_MOUSE`, `NK_MOUSEWHEEL`, `NK_GAMEPAD`,
`NK_GAMEPAD_AXIS`). `NkInputCode` réunit `device` + `code` + `modifier` et fournit des **fabriques**
lisibles : `Key(k, mod)`, `Mouse(b, mod)`, `Wheel(horizontal)` (code 1 si horizontal sinon 0),
`Gamepad(b, mod)`, `GamepadAxis(a)` (modifier toujours 0). L'égalité compare les trois champs. C'est
la **clé de rebinding** : on stocke des `NkInputCode` dans un fichier de config et on les recharge.

### `NkActionCommand`, `NkActionManager` — actions discrètes

Une `NkActionCommand` lie un **nom** à un `NkInputCode`, avec un drapeau `repeatable` (et un
`privRepeatable` interne). Son **égalité se fait sur le `mCode` seul** (le nom n'entre pas en compte)
— `RemoveCommand` retire donc par code. `NkActionManager` orchestre : `CreateAction(name, handler)`
enregistre l'abonné, `AddCommand(cmd)` lie un code à un nom, `TriggerAction(code, isPressed)` cherche
les commandes liées et **notifie** (`name, code, isPressed, isRepeat`). `GetActionCount` /
`GetCommandCount` (global ou par nom) informent. **Non thread-safe** — un seul thread.

- **Gameplay** : « Sauter », « Tirer », « Interagir » mappés à des touches/boutons rebindables.
- **Outils / éditeur** : raccourcis (`Ctrl+S`) déclarés comme actions, reconfigurables par
  l'utilisateur.
- **UI** : navigation menu (valider, annuler) indépendante du périphérique physique.

La macro `NK_ACTION_SUBSCRIBER(method)` génère la lambda d'abonnement déléguant à
`this->method(name, code, isPressed, isRepeat)`.

### `NkAxisCommand`, `NkAxisManager`, `NkAxisResolver` — axes continus

Une `NkAxisCommand` lie un nom à un `NkInputCode` avec une **échelle** (`scale`) et un **seuil**
(`minInterval`, deadzone) ; égalité sur le code seul. `NkAxisManager::UpdateAxes(resolver)` est
appelé **chaque frame** : pour chaque commande, il demande au `NkAxisResolver` la valeur brute de
`(device, code)`, multiplie par `scale`, et si `|value| >= minInterval` **notifie** l'abonné
(`name, code, value`). Le resolver est votre adaptateur entre périphérique physique et valeur
normalisée (typiquement `[-1,1]` ou `[0,1]`, `0` si inactif). **Non thread-safe.**

- **Gameplay** : « MoveX », « MoveY », « LookX », « LookY » — un axe par direction, alimenté indistinctement
  par stick analogique ou paire de touches via le resolver.
- **Animation / caméra** : une valeur d'axe pilote une rotation ou un blend continu.
- **Physique** : une gâchette (axe `[0,1]`) module une force (accélération d'un véhicule).

La macro `NK_AXIS_SUBSCRIBER(method)` génère la lambda déléguant à `this->method(name, code, value)`.

### `NkEventRingBuffer` — la file dual-priorité

Ring buffer pré-alloué, non copiable, avec **deux files** : HIGH (`kHighCapacity = 128`, **no-drop**)
et NORMAL (`kNormalCapacity = 512`, **drop-oldest**). `Push(ev, prio)` route selon la priorité
(retourne `false` si la file HIGH est pleine ; la file NORMAL écrase le plus ancien). `Pop()` sert
**HIGH d'abord** puis NORMAL, `nullptr` si vide. `Empty` / `Size` (somme des deux) / `Clear` (boucle
de `Pop`). Tout est `O(1)`. La séparation garantit qu'un afflux d'événements de bruit (`NK_MOUSE_MOVE`)
ne fait **jamais** perdre une touche pressée ou une fermeture de fenêtre.

- **Threading** : modèle producteur (thread OS) / consommateur (thread de pompe) protégé par verrou
  côté système.
- **Gameplay** : sous forte charge, les `MOUSE_MOVE`/`AXIS_MOTION` (NORMAL) peuvent être abandonnés
  sans perdre les transitions importantes (HIGH).

### `NkCallbackGuard` — désinscription RAII

Garde non-copiable mais **movable** : construit avec un `NkRemoverCallback`, il appelle le remover à
sa **destruction** (ou à `Release()`, une seule fois). `IsActive()` indique s'il tient encore un
remover. C'est ce que renvoie `AddEventCallbackGuard<T>` : tant que le guard vit, le callback est
abonné ; quand il sort de portée, l'abonnement disparaît — idéal pour lier la durée d'un abonnement
à celle d'un objet (une couche UI, un outil, un panneau d'éditeur).

### `NkEventSystem` — le moteur

Possédé par `NkSystem`, non copiable. **Cycle de vie** : `Init()` (enregistre le thread de pompe),
`Shutdown()`, `IsReady()`.

**Abonnements.** `AddEventCallback<T>(cb, windowId)` (avec `static_assert` que `cb` est invocable sur
`T*`) installe un callback **filtré par type** (et par fenêtre si `windowId != NK_INVALID_WINDOW_ID`) :
il fabrique un wrapper `(NkEvent*)` qui vérifie la fenêtre, fait `As<T>()` puis appelle. Il **ignore**
un callback bool-testable et faux, ou un `T::GetStaticType() >= NK_EVENT_COUNT`.
`AddEventCallbackGuard<T>(...)` renvoie un `NkCallbackGuard` qui retire le callback à sa destruction.
`ClearEventCallbacks<T>()` et `ClearAllCallbacks()` nettoient. À plus gros grain :
`SetGlobalCallback` (tout) et `SetWindowCallback`/`RemoveWindowCallback` (par fenêtre).

**Pompe.** `PollEvent()` → pointeur **valide jusqu'au prochain appel** (à ne pas stocker) ;
`PollEvent(NkEvent*&)` → même chose en out-param avec `bool` ; `PollEventCopy()` → `NkEventPtr`
**possédé** (traitement différé) ; `PollEvents()` pompe la frame.

**Dispatch direct.** `DispatchEvent(NkEvent&)` et `DispatchEvent<T>(T&&)` (avec `static_assert` base)
poussent un événement immédiatement aux callbacks. `Enqueue_Public(evt, winId)` est le **pont public**
pour les callbacks statiques des backends plateforme.

**État & info.** `GetInputState()` (const/non) donne le `NkEventState` lu par `NkInput` ;
`GetHidMapper()` le mapper HID générique ; `SetGamepadSystem` injecte le système manette ;
`GetPendingEventCount`, `GetTotalEventCount`, `GetPlatformName` informent. Les **toggles**
(`SetAutoUpdateInputState`, `SetAutoGamepadPoll`, `SetQueueMode`, tous `noexcept`, `true` par défaut)
règlent les comportements automatiques.

**Boucle modale.** Sous Win32, un resize/move bloque la boucle ; `SetSizeMoveFrameCallback(fn, user)`
+ `InvokeSizeMoveFrame()` permettent de **continuer à rendre** pendant ce blocage (no-op si non
enregistré).

Domaines :

- **Rendu** : abonnement à `NkWindowResizeEvent` / DPI pour recréer la swapchain ;
  `SetSizeMoveFrameCallback` pour rendre pendant un drag de bordure.
- **Gameplay** : boucle `while (PollEvent(e))` + `NkEventDispatcher` ; `GetInputState` pour le pull.
- **IO / réseau** : `PollEventCopy()` pour mettre un événement en file vers un autre thread.
- **Outils / éditeur** : `SetGlobalCallback` pour un inspecteur d'événements ; `AddEventCallbackGuard`
  pour des abonnements liés à la durée de vie d'un panneau.

> **Threading.** `PollEvent()` / la pompe OS s'appellent depuis le **thread enregistré à `Init()`**
> (assertions). Le pointeur de `PollEvent()` est invalidé au prochain appel : conservez via
> `PollEventCopy()`. Deux verrous séparent le dispatch direct et la file producteur/consommateur.

### `NkEventState` et ses snapshots

`NkEventState` est l'**agrégat** lu par `NkInput` : quatre snapshots publics — `keyboard`
(`NkKeyboardInputState`), `mouse` (`NkMouseInputState`), `touch` (`NkTouchInputState`), `gamepads`
(`NkGamepadSetState`). `IsAnyInputActive()` fait l'OR de « une touche pressée », « un bouton souris »,
« un toucher actif », « au moins une manette connectée » ; `Clear()` réinitialise tout. Les getters
existent en const (lecture applicative) et non-const (**écriture réservée** au système et aux
backends).

Chaque snapshot suit le même esprit : des **lectures** `const noexcept` (avec bounds-check sur les
tableaux indexés par enum) et des **mutateurs `On*`** réservés au système.

- **`NkKeyboardInputState`** (`KEY_COUNT = 256`) : `pressed[256]`, `repeated[256]`, modificateurs,
  dernière touche/scancode ; `IsKeyPressed`/`IsKeyRepeated` (bornés), `IsAnyKeyPressed` (`O(256)`),
  `IsCtrl/Alt/Shift/SuperDown`. Mutateurs `OnKeyPress/Repeat/Release`, `Clear`. **Gameplay** : la
  source de vérité du clavier ; **outils** : capture d'un raccourci pour le rebinding.
- **`NkMouseInputState`** : position client + écran, deltas, **deltas bruts** (`rawDeltaX/Y`, pour la
  visée FPS sans accélération), boutons, modificateurs, `captured`/`insideWindow`/`insideFrame`
  (pixels physiques, origine haut-gauche). `OnMove` calcule le delta, `OnRaw`, `OnButtonPress/Release`,
  `OnEnter`/`OnLeave`. **Rendu/2D** : survol et picking ; **gameplay** : caméra à la souris via deltas
  bruts.
- **`NkTouchInputState`** : `TouchSlot[NK_MAX_TOUCH_POINTS]` (id, x, y, pression, actif), `activeCount`,
  centroïde. `IsTouching`, `IsMultiTouch` (≥ 2), `FindById` (`O(N)`). `OnBegin` (premier slot libre),
  `OnMove` (par id), `OnEnd`, `OnCancel`, `UpdateCentroid`. **UI mobile** : pinch/pan/rotate via le
  centroïde et plusieurs slots.
- **`NkGamepadInputState`** : `connected`, `info`, `batteryLevel`, `buttons[]`, `axes[]`, `prevAxes[]`.
  `IsButtonDown`, `GetAxis`/`GetPrevAxis` (utile pour détecter une **transition** d'axe),
  `GetAxisState` (→ `NkAxisState::Classify`), `IsConnected`, `IsCharging` (**placeholder**, renvoie
  toujours `false`). `OnButtonPress/Release`, `OnAxisMove` (sauve le précédent), `Clear`.
- **`NkGamepadSetState`** (`NK_MAX_GAMEPADS_STATE = 8`) : `slots[]` + `connectedCount` ; `GetSlot`
  (borné, `nullptr` hors plage), `IsButtonDown`/`GetAxis` par index, `OnConnect`/`OnDisconnect`.
  **Multijoueur local** : un slot par manette.

Les **états sémantiques** (`NkWindowState`, `NkRegionState`, `NkConnectionState`, `NkResizeState`,
`NkAxisState`, `NkAxisDirection`, `NkStatusState`, `NkDraggedState`, `NkFocusState`) sont de petits
enums `Value` (toujours `..._UNDEFINED = 0` en tête) avec un `ToString`. `NkAxisState::Classify(value,
deadzone)` classe une valeur brute en `POSITIVE` / `NEGATIVE` / `NEUTRAL` selon la deadzone — pratique
pour transformer un stick analogique en directions discrètes (menus, déplacement en grille).

> **Threading.** Tout `NkEventState` est **non thread-safe** : lecture seule côté applicatif,
> écriture réservée à `NkEventSystem` / aux backends. Les tableaux sont indexés par la valeur `uint32`
> des enums, avec bounds-check dans les accesseurs.

### Le socle commun

- **Cohérence mémoire NKMemory.** `NkEventPtr` libère via `NkEventDelete` (`delete`) : les
  événements mis en file ou clonés doivent être alloués de façon cohérente avec ce deleter. `Clone()`
  laisse l'appelant responsable de la libération.
- **RTTI léger.** `Is<T>` / `As<T>` reposent sur `T::GetStaticType()` (entier unique par type) —
  `O(1)`, sans table RTTI. Les macros `NK_EVENT_TYPE_FLAGS` / `NK_EVENT_STATIC_TYPE` le fournissent.
- **Deux modèles complémentaires.** Push (`NkEventDispatcher`, callbacks) pour les **transitions** ;
  pull (`NkInputQuery` / `NkInput`) pour l'**état continu**. La couche action/axe ajoute le rebinding
  logique par-dessus.
- **Threading discipliné.** La pompe sur le thread d'`Init()` ; pointeur de `PollEvent` éphémère ;
  managers d'actions/axes et `NkEventState` mono-thread.

---

### Exemple récapitulatif

```cpp
#include "NKEvent/NkEventSystem.h"   // tire NkEvent.h + NkEventDispatcher.h + NkEventState.h
using namespace nkentseu;

// --- Push : router les événements de la frame ---
NkEvent* e;
while (sys.PollEvent(e)) {              // pointeur éphémère, ne pas le stocker
    NkEventDispatcher d(*e);
    NK_DISPATCH(d, NkKeyPressedEvent,   OnKey);     // bool OnKey(NkKeyPressedEvent&)
    NK_DISPATCH(d, NkWindowResizeEvent, OnResize);  // bool OnResize(NkWindowResizeEvent&)
}

// --- Abonnement durable, désinscription RAII ---
NkCallbackGuard guard = sys.AddEventCallbackGuard<NkMouseButtonPressedEvent>(
    [](NkMouseButtonPressedEvent* m) { /* hit-test UI */ });
// guard détruit -> callback retiré automatiquement.

// --- Pull : état continu pour le gameplay ---
if (NkInput.IsKeyDown(NkKey::NK_SPACE))                 player.Jump();
float strafe = NkInput.GamepadAxis(0, NkGamepadAxis::NK_AXIS_LEFT_X);

// --- Actions/axes : rebinding logique ---
NkActionManager actions;
actions.CreateAction("Fire", NK_ACTION_SUBSCRIBER(OnFire));
actions.AddCommand({ "Fire", NkInputCode::Mouse(NkMouseButton::NK_BUTTON_LEFT) });

NkAxisManager axes;
axes.CreateAxis("MoveX", NK_AXIS_SUBSCRIBER(OnMoveX));
axes.AddCommand({ "MoveX", NkInputCode::GamepadAxis(NkGamepadAxis::NK_AXIS_LEFT_X), 1.f, 0.1f });
axes.UpdateAxes(resolver);   // chaque frame
```

---

[← Index NKEvent](README.md) · [Récap NKEvent](../NKEvent.md) · [Couche Runtime](../README.md)
