# Le point d'entrée portable

> Couche **Runtime** · NKWindow · Écrire **une seule** fonction, `nkmain`, et laisser le moteur
> fabriquer le vrai `main`/`WinMain` adapté à chaque plateforme — args UTF-8, mémoire, fenêtrage et
> configuration d'application déjà en place.

Toute application native a un point d'entrée imposé par sa plateforme : `WinMain` sur Windows desktop,
`main` sur Linux, une `UIApplicationDelegate` sur iOS, une boucle `android_native_app_glue` sur
Android, une *core window* sur UWP… Écrire ce code à la main pour chaque cible, c'est dupliquer le
parsing des arguments, l'init mémoire, l'allocation d'une console de debug et le branchement du
fenêtrage — autant de chances de se tromper. Le problème n'est donc pas « comment écrire un `main` »,
mais « comment écrire **zéro** `main` et obtenir le bon pour chaque OS ».

NKWindow répond par une **inversion d'entrée**, exactement dans l'esprit de SDL ou de SFML : vous
n'écrivez plus le point d'entrée natif, vous implémentez une fonction unique et portable.

```cpp
int nkmain(const nkentseu::NkEntryState& state);
```

Le moteur, lui, fournit le vrai entrypoint natif — il parse les arguments, initialise le runtime,
construit l'état de démarrage, appelle votre `nkmain`, puis nettoie. Vous écrivez du code identique
sur les huit plateformes ; le `#include` choisit la bonne implémentation tout seul.

- **Namespace** : `nkentseu` (tout le code ; `WinMain` et le prototype `nkmain` sont, eux, globaux)
- **Header parapluie** : `#include "NKWindow/NKMain.h"` — à inclure **une seule fois**, dans le
  `.cpp` qui implémente `nkmain`

---

## Comment ça marche : du `WinMain` à votre `nkmain`

`NKMain.h` est un parapluie minuscule : il inclut `Core/NkMain.h` et `Core/NkEntry.h`, rien de plus.
`Core/NkEntry.h` apporte tout le mécanisme portable (l'état, les helpers de cycle de vie, les
macros) ; `Core/NkMain.h` ne déclare **aucun symbole** : c'est un **sélecteur de plateforme** qui,
selon les macros de `NKPlatform/NkPlatformDetect.h`, inclut le bon entrypoint natif.

L'ordre de sélection est fixe : UWP → Xbox → **Windows desktop** → macOS → iOS → forçage *no-op* →
Wayland → XCB → XLib → Android → Emscripten → HarmonyOS → sinon *no-op*. Le header retenu (par
exemple `NkWindowsDesktop.h`) est celui qui **définit** le `WinMain` natif et la variable globale
`gState`. Tous suivent le **même patron** :

1. parser les arguments CLI en un `NkVector<NkString>` **UTF-8** ;
2. initialiser le runtime avec `NkEntryRuntimeInit` (mémoire + AppData + fenêtrage) ;
3. construire un `NkEntryState` (avec les handles natifs de la plateforme) ;
4. appliquer le nom d'application puis publier le pointeur global `gState` ;
5. appeler **votre** `nkmain(state)` ;
6. remettre `gState = nullptr` et appeler `NkEntryRuntimeShutdown` ;
7. retourner le code rendu par `nkmain`.

Ce n'est **pas** un simple `#define main nkmain` : il y a un vrai état de démarrage, une vraie
séquence d'init/shutdown encadrant votre fonction, et un point de configuration (l'AppData) injecté
*avant* que vous ne preniez la main.

> **En résumé.** Implémentez `int nkmain(const NkEntryState&)`, incluez `NKWindow/NKMain.h` dans **un
> seul** `.cpp`, n'écrivez jamais `main`/`WinMain`. Le sélecteur `NkMain.h` choisit l'entrypoint
> natif ; celui-ci parse les args, initialise mémoire+fenêtrage, vous appelle, puis nettoie.

---

## L'état de démarrage : `NkEntryState`

