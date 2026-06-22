# NKWindow — créer une fenêtre, le point d'entrée et la boucle principale

> Guide 2 de la série Nkentseu (style SFML). On part de zéro : on ouvre une fenêtre,
> on écrit le point d'entrée portable, on tourne dans la boucle principale, et on apprend
> à piloter la fenêtre (titre, taille, plein écran, orientation, multi-écran).
> Langue : français. Prérequis conseillé : [NKMemory](01-NKMemory.md).
> Suite : [NKEvent](03-NKEvent.md) pour les entrées, puis [NKCanvas](05-NKCanvas.md) pour dessiner.
> Retour au sommaire : [README.md](README.md).

---

## Introduction

**NKWindow** est le module *Runtime* qui crée et gère les **fenêtres natives** de Nkentseu,
sur toutes les plateformes : Windows, Linux (XLib / XCB / Wayland), macOS, iOS, Android,
Web (Emscripten), HarmonyOS, UWP, Xbox. C'est la première brique visible : avant de dessiner
quoi que ce soit, il faut une fenêtre (ou une *surface* sur mobile/web).

NKWindow couvre trois choses :

1. **Le point d'entrée portable** — tu écris `nkmain(...)`, pas `main()`. Le moteur fournit
   le vrai `main`/`WinMain`/`android_main`/`emscripten` selon la cible.
2. **La fenêtre** — la classe `NkWindow`, configurée par une `NkWindowConfig`.
3. **La boucle principale** — `IsOpen()` + le pompage des événements (qui appartient au module
   [NKEvent](03-NKEvent.md), accessible via `NkEvents()`).

> NKWindow ne dessine rien. Il vous donne une fenêtre et une *surface* graphique
> (`GetSurfaceDesc()`). Le rendu 2D se fait avec [NKCanvas](05-NKCanvas.md), qui consomme une
> `NkWindow`.

Toute l'API publique vit dans le namespace `nkentseu`. L'inclusion de référence est :

```cpp
#include "NKWindow/NKWindow.h"   // NkWindow, NkWindowConfig, NkEvents(), systèmes…
#include "NKWindow/NKMain.h"     // point d'entrée portable + prototype de nkmain()
```

---

## 1. Le point d'entrée : `nkmain` + `NKENTSEU_DEFINE_APP_DATA`

Au lieu d'un `main()` classique, tu implémentes :

```cpp
int nkmain(const nkentseu::NkEntryState& state);
```

Le moteur génère le vrai point d'entrée natif (déclenché par l'inclusion de `NKMain.h`),
initialise la mémoire et le runtime fenêtre/événements, puis appelle ton `nkmain`. Le
paramètre `state` (de type `NkEntryState`) contient :

- `state.GetAppName()` — le nom de l'application ;
- `state.GetArgs()` — les arguments de la ligne de commande, sous forme de `NkVector<NkString>` ;
- des handles natifs optionnels selon la plateforme (HINSTANCE sous Windows, `android_app*`
  sous Android, `Display*` sous XLib…), normalement inutiles dans le code applicatif courant.

### Déclarer les métadonnées de l'app

Le runtime lit un `NkAppData` au démarrage (nom, version, options de debug). On le déclare une
seule fois, dans le même `.cpp` que `nkmain`, avec la macro `NKENTSEU_DEFINE_APP_DATA(...)` :

```cpp
struct NkAppData {
    bool      enableRendererDebug = false;
    bool      enableEventLogging  = false;   // trace les événements dans le log
    NkString  appName             = "NkApp";
    NkString  appVersion          = "1.0.0";
    bool      enableMultiWindow   = true;     // autorise plusieurs fenêtres
    void*     userData            = nullptr;
};
```

Le squelette minimal complet d'une application Nkentseu :

```cpp
// main.cpp
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"

using namespace nkentseu;

// Métadonnées de l'app — lues par le runtime AVANT nkmain().
// On passe une expression qui retourne un NkAppData (ici une lambda appelée).
NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "MonJeu";
    d.appVersion = "0.1.0";
    return d;
})());

int nkmain(const NkEntryState& state) {
    // 1) Décrire la fenêtre
    NkWindowConfig cfg;
    cfg.title  = "Hello NKWindow";
    cfg.width  = 1280;
    cfg.height = 720;

    // 2) Créer la fenêtre
    NkWindow window;
    if (!window.Create(cfg)) {
        return -1;   // échec de création
    }

    // 3) Boucle principale (voir §3)
    while (window.IsOpen()) {
        while (NkEvent* ev = NkEvents().PollEvent()) {
            // traiter les entrées — détaillé dans le guide NKEvent
        }
        // mettre à jour la logique, puis dessiner (NKCanvas)
    }

    return 0;
}
```

