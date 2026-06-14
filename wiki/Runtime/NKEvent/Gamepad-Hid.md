# Manettes et HID générique

> Couche **Runtime** · NKEvent · Brancher des **périphériques de jeu** : le système haut niveau
> `NkGamepadSystem` (manettes Xbox/PlayStation/Switch…), les **événements** de connexion, de
> boutons, d'axes et de retour de force, le **remapping** par joueur avec persistance sur disque, et
> la couche **HID générique** pour tout ce qui n'est pas une manette standard (volants, palonniers,
> contrôleurs exotiques).

Une manette n'est jamais aussi simple qu'un clavier. Deux exemplaires de la « même » manette peuvent
renvoyer leurs boutons dans un ordre différent ; une Xbox, une DualSense et un Pro Controller
nomment leurs boutons autrement (A/Cross/B selon le constructeur) ; un joueur gaucher veut inverser
ses sticks ; et certains périphériques — un volant de course, un palonnier de vol — ne sont même pas
des manettes au sens classique mais des **rapports HID bruts** qu'il faut décoder soi-même. NKEvent
répond à tout cela en **trois étages** : un système haut niveau qui vous donne un layout
**universel Xbox-compatible** prêt à l'emploi, une famille d'**événements** typés pour réagir aux
changements, et un étage bas niveau **HID générique** pour le matériel hors norme.

Le fil conducteur de tout le module tient en une idée : **séparer le physique du logique**. Le
backend (XInput, evdev, etc.) parle en *index physiques* propres au câblage du périphérique ; le
moteur, lui, raisonne en *boutons logiques* (`NK_GP_SOUTH`, `NK_GP_LT`). Entre les deux, une table de
**remapping** par joueur traduit l'un en l'autre. Cette page vous apprend à utiliser les deux faces.

- **Namespace** : `nkentseu` (tous les types)
- **Headers** : `NkGamepadEvent.h`, `NkGamepadSystem.h`, `NkGamepadMappingPersistence.h`,
  `NkGenericHidEvent.h`, `NkGenericHidMapper.h` (pas de header parapluie unique —
  `NkGamepadSystem.h` inclut déjà les événements gamepad et la persistance)

---

## Le système de manettes : `NkGamepadSystem`

C'est le point d'entrée. `NkGamepadSystem` n'est **pas** un singleton : il est **possédé par
NkSystem**, et on y accède par le raccourci global `NkGamepads()` — lequel n'est pas déclaré dans
`NkGamepadSystem.h` mais dans `NkSystem.h` (pour éviter une dépendance circulaire). Pour l'utiliser,
incluez donc `NkSystem.h`/`NkWindow.h`.

À l'initialisation, le système choisit **tout seul** un backend selon la plateforme (factory interne
dans `Init()`) ; on peut aussi lui en **injecter** un — typiquement un mock pour les tests — en
passant un `memory::NkUniquePtr<NkIGamepad>`, dont il prend alors la propriété. Une fois prêt
(`IsReady()`), chaque tour de boucle appelle `PollGamepads()` : il interroge le backend, **compare**
l'état courant à l'état précédent slot par slot, et **émet** les événements correspondants
(`NkGamepadButtonPressEvent`, `NkGamepadAxisEvent`…) via l'`EventSystem`, tout en mettant à jour les
callbacks directs et l'état partagé. En pratique, `EventSystem::PollEvents()` déclenche ce poll
automatiquement si l'auto-poll est actif.

```cpp
auto& gp = NkGamepads();              // possédé par NkSystem, pas un singleton
if (gp.IsConnected(0)) {
    // Lecture directe (n'émet aucun événement) :
    if (gp.IsButtonDown(0, NK_GP_SOUTH))      Jump();
    float32 lx = gp.GetAxis(0, NK_GP_AXIS_LX); // [-1, 1], deadzone appliquée
    Move(lx);
}
```