Quand le moteur appelle `nkmain`, il vous passe un `NkEntryState` : le conteneur des arguments et des
**handles natifs** de démarrage. Sa partie commune est portable — `appName` et `args`, relus par
`GetAppName()` et `GetArgs()` — mais sa partie native est **conditionnelle** : sur Windows desktop il
porte `hInstance`/`lpCmdLine`/`nCmdShow`, sur XCB une `xcb_connection_t*`, sur XLib un `Display*`, sur
Android un `android_app*`, sur UWP la *core window*… Un seul bloc est compilé, celui de la plateforme
courante.

```cpp
int nkmain(const nkentseu::NkEntryState& state) {
    for (const auto& arg : state.GetArgs())   // index 0 = chemin de l'exécutable
        NK_LOG("arg: {}", arg);
    NK_LOG("app: {}", state.GetAppName());
    // sur Windows uniquement, state.hInstance est disponible ici
    return 0;
}
```

Le `state` vous est donné par référence `const` et vit sur la **pile de l'entrypoint** : il n'est
valide que pendant l'appel à `nkmain`. Ce n'est **pas** un objet à conserver — ne capturez pas son
adresse pour plus tard. La même règle vaut pour la globale `gState` qui y pointe.

> **En résumé.** `NkEntryState` = `appName` + `args` (UTF-8, index 0 = l'exécutable) en commun, plus
> les handles natifs propres à la plateforme. Lecture via `GetArgs()`/`GetAppName()`. Durée de vie =
> celle de `nkmain` ; ne le gardez pas au-delà.

---

## Configurer l'application avant l'entrée

Le runtime applique une **AppData** (`NkAppData`) *avant* d'appeler `nkmain` : nom et version de
l'app, debug du renderer, log des événements, multi-fenêtre. Le souci, c'est que cette config doit
être posée **avant** votre code — au moment où l'entrypoint appelle `NkEntryRuntimeInit`. NKWindow
propose pour cela deux idiomes, tous deux à placer dans le `.cpp` de `nkmain`.

La voie **callback** enregistre une fonction qui mute l'AppData :

```cpp
static void Configure(nkentseu::NkAppData& d) {
    d.appName = "MyApp";
    d.enableEventLogging = true;
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(Configure);   // enregistré à l'init statique
```

La voie **valeur** remplit un `NkAppData` puis le fige :

```cpp
nkentseu::NkAppData appData{};
appData.appName = "MyApp";
NKENTSEU_DEFINE_APP_DATA(appData);              // (alias : NKENTSEU_APP_DATA_DEFINED)
```

La construction finale (`NkBuildEntryAppData`) suit un ordre de priorité bien défini : l'**override**
explicite d'abord, sinon une AppData neuve ; puis, si le nom est vide, le **`defaultAppName`**
(`NK_APP_NAME`, par défaut `"windows_app"` côté Win32) ; et enfin l'**updater**, qui a toujours le
dernier mot.

> **En résumé.** L'AppData est posée avant `nkmain`. Deux idiomes : `NK_REGISTER_ENTRY_APPDATA_UPDATER`
> (callback) ou `NKENTSEU_DEFINE_APP_DATA` (valeur). Priorité : override < `defaultAppName` < updater.

---

## Aperçu de l'API

Tous les symboles publics du mécanisme d'entrée, regroupés par rôle. Sauf mention, ils vivent dans le
namespace `nkentseu`.

### Le contrat utilisateur

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Entrée | `int nkmain(const NkEntryState&)` (global) | **Votre** point d'entrée portable, à implémenter. |
| Parapluie | `#include "NKWindow/NKMain.h"` | Inclut `NkMain.h` + `NkEntry.h` ; à mettre dans **un** TU. |
| Sélecteur | `Core/NkMain.h` | Choisit l'entrypoint natif par macro ; n'expose aucun symbole. |

### `NkEntryState` — l'état de démarrage

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données communes | `appName`, `args` | Nom d'app + arguments CLI (UTF-8). |
| Accès | `GetArgs()` const, `GetAppName()` const | Lecture des deux champs communs. |
| Natif (Windows) | `hInstance`, `hPrevInstance`, `lpCmdLine`, `nCmdShow` | Handles Win32 du démarrage. |
| Natif (autres) | `uwpCoreWindow` / `xboxNativeWindow` / `connection`+`screen` / `display` / `androidApp` | Handles propres à UWP/Xbox/XCB/XLib/Android. |
| Construction | constructeurs par plateforme (déplacent `args`) | Un bloc actif selon la macro de plateforme. |
| Global | `extern NkEntryState* gState` | Pointe sur le `state` courant **pendant** `nkmain` seulement. |

### Configuration d'AppData (helpers de cycle de vie, `NkEntry.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `NkEntryAppDataUpdater` (alias de `void(*)(NkAppData&)`) | Signature d'un callback de config. |
| Auto-registre | `struct NkEntryAppDataAutoRegister(updater)` | Enregistre l'updater dès sa construction. |
| Setters | `NkSetEntryAppDataUpdater`, `NkRegisterEntryAppDataUpdater` (alias) | Pose le callback de config. |
| Setters | `NkSetEntryAppData(const NkAppData&)`, `NkClearEntryAppDataOverride()` | Pose / efface l'override explicite. |
| Construction | `NkBuildEntryAppData(defaultAppName)` | Assemble l'AppData finale (override < default < updater). |
| Runtime | `NkGetEntryRuntimeAppData()`, `NkApplyEntryAppName(state, fallback)` | AppData appliquée ; pose `state.appName`. |
| Slots internes | `NkEntryAppDataUpdaterSlot`, `…OverrideSlot`, `NkEntryHasAppDataOverrideSlot`, `NkEntryRuntimeAppDataSlot` | Singletons Meyers `inline` (état des helpers). |
| Init/Shutdown | `NkEntryRuntimeInit(defaultAppName)`, `NkEntryRuntimeShutdown(reportLeaks)` | Séquences appelées par l'entrypoint. |
| Plateforme | `NkUWP*` / `NkXbox*` `IsReady`/`GetHandle`/`PumpSystemEvents` | API runtime gardée par macro (UWP/Xbox). |

### Macros (`NkEntry.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Config callback | `NK_REGISTER_ENTRY_APPDATA_UPDATER(fn)` | Enregistre `fn` (`void(NkAppData&)`) comme updater. |
| Config valeur | `NKENTSEU_DEFINE_APP_DATA(data)`, `NKENTSEU_APP_DATA_DEFINED(data)` (alias) | Fige un `NkAppData` rempli. |
| Nom d'app | `NK_APP_NAME` | Nom par défaut (Win32 : `"windows_app"`), surchargeable. |
| Interne | `NKENTSEU_INTERNAL_CONCAT_`, `…_IMPL_` | Concaténation de jetons (usage interne). |

### Dépendances directes (`Core/NkWESystem.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Config | `struct NkAppData` | Paramètres d'application appliqués au runtime. |
| Cycle de vie | `NkInitialise(const NkAppData&)`, `NkClose()` | Init / fermeture du système fenêtrage+événements. |
| Accès systèmes | `NkEvents()`, `NkGamepads()` | Alias vers les systèmes d'événements / gamepads. |

---

## Référence complète

### `nkmain` — votre point d'entrée

C'est le **seul** code que vous écrivez côté entrée. Prototype global déclaré en fin de `NkEntry.h`,
il reçoit l'état de démarrage et retourne un code de sortie (transmis tel quel par l'entrypoint
natif). Tout ce qui précède (mémoire, AppData, fenêtrage, console de debug) est déjà en place quand il
est appelé ; tout ce qui suit (fermeture, rapport de fuites) est géré après son retour. Selon le
domaine, c'est là que démarre :