> Remarque : tu n'as **pas** à appeler `NkInitialise()`/`NkClose()` ni à initialiser le système
> mémoire toi-même quand tu utilises `NKMain.h` : le point d'entrée généré s'en charge (il
> appelle l'équivalent de `NkEntryRuntimeInit`/`NkEntryRuntimeShutdown` autour de ton `nkmain`).
> Tu peux malgré tout déclencher des allocateurs supplémentaires dans ton code applicatif
> (voir [NKMemory](01-NKMemory.md)).

### Variante : configurer l'AppData via une fonction

Si tu préfères une fonction nommée plutôt qu'une lambda, utilise
`NK_REGISTER_ENTRY_APPDATA_UPDATER` :

```cpp
static void ConfigureAppData(nkentseu::NkAppData& d) {
    d.appName            = "MonJeu";
    d.appVersion         = "0.1.0";
    d.enableEventLogging = true;   // utile en debug
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)
```

Les deux formes sont équivalentes : `NKENTSEU_DEFINE_APP_DATA` génère justement un updater et
l'enregistre pour toi.

---

## 2. Créer une fenêtre

La fenêtre est décrite par une `NkWindowConfig`, puis créée par `NkWindow::Create(cfg)`.
Deux styles existent, au choix :

```cpp
// Style A : ctor par défaut + Create() — vérifie la valeur de retour
NkWindow window;
NkWindowConfig cfg;
cfg.title = "Mon App";
if (!window.Create(cfg)) {
    logger.Error("Création de la fenêtre impossible");
    return -1;
}

// Style B : ctor avec config — vérifie IsOpen() ensuite
NkWindow window(cfg);
if (!window.IsOpen()) {
    logger.Error("Création de la fenêtre impossible");
    return -1;
}
```

> `NkWindow` n'est **pas copiable** (`NkWindow(const NkWindow&) = delete`) mais elle est
> *déplaçable*. Passe-la par référence (`NkWindow&`) à tes sous-systèmes.

### Les champs de `NkWindowConfig`

Tous les champs ont une valeur par défaut raisonnable ; ne remplis que ce qui t'intéresse.

```cpp
struct NkWindowConfig {
    // --- Position et taille ---
    int32  x = 100, y = 100;          // ignorés si centered = true
    uint32 width  = 1280;
    uint32 height = 720;
    uint32 minWidth = 160, minHeight = 90;
    uint32 maxWidth = 0xFFFF, maxHeight = 0xFFFF;

    // --- Comportement ---
    bool centered      = true;        // centre la fenêtre sur l'écran
    bool resizable     = true;
    bool movable       = true;
    bool closable      = true;
    bool minimizable   = true;
    bool maximizable   = true;
    bool canFullscreen = true;
    bool fullscreen    = false;       // démarre en plein écran
    bool modal         = false;
    bool vsync         = true;
    bool dropEnabled   = false;       // drag & drop de fichiers
    NkScreenOrientation screenOrientation = NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;

    // --- Apparence ---
    bool   frame       = true;        // bordure + barre de titre
    bool   hasShadow   = true;
    bool   transparent = false;
    bool   visible     = true;
    uint32 bgColor     = 0x141414FF;  // RGBA

    // --- Identité ---
    NkString title    = "NkWindow";
    NkString name     = "NkApp";
    NkString iconPath;                // chemin vers l'icône

    // --- Mobile / Safe Area ---
    bool respectSafeArea = true;

    // --- Android ---
    bool hideSystemUI    = false;     // cacher status bar + barre de navigation
    bool lockOrientation = false;     // empêcher la rotation

    // --- Web / WASM ---
    NkWebInputOptions webInput;       // capture clavier/souris/tactile dans le canvas

    // (native, surfaceHints : usages avancés, laisser par défaut)
};
```

Un exemple concret tiré du dépôt (Pong) :

```cpp
NkWindowConfig cfg;
cfg.title       = "Pong Ultra Arena";
cfg.width       = 1280;
cfg.height      = 720;
cfg.centered    = true;
cfg.resizable   = true;
cfg.dropEnabled = false;

NkWindow window(cfg);
if (!window.IsOpen()) {
    logger.Error("[Pong] Window creation failed");
    return -1;
}
```

---

## 3. La boucle principale

Le cœur d'une application Nkentseu est une boucle qui tourne **tant que la fenêtre est
ouverte**, vide la file d'événements à chaque tour, met à jour la logique, puis dessine.

```cpp
while (window.IsOpen()) {
    // 1) Vider la file d'événements
    while (NkEvent* ev = NkEvents().PollEvent()) {
        // dispatch des entrées — détaillé dans le guide NKEvent
    }

    // 2) Mise à jour de la logique (avec le delta time)

    // 3) Rendu (NKCanvas) : Clear → Draw… → Display
}
```

- `window.IsOpen()` renvoie `false` une fois la fenêtre fermée (clic sur la croix, `Close()`,
  ou destruction de la surface native).
- `NkEvents()` est un raccourci vers le **système d'événements global** (propriété du runtime
  fenêtre). `PollEvent()` retourne le prochain `NkEvent*` ou `nullptr` quand la file est vide.

