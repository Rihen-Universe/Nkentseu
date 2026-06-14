# Les événements d'application, de fenêtre et de système

> Couche **Runtime** · NKEvent · Tout ce qui **arrive au programme depuis le dehors** : le cycle de
> vie de l'application (`NkAppEvent`), l'état de la fenêtre (`NkWindowEvent`), les changements du
> système d'exploitation (`NkSystemEvent`), du contexte graphique (`NkGraphicsEvent`), les
> glisser-déposer (`NkDropEvent`), les transferts de fichiers (`NkTransferEvent`) et les messages
> internes (`NkCustomEvent`).

Un programme interactif passe le plus clair de son temps à **réagir**. Pas à calculer dans le vide :
à répondre. La fenêtre vient d'être redimensionnée, le système prévient que la batterie est
critique, le contexte GPU a été perdu, l'utilisateur a lâché trois fichiers sur la fenêtre, un
transfert réseau progresse. Tous ces faits, qui n'ont en commun que d'**arriver du dehors**, sont
modélisés ici comme des **objets-événements** : un type précis, des données attachées, un instant.
Le rôle de cette page est de présenter cette famille — celle qui ne concerne *ni* le clavier/souris
(voir les pages *Input*) *ni* le réseau brut, mais le **cycle de vie** et l'**environnement** du
programme.

Le principe est uniforme et c'est ce qui rend l'ensemble simple. Chaque événement est une **classe**
dérivée de `NkEvent`, marquée par deux étiquettes : un **type** (`NkEventType`, par ex.
`NK_WINDOW_RESIZE`) qui dit *ce qui s'est passé*, et une **catégorie** (`NkEventCategory`, par ex.
`NK_CAT_WINDOW`) qui regroupe les types apparentés pour les filtrer en bloc. On reçoit un
`NkEvent*`, on teste sa catégorie ou son type, on le *transtype* vers la classe concrète, on lit ses
accesseurs. Ce n'est **pas** un système de messages texte ni un *variant* fourre-tout : chaque
événement est un type C++ fort, avec des champs nommés et des helpers métier (`GotLarger()`,
`IsCritical()`, `GetProgressPercent()`).

Deux idiomes traversent toute la famille. D'abord, **`Clone()`** : chaque classe sait se recopier
polymorphiquement (`NkEvent* Clone() const`) — utile pour mettre un événement en file et le traiter
plus tard, mais attention, la copie est faite avec le `new` du CRT, **c'est au receveur de la
`delete`** (c'est l'unique allocation hors NKMemory du module). Ensuite, l'**identifiant de
fenêtre** : presque tous les constructeurs prennent un `uint64 windowId = 0` **en dernier
paramètre** ; `0` (`NK_INVALID_WINDOW_ID`) désigne un événement global, sans fenêtre attitrée.

- **Namespace** : `nkentseu`
- **Headers** : `NkApplicationEvent.h`, `NkWindowEvent.h`, `NkSystemEvent.h`, `NkGraphicsEvent.h`,
  `NkCustomEvent.h`, `NkDropEvent.h`, `NkDropSystem.h`, `NkTransferEvent.h`, `NkWindowId.h`,
  `NkSafeArea.h`

---

## Le cycle de vie de l'application : `NkAppEvent`

Avant la fenêtre, avant le rendu, il y a le **programme lui-même** : il démarre, il bat la mesure à
chaque frame, il s'arrête. Cette cadence est portée par la catégorie `NK_CAT_APPLICATION`, dont la
base abstraite est `NkAppEvent`. On ne l'instancie pas directement ; on reçoit ses cinq dérivés
concrets, qui balisent la vie du programme du premier instant au dernier.

`NkAppLaunchEvent` arrive **une fois**, au lancement, et transporte la ligne de commande
(`argc`/`argv`). Le couple `NkAppTickEvent` / `NkAppUpdateEvent` rythme la boucle : le *tick*
fournit le `deltaTime` (et un `GetFps()` calculé pour vous, `1/delta`), l'*update* distingue en plus
le pas **fixe** du pas variable (`IsFixedStep()`) — la séparation classique entre la simulation à
fréquence constante et la mise à jour à fréquence d'affichage. `NkAppRenderEvent` signale le moment
de dessiner, avec un `alpha` d'**interpolation** (pour lisser l'affichage entre deux pas de
simulation) et un `frameIndex`. Enfin `NkAppCloseEvent` demande l'arrêt.

Ce dernier mérite une nuance : il est **annulable**. Un `Cancel()` permet de refuser la fermeture
(« voulez-vous enregistrer ? ») — *sauf* si l'événement est `forced`, auquel cas `Cancel()` est
ignoré silencieusement. C'est exactement ce qu'on veut : un clic sur la croix est annulable, une
extinction du système ne l'est pas.

```cpp
void OnEvent(NkEvent* e) {
    if (auto* tick = e->As<NkAppTickEvent>()) {
        simulation.Step(tick->GetDeltaTime());
        hud.SetFps(tick->GetFps());
    }
    if (auto* close = e->As<NkAppCloseEvent>()) {
        if (!close->IsForced() && document.IsDirty())
            close->Cancel();          // on garde la main : popup « enregistrer ? »
    }
}
```

> **En résumé.** `NK_CAT_APPLICATION` balise la vie du programme : `Launch` (une fois, avec
> `argc/argv`), `Tick`/`Update` (cadence, delta, pas fixe), `Render` (`alpha` d'interpolation,
> `frameIndex`), `Close` (annulable par `Cancel()` sauf si `IsForced()`). Le delta et les FPS sont
> servis prêts à l'emploi.

---

## L'état de la fenêtre : `NkWindowEvent`

C'est la sous-famille la plus fournie, parce qu'une fenêtre a beaucoup d'états et de transitions.
Tous ces événements partagent la catégorie `NK_CAT_WINDOW` et portent le `windowId` de la fenêtre
concernée — indispensable dès qu'une application gère **plusieurs fenêtres**.

Le groupe le plus important pour le rendu est celui des **dimensions**. `NkWindowResizeEvent`
n'apporte pas que la nouvelle taille : il donne aussi l'**ancienne** (`GetPrevWidth/Height`) et deux
helpers — `GotLarger()` / `GotSmaller()` — qui évitent de comparer à la main, plus un enum
`ResizeState` (agrandi / réduit / inchangé). C'est *l'*événement à écouter pour recréer la swapchain
et reconfigurer le viewport. Il est encadré par `NkWindowResizeBeginEvent` /
`NkWindowResizeEndEvent`, qui délimitent un **glissement de poignée** : on peut suspendre le rendu
coûteux pendant le drag et ne recréer les ressources qu'à la fin. Le même schéma vaut pour le
déplacement : `NkWindowMoveEvent` (avec `GetDeltaX/Y`) entre `MoveBegin` et `MoveEnd`.