- **Gameplay / boucle** — création de la fenêtre, de la scène, et entrée dans la *game loop* ;
  `nkmain` est le sommet de la pile d'appels de toute l'application.
- **Outils / éditeur** — un outil en ligne de commande lit `state.GetArgs()` pour ses options (chemin
  de projet, mode batch) sans réécrire de parser ; index 0 = chemin de l'exécutable.
- **Tests / sandbox** — un `nkmain` minimal qui sélectionne une démo selon un argument est le patron
  des bancs d'essai (cf. les démos Sandbox du moteur).

### `NkEntryState` — état et handles de démarrage

Le conteneur portable des informations de lancement. Sa partie **commune** — `appName` et `args` —
est lue par `GetArgs()` (le vecteur d'arguments, UTF-8, index 0 = l'exécutable) et `GetAppName()` (le
nom résolu de l'app). Sa partie **native** est conditionnelle : un seul bloc `#if` est compilé.

- **Windows desktop** — `hInstance`, `hPrevInstance`, `lpCmdLine`, `nCmdShow` : ce que `WinMain`
  reçoit, indispensable pour enregistrer une classe de fenêtre Win32 ou contrôler l'affichage initial.
- **GPU / fenêtrage Linux** — `connection`+`screen` (XCB) ou `display` (XLib) sont les handles serveur
  X qu'un backend de rendu ou de fenêtre devra réutiliser pour créer sa surface.
- **Mobile** — `androidApp` (l'`android_app*` de `native_app_glue`) porte l'état d'activité, la file
  d'événements et la surface ; tout le cycle de vie Android en dépend. Sur UWP/Xbox, c'est la *core
  window* / la fenêtre native équivalente.

Les **constructeurs** sont propres à chaque plateforme mais partagent un comportement : ils
**déplacent** les arguments dans `args` (`traits::NkMove`), y compris depuis un paramètre `const&`,
pour éviter une copie du vecteur de chaînes. C'est l'entrypoint qui les appelle — vous n'avez en
principe jamais à construire un `NkEntryState` vous-même.

La **durée de vie** est le point sensible : l'objet vit sur la pile de l'entrypoint et n'existe que le
temps de `nkmain`. Le retenir au-delà (stocker `&state` dans un singleton) est un bug d'*use after
free*.

### `gState` — le pointeur global du démarrage

`extern NkEntryState* gState;` est déclaré dans `NkEntry.h` et **défini** (`= nullptr`) dans chaque
entrypoint natif (par exemple `NkWindowsDesktop.h`). Pendant `nkmain`, il pointe sur le `state`
courant ; juste avant et après, il vaut `nullptr`. Il rend l'état de démarrage accessible depuis du
code qui n'a pas reçu le `state` en paramètre — typiquement, un sous-système (audio, réseau, fenêtre)
qui a besoin d'un handle natif et que `nkmain` n'a pas câblé explicitement. Même contrainte de durée
de vie que `NkEntryState` : ne le lisez que pendant `nkmain`, jamais en dehors.

### `NkAppData` — la configuration d'application

La structure de paramètres appliquée au runtime avant `nkmain`. Ses champs et leurs défauts :
`enableRendererDebug` (`false`), `enableEventLogging` (`false`), `appName` (`"NkApp"`), `appVersion`
(`"1.0.0"`), `enableMultiWindow` (`true`), `userData` (`nullptr`, votre pointeur libre). Selon le
domaine :

- **Rendu / GPU** — `enableRendererDebug` active les couches de validation et les messages debug du
  backend (utile en développement, à couper en release).
- **IO / événements** — `enableEventLogging` trace les événements fenêtre/entrée ; un interrupteur
  précieux pour diagnostiquer un souci de focus, de resize ou de saisie.
- **UI / outils** — `appName`/`appVersion` alimentent le titre de fenêtre et les métadonnées ;
  `enableMultiWindow` autorise plusieurs fenêtres (éditeur multi-vues) ; `userData` transporte un
  contexte applicatif arbitraire jusque dans le système.

### `NkBuildEntryAppData` — la stratégie de résolution

Le cœur de la configuration. Il part de l'**override** explicite s'il en existe un (posé par
`NkSetEntryAppData`), sinon d'un `NkAppData{}` neuf. Puis, si le nom est encore vide (pas d'override,
ou nom vide) **et** qu'un `defaultAppName` non vide est fourni, il l'adopte comme `appName`. Enfin, si
un **updater** est enregistré, il l'invoque sur la donnée — l'updater a donc toujours le dernier mot.
Le résultat est rendu par valeur. Cette priorité **override < defaultAppName < updater** est ce qui
permet aux deux idiomes de config (valeur et callback) de coexister sans surprise.

### Les setters et slots de config

`NkSetEntryAppDataUpdater` pose le callback ; `NkRegisterEntryAppDataUpdater` en est un alias lisible.
`NkSetEntryAppData(data)` copie une AppData complète dans le slot d'override et lève le drapeau
« override présent » ; `NkClearEntryAppDataOverride()` rabaisse ce drapeau. Les quatre **slots**
(`NkEntryAppDataUpdaterSlot`, `…OverrideSlot`, `NkEntryHasAppDataOverrideSlot`,
`NkEntryRuntimeAppDataSlot`) sont des singletons Meyers `inline` qui portent l'état interne — vous
n'avez normalement pas à y toucher, ce sont les fondations sur lesquelles s'appuient les setters et
`NkBuildEntryAppData`.

### `NkEntryAppDataAutoRegister` et les macros de config

`NkEntryAppDataAutoRegister(updater)` enregistre l'updater **dès sa construction** (en appelant
`NkSetEntryAppDataUpdater`). L'idiome est d'en créer une instance **statique globale** dans le `.cpp`
de `nkmain` : la construction a lieu pendant l'init statique du TU, donc **avant** l'entrée, ce qui
garantit que la config est posée à temps. Les macros automatisent ce branchement :