Il existe **deux façons** de consommer les événements (les deux passent par `NkEvents()`) :

- **`PollEvent()`** dans une boucle interne — modèle « pull », proche de
  `sf::Window::pollEvent` de SFML. C'est le modèle utilisé par Mú et recommandé pour débuter.
- **`AddEventCallback<T>(...)`** — modèle « push » par callbacks typés, enregistrés une fois à
  l'initialisation. Pratique pour câbler proprement fermeture, focus, souris…

Les deux modèles, ainsi que la liste des types d'événements, sont détaillés dans le guide
[NKEvent](03-NKEvent.md). Pour donner le ton, voici l'extrait réel de Mú qui fait coexister
les deux :

```cpp
auto& events = NkEvents();

// Push : la croix de fenêtre arrête la boucle.
events.AddEventCallback<NkWindowCloseEvent>([this](NkWindowCloseEvent*) {
    mRunning = false;
});

// ... plus loin, dans la boucle, Pull pour la logique de scène :
while (NkEvent* ev = NkEvents().PollEvent()) {
    if (mCurrentScene == AppScene::GameScene && mCurrentGame) HandleGameEvent(ev);
    else HandleMainMenuEvent(ev);
}
```

### Le delta time

Pour un mouvement indépendant du framerate, on calcule le temps écoulé entre deux frames.
Nkentseu fournit une horloge dans NKTime (`NkClock`). Modèle réel (Mú) :

```cpp
float32 dt = mClock.Tick().delta;          // secondes écoulées depuis le dernier Tick()
if (dt > MAX_DELTA_TIME) dt = MAX_DELTA_TIME;  // clamp anti-saut (debug, pauses…)
```

> `NkClock::Tick()` renvoie une structure `{ delta, total }` en `float32` ; il n'y a pas de
> `AsSeconds()` — utilise directement `.delta`.

---

## 4. Propriétés de la fenêtre

`NkWindow` expose des accesseurs et des mutateurs. Les coordonnées et tailles utilisent
`math::NkVec2u` (deux entiers non signés).

```cpp
// Lecture
NkString      title = window.GetTitle();
math::NkVec2u size  = window.GetSize();        // taille de la zone client
math::NkVec2u pos   = window.GetPosition();
float32       dpi   = window.GetDpiScale();     // 1.0 = 100%, 1.5 = 150%…
NkWindowId    id    = window.GetId();
NkWindowConfig c    = window.GetConfig();

// Écriture
window.SetTitle("Nouveau titre");
window.SetSize(1600, 900);                       // ou SetSize(math::NkVec2u{...})
window.SetPosition(40, 40);
window.SetVisible(true);
window.Minimize();
window.Maximize();
window.Restore();
```

Exemple d'usage de `GetSize()` chaque frame (la taille peut changer après un resize) :