Vient ensuite la **visibilité et le focus**, série d'événements *sans données* (le type suffit) :
`FocusGained`/`FocusLost`, `Minimize`/`Maximize`/`Restore`, `Fullscreen`/`Windowed`,
`Shown`/`Hidden`. Le focus perdu, c'est le réflexe de mettre le jeu en pause ; la minimisation, de
couper le rendu pour ne pas chauffer le GPU dans le vide.

La modernité mobile et haute densité apporte le reste. `NkWindowDpiEvent` signale un changement
d'échelle (`GetScale`, `GetPrevScale`, `GetDpi`) — à traiter **comme un resize en pixels physiques**.
`NkWindowThemeEvent` annonce le passage clair/sombre (`IsDark()`/`IsLight()`). Sur mobile, le couple
`NkWindowSurfaceCreatedEvent` / `NkWindowSurfaceDestroyedEvent` encadre la **disponibilité réelle de
la surface native** (Android peut la détruire à tout moment : c'est le signal pour libérer puis
recréer le contexte GPU). `NkWindowOrientationChangedEvent` gère la rotation
(`IsPortrait()`/`IsLandscape()`), `NkWindowSafeAreaChangedEvent` les **marges sûres** (encoche,
barre système), et `NkWindowVirtualKeyboardChangedEvent` l'ouverture du clavier logiciel — avec sa
hauteur, pour pousser l'UI au-dessus.

```cpp
if (auto* r = e->As<NkWindowResizeEvent>()) {
    if (r->GotLarger() || r->GotSmaller())
        renderer.RecreateSwapchain(r->GetWidth(), r->GetHeight());
}
if (e->GetType() == NK_WINDOW_FOCUS_LOST) game.Pause();
```

> **En résumé.** `NK_CAT_WINDOW` couvre dimensions (`Resize` avec ancien/nouveau + `GotLarger/
> Smaller`, encadré par `ResizeBegin/End`), déplacement (`Move` + deltas), visibilité/focus
> (événements sans données), et le monde mobile/HD (`Dpi`, `Theme`, `Surface*`, `Orientation`,
> `SafeArea`, `VirtualKeyboard`). Traitez le DPI comme un resize physique. Pas d'enum `NkWindowState`
> — seulement `NkWindowStateToString(uint32)`.

---

## L'environnement système : `NkSystemEvent`

Au-delà de la fenêtre, c'est le **système d'exploitation** qui parle. La catégorie `NK_CAT_SYSTEM`
remonte des faits qui ne dépendent pas de votre programme mais qui le concernent : l'alimentation, la
mémoire, les écrans branchés, la langue.

`NkSystemPowerEvent` est le plus stratégique sur portable et mobile. Il porte un `NkPowerState`
(batterie faible/critique, branché/débranché, suspension, reprise, extinction…), le niveau de
batterie en `[0,1]` (ou `-1` si inconnu), et surtout deux helpers : `IsSuspending()` (le système va
dormir → sauvegarder et relâcher les ressources GPU) et `IsResuming()` (réveil → tout recréer).
`NkSystemMemoryEvent` joue le même rôle pour la **RAM** : un `NkMemoryPressure`, les octets
disponibles/totaux, `IsCritical()` et un `GetAvailableMb()` prêt à afficher — le signal pour vider
les caches avant que l'OS ne tue le processus.

`NkSystemDisplayEvent` décrit un **changement d'écran** (ajout, retrait, résolution, orientation,
DPI, écran principal), accompagné d'un `NkDisplayInfo` complet (taille logique et physique,
rafraîchissement, position, DPI, nom). C'est ce qui permet de déplacer proprement une fenêtre quand
on débranche le moniteur sur lequel elle vivait. `NkSystemLocaleEvent` signale un changement de
**langue** (ancienne et nouvelle locale) pour recharger les traductions à chaud.

Un point d'attention important pour le filtrage. Trois classes système **réutilisent un type
existant** au lieu d'avoir le leur : `NkSystemTimeZoneEvent` porte le type `NK_SYSTEM_LOCALE`, tandis
que `NkSystemThemeEvent` et `NkSystemAccessibilityEvent` portent `NK_SYSTEM_DISPLAY`. Conséquence
concrète : **filtrer par `GetType()` ne les distingue pas** — il faut transtyper avec `As<T>()` pour
savoir laquelle on tient. `NkSystemThemeEvent` ajoute la couleur d'accentuation du bureau ;
`NkSystemAccessibilityEvent` remonte les préférences d'accessibilité (mouvement réduit, contraste,
inversion des couleurs, gras, grand texte, `GetFontScale()`) — à honorer pour respecter les réglages
de l'utilisateur.

> **En résumé.** `NK_CAT_SYSTEM` remonte l'OS : `Power` (`IsSuspending/Resuming`), `Memory`
> (`IsCritical`, `GetAvailableMb`), `Display` (avec `NkDisplayInfo`), `Locale`, plus TimeZone, Theme
> et Accessibility. Piège : TimeZone/Theme/Accessibility **partagent le type** de Locale/Display —
> distinguez-les par `As<T>()`, jamais par `GetType()`.

---

## Le contexte graphique : `NkGraphicsEvent`

La catégorie `NK_CAT_GRAPHICS` décrit la vie du **contexte GPU** : sa disponibilité, sa perte, son
redimensionnement, le rythme des frames côté GPU, la pression mémoire vidéo, le VSync. C'est le pont
entre l'événementiel et le rendu.

`NkGraphicsContextReadyEvent` annonce que le contexte est prêt (avec l'API choisie — `NkGraphicsApi`
: OpenGL, Vulkan, DX11/12, Metal, WebGPU, Software… — et la taille initiale) : c'est le moment de
créer pipelines et buffers. Son pendant `NkGraphicsContextLostEvent` signale une **perte de
device** (typique DX/Vulkan), avec une `reason` textuelle — il faut alors tout reconstruire.
`NkGraphicsContextResizeEvent` apporte ancienne et nouvelle taille **et** un `GetAspectRatio()`
calculé, prêt pour la matrice de projection.