- `NK_REGISTER_ENTRY_APPDATA_UPDATER(fn)` crée, dans un namespace anonyme, un
  `NkEntryAppDataAutoRegister` au nom unique (suffixé par `__LINE__`) initialisé avec `fn`. C'est la
  voie **callback** : `fn` (signature `void(NkAppData&)`) devient l'updater.
- `NKENTSEU_DEFINE_APP_DATA(appData)` (alias `NKENTSEU_APP_DATA_DEFINED`) prend un `NkAppData` déjà
  rempli : il génère une fonction statique qui assigne cette valeur, puis l'enregistre via la macro
  précédente. C'est la voie **valeur**, plus directe quand on veut juste figer une structure remplie.

Les macros `NKENTSEU_INTERNAL_CONCAT_` / `…_IMPL_` ne servent qu'à fabriquer ces noms uniques ; elles
sont protégées par `#ifndef` et n'ont pas d'usage applicatif.

### `NkEntryRuntimeInit` / `NkEntryRuntimeShutdown` — encadrer `nkmain`

Ces deux fonctions sont appelées par l'entrypoint natif, **autour** de votre `nkmain` — vous ne les
appelez pas vous-même. `NkEntryRuntimeInit(defaultAppName)` enchaîne : (1) initialiser
`NkMemorySystem` (toute la mémoire du moteur passe par NKMemory) ; (2) construire l'AppData via
`NkBuildEntryAppData` et la stocker comme AppData runtime ; (3) appeler `NkInitialise` (fenêtrage +
événements). En cas d'échec de `NkInitialise`, il **annule proprement** : réinitialise l'AppData
runtime, ferme `NkMemorySystem` (avec rapport de fuites), et retourne `false` — l'entrypoint Win32
fait alors `return -1`.