```cpp
const math::NkVec2u sz = window.GetSize();
const float32 W = static_cast<float32>(sz.x);
const float32 H = static_cast<float32>(sz.y);
// adapter le rendu / la mise en page à W × H
```

Autres réglages utiles :

```cpp
window.ShowMouse(false);          // cacher le curseur
window.CaptureMouse(true);        // capturer la souris (FPS)
window.ClipMouseToClient(true);   // confiner le curseur à la zone client
window.SetMousePosition(W/2, H/2);
window.SetProgress(0.42f);        // barre de progression OS (taskbar/dock)
```

---

## 5. Plein écran et orientation (surtout mobile)

### Plein écran

```cpp
window.SetFullscreen(true);   // bascule en plein écran
window.SetFullscreen(false);  // revient en fenêtré
```

On peut aussi démarrer directement plein écran via `cfg.fullscreen = true`.

### Orientation de l'écran

L'orientation est pertinente surtout sur mobile (Android, iOS, Web, HarmonyOS). L'enum est :

```cpp
enum class NkScreenOrientation : uint32 {
    NK_SCREEN_ORIENTATION_AUTO = 0,   // suit le capteur du téléphone
    NK_SCREEN_ORIENTATION_PORTRAIT,
    NK_SCREEN_ORIENTATION_LANDSCAPE,
};
```

API associée :

```cpp
bool ok = window.SupportsOrientationControl();   // false sur desktop
window.SetScreenOrientation(NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE);
NkScreenOrientation cur = window.GetScreenOrientation();

window.SetAutoRotateEnabled(false);
bool auto = window.IsAutoRotateEnabled();

// Android (et plateformes mobiles compatibles)
window.SetHideSystemUI(true);    // masque status bar + barre de navigation
window.SetLockOrientation(true); // empêche la rotation
```

> **Convention Nkentseu : les jeux sont verrouillés en paysage** (comme Pong et Mú), jamais en
> portrait. On combine donc `screenOrientation = NK_SCREEN_ORIENTATION_LANDSCAPE`,
> `lockOrientation = true` et, côté descripteur Jenga Android, `allowrotation False`.

Le modèle complet, tiré de Mú, applique ces réglages **à la fois** dans la config (avant
`Create`) **et** après création, sous garde de plateforme :

```cpp
NkWindowConfig cfg;
cfg.title     = "Mu - jeux educatifs";
cfg.width     = 1280;
cfg.height    = 720;
cfg.centered  = true;
cfg.resizable = true;

#if defined(NKENTSEU_PLATFORM_ANDROID)
    cfg.fullscreen        = true;
    cfg.hideSystemUI      = true;
    cfg.screenOrientation = NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE;
    cfg.lockOrientation   = true;
#endif

if (!mWindow.Create(cfg)) {
    return false;
}

#if defined(NKENTSEU_PLATFORM_ANDROID)
    mWindow.SetScreenOrientation(NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE);
    mWindow.SetLockOrientation(true);
    mWindow.SetFullscreen(true);
    mWindow.SetHideSystemUI(true);
#endif
```

### Safe Area (encoche, barre de statut…)

Sur mobile, certains bords sont masqués (encoche, Dynamic Island, home indicator). Pour ne pas
y dessiner d'éléments interactifs, lis les marges à respecter :

```cpp
NkSafeAreaInsets insets = window.GetSafeAreaInsets();
// insets.top / bottom / left / right  (en pixels physiques)
uint32 usableW = insets.UsableWidth(window.GetSize().x);
uint32 usableH = insets.UsableHeight(window.GetSize().y);
```

Sur desktop (Windows, Linux, macOS) et Web, les insets valent `{0,0,0,0}`.

---

## 6. Multi-écran et DPI

NKWindow expose l'énumération des moniteurs (recalculée à chaque appel, donc à jour après un
branchement/débranchement d'écran à chaud).

```cpp
uint32 count = window.GetMonitorCount();              // >= 1

NkVector<NkDisplayInfo> monitors = window.EnumerateMonitors();
for (usize i = 0; i < monitors.Size(); ++i) {
    const NkDisplayInfo& m = monitors[i];
    logger.Infof("Ecran %u : %ux%u @ %uHz, DPI x%.2f, primaire=%d, nom=%s",
                 m.index, m.width, m.height, m.refreshRate, m.dpiScale,
                 (int)m.isPrimary, m.name);
}

NkDisplayInfo cur = window.GetCurrentMonitor();        // écran contenant la fenêtre
```