Le couple `NkGraphicsFrameBeginEvent` / `NkGraphicsFrameEndEvent` instrumente la frame :
`frameIndex`, indice de la frame *in-flight* à l'entrée ; temps GPU et CPU en millisecondes à la
sortie — exactement ce qu'il faut pour un graphe de performance. `NkGraphicsGpuMemoryEvent`
(`NkGpuMemoryLevel`, Mo disponibles/totaux, `IsCritical()`) joue pour la VRAM le rôle que
`NkSystemMemoryEvent` joue pour la RAM. `NkGraphicsVSyncEvent` remonte l'écran et son taux de
rafraîchissement.

La **particularité forte** de cette sous-famille, à connaître absolument : aucune n'a son propre
`NkEventType`. Les sept classes **réutilisent les types `NK_APP_*`** (`ContextReady`→`NK_APP_RENDER`,
`ContextLost`→`NK_APP_CLOSE`, `ContextResize`→`NK_APP_UPDATE`, `FrameBegin`→`NK_APP_TICK`,
`FrameEnd`→`NK_APP_LAUNCH`, `GpuMemory`→`NK_APP_TICK`, `VSync`→`NK_APP_RENDER`). Comme pour les
événements système ci-dessus, **le filtrage par type ne suffit pas** : utilisez `As<T>()`.

> **En résumé.** `NK_CAT_GRAPHICS` = vie du contexte GPU : `ContextReady` (API + taille), `Context
> Lost` (raison), `ContextResize` (+ aspect ratio), `Frame Begin/End` (timings GPU/CPU),
> `GpuMemory`, `VSync`. Comme système : ces 7 classes **empruntent les types `NK_APP_*`**, donc
> identifiez-les par `As<T>()`.

---

## Les messages internes : `NkCustomEvent`

Tous les événements ci-dessus viennent du dehors. Parfois on veut faire transiter ses **propres**
messages dans le même bus — un gameplay qui notifie « niveau terminé », un système qui en réveille un
autre. C'est le rôle de la catégorie `NK_CAT_CUSTOM`, qui propose **trois véhicules** selon la nature
de la charge utile, tous étiquetés par un `customType` libre (un entier que vous définissez).

`NkCustomEvent` est le plus rapide : il embarque un **payload inline** de 128 octets maximum,
**sans aucune allocation tas**. On y range une petite struct *trivially copyable* via
`SetPayload<T>()` (un `static_assert` refuse les types trop gros à la compilation), et on la relit
par `GetPayload<T>()`. Attention : la relecture ne vérifie **pas** le type — un mauvais `T` est un
comportement indéfini ; vérifiez `GetCustomType()` avant. `NkCustomPtrEvent`, lui, **possède** ses
données dans un `NkVector<uint8>` (charge dynamique de taille quelconque) et offre un `ViewAs<T>()`
qui réinterprète le tampon **sans copie**. `NkCustomStringEvent` transporte un message **texte
UTF-8** avec un *tag* — idéal pour un canal de logs ou de debug, avec `IsTag("réseau")` pour filtrer.

Les trois exposent aussi un `userPtr` (`void*`) **non possédé** : un raccourci vers un objet existant
dont la durée de vie reste à votre charge.

```cpp
struct ScoreMsg { int player; int points; };          // trivially copyable
NkCustomEvent ev(MyEvents::SCORE);
ev.SetPayload(ScoreMsg{ 0, 250 });                     // copie inline, zéro alloc

// à la réception :
if (ev.GetCustomType() == MyEvents::SCORE) {
    ScoreMsg m; ev.GetPayload(m);
    hud.AddScore(m.player, m.points);
}
```

> **En résumé.** `NK_CAT_CUSTOM` fait passer vos propres messages : `NkCustomEvent` (≤128 o inline,
> zéro alloc, `SetPayload<T>`/`GetPayload<T>`), `NkCustomPtrEvent` (tampon possédé + `ViewAs<T>`
> zéro-copie), `NkCustomStringEvent` (texte + tag). Ni `GetPayload`/`ViewAs` ne vérifient le type :
> contrôlez `GetCustomType()`/`GetSize()` d'abord. `userPtr` n'est jamais possédé.

---

## Glisser-déposer et transferts : `NkDropEvent`, `NkTransferEvent`

Deux dernières familles closent le tableau, autour de l'**échange de données**.

Le **glisser-déposer** (catégorie `NK_CAT_DROP`) suit le geste de bout en bout :
`NkDropEnterEvent` (le curseur entre, avec un aperçu du contenu — nombre de fichiers, texte/image
présents), `NkDropOverEvent` (il se déplace, position courante), `NkDropLeaveEvent` (il ressort), et
les trois événements de dépôt effectif — `NkDropFileEvent` (liste de chemins),
`NkDropTextEvent` (texte + MIME), `NkDropImageEvent` (pixels RGBA8 + dimensions). **Particularité** :
ici, contrairement au reste du module, la charge utile est un **membre public `data`** (et non des
accesseurs) — on écrit `ev.data.paths`. Côté plomberie, le header `NkDropSystem.h` expose les deux
seules fonctions cross-platform du module : `NkEnableDropTarget(nativeHandle)` /
`NkDisableDropTarget(nativeHandle)` pour armer/désarmer une fenêtre native comme cible de dépôt.

Les **transferts** (catégorie `NK_CAT_TRANSFER`) modélisent un échange de données de longue durée —
fichier, HTTP, Bluetooth, USB, IPC — identifié par un `transferId`. Le cycle est complet :
`NkTransferBeginEvent` (nom, taille totale, sens envoi/réception, protocole),
`NkTransferProgressEvent` (octets transférés, vitesse, et un `GetProgressPercent()` qui renvoie
`-1.0` si la taille totale est inconnue), puis l'un des trois épilogues —
`NkTransferCompleteEvent` (durée, chemin de sortie, vitesse moyenne), `NkTransferErrorEvent`
(`NkTransferStatus` : erreur, timeout, refusé, corrompu, annulé) ou `NkTransferCancelledEvent`.
`NkTransferDataEvent` permet en plus de **streamer** les octets eux-mêmes par morceaux (données
possédées, avec un `offset`).

> **En résumé.** `NK_CAT_DROP` suit le drag&drop (`Enter`/`Over`/`Leave` puis `File`/`Text`/`Image`,
> payload en **membre public `data`**) ; armer la fenêtre via `NkEnableDropTarget`. `NK_CAT_TRANSFER`
> modélise un échange long par `transferId` : `Begin`→`Progress` (`GetProgressPercent()` = -1 si
> taille inconnue)→`Complete`/`Error`/`Cancelled`, plus `Data` pour le streaming.

