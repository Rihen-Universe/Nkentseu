# Le cœur applicatif

> Couche **Engine** · Noge · Le squelette d'une application : la classe `NkApplication`, sa
> configuration `NkApplicationConfig`, le point d'entrée `NkMainApplication`, les couches
> `NkLayer` / `NkOverlay` empilées dans `NkLayerStack`, le bus d'événements `NkEventBus` et le
> profileur `NkProfiler`.

Sous chaque module, chaque démo, chaque jeu écrit avec Nkentseu, il y a toujours **la même
mécanique** : ouvrir une fenêtre, créer un device GPU, entrer dans une boucle qui met à jour le
monde puis le dessine, et tout refermer proprement quand l'utilisateur ferme. Écrire cette
mécanique à la main, à chaque projet, c'est répéter cent lignes de plomberie identiques et fragiles
(l'ordre d'initialisation, le cap du delta-time, la propagation des événements). Le **cœur de
Noge** capture cette plomberie une fois pour toutes : vous héritez de `NkApplication`, vous
remplissez quelques **callbacks** (`OnUpdate`, `OnRender`…), et la boucle, la fenêtre, le device, la
pile de couches et le routage d'événements vous sont fournis.

Ce n'est **pas** un moteur de rendu (ça, c'est NKRenderer) ni un ECS (ça, c'est NKECS, intégré plus
haut par `NkEngineLayer`) : c'est la **charpente** qui fait tenir tout le reste ensemble et qui
donne à votre code un endroit précis où s'accrocher dans le cycle de vie.