La structure `NkDisplayInfo` :

```cpp
struct NkDisplayInfo {
    uint32  index;          // 0 = principal (selon le système)
    uint32  width, height;  // résolution logique (DPI pris en compte)
    uint32  physWidth, physHeight;  // pixels physiques réels
    uint32  refreshRate;    // Hz
    float32 dpiScale;       // 1.0 = 100%, 1.5 = 150%, 2.0 = 200%
    float32 dpiX, dpiY;     // densité physique
    int32   posX, posY;     // position dans l'espace virtuel multi-écrans
    bool    isPrimary;      // ne PAS supposer que monitors[0] est le primaire :
                            // tester isPrimary
    char    name[64];       // ex. "DELL U2720Q"
};
```

> Le premier élément de `EnumerateMonitors()` n'est **pas garanti** d'être le moniteur
> principal : identifie-le via `isPrimary`.

### DPI / résolution

`window.GetDpiScale()` te donne le facteur d'échelle de l'écran courant. Le changement de DPI
ou de taille arrive sous forme d'événements ([NKEvent](03-NKEvent.md) :
`NkWindowResizeEvent`, événement DPI, `NkSystemDisplayEvent` pour le hot-plug). La règle
pratique : **resize et changement DPI déclenchent la même routine** (recréer la swapchain /
adapter le viewport — voir §7 et le guide [NKCanvas](05-NKCanvas.md)).

---

## 7. Spécificités par plateforme

NKWindow présente la **même API** partout, mais le cycle de vie diffère par cible :

- **Windows / Linux (XLib, XCB, Wayland) / macOS** — desktop classique : la fenêtre vit tant
  que la boucle tourne. `GetSafeAreaInsets()` = 0, DPI variable selon l'écran.
- **Android** (`NativeActivity` / `android_native_app_glue`) — pas de « fenêtre » au sens
  desktop : une **surface** (`ANativeWindow`) liée au cycle de vie de l'activité. Quand l'app
  passe en arrière-plan, la surface est **détruite** (événement *hidden*) ; au retour, elle est
  **recréée** (événement *shown*) et il faut **recréer la surface graphique** (sinon écran
  noir). Modèle réel de Mú :

  ```cpp
  #if defined(NKENTSEU_PLATFORM_ANDROID)
  events.AddEventCallback<NkWindowHiddenEvent>([this](NkWindowHiddenEvent*) {
      mActive = false;                         // on cesse de rendre
  });
  events.AddEventCallback<NkWindowShownEvent>([this](NkWindowShownEvent*) {
      if (mRenderTarget) mRenderTarget->RecreateSurface();  // sinon écran noir au resume
      mActive = true;
  });
  #endif
  ```

  Dans la boucle, quand `!mActive`, on **ne dessine pas** et on relâche le CPU :

  ```cpp
  if (!mActive) { nkentseu::NkChrono::Sleep((nkentseu::int64)16); continue; }
  ```

- **Web (Emscripten)** — la fenêtre est un `<canvas>`. Les options de capture clavier/souris/
  tactile passent par `cfg.webInput` (`NkWebInputOptions`) ou `window.SetWebInputOptions(...)`.
  L'orientation suit l'API d'orientation du navigateur.
- **iOS / HarmonyOS / UWP / Xbox** — même API ; surface fournie par le système, plein écran
  par nature. Les handles natifs nécessaires arrivent par `NkEntryState` (gérés par le runtime).

> En pratique, ton `nkmain` et ta boucle sont identiques sur toutes les plateformes ; seules
> quelques poignées de réglages mobiles sont entourées de `#if defined(NKENTSEU_PLATFORM_…)`.

---

## 8. Pièges courants