---

## Aperçu de l'API

Tous les éléments publics, par sous-famille. Sauf mention, chaque classe est `final`, dérive de
`NkEvent`, fournit `Clone()` (copie `new` CRT — `delete` à la charge du receveur) et `ToString()`,
et prend `uint64 windowId = 0` en dernier paramètre de constructeur.

### Types de base et utilitaires

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Identifiant | `NkWindowId` (= `uint64`), `NK_INVALID_WINDOW_ID` (= 0) | ID de fenêtre ; 0 = global/invalide. |
| Safe area | `struct NkSafeAreaInsets` | Marges `top/bottom/left/right` (pixels physiques). |
| Safe area | `IsZero`, `UsableWidth/Height(total)`, `ClipPoint(x,y,w,h)`, `ToString` | Test ; surface utile clampée ≥0 ; point dans la zone sûre. |
| Safe area | `struct NkSafeAreaData` | `insets` + `displayWidth/Height`. |
| Orientation | `enum NkScreenOrientation` (`AUTO/PORTRAIT/LANDSCAPE`) | Orientation d'écran. |

### `NkAppEvent` — catégorie `NK_CAT_APPLICATION`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Base | `NkAppEvent` (abstraite) | Base de la catégorie. |
| Lancement | `NkAppLaunchEvent` — `GetArgc`, `GetArgv`, `GetArg(i)` `[O(1)]` | Ligne de commande (non possédée, bornée). |
| Cadence | `NkAppTickEvent` — `GetDeltaTime`, `GetTotalTime`, `GetFps` | Delta + FPS (`1/delta`, 0 si ≤0). |
| Cadence | `NkAppUpdateEvent` — `GetDeltaTime`, `IsFixedStep` | Mise à jour ; pas fixe ou variable. |
| Rendu | `NkAppRenderEvent` — `GetAlpha`, `GetFrameIndex` | Interpolation + indice de frame. |
| Fermeture | `NkAppCloseEvent` — `IsForced`, `IsCancelled`, `Cancel` | Annulable sauf si forcée. |

### `NkWindowEvent` — catégorie `NK_CAT_WINDOW`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum/fns | `NkWindowTheme`, `NkWindowThemeToString`, `NkWindowStateToString(uint32)` | Thème ; texte d'état (codes 0–6, pas d'enum d'état). |
| Base | `NkWindowEvent` | Base ; `GetCategoryFlags()` → `NK_CAT_WINDOW`. |
| Vie | `NkWindowCreateEvent` (`GetWidth/Height`), `NkWindowCloseEvent` (`IsForced`), `NkWindowDestroyEvent` | Création / fermeture / destruction. |
| Dessin | `NkWindowPaintEvent` — `IsFullPaint`, `GetDirtyX/Y/W/H` | Repeint complet ou zone sale. |
| Taille | `NkWindowResizeEvent` — `GetWidth/Height/PrevWidth/PrevHeight`, `GotSmaller`, `GotLarger`, `GetResizeState`, enum `ResizeState` | Redim. + ancienne taille + helpers. |
| Taille | `NkWindowResizeBeginEvent`, `NkWindowResizeEndEvent` | Début/fin de glissement de poignée. |
| Position | `NkWindowMoveEvent` — `GetX/Y/PrevX/PrevY`, `GetDeltaX/Y` | Déplacement + deltas. |
| Position | `NkWindowMoveBeginEvent`, `NkWindowMoveEndEvent` | Début/fin de déplacement. |
| Focus | `NkWindowFocusGainedEvent`, `NkWindowFocusLostEvent` | Gain/perte de focus. |
| État | `NkWindowMinimize/Maximize/RestoreEvent`, `Fullscreen/WindowedEvent`, `Shown/HiddenEvent` | Transitions d'état (sans données). |
| HD/mobile | `NkWindowDpiEvent` — `GetScale/PrevScale/Dpi` | Changement d'échelle DPI. |
| HD/mobile | `NkWindowThemeEvent` — `GetTheme`, `IsDark`, `IsLight` | Clair/sombre. |
| Mobile | `NkWindowSurfaceCreated/DestroyedEvent` — `GetWidth/Height` | Surface native dispo/perdue. |
| Mobile | `NkWindowOrientationChangedEvent` — `GetOrientation`, `GetRotationDeg`, `IsPortrait`, `IsLandscape` | Rotation d'écran. |
| Mobile | `NkWindowSafeAreaChangedEvent` — `GetInsets`, `GetTop/Right/Bottom/Left`, `HasInsets` | Marges sûres. |
| Mobile | `NkWindowVirtualKeyboardChangedEvent` — `IsVisible`, `GetHeight`, `IsShowing`, `IsHiding` | Clavier logiciel. |

### `NkSystemEvent` — catégorie `NK_CAT_SYSTEM`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums/fns | `NkPowerState`(+ToString), `NkDisplayChange`, `NkMemoryPressure`(+ToString) | États alim./écran/mémoire. |
| Struct | `NkDisplayInfo` | Infos écran (taille, DPI, position, nom…). |
| Base | `NkSystemEvent` | Base de la catégorie. |
| Alim. | `NkSystemPowerEvent` — `GetState`, `GetBatteryLevel`, `IsPluggedIn`, `IsSuspending`, `IsResuming` | Alimentation/batterie. |
| Langue | `NkSystemLocaleEvent` — `GetLocale`, `GetPrevLocale` | Changement de locale. |
| Écran | `NkSystemDisplayEvent` — `GetChange`, `GetInfo`, `IsAdded/Removed/ResolutionChange/DpiChange` | Branchement/résolution d'écran. |
| RAM | `NkSystemMemoryEvent` — `GetPressure`, `GetAvailableBytes/TotalBytes`, `IsCritical`, `GetAvailableMb` | Pression mémoire. |
| Fuseau | `NkSystemTimeZoneEvent` (type `NK_SYSTEM_LOCALE`) — `GetTimeZoneId`, `GetPrevTimeZoneId`, `GetOffsetMinutes` | Fuseau horaire (type partagé). |
| Thème | `NkSystemThemeEvent` (type `NK_SYSTEM_DISPLAY`) — `GetTheme/PrevTheme`, `GetAccentR/G/B`, `IsDark/Light/HighContrast`, enum `Theme` | Thème système (type partagé). |
| Accès. | `NkSystemAccessibilityEvent` (type `NK_SYSTEM_DISPLAY`) — `ReduceMotion`, `IncreaseContrast`, `InvertColors`, `BoldText`, `LargeText`, `GetFontScale` | Accessibilité (type partagé). |