On dispose ainsi de **deux styles** complémentaires : le **polling direct** (`IsButtonDown`,
`GetAxis`) pour interroger l'état à la frame courante, et les **événements / callbacks** pour
réagir aux *transitions* (un appui, un relâchement, un franchissement d'axe). Le polling ne génère
jamais d'événement ; les événements, eux, naissent du `PollGamepads()`.

Le moteur supporte jusqu'à `NK_MAX_GAMEPADS = 8` slots simultanés. Les capacités internes sont
volontairement larges : `NK_GAMEPAD_BUTTON_MAPPING_CAPACITY = 102` boutons physiques et
`NK_GAMEPAD_AXIS_MAPPING_CAPACITY = 54` axes physiques par slot — bien au-delà des ~25 boutons et
~8 axes logiques, justement pour absorber les périphériques bavards avant remapping.

> **En résumé.** `NkGamepadSystem` est possédé par NkSystem (accès `NkGamepads()`, inclure
> `NkSystem.h`), pas un singleton. `Init()` choisit un backend ou en accepte un injecté ;
> `PollGamepads()` (déclenché par `PollEvents()`) compare l'état et émet les événements. Combinez
> **polling direct** (état courant) et **événements** (transitions). Jusqu'à 8 manettes.

---

## Le layout universel : types, boutons, axes

Plutôt que d'exposer le câblage de chaque constructeur, NKEvent normalise tout sur un **layout
universel Xbox-compatible**. `NkGamepadType` identifie la *famille* (`NK_GP_TYPE_XBOX`,
`NK_GP_TYPE_PLAYSTATION`, `NK_GP_TYPE_NINTENDO`, `NK_GP_TYPE_STEAM`, `NK_GP_TYPE_STADIA`,
`NK_GP_TYPE_GENERIC`, `NK_GP_TYPE_MOBILE`…), surtout utile pour **afficher la bonne icône de bouton**
à l'écran. `NkGamepadButton` énumère les boutons *logiques* : les quatre faces directionnelles
(`NK_GP_SOUTH`/`EAST`/`WEST`/`NORTH`, c.-à-d. A/B/X/Y sur Xbox, Cross/Circle/Square/Triangle sur
PlayStation), les gâchettes (`NK_GP_LB`/`RB`, `NK_GP_LT_DIGITAL`/`RT_DIGITAL`), les clics de sticks
(`NK_GP_LSTICK`/`RSTICK`), la croix directionnelle, les boutons système (`NK_GP_START`/`BACK`/`GUIDE`),
et les extras modernes (`NK_GP_TOUCHPAD`, `NK_GP_CAPTURE`, `NK_GP_MIC`, paddles `NK_GP_PADDLE_1..4`).
`NkGamepadAxis` couvre les axes analogiques avec leurs **conventions** : sticks dans `[-1, 1]`
(`NK_GP_AXIS_LX/LY/RX/RY`), gâchettes dans `[0, 1]` (`NK_GP_AXIS_LT/RT`), et la croix vue comme un axe
(`NK_GP_AXIS_DPAD_X/DPAD_Y`).

Chaque enum vient avec une fonction libre `…ToString` (`NkGamepadTypeToString`,
`NkGamepadButtonToString`, `NkGamepadAxisToString`) en `O(1)`, qui renvoie des libellés
multi-plateformes lisibles — par exemple `"South(A/Cross)"`, `"LB/L1"`, `"L3"`, `"TriggerLeft"` — et
`"Unknown"` par défaut. Idéal pour le débogage et les écrans de configuration des touches.

Ce **n'est pas** une couche d'abstraction par constructeur (il n'y a pas de classe « manette Xbox »
distincte d'une « manette PlayStation ») : c'est *un seul* vocabulaire logique, et c'est le
remapping qui adapte chaque périphérique réel à ce vocabulaire.

> **En résumé.** Un seul layout logique **universel Xbox-compatible** : `NkGamepadType` (famille,
> pour les icônes), `NkGamepadButton` (boutons logiques A/B/X/Y → South/East/West/North),
> `NkGamepadAxis` (sticks `[-1,1]`, gâchettes `[0,1]`). Chaque enum a un `…ToString` `O(1)` à
> libellés multi-plateformes.

---

## Réagir : les événements de manette

Tous les événements gamepad dérivent de `NkGamepadEvent` (catégorie `NK_CAT_GAMEPAD`), qui porte
l'**index** du slot concerné (`GetGamepadIndex()`). La famille couvre tout le cycle de vie d'une
manette :

- **Connexion / déconnexion** — `NkGamepadConnectEvent` transporte une copie complète de la
  `NkGamepadInfo` (nom, type, IDs USB, capacités) ; `NkGamepadDisconnectEvent` ne porte que l'index.
- **Boutons** — `NkGamepadButtonPressEvent` et `NkGamepadButtonReleaseEvent` (tous deux dérivés d'une
  base interne `NkGamepadButtonEvent`) exposent le bouton logique, l'état (`NK_PRESSED`/`NK_RELEASED`)
  et une **valeur analogique** (`GetAnalogValue()`) — car une gâchette « appuyée » a une intensité.
- **Axes** — `NkGamepadAxisEvent` porte la valeur courante, la précédente, le **delta** (calculé à la
  construction), et la deadzone employée. `IsInDeadzone()` dit si la valeur est dans la zone morte.

`NkGamepadAxisEvent` mérite une note : c'est le **seul** événement gamepad qui **n'a pas** de macro
`NK_EVENT_TYPE_FLAGS` dans le header — il expose ses accesseurs mais pas de type d'événement déclaré
de la façon habituelle.

```cpp
if (auto* e = ev->As<NkGamepadButtonPressEvent>()) {
    if (e->GetButton() == NK_GP_START) TogglePause(e->GetGamepadIndex());
}
if (auto* a = ev->As<NkGamepadAxisEvent>()) {
    if (!a->IsInDeadzone() && a->GetAxis() == NK_GP_AXIS_LX)
        Steer(a->GetValue());     // delta dispo via a->GetDelta()
}
```

Deux événements ne sont **pas** des *entrées* mais des *commandes/sorties* : `NkGamepadRumbleEvent`
est **sortant** (l'application le crée pour demander une vibration : moteurs basse/haute fréquence,
retours de gâchette, durée ; `IsStop()` quand tout est à zéro, `durationMs=0` = infini jusqu'au
stop) ; et `NkGamepadBatteryEvent` rapporte le niveau de batterie (`GetLevel()`, `IsCharging()`,
`IsWired()` quand le niveau est négatif, `IsCritical()` sous 10 %).

> **Piège à connaître.** `NkGamepadBatteryEvent` est déclaré avec
> `NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_CONNECT)` : il **réutilise le type CONNECT** au lieu d'un type
> batterie dédié (vraisemblablement un bug). À garder en tête si vous filtrez par type d'événement.

> **En résumé.** Toute la famille dérive de `NkGamepadEvent` (porte l'index). Connect/Disconnect,
> Button Press/Release (avec valeur analogique), Axis (delta + deadzone). `NkGamepadRumbleEvent` est
> **sortant**. Deux quirks : `NkGamepadAxisEvent` sans `NK_EVENT_TYPE_FLAGS`, et
> `NkGamepadBatteryEvent` qui réutilise le type CONNECT.

---

## Adapter : le remapping et sa persistance

Aucune manette n'envoie ses boutons exactement dans l'ordre logique du moteur, et tout joueur
sérieux veut **reconfigurer** ses touches. `NkGamepadSystem` intègre donc, **par slot (0..7)**, un
profil de remapping qui traduit chaque index *physique* en bouton/axe *logique*. On lit l'état de
deux façons : `GetSnapshot(idx)` renvoie l'état **remappé** (ce que le gameplay doit voir), tandis
que `GetRawSnapshot(idx)` renvoie l'état **brut**, avant remapping — précieux justement pour un écran
« appuyez sur le bouton à assigner ».

```cpp
auto& gp = NkGamepads();
// Le joueur veut que son bouton physique 3 devienne « sauter » (South) :
gp.SetButtonMapByIndex(0, /*physique*/3, NK_GP_SOUTH);
// Gaucher : inverser le stick gauche, doubler sa sensibilité :
gp.SetAxisMapByIndex(0, /*physique*/0, NK_GP_AXIS_LX, /*invert*/true, /*scale*/2.f);
gp.ClearMapping(0);                  // tout remettre à l'identité
```

La sentinelle `NK_GAMEPAD_UNMAPPED` (`0xFFFFFFFF`) désactive une entrée ; passer `NK_GP_UNKNOWN`
comme bouton logique fait de même. On peut **sauvegarder** et **recharger** ces profils :
`ExportMappingProfile()`/`ImportMappingProfile()` font le pont avec une structure de données
sérialisable, et `SaveMappingProfile(userId)`/`LoadMappingProfile(userId)` passent par une
**persistance** branchable (`SetMappingPersistence`). Le backend par défaut,
`NkTextGamepadMappingPersistence`, écrit un format texte lisible `.nkmap`. **Toujours** assainir
l'identifiant utilisateur via `SanitizeUserId` (anti-injection de chemin) — c'est ce que fait le
backend en interne.

> **En résumé.** Remapping **par slot** intégré au système : `SetButtonMap`/`SetAxisMap` (invert,
> scale), `Clear`/`Disable`, sentinelle `NK_GAMEPAD_UNMAPPED`. Lecture **remappée**
> (`GetSnapshot`) vs **brute** (`GetRawSnapshot`). Persistance branchable
> (`Save/LoadMappingProfile`, défaut texte `.nkmap`) — assainir toujours le `userId`.

---

## Le matériel hors norme : HID générique

Un volant de course, un palonnier de vol, un panneau de boutons exotique : ce ne sont pas des
manettes standard, mais des périphériques **HID** qui émettent des *rapports* qu'il faut décoder
soi-même. NKEvent en propose une couche dédiée, indépendante du système gamepad. Les événements
dérivent de `NkGenericHidEvent` (catégorie `NK_CAT_GENERIC_HID`) et identifient le périphérique par
un `deviceId` (et non un slot) : connexion (`NkHidConnectEvent`, avec `NkHidDeviceInfo` complète :
nom, chemin, VID/PID, **pages d'usage USB HID**…), déconnexion, boutons (press/release, avec page
et identifiant d'usage HID), axes (`NkHidAxisEvent`, avec valeur normalisée *et* `rawValue` entier
d'origine), et le **rapport brut** `NkHidRawInputEvent` qui transporte le tableau d'octets tel quel.

Pour traduire ces périphériques en commandes logiques, `NkGenericHidMapper` joue le rôle du
remapping gamepad, mais **par `deviceId`** (clé d'une `NkUnorderedMap`) et **header-only**. Sa
philosophie : **non configuré → identité** (un bouton non mappé renvoie son propre index, un axe non
mappé est simplement borné à `[-1, 1]`). On configure des bindings de boutons
(`SetButtonMap`/`DisableButton`) et d'axes (`SetAxisMap`, avec un `NkHidAxisBinding` qui applique,
**dans cet ordre**, invert → scale → offset → deadzone → clamp). À l'arrivée d'un événement,
`MapButtonEvent`/`MapAxisEvent` résolvent l'index logique et la valeur transformée.

```cpp
NkGenericHidMapper mapper;
mapper.SetAxisMap(wheelId, /*physique*/0, /*logique*/AXIS_STEER,
                  /*invert*/false, /*scale*/1.f, /*deadzone*/0.02f);
uint32 outAxis; float32 outVal;
if (mapper.MapAxisEvent(axisEv, outAxis, outVal)) ApplySteering(outAxis, outVal);
```

Attention : `NkGenericHidMapper` est explicitement **non thread-safe** (synchronisez à l'extérieur si
besoin), et il y a **deux univers de mapping distincts** dans le module — celui intégré au
`NkGamepadSystem` (par slot, capacités 102/54) et ce mapper HID autonome (par deviceId). Leurs
sentinelles portent des noms différents (`NK_GAMEPAD_UNMAPPED`, `NK_GAMEPAD_MAPPING_UNMAPPED`,
`NK_HID_UNMAPPED`) mais valent toutes `0xFFFFFFFF`.

> **Piège à connaître.** `NkHidRawInputEvent` est déclaré avec
> `NK_EVENT_TYPE_FLAGS(NK_GENERIC_BUTTON_PRESSED)` : il **réutilise le type bouton-pressé** au lieu
> d'un type RAW dédié (vraisemblablement un bug). C'est aussi le **seul** ctor d'événement du module
> qui n'est **pas** `noexcept` (il déplace un `NkVector<uint8>`).

> **En résumé.** Couche HID générique pour le matériel non standard : événements par `deviceId`
> (connect, boutons avec page/usage, axes avec `rawValue`, rapport `NkHidRawInputEvent`) +
> `NkGenericHidMapper` header-only par `deviceId` (identité par défaut, binding invert→scale→offset→
> deadzone→clamp). **Non thread-safe**. Distinct du remapping gamepad.

---

## Aperçu de l'API

La liste de **tous** les éléments publics. Chacun est détaillé dans la « Référence complète ».

### `NkGamepadEvent.h` — types, boutons, axes, événements

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkGamepadType` (`NK_GP_TYPE_UNKNOWN`…`MAX`) | Famille de manette (icônes). |
| Enum | `NkGamepadButton` (`NK_GP_SOUTH`…`PADDLE_4`) | Boutons logiques universels. |
| Enum | `NkGamepadAxis` (`NK_GP_AXIS_LX`…`DPAD_Y`) | Axes (sticks `[-1,1]`, gâchettes `[0,1]`). |
| Enum | `NkButtonState` (`NK_PRESSED`/`NK_RELEASED`) | État d'un bouton. |
| Libre | `NkGamepadTypeToString` / `…Button…` / `…Axis…` | Libellés multi-plateformes `O(1)`, défaut `"Unknown"`. |
| Struct | `NkGamepadInfo` | Métadonnées (index, id, name, type, VID/PID, capacités). |
| Base | `NkGamepadEvent` : `GetGamepadIndex()` | Base abstraite (cat. `NK_CAT_GAMEPAD`). |
| Event | `NkGamepadConnectEvent` : `GetInfo()` | Connexion (copie `NkGamepadInfo`). |
| Event | `NkGamepadDisconnectEvent` | Déconnexion (index seul). |
| Event | `NkGamepadButtonPressEvent` / `…Release…` | Appui/relâchement : `GetButton`, `GetState`, `GetAnalogValue`. |
| Event | `NkGamepadAxisEvent` (sans `NK_EVENT_TYPE_FLAGS`) | Axe : `GetValue/PrevValue/Delta/Deadzone`, `IsInDeadzone`. |
| Event | `NkGamepadRumbleEvent` (**sortant**) | Vibration : moteurs, gâchettes, durée ; `IsStop`. |
| Event | `NkGamepadBatteryEvent` (réutilise type CONNECT) | Batterie : `GetLevel`, `IsCharging`, `IsWired`, `IsCritical`. |
| Commun | `Clone()`, `ToString()` | Copie heap CRT (`new`) ; description texte. |

### `NkGamepadSystem.h` — système, backend, polling, remapping

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Const | `NK_MAX_GAMEPADS=8`, `NK_GAMEPAD_UNMAPPED=0xFFFFFFFF` | Slots max ; sentinelle désactivation. |
| Const | `…BUTTON_MAPPING_CAPACITY=102`, `…AXIS…=54` | Capacités physiques par slot. |
| Struct | `NkGamepadSnapshot` : `IsButtonDown`, `GetAxis`, `Clear` | État complet par poll (boutons, axes, capteurs, batterie). |
| Alias | `NkGamepadConnect/Button/AxisCallback` | Signatures des callbacks directs. |
| Interface | `NkIGamepad` | Backend PIMPL : `Init/Shutdown/Poll/GetSnapshot/Rumble/SetLEDColor/HasMotion/GetName`. |
| Imbriqué | `NkAxisRemap`, `NkRemapProfile` | Données de remapping (par slot). |
| Cycle | `Init(backend={})`, `Shutdown`, `IsReady` | Démarrage (factory ou injection), arrêt, prêt ? |
| Pompe | `PollGamepads()` | Poll backend → diff → émet événements + callbacks. |
| Callbacks | `SetConnect/Button/AxisCallback` | Réaction directe (sans passer par l'EventSystem). |
| Polling | `GetConnectedCount`, `IsConnected`, `GetInfo` | État de connexion / métadonnées. |
| Polling | `GetSnapshot` (remappé) / `GetRawSnapshot` (brut) | État courant par slot. |
| Polling | `IsButtonDown(`/`ByIndex)`, `GetAxis(`/`ByIndex)` | Lecture logique (+ variantes par index brut `IsRaw…`/`GetRaw…`). |
| Sortie | `Rumble`, `RumbleStop`, `SetLEDColor` | Vibration, LED. |
| Config | `SetDeadzone`/`GetDeadzone`, `SetAxisEpsilon`/`Get…` | Zone morte (défaut 0.08), epsilon axe (défaut 0.001). |
| Remap | `SetButtonMap(`/`ByIndex)`, `SetAxisMap(`/`ByIndex)` | Mapper bouton/axe (invert, scale). |
| Remap | `Disable…ByIndex`, `ClearMapping`, `ClearAllMappings`, `GetMapping` | Désactiver / réinitialiser / lire un profil. |
| Persist | `SetMappingPersistence`, `Get…`, `Export/ImportMappingProfile` | Brancher la persistance ; (dé)sérialiser un profil. |
| Persist | `Save/LoadMappingProfile(userId, …)` | Écrire/lire sur disque. |
| Backend | `SetEventSystem`, `GetBackend` | Injection de dépendance ; accès backend. |
| Libre | `NkGamepads()` (dans `NkSystem.h`) | Raccourci d'accès (délègue à NkSystem). |

### `NkGamepadMappingPersistence.h` — sérialisation des profils

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Const | `NK_GAMEPAD_MAPPING_UNMAPPED=0xFFFFFFFF` | Sentinelle. |
| Struct | `NkGamepadButtonMapEntry`, `NkGamepadAxisMapEntry` | Entrées (physique→logique, scale/invert). |
| Struct | `NkGamepadMappingSlotData`, `…ProfileData` | Profil d'un slot ; profil complet (version, backend, slots). |
| Interface | `NkIGamepadMappingPersistence` : `GetFormatName`, `Save`, `Load` | Backend de persistance abstrait. |
| Impl | `NkTextGamepadMappingPersistence` | Backend texte `.nkmap` (défaut). |
| Statics | `ResolveDefaultBaseDirectory`, `ResolveCurrentUserId`, `SanitizeUserId` | Chemins ; assainissement (anti-injection). |

### `NkGenericHidEvent.h` — HID générique (événements)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkHidUsagePage` (`…GENERIC`/`GAME`/`SENSOR`/`VENDOR_*`…) | Pages d'usage USB HID. |
| Struct | `NkHidDeviceInfo` | Métadonnées (deviceId, name, path, VID/PID, usage, compteurs). |
| Base | `NkGenericHidEvent` : `GetDeviceId()` | Base abstraite (cat. `NK_CAT_GENERIC_HID`). |
| Event | `NkHidConnectEvent` : `GetInfo()` / `NkHidDisconnectEvent` | Connexion / déconnexion. |
| Event | `NkHidButtonPressEvent` / `…Release…` | Bouton : `GetButtonIndex`, `GetState`, `GetUsagePage/Id`. |
| Event | `NkHidAxisEvent` | Axe : `GetValue/PrevValue/Delta`, `GetRawValue`, `GetUsagePage/Id`. |
| Event | `NkHidRawInputEvent` (réutilise type BUTTON_PRESSED) | Rapport brut : `GetReportId`, `GetData/Bytes/Size`. |

### `NkGenericHidMapper.h` — HID générique (remapping)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Const | `NK_HID_UNMAPPED=0xFFFFFFFF` | Sentinelle. |
| Struct | `NkHidAxisBinding` | Transformation d'axe (invert→scale→offset→deadzone→clamp). |
| Classe | `NkGenericHidMapper` (header-only, **non thread-safe**) | Mapping par `deviceId`. |
| Config | `Clear`, `ClearDevice`, `SetButtonMap`, `DisableButton`, `SetAxisMap`, `DisableAxis` | Configurer les bindings. |
| Résolution | `ResolveButton` `[O(1)]`, `ResolveAxis` `[O(1)]` | Index physique → logique (+ valeur). |
| Résolution | `MapButtonEvent`, `MapAxisEvent` | Résoudre depuis un événement HID. |

---

## Référence complète

Chaque élément est repris ici à fond. Les éléments triviaux (getters, structs de données) sont
décrits brièvement ; le système, le remapping et la couche HID le sont en détail, avec leurs usages
dans les différents domaines.

### `NkGamepadType`, `NkGamepadButton`, `NkGamepadAxis` — le vocabulaire logique

Trois enums définissent **tout** le langage des manettes. `NkGamepadType : uint32` énumère les
familles, de `NK_GP_TYPE_UNKNOWN=0` à `NK_GP_TYPE_MAX`, en passant par Xbox, PlayStation, Nintendo,
Steam, Stadia, Generic et Mobile. `NkGamepadButton : uint32` décrit le **layout universel**
(`NK_GP_UNKNOWN=0` jusqu'à `NK_GAMEPAD_BUTTON_MAX`) : faces South/East/West/North, bumpers et
gâchettes digitales, clics de sticks, croix, Start/Back/Guide, et extras (touchpad, capture, micro,
paddles). `NkGamepadAxis : uint32` liste les axes (`NK_GP_AXIS_LX=0`…`NK_GAMEPAD_AXIS_MAX`), sticks
en `[-1,1]`, gâchettes en `[0,1]`. Chaque enum possède une fonction libre `…ToString` `noexcept` en
`O(1)` retournant un libellé lisible multi-plateformes, défaut `"Unknown"`.

- **Gameplay / IA** — on raisonne *toujours* en boutons logiques : `NK_GP_SOUTH` = « action
  principale » quel que soit le constructeur, ce qui rend le code de gameplay portable d'une manette
  à l'autre.
- **UI / 2D** — `NkGamepadType` + `NkGamepadButtonToString` permettent d'afficher la bonne icône et
  le bon nom (« appuyez sur A » vs « appuyez sur Cross ») selon la manette branchée.
- **Outils / éditeur** — les libellés `…ToString` peuplent un écran de reconfiguration des touches
  et les logs de débogage d'entrées.

### `NkGamepadInfo` — la carte d'identité d'une manette

Struct de métadonnées : `index`, `id[128]`, `name[128]`, `type`, `vendorId`/`productId` (USB), les
compteurs `numButtons`/`numAxes`, et une série de **capacités** (`hasRumble`, `hasTriggerRumble`,
`hasTouchpad`, `hasGyro`, `hasLED`, `hasBattery`). Le constructeur zéro-initialise `id` et `name` via
`memory::NkMemSet`. C'est ce que transporte `NkGamepadConnectEvent` et ce que renvoie
`GetInfo(idx)`.

- **Gameplay** — tester `hasRumble`/`hasTriggerRumble` avant de demander une vibration ; tester
  `hasGyro` avant d'activer une visée gyroscopique.
- **UI / 2D** — afficher `name` et l'icône déduite de `type` dans le menu des joueurs.
- **Outils / éditeur** — `vendorId`/`productId` identifient le modèle exact pour un préréglage de
  mapping.

### Les événements gamepad — réagir aux transitions

Tous dérivent de `NkGamepadEvent` (cat. `NK_CAT_GAMEPAD`), qui porte `GetGamepadIndex()`.

- **`NkGamepadConnectEvent`** (type `NK_GAMEPAD_CONNECT`) — copie la `NkGamepadInfo` complète et
  propage son `index`. `GetInfo()` la relit. `ToString()` → `"GamepadConnect(#idx \"name\" Type)"`.
  Usage **gameplay** (faire apparaître un joueur), **UI** (mettre à jour le menu).
- **`NkGamepadDisconnectEvent`** (type `NK_GAMEPAD_DISCONNECT`) — ne porte que l'index.
  `ToString()` → `"GamepadDisconnect(#idx)"`. Usage **gameplay** : mettre la partie en pause si la
  manette du joueur saute.
- **`NkGamepadButtonPressEvent` / `NkGamepadButtonReleaseEvent`** (types
  `NK_GAMEPAD_BUTTON_PRESSED`/`RELEASED`, base interne `NkGamepadButtonEvent`) — `GetButton()`,
  `GetState()`, `GetAnalogValue()`. Le press force `NK_PRESSED` (valeur analogique par défaut `1.f`),
  le release force `NK_RELEASED` et **annule** la valeur analogique à `0.f`. Usage **gameplay** :
  déclencher un saut sur le press, relâcher une charge sur le release ; la valeur analogique sert aux
  gâchettes (accélérateur progressif).
- **`NkGamepadAxisEvent`** — `GetAxis()`, `GetValue()`, `GetPrevValue()`, `GetDelta()` (calculé
  `value - prevValue` à la construction), `GetDeadzone()`, et `IsInDeadzone()` (vrai si
  `-deadzone < value < deadzone`). Constante `DEFAULT_DEADZONE = 0.08f`. **Particularité** : c'est le
  seul événement gamepad **sans** macro `NK_EVENT_TYPE_FLAGS` dans le header. Usage **gameplay** :
  déplacement/visée au stick ; le **delta** mesure une impulsion de mouvement, la **deadzone** ignore
  la dérive du repos.
- **`NkGamepadRumbleEvent`** (type `NK_GAMEPAD_RUMBLE`) — **événement sortant** (application →
  backend). Porte les intensités moteur basse/haute (`GetMotorLow/High`), gâchettes
  (`GetTriggerLeft/Right`), durée (`GetDurationMs`). `IsStop()` est vrai quand les quatre intensités
  valent `0.f` ; `durationMs=0` signifie « infini jusqu'au stop ». Usage **gameplay / audio-haptique**
  : retour de tir, d'impact, de texture de route.
- **`NkGamepadBatteryEvent`** — `GetLevel()`, `IsCharging()`, `IsWired()` (niveau négatif),
  `IsCritical()` (niveau dans `[0, 0.1[`). **Piège** : déclaré avec
  `NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_CONNECT)`, il **réutilise le type CONNECT** (bug probable). Usage
  **UI** : afficher une jauge, avertir avant déconnexion imminente.

Chaque événement implémente `Clone()` et `ToString()`. **Attention ownership** : `Clone()` fait
`new NkXxx(*this)` — une **allocation heap CRT** (opérateur `new` global, *pas* NKMemory) ; le
consommateur du `NkEvent*` cloné doit le `delete`. Les données lourdes (`NkGamepadInfo`) sont copiées
par valeur dans l'événement.

### `NkGamepadSnapshot` — l'état complet d'un poll

Struct fournie par le backend à chaque `Poll()` (à ne pas confondre avec `NkGamepadInputState` de
`NkEventState.h`). Champs : `connected`, la `NkGamepadInfo`, les tableaux `buttons[102]` et
`axes[54]`, les capteurs gyro `gyroX/Y/Z` (rad/s) et accéléro `accelX/Y/Z` (m/s²), `batteryLevel`,
`isCharging`. `IsButtonDown(b)` et `GetAxis(a)` sont **bornés** par la capacité (renvoient `false`/
`0.f` hors borne) en `O(1)` ; `Clear()` réinitialise tout via `NkMemSet`.

- **Gameplay / IA** — c'est l'image fidèle de l'entrée à la frame ; on la lit pour piloter un
  personnage ou alimenter une IA de pilotage.
- **Physique / animation** — les capteurs gyro/accéléro nourrissent une visée par mouvement ou une
  pose de manette.
- **UI / 2D** — `batteryLevel`/`isCharging` alimentent un indicateur de batterie.

### `NkIGamepad` — l'interface backend

Interface pure virtuelle (PIMPL) implémentée par chaque plateforme : `Init()`/`Shutdown()`,
`Poll()` (met à jour les snapshots internes mais **n'émet aucun événement** — c'est
`NkGamepadSystem` qui les dérive), `GetConnectedCount()`, `GetSnapshot(idx)`, `Rumble(...)`, et
`GetName()` (ex. `"XInput"`, `"evdev"`). Deux méthodes sont **non pures** avec défaut : `SetLEDColor`
(no-op par défaut ; RGBA `0xRRGGBBAA`) et `HasMotion` (`false` par défaut).

- **Threading / outils** — on **injecte** une implémentation mock de cette interface dans `Init()`
  pour rejouer des entrées scriptées en test, sans matériel.

### `NkGamepadSystem` — le cœur

Possédé par NkSystem (pas un singleton), non-copiable. Le destructeur appelle `Shutdown()` si
`mReady`. Constantes statiques : `BUTTON_COUNT=102`, `AXIS_COUNT=54`,
`EVENT_BUTTON_COUNT = NK_GAMEPAD_BUTTON_MAX`, `EVENT_AXIS_COUNT = NK_GAMEPAD_AXIS_MAX`.

**Cycle de vie.** `Init(backend={})` : si `backend` est nul, une factory crée le backend de la
plateforme ; sinon il prend possession du backend injecté (tests/mock). `Shutdown()` libère.
`IsReady()` = `mReady && backend valide`.

**Pompe.** `PollGamepads()` parcourt chaque slot : `backend->Poll()`, **compare** au
`mPrevSnapshot[idx]`, **émet** `NkGamepadButtonPress/ReleaseEvent` et — au-delà d'un epsilon —
`NkGamepadAxisEvent` via `EventSystem::DispatchEvent()`, met à jour `mPrevSnapshot`, appelle les
callbacks directs, puis met à jour `NkEventState`. Coût `O(slots × (boutons + axes))`. Déclenché
automatiquement par `EventSystem::PollEvents()` si l'auto-poll est actif.

**Callbacks directs.** `SetConnectCallback`/`SetButtonCallback`/`SetAxisCallback` enregistrent des
`NkFunction` appelées pendant le poll, sans transiter par l'EventSystem — pratique pour brancher du
gameplay sans s'abonner à des événements.

**Polling direct** (n'émet rien) : `GetConnectedCount()`, `IsConnected(idx)`, `GetInfo(idx)` (réf
vers un `sDummyInfo` si l'index est invalide), `GetSnapshot(idx)` (état **remappé** ; `sDummySnapshot`
si invalide), `IsButtonDown(idx, btn)` / `IsButtonDownByIndex(idx, i)`, `GetAxis(idx, ax)` /
`GetAxisByIndex(idx, i)`. Pour lire l'état **avant** remapping : `GetRawSnapshot(idx)`,
`IsRawButtonDownByIndex`, `GetRawAxisByIndex` — exactement ce qu'il faut pour un écran d'assignation
de touches.

**Sortie.** `Rumble(idx, motorLow, motorHigh, triggerLeft, triggerRight, durationMs)`,
`RumbleStop(idx)` (= `Rumble(idx,0,0,0,0,0)`), `SetLEDColor(idx, rgba)`.

**Configuration.** `SetDeadzone(d)` valide (NaN → 0.08) et **clampe** dans `[0, 0.95]` ;
`SetAxisEpsilon(e)` valide (NaN → 0.001) et clampe dans `[0, 1]`. Défauts 0.08 et 0.001. La deadzone
filtre la dérive des sticks ; l'epsilon fixe le seuil au-delà duquel un mouvement d'axe **génère** un
événement.

- **Gameplay / IA** — lecture directe par frame (`IsButtonDown`, `GetAxis`) pour piloter ;
  callbacks pour les actions ponctuelles.
- **Threading** — le polling est pensé **mono-thread** (boucle principale) ; ne pas le partager
  entre threads sans synchronisation.

### Le remapping intégré : `NkAxisRemap`, `NkRemapProfile` et les setters

Le système gère, **par slot 0..7**, un `NkRemapProfile { active, buttonMap[102], axisMap[54] }` où
chaque axe est un `NkAxisRemap { logicalAxis, scale, invert }`. Les setters (tous `noexcept`) :
`SetButtonMapByIndex(idx, physBtn, logBtn)` et son équivalent par enum `SetButtonMap` ;
`SetAxisMapByIndex(idx, physAxis, logAxis, invert, scale)` et `SetAxisMap` ;
`DisableButtonByIndex`/`DisableAxisByIndex` ; `ClearMapping(idx)` (un slot) et `ClearAllMappings()`.
`GetMapping(idx)` renvoie le profil (ou `nullptr`). La sentinelle `NK_GAMEPAD_UNMAPPED` (`0xFFFFFFFF`)
et `NK_GP_UNKNOWN` désactivent une entrée. La transformation d'axe applique invert puis scale puis
un **clamp adapté à la cible** (les gâchettes se bornent à `[0,1]`, les sticks à `[-1,1]`).

- **Gameplay** — reconfiguration complète des touches par joueur (gaucher/droitier, préférences).
- **Outils / éditeur** — un écran « appuyez sur le bouton » lit `GetRawSnapshot`, puis appelle
  `SetButtonMapByIndex` pour fixer l'association.

### Persistance des profils — `NkGamepadMappingPersistence.h`

Pour survivre entre deux sessions, un profil se sérialise. Les structs de données :
`NkGamepadButtonMapEntry { physicalButton, logicalButton }`,
`NkGamepadAxisMapEntry { physicalAxis, logicalAxis, scale, invert }` (ordre des transformations :
invert → scale → clamp), `NkGamepadMappingSlotData { slotIndex, active, buttons, axes }` et
`NkGamepadMappingProfileData { version=1, backendName, slots }`. Côté système :
`ExportMappingProfile()` produit cette structure, `ImportMappingProfile(profile, clearExisting,
outError)` l'applique, et `Save/LoadMappingProfile(userId, …)` passent par la persistance branchée
(`SetMappingPersistence`, `GetMappingPersistence`).

L'interface `NkIGamepadMappingPersistence` (abstraite) déclare `GetFormatName()`, `Save(userId,
profile, outError)`, `Load(userId, outProfile, outError)` — `outError` est partout **optionnel**
(`nullptr` accepté). L'implémentation par défaut `NkTextGamepadMappingPersistence` écrit un format
texte `.nkmap` (en-tête `nkmap <version>` / `backend <nom>`, blocs `slot…end_slot`, lignes
`button`/`axis`) ; `GetFormatName()` renvoie `"text/nkmap"`. Son constructeur prend un répertoire de
base et une extension (`.nkmap` par défaut) ; `SetBaseDirectory`/`GetBaseDirectory` ajustent l'un. Les
statics `ResolveDefaultBaseDirectory()`, `ResolveCurrentUserId()` et surtout **`SanitizeUserId(raw)`**
(anti-injection de chemin ; vide → `"default"`) sécurisent la construction du chemin.

- **IO / outils** — sauvegarde des préférences de touches par utilisateur, rechargées au lancement.

> **Piège doc.** L'exemple en commentaire du header utilise `std::make_unique` / `std::unique_ptr` —
> c'est **fictif** : le vrai système emploie `memory::NkUniquePtr`.

### La couche HID générique — `NkGenericHidEvent.h`

Pour le matériel qui n'est pas une manette standard. `NkHidUsagePage : uint16` énumère les pages
d'usage USB HID (`NK_HID_PAGE_UNDEFINED=0x00`, `GENERIC=0x01`, `SIMULATION`, `VR`, `SPORT`, `GAME`,
`KEYBOARD`, `LED`, `BUTTON`, `DIGITIZER`, `CONSUMER`, `SENSOR=0x20`, et la plage vendeur
`VENDOR_FIRST=0xFF00`…`VENDOR_LAST=0xFFFF`). `NkHidDeviceInfo` décrit un périphérique : `deviceId`,
`name[128]`, `path[256]`, `vendorId`/`productId`, `usagePage`/`usageId`, compteurs
`numButtons`/`numAxes`/`numHats`, `isWireless` (ctor `NkMemSet` sur `name` et `path`).

Les événements dérivent de `NkGenericHidEvent` (cat. `NK_CAT_GENERIC_HID`), porteur de
`GetDeviceId()` :

- **`NkHidConnectEvent`** (type `NK_GENERIC_CONNECT`) — copie la `NkHidDeviceInfo`, `GetInfo()`.
- **`NkHidDisconnectEvent`** (type `NK_GENERIC_DISCONNECT`) — `deviceId` seul.
- **`NkHidButtonPressEvent` / `NkHidButtonReleaseEvent`** (types
  `NK_GENERIC_BUTTON_PRESSED`/`RELEASED`, base interne `NkHidButtonEvent`) — `GetButtonIndex()`,
  `GetState()`, `GetUsagePage()`, `GetUsageId()`.
- **`NkHidAxisEvent`** (type `NK_GENERIC_AXIS_MOTION`) — `GetAxisIndex()`, `GetValue()`,
  `GetPrevValue()`, `GetDelta()` (`value - prevValue`), `GetRawValue()` (l'entier d'origine,
  `int32`), `GetUsagePage()`, `GetUsageId()`.
- **`NkHidRawInputEvent`** — le **rapport HID brut** : `GetReportId()`, `GetData()` (le
  `NkVector<uint8>`), `GetBytes()` (= `mData.Data()`), `GetSize()`. **Deux particularités** : il
  réutilise `NK_EVENT_TYPE_FLAGS(NK_GENERIC_BUTTON_PRESSED)` (bug probable, pas de type RAW dédié), et
  son constructeur est le **seul non-`noexcept`** du module (il prend le `NkVector<uint8>` par valeur
  puis `traits::NkMove` — transfert, sans copie redondante).

- **IO / matériel** — décoder un volant, un palonnier, un panneau de simulateur : on lit le rapport
  brut (`GetBytes`/`GetSize`) et on en extrait les axes selon la fiche du périphérique.
- **Gameplay** — une fois décodés, ces axes/boutons pilotent un véhicule ou un avion.

### `NkGenericHidMapper` — le remapping HID

Mapper **header-only**, **non thread-safe** (synchronisez à l'extérieur). Il stocke ses associations
dans un `NkUnorderedMap<uint64, DeviceMapping>` indexé par `deviceId`. Philosophie : un mapping non
configuré renvoie l'**identité** (le bouton renvoie son propre index, l'axe est simplement borné).

- `Clear()` / `ClearDevice(deviceId)` — vider tout / un périphérique (`noexcept`).
- `SetButtonMap(deviceId, physBtn, logBtn)` — associe (crée le device si absent ; **non**-`noexcept`,
  peut allouer) ; `DisableButton(...)` pose `NK_HID_UNMAPPED`.
- `SetAxisMap(deviceId, physAxis, logAxis, invert, scale, deadzone, offset)` — construit un
  `NkHidAxisBinding` ; `DisableAxis(...)` pose un binding désactivé.
- `ResolveButton(deviceId, physBtn)` `O(1)` moyen, `noexcept` — device/bouton absent → renvoie
  `physBtn` (identité) ; sinon la valeur mappée (peut être `NK_HID_UNMAPPED`).
- `ResolveAxis(deviceId, physAxis, raw, outLog, outVal)` `O(1)` moyen, `noexcept` — absent → identité
  + clamp `[-1,1]` (retourne `true`) ; binding désactivé → `false` ; sinon applique
  **invert → scale → offset → deadzone → clamp** (retourne `true`).
- `MapButtonEvent(ev, outLog, outState)` / `MapAxisEvent(ev, outLog, outVal)` — résolvent directement
  depuis un événement HID (`false` si le bouton est `NK_HID_UNMAPPED`).

Le `NkHidAxisBinding { logicalAxis, scale, offset, deadzone, invert }` porte ces paramètres. Le mapper
n'utilise que les helpers maison `math::NkClamp` et `math::NkFabs` (le header inclut `<algorithm>` et
`<cmath>` mais le code ne s'en sert pas).

- **IO / matériel** — normaliser une plage brute (`rawValue`) en `[-1,1]`, calibrer une zone morte
  par périphérique.
- **Gameplay** — réassigner les axes d'un volant exotique vers les axes logiques du jeu.

### Idiomes et pièges transverses

- **Ownership des événements.** `Clone()` = `new` (heap CRT global, **pas** NKMemory) ; le dispatcher
  doit `delete`. Les données lourdes (`NkVector<uint8>`, `NkGamepadInfo`, `NkHidDeviceInfo`) sont
  copiées par valeur dans l'événement.
- **Deux univers de mapping.** (a) remap intégré au `NkGamepadSystem` (par slot 0..7, capacités
  102/54) ; (b) `NkGenericHidMapper` autonome (par `deviceId`). Sentinelles aux noms distincts
  (`NK_GAMEPAD_UNMAPPED`, `NK_GAMEPAD_MAPPING_UNMAPPED`, `NK_HID_UNMAPPED`) mais toutes égales à
  `0xFFFFFFFF`.
- **Types d'événement réutilisés.** `NkGamepadBatteryEvent` réutilise `NK_GAMEPAD_CONNECT` et
  `NkHidRawInputEvent` réutilise `NK_GENERIC_BUTTON_PRESSED` — bugs probables, documentés tels quels.
- **`NkGamepadAxisEvent`** est le seul événement gamepad **sans** `NK_EVENT_TYPE_FLAGS`.
- **Pas de singleton.** `NkGamepadSystem` s'obtient par `NkGamepads()` (déclaré dans `NkSystem.h`).
- **Threading.** `NkGenericHidMapper` explicitement non thread-safe ; le polling gamepad est
  mono-thread (boucle principale).

---

### Exemple récapitulatif

```cpp
#include "NKWindow/NkSystem.h"      // pour NkGamepads()
using namespace nkentseu;

auto& gp = NkGamepads();

// 1) Réagir aux connexions + actions ponctuelles via callbacks directs.
gp.SetConnectCallback([](const NkGamepadInfo& info, bool connected) {
    NkLog("Manette %s : %s", NkGamepadTypeToString(info.type),
          connected ? "branchee" : "debranchee");
});
gp.SetButtonCallback([](uint32 idx, NkGamepadButton b, NkButtonState s) {
    if (b == NK_GP_GUIDE && s == NK_PRESSED) ToggleMenu(idx);
});

// 2) Reconfiguration par joueur : gaucher, sensibilite doublee.
gp.SetAxisMapByIndex(0, /*phys*/0, NK_GP_AXIS_LX, /*invert*/true, /*scale*/2.f);
gp.SaveMappingProfile("rihen");         // persiste sur disque (.nkmap)

// 3) Boucle de jeu : lecture directe de l'etat courant.
if (gp.IsConnected(0)) {
    float32 lx = gp.GetAxis(0, NK_GP_AXIS_LX);   // [-1,1], deadzone appliquee
    Move(lx);
    if (gp.IsButtonDown(0, NK_GP_SOUTH)) Jump();
}

// 4) Retour haptique sur impact, puis arret.
gp.Rumble(0, /*low*/0.8f, /*high*/0.4f, 0.f, 0.f, /*ms*/150);
// ... plus tard : gp.RumbleStop(0);

// 5) Materiel hors norme (volant) : remapping HID generique par deviceId.
NkGenericHidMapper hid;
hid.SetAxisMap(wheelId, /*phys*/0, /*log*/AXIS_STEER, false, 1.f, /*deadzone*/0.02f);
uint32 outAxis; float32 outVal;
if (hid.MapAxisEvent(hidAxisEv, outAxis, outVal)) ApplySteering(outVal);
```

---

[← Index NKEvent](README.md) · [Récap NKEvent](../NKEvent.md) · [Couche Runtime](../README.md)