- **Resize.** La taille de la fenêtre change en cours de route (utilisateur, rotation, DPI).
  Ne mets pas la taille en cache une fois pour toutes : relis `window.GetSize()` (ou la taille
  de ta cible de rendu) chaque frame et ne ré-adapte le rendu **que si elle a réellement
  changé**. Modèle réel de Mú :

  ```cpp
  const math::NkVec2u sz = mRenderTarget->GetSize();
  if (sz.x != mLastW || sz.y != mLastH) {
      if (mLastW != 0 && sz.x > 0 && sz.y > 0) {
          mRenderTarget->OnResize(sz.x, sz.y);     // recrée viewport/swapchain
      }
      mLastW = sz.x; mLastH = sz.y;
  }
  ```

  > Sous Windows, un `WM_SIZE` est émis **à la création** de la fenêtre. Un handler de resize
  > naïf déclencherait donc un OnResize inutile avant la première frame (et peut faire planter
  > certains backends comme DX12). D'où le garde « ne réagir que si la taille a vraiment changé ».

- **Focus / arrière-plan.** Quand la fenêtre perd le focus, mets le jeu (et l'audio) en pause,
  et reprends au retour, via les callbacks de focus :

  ```cpp
  events.AddEventCallback<NkWindowFocusLostEvent>([this](NkWindowFocusLostEvent*)   { mPaused = true;  });
  events.AddEventCallback<NkWindowFocusGainedEvent>([this](NkWindowFocusGainedEvent*){ mPaused = false; });
  // dans la boucle :
  if (mPaused) { nkentseu::NkChrono::Sleep((nkentseu::int64)16); continue; }
  ```

  Ne confonds pas **focus perdu** (fenêtre toujours visible, on fige) et **arrière-plan
  Android** (surface détruite, il faudra la recréer).

- **Fermeture.** `IsOpen()` suffit pour terminer la boucle, mais pour réagir tout de suite à la
  croix (et arrêter proprement), capture `NkWindowCloseEvent` et baisse ton drapeau de boucle.

- **Copie de `NkWindow`.** Interdite (type non copiable). Passe-la par référence.

- **`NkChrono::Sleep`.** Prend un `int64` ou un `float64`, **pas** un `float32` (ambigu). Caste
  explicitement : `NkChrono::Sleep((int64)16)`.

---

## 9. Récapitulatif et dépendances Jenga

Ce que tu sais faire maintenant :

- Écrire le point d'entrée portable avec `nkmain(...)` + `NKENTSEU_DEFINE_APP_DATA(...)`.
- Décrire une fenêtre avec `NkWindowConfig` et la créer avec `NkWindow::Create()` /
  `NkWindow(cfg)`.
- Tourner dans la boucle principale (`IsOpen()` + `NkEvents().PollEvent()`).
- Lire/modifier les propriétés (titre, taille, position, DPI, curseur).
- Gérer plein écran, orientation et safe area (mobile), multi-écran (`EnumerateMonitors`).
- Encaisser resize, focus et arrière-plan sans planter.

### Dépendances Jenga

NKWindow tire automatiquement [NKEvent](03-NKEvent.md) (les types d'événements et `NkEvents()`
vivent dans NKEvent, et `NKWindow.h` les inclut). Déclare au minimum :

```python
# MonJeu.jenga (extrait)
with project("MonJeu"):
    kind("app")
    files(["src/**.cpp"])
    nkentseudependson(
        ["NKWindow", "NKEvent",          # fenêtre + entrées
         "NKMemory", "NKCore", "NKMath", # fondations (vecteurs, types, mémoire)
         "NKLogger", "NKTime"],          # log + horloge (delta time)
        extra_includes=["src"],
    )
```

Pour dessiner dans la fenêtre, ajoute `NKCanvas` (et `NKImage` pour les textures) — voir le
guide [NKCanvas](05-NKCanvas.md). Jenga propage les includes/libs des dépendances
transitivement.

---

## Résumé

NKWindow donne une **fenêtre native cross-plateforme** et un **point d'entrée portable**
(`nkmain`) : tu décris la fenêtre avec `NkWindowConfig`, tu la crées (`Create`/ctor), puis tu
boucles sur `IsOpen()` en pompant `NkEvents().PollEvent()`. La même API couvre desktop, mobile
et web ; seuls quelques réglages mobiles (plein écran, orientation paysage, safe area,
recréation de surface en retour d'arrière-plan) sont à entourer de gardes de plateforme.

➡️ Suite logique : les entrées clavier/souris/tactile/manette dans [NKEvent](03-NKEvent.md),
puis le rendu 2D dans [NKCanvas](05-NKCanvas.md). Retour au [sommaire](README.md).