### `NkGraphicsEvent` — catégorie `NK_CAT_GRAPHICS`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias/fns | `NkGraphicsApi`, `NkGraphicsApiToString`, `enum NkGpuMemoryLevel` | API GPU + niveau VRAM. |
| Base | `NkGraphicsEvent` | Base de la catégorie. |
| Contexte | `NkGraphicsContextReadyEvent` (type `NK_APP_RENDER`) — `GetApi`, `GetWidth/Height` | Contexte prêt. |
| Contexte | `NkGraphicsContextLostEvent` (type `NK_APP_CLOSE`) — `GetReason` | Perte de device. |
| Contexte | `NkGraphicsContextResizeEvent` (type `NK_APP_UPDATE`) — `GetWidth/Height/PrevWidth/PrevHeight`, `GetAspectRatio` | Redim. du contexte. |
| Frame | `NkGraphicsFrameBeginEvent` (type `NK_APP_TICK`) — `GetFrameIndex`, `GetFrameInFlight` | Début de frame GPU. |
| Frame | `NkGraphicsFrameEndEvent` (type `NK_APP_LAUNCH`) — `GetFrameIndex`, `GetGpuTimeMs`, `GetCpuTimeMs` | Fin de frame + timings. |
| VRAM | `NkGraphicsGpuMemoryEvent` (type `NK_APP_TICK`) — `GetLevel`, `GetAvailableMb/TotalMb`, `IsCritical` | Pression VRAM. |
| VSync | `NkGraphicsVSyncEvent` (type `NK_APP_RENDER`) — `GetDisplayIndex`, `GetRefreshRateHz` | Rafraîchissement écran. |

### `NkCustomEvent` — catégorie `NK_CAT_CUSTOM`, type `NK_CUSTOM`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Constante | `NK_CUSTOM_PAYLOAD_MAX` (= 128) | Taille max du payload inline. |
| Inline | `NkCustomEvent` — `Get/SetCustomType`, `Get/SetUserPtr`, `SetPayload<T>`, `GetPayload<T>`, `GetRawPayload`, `GetDataSize`, `HasPayload` | Payload ≤128 o, zéro alloc (`static_assert sizeof≤128`). |
| Possédé | `NkCustomPtrEvent` — `GetCustomType`, `Get/SetUserPtr`, `GetData`, `GetBytes`, `GetSize`, `HasData`, `ViewAs<T>` | Tampon `NkVector<uint8>` possédé ; vue zéro-copie. |
| Texte | `NkCustomStringEvent` — `GetCustomType`, `GetTag`, `GetMessage`, `HasTag`, `HasMessage`, `IsTag` | Message UTF-8 + tag (comparaison exacte). |

### `NkDropEvent` — catégorie `NK_CAT_DROP` · `NkDropSystem`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkDropType` (`UNKNOWN/FILE/TEXT/IMAGE/URL`) | Nature du contenu. |
| Données | `NkDropEnterData`, `NkDropOverData`, `NkDropLeaveData`, `NkDropFilePath`, `NkDropFileData`, `NkDropTextData`, `NkDropImageData` | Structs payload (membres publics). |
| Geste | `NkDropEnterEvent`, `NkDropOverEvent`, `NkDropLeaveEvent` | Entrée / survol / sortie. |
| Dépôt | `NkDropFileEvent`, `NkDropTextEvent`, `NkDropImageEvent` | Fichiers / texte / image (membre public `data`). |
| Système | `NkEnableDropTarget(handle)`, `NkDisableDropTarget(handle)` | Armer/désarmer une fenêtre native (no-op si null). |

### `NkTransferEvent` — catégorie `NK_CAT_TRANSFER`, type `NK_TRANSFER`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums/fns | `NkTransferDirection`, `NkTransferProtocol`(+ToString), `NkTransferStatus`(+ToString) | Sens, protocole, statut. |
| Base | `NkTransferEvent` — `GetTransferId` | Base ; identifiant de transfert. |
| Début | `NkTransferBeginEvent` — `GetTransferName`, `GetTotalBytes`, `GetDirection`, `GetProtocol`, `IsSend/Receive`, `IsSizeKnown` | Démarrage. |
| Progrès | `NkTransferProgressEvent` — `GetBytesTransferred/TotalBytes/SpeedBytesPerSec`, `GetProgressPercent` (-1 si inconnu), `GetSpeedKBps` | Avancement. |
| Fin | `NkTransferCompleteEvent` — `GetTotalBytes`, `GetDurationMs`, `GetOutputPath`, `GetAverageSpeedBytesPerSec` | Succès. |
| Fin | `NkTransferErrorEvent` — `GetStatus`, `GetMessage`, `GetBytesTransferred` | Échec. |
| Fin | `NkTransferCancelledEvent` — `GetBytesTransferred` | Annulation. |
| Stream | `NkTransferDataEvent` — `GetData`, `GetOffset`, `GetSize`, `GetBytes` | Chunk de données possédées. |

---

## Référence complète

Chaque élément repris en détail, avec son comportement et ses usages par domaine. Les classes
purement « signal » (sans données) sont brèves ; celles qui portent une logique le sont à fond.

### Les fondations : `NkWindowId`, `NkSafeArea`

`NkWindowId` est un simple `using = uint64`, et `NK_INVALID_WINDOW_ID = 0` sert de sentinelle :
toute valeur `0` désigne un événement **global** (pas rattaché à une fenêtre). En multi-fenêtres, on
compare le `windowId` de l'événement à celui de chaque fenêtre pour router le traitement.

`NkSafeAreaInsets` modélise les marges *infranchissables* d'un écran (encoche, coins arrondis, barre
de gestes). Ses quatre `float32` sont en **pixels physiques** ; le constructeur prend les marges dans
l'ordre **`t, b, l, r`** (top, bottom, left, right). Ses helpers évitent l'arithmétique manuelle :
`UsableWidth/Height(total)` retire les marges en **clampant à 0** (jamais de largeur négative), et
`ClipPoint(x,y,w,h)` teste si un point tombe dans la zone sûre. Usages : **UI/2D** — ancrer les
boutons et le HUD à l'intérieur des marges sur mobile ; **outils/éditeur** — dessiner le cadre de
sécurité d'un *layout* responsive. `NkSafeAreaData` y ajoute la taille de l'écran, et
`NkScreenOrientation` (`AUTO/PORTRAIT/LANDSCAPE`) sert de type partagé pour l'orientation.