`NkEntryRuntimeShutdown(reportLeaks = true)` déroule la séquence inverse, **idempotente** : `NkClose()`
(ferme fenêtres et événements), `NkMemorySystem::Shutdown(reportLeaks)` (vide la mémoire + GC, et
**rapporte les fuites** par défaut), puis réinitialise l'AppData runtime. La conséquence pratique pour
vous : **ne réinitialisez ni ne fermez jamais `NkMemorySystem` à la main** dans `nkmain` — c'est déjà
géré, et le faire fausserait le rapport de fuites ou provoquerait un double-shutdown.

`NkGetEntryRuntimeAppData()` rend l'AppData effectivement appliquée (utile pour relire la config
finale), et `NkApplyEntryAppName(state, fallback)` fixe `state.appName` à partir d'elle (ou du
`fallback` si elle est vide) — c'est ce que fait l'entrypoint juste avant de publier `gState`.

### L'entrypoint Windows desktop (variante de référence)

`NkWindowsDesktop.h` est inclus quand `NKENTSEU_PLATFORM_WINDOWS` est défini. Il **définit** la
globale `gState` et la fonction `int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)`. Son déroulé,
identique en esprit sur toutes les plateformes :

1. en `_DEBUG` (ou si `NKENTSEU_DEBUG_CONSOLE`), `AllocConsole()` et redirection de stdout/stderr vers
   `CONOUT$` — d'où la console qui apparaît seulement en build debug ;