- **Namespace** : `nkentseu` (le point d'entrée `NkMainApplication` est déclaré au namespace global)
- **Headers** : `#include "Noge/Nkentseu.h"` (parapluie) ou individuellement
  `Core/NkApplication.h`, `Core/NkApplicationConfig.h`, `Core/NkMainApp.h`, `Core/NkLayer.h`,
  `Core/NkLayerStack.h`, `Core/NkEventBus.h`, `Core/NkProfiler.h`

---

## L'application : `NkApplication` et sa configuration

`NkApplication` est une classe **abstraite** dont on hérite. On ne l'instancie jamais directement :
on dérive une classe à soi, on surcharge les callbacks utiles, et le moteur s'occupe du reste. Son
cycle de vie est rigoureusement ordonné — `Init` ouvre la plateforme et le device, puis appelle vos
hooks dans cet ordre : `OnPreInit` (avant que la fenêtre et le device n'existent) → création de la
fenêtre et du device → `OnInit` (ils existent maintenant) → `OnStart` (une seule fois, juste avant
la première frame) → **boucle** → `OnShutdown` → destruction.

Cet ordre n'est pas décoratif : c'est lui qui dit **où** mettre quoi. La fenêtre et le device GPU
n'existent pas encore dans `OnPreInit` — c'est l'unique endroit où modifier `mConfig` (changer la
taille de fenêtre, le backend GPU) avant qu'ils ne soient créés. Inversement, charger une texture
ou compiler un shader doit attendre `OnInit`, car cela réclame le device.

```cpp
class MyGame : public NkApplication {
public:
    explicit MyGame(const NkApplicationConfig& cfg) : NkApplication(cfg) {}

    void OnPreInit() override   { mConfig.windowConfig.title = "Mon Jeu"; }  // avant fenêtre
    void OnInit() override      { /* charger assets : device prêt */ }
    void OnUpdate(float dt) override   { /* logique de jeu, chaque frame */ }
    void OnRender() override            { /* soumission GPU */ }
    void OnShutdown() override          { /* libérer ses ressources */ }
};
```

La configuration arrive par `NkApplicationConfig`, une simple **struct à champs publics** passée au
constructeur. Elle décrit le nom et la version de l'app, le niveau de log, et surtout deux réglages
de **timing** qui méritent qu'on s'y arrête : `fixedTimeStep` (le pas fixe pour la physique et le
réseau, `1/60` s par défaut, `0` pour le désactiver) et `maxDeltaTime` (`0.25` s, le **cap** du
delta pour éviter la *spiral of death* — quand une frame très longue produirait un dt énorme qui
ferait exploser la simulation). Elle porte aussi `windowConfig` (la fenêtre) et `deviceInfo` (le
device GPU RHI).

On accède aux ressources créées via les accesseurs : `GetWindow()`, `GetDevice()`, `GetCmd()` (le
command buffer), `GetConfig()`. Un singleton statique `NkApplication::Get()` donne accès à
l'instance unique de n'importe où — **mais attention**, il déréférence un pointeur interne *sans
garde* : l'appeler avant que l'application ne soit construite est un comportement indéfini.

> **En résumé.** On hérite de `NkApplication`, on surcharge les callbacks, et le moteur fournit la
> boucle, la fenêtre et le device. `OnPreInit` = avant la fenêtre (modifier `mConfig` ici uniquement) ;
> `OnInit` = device prêt (charger les assets). `NkApplicationConfig` règle le timing : `fixedTimeStep`
> pour la physique, `maxDeltaTime` pour caper le dt. `Get()` n'a **pas** de garde — ne l'appelez pas
> trop tôt.

---

## Le point d'entrée : `NkMainApplication`

Un programme C++ a besoin d'un `main`. Mais un `main` portable diffère d'une plateforme à l'autre
(Windows veut `WinMain`, Android un `android_main`, etc.). Noge cache cette divergence : **vous
n'écrivez jamais de `main`**. À la place, vous **définissez une fonction de fabrique** que le moteur
appellera :

```cpp
nkentseu::NkApplication* NkMainApplication(const nkentseu::NkApplicationConfig& config) {
    return new MyGame(config);
}
```

C'est tout. La fonction `nkmain`, définie *inline* dans `NkMainApp.h` et appelée par le point
d'entrée natif de chaque plateforme, se charge du reste : elle construit la config, appelle votre
`NkMainApplication`, puis enchaîne `Init()`, `Run()` (qui **bloque** jusqu'à la fermeture), et
`delete` l'application. Ses codes de retour sont `1` (création échouée), `2` (init échouée), `0`
(succès).

Ce n'est **pas** un `main` que vous écrivez vous-même : c'est une *fabrique* que le moteur invoque.
Deux pièges réels à connaître. D'abord, `nkmain` étant défini inline dans le header, **n'incluez
`NkMainApp.h` qu'une seule fois** dans tout le programme (sinon violation de l'ODR). Ensuite, la
déclaration de `NkMainApplication` se trouve dans le namespace **global** alors que l'appel interne
la qualifie `nkentseu::NkMainApplication` — une incohérence visible dans le header ; définissez-la
au plus simple comme ci-dessus et vérifiez l'édition de liens si le symbole reste introuvable.

> **En résumé.** Pas de `main` à écrire : définissez `NkMainApplication(config)` qui renvoie votre
> `NkApplication*`. `nkmain` (inline dans `NkMainApp.h`) enchaîne `Init`→`Run`→`delete`. N'incluez
> `NkMainApp.h` **qu'une fois** (ODR).

---

## Les couches : `NkLayer`, `NkOverlay`, `NkLayerStack`

Une application réelle n'est pas un bloc monolithique : c'est un **empilement de responsabilités**
— le monde du jeu, l'interface, un panneau de debug, un éditeur par-dessus. Plutôt que de tout
mélanger dans `OnUpdate`/`OnRender`, Noge structure ce code en **couches**. Une `NkLayer` est un
morceau autonome avec son propre cycle de vie miniature : `OnAttach` (quand on l'ajoute),
`OnDetach` (quand on la retire), `OnUpdate`, `OnFixedUpdate`, `OnRender`, `OnEvent`, `OnUIRender`.
On en empile autant qu'on veut via `PushLayer`.

Une `NkOverlay` est **exactement** une `NkLayer`, à une nuance près : elle est placée **au-dessus**
de la pile. Les overlays servent à ce qui doit dessiner par-dessus tout et capter les événements en
premier : une UI de debug, un menu pause, les outils d'un éditeur.

L'ordre dans la pile a une logique précise, et c'est tout l'intérêt. La pile est rangée
`[Couches… | Overlays…]`. La **mise à jour** la parcourt de **gauche à droite** (le monde se met à
jour avant l'UI). Le **routage des événements**, lui, va de **droite à gauche** : l'overlay du
dessus voit l'événement en premier, et s'il le **consomme** (son `OnEvent` renvoie `true`), il
arrête la propagation — exactement ce qu'on veut quand un menu pause est ouvert : un clic sur le
menu ne doit pas atteindre le jeu en dessous.

```cpp
void MyGame::OnInit() {
    PushLayer(new GameWorldLayer());     // se met à jour en premier, voit les events en dernier
    PushOverlay(new DebugUILayer());     // dessine au-dessus, voit les events en premier
}
```

Le tout est détenu par `NkLayerStack`. Point capital sur la **propriété** : pousser une couche
**transfère son ownership** à la pile. C'est le destructeur de `NkLayerStack` qui fait `delete` sur
chaque couche — vous ne devez donc **jamais** détruire vous-même une couche que vous avez poussée,
sous peine de double-`delete`.

> **En résumé.** Découpez l'app en `NkLayer` (monde, UI…) ; une `NkOverlay` est une couche placée
> en haut. Update : **gauche→droite** (monde avant UI). Événements : **droite→gauche**, et `OnEvent`
> renvoyant `true` **consomme** et stoppe la propagation. La `NkLayerStack` **possède** les couches
> et les `delete` — ne les détruisez jamais vous-même.

---

## Le bus d'événements : `NkEventBus`

Les callbacks de couche (`OnEvent`) routent les événements *à travers la pile*, dans l'ordre. Mais
parfois un système veut simplement « être prévenu quand tel type d'événement arrive », sans se
soucier de la pile ni de l'ordre. C'est le rôle du `NkEventBus` : un mécanisme **publish/subscribe
typé**, entièrement **statique**, sans allocation dynamique (le foncteur est stocké *inline*).

On **s'abonne** à un type d'événement en fournissant un foncteur `bool(TEvent*)` (renvoyer `true`
consomme l'événement), ou une méthode membre. L'abonnement rend un identifiant
(`NkEventHandlerId`) qui sert à se désabonner. On **publie** avec `Dispatch<TEvent>(event)`, qui
parcourt les handlers et s'arrête au premier qui consomme.

```cpp
auto id = NkEventBus::Subscribe<NkKeyPressedEvent>([](NkKeyPressedEvent* e) {
    return e->key == NkKey::Escape;   // true = consommé
});
// ... plus tard
NkEventBus::Unsubscribe<NkKeyPressedEvent>(id);
```

Ce n'est **pas** un système illimité : par conception zéro-allocation, il a des **limites
statiques** — `NK_EVENTBUS_MAX_HANDLERS` (32 handlers par type d'événement) et
`NK_EVENTBUS_MAX_EVENT_TYPES` (64 types distincts). Et le foncteur capturé doit tenir dans le
stockage *inline* : un lambda qui capture trop est rejeté à la compilation par `static_assert`.

> ⚠️ **Bug connu (à signaler).** Les deux surcharges de `Subscribe` référencent dans leur
> `static_assert` un symbole `internal::NkkLambdaStorage` (double `k`) **qui n'est pas déclaré** —
> seul `kLambdaStorage` existe. En l'état, `Subscribe` **ne compile pas**. C'est le bug
> `NkkLambdaStorage`/`kLambdaStorage` déjà répertorié dans l'audit de Noge ; il doit être corrigé
> avant d'utiliser le bus.

> **En résumé.** `NkEventBus` = pub/sub **typé, statique, zéro-alloc**. `Subscribe<T>(foncteur)` /
> `Subscribe<T>(obj, &Cls::fn)` rend un id ; `Dispatch<T>` publie (arrêt au premier qui consomme) ;
> `UnsubscribeAll(obj)` dans `OnDetach`. Limites : **32** handlers/type, **64** types. ⚠️ Bug
> `NkkLambdaStorage` empêche actuellement `Subscribe` de compiler.

---

## Le profileur : `NkProfiler`

Quand une frame rame, il faut **mesurer** avant de deviner. `NkProfiler` est un singleton de
profilage : on encadre une portion de code avec `Begin(name)` / `End(name)`, le profileur
chronomètre l'intervalle et accumule des statistiques (min, max, total, moyenne) par nom de marqueur.
Chaque marqueur porte un **type** (`Frame`, `System`, `Query`, `Script`, `Custom`) pour classer ce
qu'on mesure.

En pratique on n'appelle presque jamais `Begin`/`End` à la main : on utilise la macro **RAII**
`NK_PROFILE_SCOPE(name)`, qui ouvre le marqueur au point d'appel et le ferme automatiquement à la
fin de la portée — impossible d'oublier le `End`, même sur un `return` anticipé.

```cpp
void GameWorldLayer::OnUpdate(float dt) {
    NK_PROFILE_SCOPE("World.Update");   // chronométré jusqu'à la fin du bloc
    UpdatePhysics(dt);
    UpdateAI(dt);
}
```

> ⚠️ **Pièges à signaler.** `NK_PROFILE_SCOPE` construit son nom de variable interne avec
> `##__LINE__`, mais cette concaténation **ne s'expanse pas** correctement (on obtient littéralement
> `__LINE__`) : deux `NK_PROFILE_SCOPE` dans le même bloc peuvent entrer en **collision de nom**.
> Mettez-en un seul par portée, ou ouvrez des sous-blocs `{ … }`. Par ailleurs, plusieurs méthodes
> (`Begin`, `End`, `GetStats`, `ExportToJSON`, `Reset`) sont **déclarées dans le header mais
> implémentées en `.cpp`** : leur comportement exact relève de l'implémentation.

> **En résumé.** `NkProfiler` mesure des intervalles nommés et agrège min/max/total/moyenne par
> marqueur. Préférez la macro RAII `NK_PROFILE_SCOPE(name)` à `Begin`/`End` manuels. Pièges : ne
> mettez **qu'un** `NK_PROFILE_SCOPE` par bloc (bug `##__LINE__`) ; plusieurs méthodes sont en `.cpp`.

---

## Aperçu de l'API

La liste de **tous** les éléments publics du cœur de Noge, en un coup d'œil. Chacun est détaillé
dans la « Référence complète ».

### `NkApplicationConfig` — `Core/NkApplicationConfig.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Identité | `appName`, `appVersion` | Nom et version de l'application |
| Journalisation | `logLevel` (`NkLogLevel::`), `debugMode` | Niveau de log ; mode debug |
| Timing | `fixedTimeStep`, `maxDeltaTime`, `targetFPS` | Pas fixe physique (`0`=off) ; cap du dt ; FPS cible (`0`=illimité) |
| Plateforme | `entryState`, `windowConfig`, `deviceInfo` | État d'entrée ; config fenêtre ; config device GPU |
| Fichier | `appFileConfigPath` | Chemin JSON/YAML de surcharge (mentionné, non chargé par le code) |

### `NkApplication` — `Core/NkApplication.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `NkApplication(config)`, `~NkApplication()`, `Init`, `Run`, `Quit` | Construit / détruit / initialise / boucle (bloquante) / arrête |
| Couches | `PushLayer(layer)`, `PushOverlay(overlay)` | Ajoute une couche / un overlay (ownership transféré) |
| Accès | `GetWindow`, `GetCmd`, `GetDevice`, `GetConfig`, `static Get()` | Fenêtre / command buffer / device / config / singleton (**sans garde**) |
| Hooks init | `OnPreInit`, `OnInit`, `OnStart` | Avant fenêtre / après fenêtre+device / une fois avant 1re frame |
| Hooks boucle | `OnUpdate(dt)`, `OnFixedUpdate(fixedDt)`, `OnRender`, `OnUIRender` | Chaque frame / pas fixe / soumission GPU / UI applicative |
| Hooks fin | `OnClose`, `OnResize(w, h)`, `OnShutdown` | Fermeture demandée (défaut=`Quit`) / redimensionnement / avant destruction |
| Membres protégés | `mConfig`, `mWindow`, `mDevice`, `mCmd`, `mLayerStack`, `mRunning` | Accessibles aux classes dérivées |

### `NkMainApp.h` — point d'entrée — `Core/NkMainApp.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| À définir | `NkApplication* NkMainApplication(config)` | **Fabrique** de l'app, fournie par l'utilisateur |
| Fourni | `int nkmain(state)` | Entrée cross-platform inline (`Init`→`Run`→`delete`), codes `0`/`1`/`2` |

### `NkLayer` / `NkOverlay` — `Core/NkLayer.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| `NkLayer` | `NkLayer(name)`, `~NkLayer()` | Construction (nom par défaut `"NkLayer"`) |
| Cycle de vie | `OnAttach`, `OnDetach`, `OnUpdate(dt)`, `OnFixedUpdate(fixedDt)`, `OnRender`, `OnUIRender` | Ajout / retrait / frame / pas fixe / rendu / UI |
| Événement | `OnEvent(event)` | Renvoie `true` pour **consommer** |
| Accès | `GetName()`, membre `mName` | Nom de la couche |
| `NkOverlay` | `NkOverlay(name)` | `NkLayer` placée **en haut** de la pile (défaut `"Overlay"`) |

### `NkLayerStack` — `Core/NkLayerStack.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `NkLayerStack()`, `~NkLayerStack()` | Le destructeur `delete` **toutes** les couches |
| Modification | `PushLayer`, `PushOverlay`, `PopLayer`, `PopOverlay` | Ajout/retrait (ownership transféré au push) |
| Itération | `begin`/`end` (update, gauche→droite), `rbegin`/`rend` (events, droite→gauche) | Parcours avant / inverse (pointeurs bruts) |
| Taille | `Size()` | Nombre de couches |

### `NkEventBus` — `Core/NkEventBus.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Types | `NkEventHandlerId`, `NK_INVALID_HANDLER_ID` (`0`) | Id de handler ; valeur invalide |
| Limites | `NK_EVENTBUS_MAX_HANDLERS` (32), `NK_EVENTBUS_MAX_EVENT_TYPES` (64) | Handlers/type ; types distincts max |
| Abonnement | `Subscribe<T>(foncteur)`, `Subscribe<T>(obj, &Cls::fn)` ⚠️ | Foncteur / méthode membre (⚠️ bug `NkkLambdaStorage`) |
| Désabonnement | `Unsubscribe<T>(id)`, `UnsubscribeAll(obj)` | Par id / tous ceux liés à `obj` |
| Publication | `Dispatch<T>(event)`, `DispatchRaw(typeId, event)` | Typé / par typeId runtime (arrêt au 1er consommateur) |
| Debug | `GetHandlerCount(typeId)`, `Clear()` | Compte ; réinitialise tout |

### `NkProfiler` — `Core/NkProfiler.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkProfiler::MarkerType::` `Frame` `System` `Query` `Script` `Custom` | Type de marqueur |
| Structs | `Marker`, `Stats` (avec `AvgTime()`) | Marqueur ; statistiques agrégées + moyenne |
| Singleton | `static Global()` | Instance unique |
| Mesure | `Begin(name, type)`, `End(name)`, `Sample(name, type)`, `NextFrame()` | Ouvrir / fermer / mesure ponctuelle / frame suivante |
| Lecture | `GetStats(name)`, `ExportToJSON(buffer, size)`, `Reset()` | Stats d'un marqueur / export / remise à zéro |
| Macros | `NK_PROFILE_SCOPE(name)` ⚠️, `NK_PROFILE_SAMPLE(name)` | Mesure RAII (⚠️ bug `##__LINE__`) ; échantillon ponctuel |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses cas d'usage à travers les domaines du moteur. Les
éléments encore **non implémentés en `.cpp`** (boucle, lifecycle déclaré au header) sont signalés
comme **spec** là où c'est le cas.

### `NkApplicationConfig` à fond

La config est une struct **à champs publics** : on la remplit avant de construire l'app, et on peut
encore l'ajuster dans `OnPreInit` (le moteur ne lit `windowConfig`/`deviceInfo` qu'au moment de
créer la fenêtre et le device, *après* `OnPreInit`).

- **Identité** — `appName` (`"NkApp"`) et `appVersion` (`"1.0.0"`) servent au titre de fenêtre, aux
  logs, aux chemins de sauvegarde par convention.
- **Journalisation** — `logLevel` (du scope `NkLogLevel::`, défaut `NK_INFO`) filtre la verbosité ;
  `debugMode` (`false`) active les chemins de debug (couches de validation GPU, asserts étendus).
- **Timing**, le réglage le plus structurant :
  - `fixedTimeStep` (`1/60` s) est le pas du *fixed update*, utilisé partout où le déterminisme
    compte — **physique** (intégration stable), **réseau** (ticks de simulation), **IA** à cadence
    fixe. Mettre `0` désactive complètement `OnFixedUpdate`.
  - `maxDeltaTime` (`0.25` s) **cape** le delta passé à `OnUpdate` : si une frame met une seconde
    (chargement, point d'arrêt débogueur), le dt est plafonné pour éviter qu'un objet ne traverse un
    mur ou qu'une animation ne saute — la fameuse *spiral of death*.
  - `targetFPS` (`0` = illimité) ; à `0` c'est le device (vsync) qui cadence.
- **Plateforme** — `entryState` (état d'entrée fourni par le launcher natif), `windowConfig` (taille,
  titre, mode), `deviceInfo` (choix du backend RHI, options du device GPU).
- **Fichier** — `appFileConfigPath` désigne un JSON/YAML de surcharge ; il est **mentionné dans la
  struct mais non chargé** par le code visible — considérez-le comme un point d'extension non câblé.

### `NkApplication` à fond

**Le cycle de vie**, dans l'ordre exact, est ce qui rend l'application prévisible :

- `Init()` initialise la plateforme puis le device GPU. *(Implémentée en `.cpp` — spec côté header.)*
- `OnPreInit()` — **avant** que la fenêtre et le device n'existent. Seul endroit légitime pour
  modifier `mConfig`. Cas d'usage : choisir le backend GPU selon la machine, ajuster la résolution
  lue d'un fichier de paramètres, activer le mode debug en build interne.
- Création de la fenêtre et du device.
- `OnInit()` — fenêtre et device **prêts**. C'est ici qu'on charge ce qui réclame le GPU : textures,
  meshes, shaders, fontes (**rendu**), tampons audio (**audio**), atlas d'UI (**UI/éditeur**).
- `OnStart()` — une fois, juste avant la première frame. Mise en place de la scène initiale,
  spawn des entités de départ (**gameplay/scène**), première requête réseau.
- **La boucle** appelle, par frame : `OnFixedUpdate(fixedDt)` (zéro, une ou plusieurs fois selon
  l'accumulateur, **uniquement si** `fixedTimeStep > 0`) → `OnUpdate(dt)` → `OnRender()` →
  `OnUIRender()`.
- `OnShutdown()` — avant destruction : libérer ses ressources, sauvegarder l'état.

**Les callbacks**, par domaine d'emploi :

- `OnUpdate(dt)` — toute la logique dépendante du temps réel : déplacement caméra, animations
  d'interface, interpolations (**animation/gameplay**), polling d'entrées.
- `OnFixedUpdate(fixedDt)` — tout ce qui doit être déterministe et à cadence constante : intégration
  **physique**, pas de simulation **réseau**, comportements d'**IA** synchronisés.
- `OnRender()` — la soumission GPU : c'est ici qu'on enregistre les *draw calls* via `GetCmd()` et
  qu'on dialogue avec `GetDevice()` (**rendu**).
- `OnUIRender()` — l'UI applicative, dessinée **après** `OnRender()` pour passer par-dessus la scène
  (**UI/éditeur**).
- `OnClose()` — déclenché quand l'utilisateur ferme la fenêtre ; son **défaut appelle `Quit()`**.
  Surchargez-le pour intercaler une confirmation (« Quitter sans sauvegarder ? »).
- `OnResize(w, h)` — au redimensionnement de la fenêtre : recréer la swapchain, ajuster les
  matrices de projection, repositionner l'UI (**rendu/UI**).

**Le pilotage** — `Run()` démarre la boucle et **bloque** jusqu'à `Quit()`. `Quit()` demande
l'arrêt propre (la frame en cours se termine). `PushLayer`/`PushOverlay` enrichissent la pile de
couches (voir plus bas, ownership transféré).

**Les accès** — `GetWindow()` (non-const), `GetDevice()`, `GetCmd()`, `GetConfig()` (const) exposent
les ressources. `mConfig`, `mWindow`, `mDevice`, `mCmd`, `mLayerStack`, `mRunning` sont **protégés**,
donc lisibles/modifiables par les classes dérivées. Le singleton `static Get()` renvoie l'instance
unique pour un accès global — **mais il déréférence `sInstance` sans vérifier `nullptr`** : appelé
avant la construction de l'application, c'est un comportement indéfini. Contrairement à lui,
`NkEngineLayer::Get()` (voir [Layers](Layers.md)) est protégé par assert.

> Note **implémentation** : `Init`, `Run`, `Quit` et toute la mécanique interne (boucle,
> accumulateur de temps fixe, dispatch des événements vers les couches, gestion du redimensionnement)
> sont définies en `.cpp` ; le header ne fait qu'en décrire le contrat.

### `NkMainApplication` et `nkmain` à fond

`NkMainApplication(config)` est la **seule** fonction que vous devez écrire pour démarrer : une
fabrique qui renvoie une instance de votre `NkApplication`. C'est le pont entre le code générique du
moteur et votre code spécifique.

`nkmain(state)` est défini **inline dans `NkMainApp.h`** et constitue l'entrée cross-platform réelle,
appelée par le point d'entrée natif de chaque OS. Il : construit la `NkApplicationConfig` à partir de
l'`NkEntryState` reçu, appelle votre `NkMainApplication`, puis `Init()`, `Run()` et `delete` l'app.
Ses **codes de retour** — `1` si la fabrique renvoie `nullptr`, `2` si `Init()` échoue, `0` en cas
de succès — sont utiles aux scripts de CI et aux launchers.

Deux **pièges** réels :
- **ODR** : `nkmain` étant inline dans le header, n'incluez `NkMainApp.h` qu'une seule fois dans tout
  le binaire, faute de quoi vous obtenez des définitions multiples.
- **Namespace** : la déclaration de `NkMainApplication` est au namespace **global**, alors que l'appel
  interne la qualifie `nkentseu::`. Définissez votre fabrique comme dans l'exemple et, en cas
  d'erreur d'édition de liens, vérifiez le namespace de votre définition.

### `NkLayer` et `NkOverlay` à fond

Une `NkLayer` (toutes ses méthodes sont *inline* dans le header) découpe l'application en modules
qui s'attachent et se détachent proprement :

- `OnAttach()` / `OnDetach()` — symétriques : allouer/abonner à l'attache, libérer/désabonner au
  détachement. C'est l'endroit idoine pour `NkEventBus::UnsubscribeAll(this)` dans `OnDetach`.
- `OnUpdate(dt)` — la logique de la couche, chaque frame.
- `OnFixedUpdate(fixedDt)` — n'est appelé que si `fixedTimeStep > 0` ; la couche **physique** ou
  **réseau** y met sa simulation déterministe.
- `OnRender()` — le rendu de la couche, après tous les `OnUpdate`, avant la présentation.
- `OnEvent(event)` — reçoit les événements routés par la pile ; **renvoie `true` pour consommer** et
  empêcher les couches du dessous de le voir.
- `OnUIRender()` — UI ou debug optionnel de la couche.
- `GetName()` / `mName` — un nom lisible (utile au debug, au profileur, à l'éditeur).

Exemples d'usage par domaine : une couche **monde** (entités, scène), une couche **UI** (HUD,
menus), une couche **debug** (overlay de stats), une couche **éditeur** (gizmos, sélection) — chacune
indépendante, empilée dans l'ordre voulu.

`NkOverlay : public NkLayer` est strictement identique en interface ; sa seule différence est sa
**position** : poussée par `PushOverlay`, elle est rangée au-dessus des couches normales, donc elle
**dessine en dernier** (par-dessus tout) et **voit les événements en premier**. C'est le bon choix
pour tout ce qui doit primer : menu pause, console, panneaux d'éditeur.

### `NkLayerStack` à fond

La pile range les couches dans l'ordre `[Couches… | Overlays…]`, la frontière étant un index interne
(`mLayerInsertIndex`) : `PushLayer` insère à cet index (et l'incrémente), `PushOverlay` ajoute à la
toute fin. Les deux **transfèrent l'ownership** à la pile.

Le point à ne jamais oublier : **le destructeur de `NkLayerStack` fait `delete` sur chaque couche**.
La pile (donc l'application) **possède** ses couches. Vous ne devez jamais les détruire vous-même ;
le faire produit un double-`delete` (corruption mémoire, cf. la règle dure NKMemory du projet).

L'itération a deux sens, et c'est délibéré :
- `begin()`/`end()` parcourent **gauche→droite** : c'est l'ordre de **mise à jour** (le monde
  d'abord, les overlays ensuite).
- `rbegin()`/`rend()` parcourent **droite→gauche** : c'est l'ordre de **routage des événements**
  (l'overlay du dessus en premier). Attention, ce ne sont **pas** des `reverse_iterator` STL : ce
  sont des **pointeurs bruts décrémentés** (`rbegin = end()-1`, `rend = begin()-1`), à itérer
  manuellement.

`Size()` donne le nombre de couches. `PopLayer`/`PopOverlay` retirent une couche par pointeur.

### `NkEventBus` à fond

Là où le routage par la pile traverse les couches *dans l'ordre*, le bus offre un **canal direct**
vers tout système intéressé par un type d'événement, sans dépendre de la pile.

- **Abonnement** — `Subscribe<TEvent>(foncteur)` prend un callable `bool(TEvent*)` (renvoyer `true`
  consomme). Le foncteur est stocké **inline** (pas d'allocation), d'où la limite de taille de
  capture. La surcharge `Subscribe<TEvent>(obj, &Cls::fn)` abonne une **méthode membre** —
  pratique pour qu'une couche ou un système réagisse via une de ses méthodes. Chaque abonnement rend
  un `NkEventHandlerId` ; `NK_INVALID_HANDLER_ID` (`0`) signale l'échec (table pleine, type
  introuvable).
- **Désabonnement** — `Unsubscribe<TEvent>(id)` retire un handler précis. `UnsubscribeAll(obj)`
  retire **tous** les abonnements liés à un objet — le réflexe à avoir dans `OnDetach` d'une couche
  pour éviter les handlers fantômes.
- **Publication** — `Dispatch<TEvent>(event)` parcourt les handlers du type et **s'arrête au premier
  qui consomme** (renvoie `true`). `DispatchRaw(typeId, event)` fait la même chose sans connaître le
  type à la compilation, à partir d'un `typeId` runtime — utile pour un routage générique (sérialisé,
  scripté).
- **Debug** — `GetHandlerCount(typeId)` compte les handlers d'un type ; `Clear()` réinitialise tout
  le registre.

Cas d'usage transverses : un système **audio** s'abonne aux événements « explosion » pour jouer un
son ; l'**IA** réagit à « joueur repéré » ; l'**UI** écoute « inventaire changé » ; l'**éditeur**
réagit à « entité sélectionnée ». Le bus découple l'émetteur du récepteur — l'émetteur ne sait pas
qui écoute.

**Contraintes & limites** — le bus repose sur `TEvent::StaticTypeId()` (contrat côté NKEvent). Ses
limites statiques (`NK_EVENTBUS_MAX_HANDLERS` = 32 par type, `NK_EVENTBUS_MAX_EVENT_TYPES` = 64
types) sont volontaires : zéro allocation dynamique, tout est pré-réservé.

> ⚠️ **Bug à signaler.** Les deux `Subscribe` invoquent dans leur `static_assert` un
> `internal::NkkLambdaStorage` (avec un `k` en trop) qui **n'est pas déclaré** — seul
> `internal::kLambdaStorage` (= 64) existe (et `NkDispatchMember`, lui, l'utilise correctement). En
> conséquence, `Subscribe` **ne compile pas** en l'état. C'est le bug `NkkLambdaStorage`/
> `kLambdaStorage` connu de l'audit Noge, à corriger avant tout usage du bus.

Le sous-espace `nkentseu::internal` (types `NkHandlerSlot`, `NkEventTable`, `NkBusRegistry`,
fonctions `NkGetRegistry()`, templates de dispatch) est **interne** : ne l'utilisez pas directement.

### `NkProfiler` à fond

Le profileur agrège, par **nom de marqueur**, le nombre d'échantillons et les temps min/max/total ;
`Stats::AvgTime()` en déduit la moyenne (et renvoie `0` si aucun échantillon). Chaque marqueur a un
**type** (`NkProfiler::MarkerType::Frame`, `System`, `Query`, `Script`, `Custom`) pour ranger les mesures par
catégorie.

- `Global()` — accès au singleton.
- `Begin(name, type)` / `End(name)` — encadrent une mesure manuellement *(implémentées en `.cpp`)*.
- `Sample(name, type)` — *inline*, équivalent à `Begin` immédiatement suivi de `End` (mesure
  ponctuelle / marqueur instantané).
- `NextFrame()` — *inline*, incrémente l'index de frame (à appeler une fois par frame pour découper
  les statistiques par frame).
- `GetStats(name)` — renvoie les `Stats` d'un marqueur *(en `.cpp`)*.
- `ExportToJSON(buffer, size)` — sérialise les stats dans un buffer fourni, pour un viewer externe
  ou un dump CI *(en `.cpp`)*.
- `Reset()` — vide toutes les stats *(en `.cpp`)*.

Cas d'usage par domaine : chronométrer le pas **physique** vs le pas **rendu** (`NkProfiler::MarkerType::System`),
isoler le coût d'un **script** de gameplay (`NkProfiler::MarkerType::Script`), mesurer une requête GPU
(`NkProfiler::MarkerType::Query`), suivre le temps total d'une **frame** (`NkProfiler::MarkerType::Frame`).

**Les macros** :
- `NK_PROFILE_SCOPE(name)` — la forme **RAII** recommandée : un `Begin` au point d'appel et un objet
  local dont le destructeur fait le `End`, garantissant la fermeture même sur un `return` anticipé ou
  une exception.
- `NK_PROFILE_SAMPLE(name)` — appelle simplement `Global().Sample(name)`.

> ⚠️ **Pièges.** `NK_PROFILE_SCOPE` nomme sa variable interne via `_NkProfilerEnd_##__LINE__`, mais
> cette concaténation **ne s'expanse pas** (on obtient littéralement `__LINE__`) : deux invocations
> dans le même bloc collisionnent. Un seul `NK_PROFILE_SCOPE` par portée, ou ouvrez des sous-blocs.
> Note historique : ce fichier était auparavant mal rangé dans `NkPrefab.h`.

---

### Exemple

```cpp
#include "Noge/Nkentseu.h"
using namespace nkentseu;

// 1) Une couche de jeu.
class WorldLayer : public NkLayer {
public:
    WorldLayer() : NkLayer("World") {}
    void OnUpdate(float dt) override {
        NK_PROFILE_SCOPE("World.Update");   // mesure RAII (un seul par bloc)
        // ... logique de jeu ...
    }
    bool OnEvent(NkEvent* e) override {
        // renvoyer true ici consommerait l'événement (les couches du dessous ne le verraient pas)
        return false;
    }
};

// 2) L'application : on surcharge les hooks, le moteur fournit la boucle.
class MyGame : public NkApplication {
public:
    explicit MyGame(const NkApplicationConfig& cfg) : NkApplication(cfg) {}
    void OnPreInit() override { mConfig.windowConfig.title = "Demo"; }   // avant la fenêtre
    void OnInit() override    { PushLayer(new WorldLayer()); }           // device prêt ; ownership → stack
    void OnClose() override   { /* confirmation possible */ Quit(); }
};

// 3) Le point d'entrée : pas de main(), juste cette fabrique.
NkApplication* NkMainApplication(const NkApplicationConfig& config) {
    NkApplicationConfig cfg = config;
    cfg.appName       = "Demo";
    cfg.fixedTimeStep = 1.0f / 60.0f;   // OnFixedUpdate à 60 Hz (physique/réseau)
    return new MyGame(cfg);
}
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