### `NkAppEvent` et ses dérivés

La base `NkAppEvent` n'est qu'un porteur de catégorie. Les dérivés portent la valeur.

- **`NkAppLaunchEvent`** — `argc`/`argv` sont **non possédés** : valides tant que le caller les
  maintient en vie. `GetArg(i)` est **borné** et renvoie `nullptr` hors `[0,argc)` ou si `argv` est
  null — donc sûr à appeler sans test préalable. *Outils/éditeur* : parser les options de ligne de
  commande au démarrage (chemin de projet, mode batch).
- **`NkAppTickEvent`** — sépare `deltaTime` et `totalTime`, et offre `GetFps()` = `1/delta` (ou `0`
  si le delta est nul, **pas d'assert**). *Tous domaines* : c'est le battement de la boucle, consommé
  par la **physique**, l'**animation**, l'**IA**, l'**audio** (avancer un curseur de lecture) ; *UI*
  pour un compteur de FPS.
- **`NkAppUpdateEvent`** — `IsFixedStep()` distingue le pas **fixe** (simulation déterministe :
  physique, réseau) du pas variable (présentation). C'est la pierre angulaire du découplage
  *simulation / rendu*.
- **`NkAppRenderEvent`** — `GetAlpha()` est le facteur d'**interpolation** `[0,1]` entre les deux
  derniers pas de simulation : en *rendu*, on l'utilise pour afficher les objets à leur position
  interpolée et lisser le mouvement malgré un pas fixe plus lent que l'affichage. `GetFrameIndex()`
  numérote les frames.
- **`NkAppCloseEvent`** — `Cancel()` ne pose `mCancelled` **que si** `!IsForced()` (sinon ignoré
  silencieusement) ; `IsCancelled()` relit l'état. *Gameplay/outils* : intercepter la fermeture pour
  sauvegarder, proposer un dialogue, refuser tant qu'un export tourne — mais respecter une extinction
  forcée du système.

### `NkWindowEvent` : dimensions et position

C'est le cœur de la sous-famille pour le **rendu**. `NkWindowResizeEvent` ne se contente pas de la
nouvelle taille : il garde l'**ancienne** et expose `GotLarger()` (`w>prevW || h>prevH`) /
`GotSmaller()` (`w<prevW || h<prevH`) plus un `ResizeState` (`NK_EXPANDED`/`NK_REDUCED`/
`NK_NOT_CHANGE`/`NK_NOT_DEFINE`).

- **Rendu / GPU** : l'événement à brancher sur `RecreateSwapchain` + reconfiguration du viewport et
  de la matrice de projection. Comparer ancien/nouveau évite de recréer pour rien.
- **UI/2D** : recalculer la mise en page, repositionner les ancrages, reflow du texte.
- **Optimisation** : `ResizeBegin`/`ResizeEnd` encadrent un **glissement de poignée** — on peut
  baisser la qualité ou suspendre la recréation coûteuse pendant le drag et ne la faire qu'au
  `ResizeEnd`.

`NkWindowMoveEvent` (avec `GetDeltaX/Y`) suit le déplacement, encadré de `MoveBegin`/`MoveEnd` ;
utile pour repositionner des fenêtres satellites (palettes d'**éditeur**) relatives à la principale.
`NkWindowPaintEvent` distingue le repeint **complet** (`IsFullPaint()`) du **rectangle sale**
(`GetDirtyX/Y/W/H`) — sur un backend logiciel ou une app *retained*, ne redessiner que la zone
invalidée économise énormément.

### `NkWindowEvent` : focus, état, visibilité

Cette série est faite d'événements **sans données** — le **type** porte toute l'information :

- **`FocusGained`/`FocusLost`** — *gameplay* : pause automatique quand on perd le focus ; *audio* :
  baisser ou couper le son en arrière-plan ; *outils* : suspendre un rafraîchissement coûteux.
- **`Minimize`/`Maximize`/`Restore`** — *rendu* : couper la boucle de rendu en minimisé (ne pas
  chauffer le GPU pour rien) et la reprendre au `Restore`.
- **`Fullscreen`/`Windowed`** — *rendu* : ajuster la stratégie de présentation et le mode swapchain.
- **`Shown`/`Hidden`** — pendant logique de la visibilité d'une fenêtre.

### `NkWindowEvent` : haute densité et mobile

C'est le bloc qui rend le moteur multi-plateforme propre.

- **`NkWindowDpiEvent`** (`GetScale`, `GetPrevScale`, `GetDpi`) — un changement d'échelle équivaut à
  un **resize en pixels physiques** : *rendu* recrée la swapchain à la bonne résolution, *UI* met les
  tailles à l'échelle (polices, marges) pour rester lisible sur écran HiDPI.
- **`NkWindowThemeEvent`** (`IsDark/IsLight`, `NkWindowTheme` : Unknown/Light/Dark/HighContrast) —
  *UI/éditeur* : basculer la palette de thème quand l'OS passe en sombre.
- **`NkWindowSurfaceCreated/DestroyedEvent`** (largeur/hauteur en **pixels physiques**) — crucial sur
  **Android** : la surface native peut être détruite quand l'app passe en arrière-plan. *GPU* :
  `SurfaceDestroyed` → libérer la swapchain et les ressources liées à la surface ; `SurfaceCreated` →
  les recréer. Ignorer ce couple = crash au retour de veille.
- **`NkWindowOrientationChangedEvent`** (`IsPortrait`/`IsLandscape`, `GetRotationDeg`) — *rendu/UI* :
  reconfigurer la projection et le layout au passage portrait↔paysage. Les helpers tiennent compte à
  la fois de l'orientation déclarée **et** de l'angle (portrait si deg ∈ {0,180}, paysage si {90,270}).
- **`NkWindowSafeAreaChangedEvent`** (`GetInsets`, `GetTop/Right/Bottom/Left`, `HasInsets`) — *UI* :
  re-ancrer le HUD dans la zone sûre quand les marges changent (rotation, ouverture du clavier).
- **`NkWindowVirtualKeyboardChangedEvent`** (`IsVisible`, `GetHeight`, `IsShowing` = visible & h>0,
  `IsHiding` = !visible) — *UI* : remonter le champ de saisie au-dessus du clavier logiciel, restaurer
  la mise en page à la fermeture.

Note pratique : il n'existe **pas** d'enum `NkWindowState`. Pour journaliser un état, on dispose
seulement de `NkWindowStateToString(uint32)` qui mappe des **codes numériques** (0=UNDEFINED …
6=CLOSED, défaut UNKNOWN).

### `NkSystemEvent` : alimentation, mémoire, écrans, langue

- **`NkSystemPowerEvent`** — `NkPowerState` couvre tout le spectre (batterie faible/critique,
  branché/débranché, suspend/resume/hibernate/shutdown). `GetBatteryLevel()` ∈ `[0,1]` ou `-1` si
  inconnu. Les helpers `IsSuspending()` (SUSPEND/HIBERNATE/SHUTDOWN) et `IsResuming()` (RESUME) sont
  les plus utiles. *GPU/threading* : à la suspension, **flusher** le GPU, sauvegarder, mettre les
  threads en sommeil ; à la reprise, tout recréer. *Gameplay* : basculer en mode économie (réduire
  la fréquence de simulation) sur batterie faible.
- **`NkSystemMemoryEvent`** — `NkMemoryPressure` (normal/modéré/critique), octets dispo/total,
  `IsCritical()`, `GetAvailableMb()`. *Tous domaines* : sur pression critique, vider caches de
  textures, d'assets, d'audio décodé avant que l'OS ne tue le processus.
- **`NkSystemDisplayEvent`** — `NkDisplayChange` (ajout/retrait/résolution/orientation/DPI/principal)
  + un `NkDisplayInfo` complet (taille logique/physique, `refreshRate`, `dpiScale/dpiX/dpiY`,
  `posX/posY`, `isPrimary`, `name[64]`). *Rendu/outils* : déplacer une fenêtre orpheline quand son
  écran est débranché ; adapter la cadence cible au `refreshRate` du nouvel écran.
- **`NkSystemLocaleEvent`** — ancienne et nouvelle locale (buffers `char[48]`, troncation silencieuse
  possible via `strncpy`). *UI/outils* : recharger les traductions à chaud.

### `NkSystemEvent` : les trois classes à type partagé

À distinguer **impérativement** par `As<T>()`, jamais par `GetType()` (elles empruntent
`NK_SYSTEM_LOCALE`/`NK_SYSTEM_DISPLAY`) :

- **`NkSystemTimeZoneEvent`** (type `NK_SYSTEM_LOCALE`) — `GetTimeZoneId`, `GetPrevTimeZoneId`,
  `GetOffsetMinutes`. *Outils/gameplay* : recalculer des horodatages, des événements calendaires.
- **`NkSystemThemeEvent`** (type `NK_SYSTEM_DISPLAY`) — `Theme` (Light/Dark/HighContrast) + couleur
  d'**accentuation** (`GetAccentR/G/B`), `IsDark/IsLight/IsHighContrast`. *UI/éditeur* : aligner la
  charte de couleurs sur celle du bureau, y compris la teinte d'accent.
- **`NkSystemAccessibilityEvent`** (type `NK_SYSTEM_DISPLAY`) — `ReduceMotion`, `IncreaseContrast`,
  `InvertColors`, `BoldText`, `LargeText`, `GetFontScale`. *UI/animation* : couper les transitions
  animées si `ReduceMotion`, mettre le texte à l'échelle de `GetFontScale`, respecter le contraste.

### `NkGraphicsEvent` : la vie du contexte GPU

Tous empruntent un type `NK_APP_*` (voir l'aperçu) : **identifier par `As<T>()`**.

- **`NkGraphicsContextReadyEvent`** — `GetApi()` (`NkGraphicsApi` : GL/GLES/VK/DX11/DX12/Metal/WebGL/
  WebGPU/Software) + taille initiale. *GPU* : créer pipelines, render targets, buffers persistants.
- **`NkGraphicsContextLostEvent`** — perte de device avec `GetReason()` (le **seul** ctor
  *non-noexcept* du lot, la `reason` étant une `NkString`). *GPU* : tout reconstruire après un
  *device removed* (DX/Vulkan), un changement d'adaptateur, un crash driver.
- **`NkGraphicsContextResizeEvent`** — ancienne/nouvelle taille + `GetAspectRatio()` (`w/h`, `0` si
  `h==0`) prêt pour la **matrice de projection**.
- **`NkGraphicsFrameBeginEvent` / `NkGraphicsFrameEndEvent`** — `frameIndex` + `frameInFlight` à
  l'entrée ; `GetGpuTimeMs()`/`GetCpuTimeMs()` à la sortie. *Outils/profilage* : alimenter un graphe
  de performance, détecter les pics de coût GPU/CPU.
- **`NkGraphicsGpuMemoryEvent`** — `NkGpuMemoryLevel` (normal/low/critical), Mo dispo/total,
  `IsCritical()`. *GPU* : libérer des textures non essentielles, baisser la résolution de rendu sous
  pression VRAM.
- **`NkGraphicsVSyncEvent`** — écran + `GetRefreshRateHz()`. *Rendu* : caler la cadence cible sur le
  taux de rafraîchissement réel (60/120/144 Hz).

### `NkCustomEvent` : trois véhicules de messages

- **`NkCustomEvent`** (inline ≤ `NK_CUSTOM_PAYLOAD_MAX` = 128 o, **zéro alloc tas**) —
  `SetPayload<T>()` impose `static_assert(sizeof(T) ≤ 128)` à la **compilation** et exige un type
  *trivially copyable* (copie via `NkMemCopy`, `O(sizeof T)`). `GetPayload<T>()` renvoie `false` si
  la taille stockée est trop petite, **mais ne vérifie pas le type** (mismatch = **UB**) :
  contrôlez `GetCustomType()` d'abord. `GetRawPayload()` (const et non-const) donne l'accès brut.
  *Gameplay/ECS* : notifications légères (score, état, déclencheur) entre systèmes sans allocation.
- **`NkCustomPtrEvent`** (payload dynamique **possédé**, `NkVector<uint8>`) — pour les charges de
  taille quelconque. `ViewAs<T>()` réinterprète le tampon **sans copie** (`nullptr` si `size <
  sizeof(T)`), valide tant que l'événement vit ; mismatch de type = **UB**. Ctor *non-noexcept*
  (move du vecteur). *Réseau/IO* : transporter un paquet sérialisé, un blob d'asset.
- **`NkCustomStringEvent`** (texte **UTF-8** + tag) — `IsTag("...")` compare le tag **exactement,
  sensible à la casse**. Ctor *non-noexcept*. *Outils/debug* : canal de messages catégorisés
  (`tag = "réseau"`, `"physique"`…), journalisation in-game.

Les trois exposent un `userPtr` (`void*`) **jamais possédé** : un pointeur d'accompagnement vers un
objet dont la durée de vie reste à votre charge.

### `NkDropEvent` : le glisser-déposer

Particularité de toute la catégorie : le payload est un **membre public `data`** (sauf
`NkDropLeaveEvent`, qui n'en a pas). On lit donc `ev.data.paths`, `ev.data.text`, etc.

- **Geste** : `NkDropEnterEvent` (`NkDropEnterData` : position, `numFiles`, `hasText`/`hasImage` —
  un aperçu pour décider à l'avance si on accepte le contenu), `NkDropOverEvent` (position courante,
  pour un retour visuel suivant le curseur), `NkDropLeaveEvent` (sortie).
- **Dépôt** : `NkDropFileEvent` (`NkDropFileData` : `paths`, `AddPath`, `Count`) — *outils/éditeur* :
  importer des assets par glissé ; `NkDropTextEvent` (texte + MIME) — *UI* : coller du texte ;
  `NkDropImageEvent` (`NkDropImageData` : `pixels` RGBA8, `width/height`, `HasPixels()`) —
  *rendu/éditeur* : créer une texture directement depuis l'image déposée.

`NkDropFilePath` enveloppe un chemin dans un `char[512]` (constructeur `strncpy` borné, null-safe).
Côté activation, **`NkDropSystem.h`** fournit les deux seules fonctions cross-platform du module :
`NkEnableDropTarget(nativeHandle)` arme une fenêtre native comme cible (choisit le backend selon la
plateforme, thread-safe via singleton, **no-op si le handle est null**) et `NkDisableDropTarget`
la désarme sans détruire le backend.

### `NkTransferEvent` : les transferts de longue durée

Un transfert (fichier, HTTP, FTP, Bluetooth, USB, IPC…) est identifié par un `transferId` que **tous**
les événements de la catégorie portent (`GetTransferId()`). Le cycle :

- **`NkTransferBeginEvent`** — nom, `GetTotalBytes`, `GetDirection` (`IsSend`/`IsReceive`),
  `GetProtocol`, `IsSizeKnown()` (totalBytes>0). Ctor *non-noexcept* (move du nom). *IO/réseau* :
  ouvrir une barre de progression, préparer le fichier de sortie.
- **`NkTransferProgressEvent`** — octets transférés, vitesse, `GetSpeedKBps()`, et surtout
  `GetProgressPercent()` qui vaut `100·bytes/total` **ou `-1.0` si la taille totale est inconnue**
  (pas d'assert sur la division). *UI* : barre déterminée ou indéterminée selon ce `-1`.
- **Épilogue** : `NkTransferCompleteEvent` (durée, `GetOutputPath`, `GetAverageSpeedBytesPerSec` = 0
  si durée nulle), `NkTransferErrorEvent` (`NkTransferStatus` : error/timeout/denied/corrupted/
  cancelled + message), `NkTransferCancelledEvent`. Ces trois ctors prenant une `NkString` sont
  *non-noexcept*.
- **`NkTransferDataEvent`** — **streaming** : les octets eux-mêmes (`NkVector<uint8>` **possédé**) +
  un `GetOffset()`. *IO/réseau* : consommer un flux par morceaux sans attendre la fin (lecture
  progressive, assemblage à l'offset).

### Pièges transversaux à retenir

- **`Clone()`** alloue avec le **`new` du CRT** — le receveur doit `delete`. C'est l'unique
  allocation hors NKMemory du module ; ne pas la confondre avec les pools NKMemory.
- **Types réutilisés** : les 7 classes graphiques empruntent `NK_APP_*` ; TimeZone/Theme/
  Accessibility empruntent `NK_SYSTEM_LOCALE`/`NK_SYSTEM_DISPLAY`. **Filtrer par `GetType()` ne les
  sépare pas** — passer par `As<T>()`.
- **Payload non typé** : `GetPayload<T>()` et `ViewAs<T>()` ne vérifient **que la taille**, pas le
  type — toujours contrôler `GetCustomType()`/`GetSize()` avant.
- **`userPtr` non possédé** ; **buffers `char` fixes** (`name[64]`, `locale[48]`, `tzId[64]`,
  `path[512]`) remplis par `strncpy` → **troncation silencieuse** au-delà de leur taille.
- **Ctors non-noexcept** : toutes les classes prenant une `NkString`/`NkVector` par valeur
  (transfer Begin/Complete/Error/Data, CustomPtr/CustomString, ContextLost) ; les autres sont
  `noexcept`.
- **Sentinelles sur division par zéro** : `GetFps()`→0, `GetProgressPercent()`→-1,
  `GetAspectRatio()`→0, `GetAverageSpeedBytesPerSec()`→0 — pas d'assert, à tester côté appelant.

---

### Exemple récapitulatif

```cpp
#include "NKEvent/NkApplicationEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkSystemEvent.h"
#include "NKEvent/NkGraphicsEvent.h"
#include "NKEvent/NkCustomEvent.h"
using namespace nkentseu;

void OnEvent(NkEvent* e) {
    // Cycle de l'app : cadence + fermeture annulable.
    if (auto* tick = e->As<NkAppTickEvent>())
        sim.Step(tick->GetDeltaTime());
    if (auto* close = e->As<NkAppCloseEvent>())
        if (!close->IsForced() && doc.IsDirty()) close->Cancel();

    // Fenêtre : redim. → swapchain (en comparant ancien/nouveau).
    if (auto* r = e->As<NkWindowResizeEvent>())
        if (r->GotLarger() || r->GotSmaller())
            renderer.RecreateSwapchain(r->GetWidth(), r->GetHeight());

    // Système : suspension → flush GPU + sauvegarde.
    if (auto* p = e->As<NkSystemPowerEvent>())
        if (p->IsSuspending()) { renderer.Flush(); doc.Save(); }

    // Graphique : perte de device → tout reconstruire (type partagé NK_APP_CLOSE).
    if (auto* lost = e->As<NkGraphicsContextLostEvent>())
        renderer.Recreate(lost->GetReason());

    // Custom : message gameplay léger, sans allocation.
    if (auto* c = e->As<NkCustomEvent>())
        if (c->GetCustomType() == MyEvents::SCORE) {
            struct ScoreMsg { int player; int points; } m;
            if (c->GetPayload(m)) hud.AddScore(m.player, m.points);
        }
}
```

---

[← Index NKEvent](README.md) · [Récap NKEvent](../NKEvent.md) · [Couche Runtime](../README.md)