2. récupération des arguments via `CommandLineToArgvW`, conversion UTF-16 → **UTF-8** par
   `WideCharToMultiByte`, et remplissage de `args` (déplacement, après `Reserve`) ; `wargv` libéré par
   `LocalFree` ;
3. `NkEntryRuntimeInit(NK_APP_NAME)` ; en cas d'échec, `return -1` ;
4. construction du `NkEntryState(hInstance, …, args)`, `NkApplyEntryAppName`, puis `gState = &state` ;
5. `int result = nkmain(state);` ;
6. `gState = nullptr;` puis `NkEntryRuntimeShutdown(true);` ;
7. fermeture de la console (mêmes gardes debug), `return result`.

Les autres plateformes (Cocoa, Android, UIKit, Wayland, XCB, XLib, Emscripten, HarmonyOS, UWP, Xbox,
*no-op*) suivent **le même patron** avec leur entrypoint natif et leur bloc `NkEntryState`
correspondant.

### Dépendances : `NkInitialise`, `NkClose`, `NkEvents`, `NkGamepads`

Ces fonctions `inline` de `NkWESystem.h` sont ce que pilotent les séquences d'init/shutdown.
`NkInitialise(const NkAppData& = {})` démarre le système fenêtrage+événements (`NkWESystem`) et rend
un `bool` de succès ; `NkClose()` le ferme. `NkEvents()` et `NkGamepads()` donnent accès, depuis
n'importe où dans `nkmain`, au système d'événements et au système de gamepads — c'est par eux que la
boucle de jeu interroge l'entrée (cf. le polling `NkEvents().PollEvent()`). `NkWESystem` lui-même (le
propriétaire global du registre de fenêtres et des systèmes d'entrée) dépasse le strict sujet du point
d'entrée, mais c'est ce que `NkInitialise`/`NkClose` mettent en route et arrêtent.

### Pièges à connaître

- **Un seul TU d'entrée.** `NKMain.h` / `NkMain.h` **définit** `WinMain` et `gState`. L'inclure dans
  deux `.cpp` provoque des symboles multiples au link. Mettez-le dans le seul fichier de `nkmain`.
- **Durée de vie de `gState` et `NkEntryState`.** Valides uniquement pendant `nkmain` ; ne capturez
  pas leur adresse au-delà.
- **Ne touchez pas à `NkMemorySystem`.** Son init/shutdown encadre déjà `nkmain` ; le rapport de
  fuites est activé par défaut au shutdown.
- **Args toujours UTF-8**, index 0 = chemin de l'exécutable (convention `CommandLineToArgvW`).
- **Console debug** allouée seulement en `_DEBUG` ou si `NKENTSEU_DEBUG_CONSOLE` est défini.
- **Priorité de config** : override < `defaultAppName`/`NK_APP_NAME` < updater (l'updater gagne).

---

### Exemple récapitulatif

```cpp
// main.cpp — le SEUL fichier qui inclut NKMain.h
#include "NKWindow/NKMain.h"
using namespace nkentseu;

// (1) Configurer l'app AVANT l'entrée — voie callback.
static void Configure(NkAppData& d) {
    d.appName            = "MyGame";
    d.appVersion         = "1.0.0";
    d.enableEventLogging = true;     // tracer les événements en dev
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(Configure);

// (2) Le point d'entrée portable : pas de main/WinMain.
int nkmain(const NkEntryState& state) {
    // Arguments (UTF-8, index 0 = exécutable).
    for (const auto& arg : state.GetArgs())
        NK_LOG("arg: {}", arg);

    // gState vaut &state ici, et nulle part ailleurs.
    // Mémoire + fenêtrage déjà initialisés par le runtime.

    // ... création de fenêtre, boucle de jeu via NkEvents().PollEvent() ...

    return 0;   // code de sortie ; shutdown géré après le retour
}
```

```cpp
// Variante config par VALEUR (au lieu de la voie callback) :
NkAppData appData{};
appData.appName = "MyGame";
NKENTSEU_DEFINE_APP_DATA(appData);
```

---

[← Index NKWindow](README.md) · [Récap NKWindow](../NKWindow.md) · [Couche Runtime](../README.md)
