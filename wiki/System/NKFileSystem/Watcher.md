# La surveillance de fichiers

> Couche **System** · NKFileSystem · Détecter **en temps réel** les changements sur
> disque — fichier créé, modifié, supprimé, renommé — pour recharger à chaud assets,
> shaders, scènes et configs sans relancer le programme.

Tout moteur sérieux finit par poser la même question : **« comment savoir qu'un fichier a
changé sans le relire en boucle ? »**. L'éditeur a sauvegardé un shader, l'artiste a écrasé
une texture, le designer a touché un fichier de niveau — et on voudrait que le programme s'en
aperçoive **tout seul**, dans la seconde, pour recharger l'asset à chaud. La solution naïve —
relire le fichier à chaque frame et comparer sa date de modification — fonctionne pour trois
fichiers, mais s'effondre dès qu'on en surveille des centaines : ce serait du *polling* qui
brûle du CPU pour rien la plupart du temps. Les systèmes d'exploitation savent faire mieux :
ils notifient l'application **quand** quelque chose bouge, et seulement à ce moment-là. C'est
exactement ce que `NkFileWatcher` enveloppe.

Le module abstrait les API natives — **inotify** sous Linux, **ReadDirectoryChangesW** sous
Windows — derrière une seule interface, avec un *fallback silencieux* sur Web/Emscripten (où
la notion de système de fichiers surveillé n'existe pas). Toute la surveillance tourne dans un
**thread dédié** : votre boucle principale n'est jamais bloquée, et c'est précisément ce qui
impose la seule vraie discipline de cette page — **le callback est appelé depuis un autre
thread**.

- **Namespace** : `nkentseu` (alias de compatibilité dans `nkentseu::entseu`)
- **Header** : `#include "NKFileSystem/NkFileWatcher.h"`

---

## Décrire un changement : `NkFileChangeType` et `NkFileChangeEvent`

Avant de surveiller, il faut un vocabulaire pour **dire ce qui s'est passé**. C'est le rôle de
l'énumération `NkFileChangeType`, qui distingue les cinq natures d'événement qu'un système de
fichiers peut produire : `NK_CREATED` (un fichier ou répertoire vient d'apparaître),
`NK_DELETED` (il a disparu), `NK_MODIFIED` (son contenu a changé — le cas du hot-reload),
`NK_RENAMED` (il a été renommé ou déplacé), et `NK_ATTRIBUTE_CHANGED` (ses métadonnées seules
— permissions, date — ont bougé, sans toucher au contenu).

Chaque notification arrive empaquetée dans un `NkFileChangeEvent`, une simple structure de
données à champs publics. On y lit directement le `Type` (un `NkFileChangeType`), le `Path`
(le chemin complet de l'élément concerné, en `NkString`), un `OldPath` qui n'est renseigné
**que** pour un `NK_RENAMED` (l'ancien nom avant déplacement), et un `Timestamp` (un
`nk_int64`, horodatage epoch Unix de l'événement).

```cpp
void OnChange(const NkFileChangeEvent& e) {
    if (e.Type == NkFileChangeType::NK_MODIFIED) {
        // e.Path a été réécrit → recharger l'asset
    } else if (e.Type == NkFileChangeType::NK_RENAMED) {
        // e.OldPath → e.Path : mettre à jour la table des chemins
    }
}
```

Ce n'est **pas** un objet à durée de vie complexe : la structure est copiable par défaut (son
seul membre non trivial, `NkString`, l'est aussi), ce qui permet sans risque de la *pousser
dans une file* pour la traiter plus tard, dans le bon thread. On la construit soit par défaut
(tous les membres neutres), soit par le constructeur paramétré `NkFileChangeEvent(type, path)`
où `path` peut même être `nullptr`.

> **En résumé.** `NkFileChangeType` nomme les cinq sortes de changements
> (créé/supprimé/modifié/renommé/attributs) ; `NkFileChangeEvent` les transporte avec
> `Type`/`Path`/`OldPath`/`Timestamp`. `OldPath` n'a de sens que pour `NK_RENAMED`. La
> structure est copiable — donc *file-friendly* pour passer du thread de surveillance au thread
> principal.

---

## Recevoir les notifications : `NkFileWatcherCallback`

Comment le watcher vous prévient-il ? Par **rappel** (callback). `NkFileWatcherCallback` est
une **interface abstraite** : on en hérite, on implémente l'unique méthode pure virtuelle
`OnFileChanged(const NkFileChangeEvent&)`, et le watcher l'appelle à chaque changement détecté.
Le destructeur est virtuel — comme il se doit pour une classe-base polymorphe.

```cpp
class ShaderReloader : public nkentseu::NkFileWatcherCallback {
public:
    void OnFileChanged(const NkFileChangeEvent& e) override {
        // ⚠ appelé depuis le thread de surveillance, PAS le thread principal
        mDirty.PushBack(e.Path);   // pousser, ne pas recompiler ici
    }
};
```

Le point capital — et c'est la **seule** vraie règle de tout le module — tient en une phrase :
`OnFileChanged` **est exécuté depuis le thread de surveillance dédié**, jamais depuis votre
boucle principale. Ce n'est **pas** un événement que vous dépilez quand ça vous arrange : il
vous tombe dessus de façon asynchrone. L'implémentation doit donc être **thread-safe**, et
surtout ne **rien** faire de long ou de bloquant dedans (recompiler un shader, uploader une
texture GPU). Le bon réflexe : dans le callback, on **pousse** juste l'événement dans une file
protégée par mutex, et on fait le vrai travail plus tard, à un moment maîtrisé du thread
principal.

> **En résumé.** Héritez de `NkFileWatcherCallback`, implémentez `OnFileChanged`. Cette méthode
> est appelée **depuis un autre thread** : protégez vos ressources partagées et ne faites rien
> de bloquant — contentez-vous d'enfiler l'événement pour le traiter dans le thread principal.

---

## Surveiller : `NkFileWatcher`

`NkFileWatcher` est la classe principale. On lui donne **un chemin** (un fichier ou, plus
souvent, un répertoire) et **un callback**, on appelle `Start()`, et à partir de là un thread
dédié observe ce chemin et invoque votre `OnFileChanged` à chaque mouvement. On peut surveiller
récursivement (les sous-répertoires) ou non, via le paramètre `recursive`.

```cpp
ShaderReloader reloader;
NkFileWatcher watcher("assets/shaders", &reloader, /*recursive=*/true);
if (watcher.Start()) {
    // surveillance active en arrière-plan ; la boucle continue normalement
}
// ... plus tard, ou simplement à la destruction :
watcher.Stop();
```

`Start()` lance la surveillance en arrière-plan et renvoie `true` en cas de succès, `false`
sinon (chemin invalide, callback nul…) — **vérifiez toujours son retour** avant de considérer
la surveillance active. `Stop()` l'arrête et libère les ressources ; c'est une opération
**bloquante** qui attend la fin du thread de surveillance (un *join*) avant de rendre la main.
`IsWatching()` dit si l'on est entre un `Start()` réussi et un `Stop()`.

C'est un objet **RAII strict**. Son destructeur appelle automatiquement `Stop()` si la
surveillance est encore active, joint le thread et rend les ressources plateforme — vous ne
*pouvez* pas oublier d'arrêter. En contrepartie, parce qu'il possède un thread et un handle
système, il est **non-copiable** (copie et affectation `= delete`) et de fait **non-movable** :
un watcher se passe par référence ou par pointeur, jamais par valeur. Attention aussi à
l'*ownership* : le watcher **ne possède pas** le callback (c'est un pointeur brut). La durée de
vie de l'objet callback doit donc **dépasser** celle du watcher, sinon on retombe sur un appel
à un objet détruit depuis le thread de surveillance.

Le watcher se **reconfigure à chaud**. `SetPath` (en `const char*` ou en `NkPath`),
`SetCallback` et `SetRecursive` changent la cible sans recréer l'objet — et fait notable, si la
surveillance est **active**, `SetPath` et `SetRecursive` la **redémarrent automatiquement**
avec la nouvelle configuration. Côté lecture, `GetPath()` renvoie une référence constante vers
le chemin normalisé effectivement surveillé, et `IsRecursive()` indique si les sous-répertoires
sont inclus.

Une mise en garde de **plateforme** : ce n'est **pas** une garantie universelle. Sur
Web/Emscripten, `Start()` renvoie `true` mais **rien n'est détecté** (fallback silencieux).
Sous Linux, la surveillance est plafonnée par `fs.inotify.max_user_watches` et inotify ne
fonctionne pas sur certains systèmes de fichiers (NFS). Concevez le hot-reload comme un
**confort**, jamais comme une fonctionnalité dont dépend la correction du programme.

> **En résumé.** `NkFileWatcher(path, callback, recursive)` puis `Start()` (vérifier le retour)
> lance un thread de surveillance ; `Stop()` (bloquant, *join*) l'arrête ; le destructeur le
> fait pour vous. Non-copiable, non-movable, **ne possède pas** le callback. `SetPath` /
> `SetRecursive` redémarrent à chaud si actifs. Hot-reload = confort, pas garantie (muet sur
> Web, plafonné sous Linux).

---

## Le raccourci lambda : `NkSimpleFileWatcher`

Hériter d'une interface pour un simple rechargement est parfois lourd. Quand on compile en
C++11 ou plus (`#if defined(NK_CPP11)`), `NkSimpleFileWatcher` offre un **adaptateur
fonctionnel** : on lui donne une fonction (ou une lambda) au lieu d'écrire une classe. Il
**hérite** lui-même de `NkFileWatcherCallback` et **compose** en interne un `NkFileWatcher`,
auquel il délègue tout.

```cpp
NkSimpleFileWatcher watcher("config/", /*recursive=*/false);
watcher.OnChanged = [](const NkFileChangeEvent& e) {
    // recharger la config ; lambda SANS capture obligatoire
};
watcher.Start();
```

Le membre public `OnChanged` est de type `CallbackFunc`, soit `void(*)(const NkFileChangeEvent&)`
— un **pointeur de fonction C** nu. Cela impose une limite à connaître : seules les lambdas
**sans capture** sont compatibles (elles seules se convertissent en pointeur de fonction). Une
lambda qui capture du contexte ne compile pas ici — il faut alors revenir à
`NkFileWatcherCallback`. On assigne `OnChanged` **avant** `Start()`, puis on pilote l'objet par
ses méthodes déléguées `Start()` / `Stop()` / `IsWatching()`. Le constructeur ne prend qu'un
`const char*` (pas de surcharge `NkPath` ici, contrairement à `NkFileWatcher`). Comme il
contient un `NkFileWatcher` par composition, il est lui aussi non-copiable et non-movable.

> **En résumé.** `NkSimpleFileWatcher` (C++11) évite d'écrire une classe : on assigne une
> fonction à `OnChanged`, on appelle `Start()`. Mais `OnChanged` est un **pointeur de fonction**
> → lambdas **sans capture** uniquement. Constructeur `const char*` seul. Pour du contexte
> capturé, repassez à `NkFileWatcherCallback`.

---

## Aperçu de l'API

Tous les symboles publics du header, en un coup d'œil. Chacun est détaillé dans la « Référence
complète ».

### `NkFileChangeType` — nature du changement

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Valeur | `NK_CREATED` | Fichier/répertoire créé. |
| Valeur | `NK_DELETED` | Fichier/répertoire supprimé. |
| Valeur | `NK_MODIFIED` | Contenu modifié (cas du hot-reload). |
| Valeur | `NK_RENAMED` | Renommé / déplacé (`OldPath` renseigné). |
| Valeur | `NK_ATTRIBUTE_CHANGED` | Métadonnées modifiées (permissions, date…). |

### `NkFileChangeEvent` — données d'un événement

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `Type` (`NkFileChangeType`) | Nature de l'événement. |
| Données | `Path` (`NkString`) | Chemin complet de l'élément concerné. |
| Données | `OldPath` (`NkString`) | Ancien chemin — **uniquement** pour `NK_RENAMED`. |
| Données | `Timestamp` (`nk_int64`) | Horodatage epoch Unix. |
| Construction | `NkFileChangeEvent()` | Par défaut, membres neutres. |
| Construction | `NkFileChangeEvent(type, path)` | Paramétré (`path` peut être `nullptr`). |

### `NkFileWatcherCallback` — interface de notification

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Destruction | `virtual ~NkFileWatcherCallback()` | Destructeur virtuel (inline). |
| Notification | `virtual void OnFileChanged(const NkFileChangeEvent&) = 0` | **Pur** ; appelé **depuis le thread de surveillance**. |

### `NkFileWatcher` — surveillance asynchrone

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkFileWatcher()` | Par défaut. |
| Construction | `NkFileWatcher(const char* path, callback, recursive = false)` | Chemin C-string + callback. |
| Construction | `NkFileWatcher(const NkPath& path, callback, recursive = false)` | Variante `NkPath`. |
| Destruction | `~NkFileWatcher()` | Arrête si actif, joint le thread, libère. |
| Copie | `= delete` (copie et affectation) | **Non-copiable**, **non-movable**. |
| Cycle de vie | `bool Start()` | Démarre en arrière-plan ; `false` si échec. |
| Cycle de vie | `void Stop()` | Arrête ; **bloquant** (*join* du thread). |
| Cycle de vie | `bool IsWatching() const` | Surveillance active ? |
| Config | `void SetPath(const char*)` / `SetPath(const NkPath&)` | Chemin ; **redémarre si actif**. |
| Config | `void SetCallback(callback*)` | Définit le callback de notification. |
| Config | `void SetRecursive(bool)` | Récursivité ; **redémarre si actif**. |
| Lecture | `const NkString& GetPath() const` | Chemin normalisé surveillé. |
| Lecture | `bool IsRecursive() const` | Inclut les sous-répertoires ? |

### `NkSimpleFileWatcher` — adaptateur lambda *(`#if NK_CPP11`)*

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `using CallbackFunc = void(*)(const NkFileChangeEvent&)` | Pointeur de fonction (lambda **sans capture**). |
| Membre | `CallbackFunc OnChanged` | Fonction callback à assigner avant `Start()`. |
| Construction | `NkSimpleFileWatcher(const char* path, recursive = false)` | Chemin + récursivité (pas de surcharge `NkPath`). |
| Notification | `void OnFileChanged(...) override` | Forwarde vers `OnChanged`. |
| Cycle de vie | `bool Start()` / `void Stop()` / `bool IsWatching() const` | Délègue au `NkFileWatcher` interne. |

### `nkentseu::entseu` — alias de compatibilité legacy

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias | `NkFileChangeType`, `NkFileChangeEvent`, `NkFileWatcherCallback`, `NkFileWatcher` | Réexports `using` vers `::nkentseu::`. |
| Alias | `NkSimpleFileWatcher` | Réexport — **uniquement** `#if NK_CPP11`. |

---

## Référence complète

Chaque élément est repris ici en détail : comportement, complexité et usages dans les
différents domaines (rendu, ECS, gameplay/IA, audio, UI/2D, outils/éditeur, IO/réseau,
threading). Les éléments triviaux sont décrits brièvement ; le cœur du module — le cycle de vie
threadé et le callback — l'est à fond.

### `NkFileChangeType` — les cinq natures de changement

L'énumération n'a pas d'initialiseurs explicites : ses valeurs courent donc de 0 à 4 dans
l'ordre de déclaration. La distinction qui compte le plus en pratique est
`NK_MODIFIED` (le contenu a changé → il faut **recharger**) face à `NK_ATTRIBUTE_CHANGED` (seules
les métadonnées ont bougé → en général on **ignore**, sauf si l'on surveille des droits
d'accès). `NK_RENAMED` est le seul qui s'accompagne d'un `OldPath` utile.

- **Outils / éditeur** : un éditeur d'assets distingue typiquement les quatre cas pour mettre à
  jour son arborescence (ajout/retrait de nœud sur `NK_CREATED`/`NK_DELETED`, rafraîchissement
  de l'aperçu sur `NK_MODIFIED`, déplacement de l'entrée sur `NK_RENAMED`).
- **Rendu / GPU** : seuls `NK_MODIFIED` et `NK_CREATED` déclenchent une recompilation de shader
  ou un réupload de texture ; un `NK_ATTRIBUTE_CHANGED` est sans effet visuel.
- **Gameplay** : un fichier de niveau supprimé (`NK_DELETED`) peut basculer le designer sur une
  scène par défaut plutôt que de crasher.

### `NkFileChangeEvent` — le message

Structure de données pure, à champs publics lus directement. `Type`, `Path`, `OldPath`,
`Timestamp`. La copie implicite est valide (le seul membre non trivial, `NkString`, est
copiable) — propriété **essentielle** : elle autorise à recopier l'événement du thread de
surveillance vers une file consommée par le thread principal, sans gestion de durée de vie
délicate.

- **Threading** : recopier l'événement dans une `NkVector`/file sous mutex est l'idiome de base
  pour franchir la frontière de thread sans data race.
- **IO / réseau** : `Path` et `Timestamp` suffisent à journaliser ou à propager une
  invalidation de cache (« cet asset, daté de tel epoch, est périmé »).
- **Outils** : `OldPath` → `Path` sur `NK_RENAMED` permet de migrer les références internes (un
  mesh qui pointait vers l'ancien nom de texture).

Construction : `NkFileChangeEvent()` met tout à des valeurs neutres ; le constructeur
`(type, path)` accepte un `path` à `nullptr` sans broncher.

### `NkFileWatcherCallback` — recevoir, sans bloquer

Interface abstraite à une seule méthode pure : `OnFileChanged`. Le destructeur virtuel inline
permet la destruction polymorphe correcte d'une implémentation tenue par pointeur de base.

Le comportement à intégrer dans tout le code : **`OnFileChanged` s'exécute sur le thread de
surveillance**. Ce n'est pas négociable et c'est la source de tous les bugs si on l'oublie.

- **Threading** : toute donnée touchée par le callback ET par le thread principal doit être
  protégée (mutex) ou échangée par une file ; l'idiome universel est *« le callback pousse, le
  thread principal traite »*.
- **Rendu / GPU** : ne **jamais** appeler une API GPU (compilation de shader, upload de
  texture) depuis le callback — ces API exigent souvent le thread qui détient le contexte ;
  poussez plutôt un *« à recharger »* consommé en début de frame.
- **Audio** : même logique pour recharger un sample — on marque, on recharge sur le thread
  audio/principal, pas dans le callback.
- **UI / éditeur** : ne pas muter l'arbre de widgets depuis le callback ; signaler un *dirty
  flag* et rafraîchir au prochain tick UI.

### `NkFileWatcher` — le cœur threadé

La classe encapsule un handle système (`mHandle`), le chemin normalisé (`mPath`), le pointeur
de callback (non possédé), les drapeaux `mIsWatching`/`mRecursive` et un handle de thread
(`mThread`). En interne, `WatchThread()` est la boucle de surveillance (implémentée par
plateforme : inotify, ReadDirectoryChangesW…) et `ThreadProc` en est le point d'entrée statique
(il caste l'instance et retourne toujours `nullptr`). Ces détails sont privés mais expliquent le
**modèle de coût** : un watcher = un thread + un handle OS.

**Cycle de vie.** `Start()` crée le thread et arme la surveillance ; il renvoie `false` si la
pré-condition n'est pas remplie (chemin invalide, callback nul) — d'où l'obligation d'en
**vérifier le retour**. `Stop()` est **bloquant** : il signale l'arrêt puis *joint* le thread,
donc il ne rend la main qu'une fois la surveillance réellement éteinte (ne l'appelez pas depuis
le callback lui-même — auto-join). `IsWatching()` reflète l'état logique entre `Start()` et
`Stop()`.

**RAII et ownership.** Le destructeur appelle `Stop()` si nécessaire : impossible de fuiter le
thread. La classe est **non-copiable** (`= delete`) et, sans move déclaré, **non-movable** — un
thread et un handle ne se dupliquent pas. Le callback est un **pointeur brut non possédé** : sa
durée de vie doit englober celle du watcher.

**Reconfiguration à chaud.** `SetPath`/`SetRecursive` **redémarrent** la surveillance si elle
est active (arrêt + relance transparents avec la nouvelle config) ; `SetCallback` n'est pas
documenté comme redémarrant. `GetPath()` rend le chemin **normalisé** (ce qui est réellement
surveillé, pas forcément la chaîne brute fournie), `IsRecursive()` l'option courante.

Usages, par domaine :
- **Rendu / GPU** — surveiller `assets/shaders` en récursif : à chaque `NK_MODIFIED`, marquer
  le shader pour recompilation au début de la frame suivante. La pierre angulaire du hot-reload
  graphique.
- **ECS / scène** — surveiller le dossier de scènes : recharger une entité-prototype ou une
  table de spawn éditée à chaud, sans relancer le jeu.
- **Gameplay / IA** — recharger des courbes d'équilibrage, des tables de loot ou des
  *behavior trees* tunés en direct par le designer.
- **Audio** — détecter qu'un sample ou une banque a été réexporté et le recharger.
- **UI / 2D** — recharger une feuille de style, un atlas ou une police modifiés par le designer
  UI.
- **Outils / éditeur** — détecter les modifications externes faites hors de l'éditeur (un
  artiste qui sauvegarde depuis Photoshop) pour proposer un rechargement ou prévenir d'un
  conflit.
- **IO / réseau** — observer un répertoire de *drop* où d'autres processus déposent des
  fichiers à ingérer (pipeline d'import).
- **Threading** — modèle implicite : un thread producteur (la surveillance) → une file → le
  thread consommateur (la boucle principale).

**Plateformes.** Web/Emscripten : `Start()` renvoie `true` mais ne détecte rien (fallback
muet) — prévoyez un fonctionnement correct sans hot-reload. Linux : limité par
`fs.inotify.max_user_watches`, inactif sur certains FS (NFS). Traitez la surveillance comme un
**confort de développement**, pas comme une garantie de production.

### `NkSimpleFileWatcher` — l'adaptateur fonctionnel

Disponible seulement sous `#if defined(NK_CPP11)`. Il **hérite** de `NkFileWatcherCallback`
(implémente `OnFileChanged` en forwardant vers `OnChanged`) et **compose** un `NkFileWatcher`
interne auquel `Start()`/`Stop()`/`IsWatching()` délèguent. Cette double nature — héritage de
l'interface + composition du watcher — le rend, comme son membre `NkFileWatcher`, non-copiable
et non-movable.

Le type membre `CallbackFunc` est `void(*)(const NkFileChangeEvent&)` : un **pointeur de
fonction C**. La conséquence directe : `OnChanged` n'accepte qu'une fonction libre ou une
**lambda sans capture** (les seules convertibles en pointeur de fonction). Assignez `OnChanged`
**avant** d'appeler `Start()`. Le constructeur ne propose que la variante `const char*` (pas de
`NkPath`).

- **Outils / prototypage** — la voie rapide pour un rechargement « 5 lignes » sans définir de
  classe : idéal dans un sandbox, un test, un petit outil.
- **Limite à connaître** — dès qu'il faut **capturer** un contexte (un pointeur vers le
  gestionnaire d'assets, par ex.), `CallbackFunc` ne suffit plus : repassez à une vraie
  implémentation de `NkFileWatcherCallback`.

### `nkentseu::entseu` — alias de compatibilité

Un namespace `entseu` réexporte par `using` tous les types vers `::nkentseu::`
(`NkFileChangeType`, `NkFileChangeEvent`, `NkFileWatcherCallback`, `NkFileWatcher`, et
`NkSimpleFileWatcher` **uniquement** sous `#if NK_CPP11`). Il existe pour le code legacy qui
qualifiait encore `nkentseu::entseu::…` ; dans du code neuf, utilisez directement `nkentseu`.

---

### Exemple récapitulatif

```cpp
#include "NKFileSystem/NkFileWatcher.h"
using namespace nkentseu;

// --- Voie 1 : implémentation complète, callback thread-safe ---
class ShaderHotReload : public NkFileWatcherCallback {
public:
    void OnFileChanged(const NkFileChangeEvent& e) override {
        // Appelé depuis le thread de surveillance : on POUSSE, on ne recompile pas ici.
        if (e.Type == NkFileChangeType::NK_MODIFIED ||
            e.Type == NkFileChangeType::NK_CREATED) {
            // mDirty protégée par mutex, consommée au début de la frame
            mDirty.PushBack(e.Path);
        }
    }
    // ... drainage de mDirty dans le thread principal, là où le GPU est sûr
};

ShaderHotReload reloader;
NkFileWatcher watcher("assets/shaders", &reloader, /*recursive=*/true);
if (!watcher.Start()) {
    // chemin invalide ou callback null : continuer SANS hot-reload
}
// ... watcher.Stop() est appelé automatiquement à la destruction (RAII)

// --- Voie 2 : raccourci lambda (C++11), config simple ---
NkSimpleFileWatcher cfgWatcher("config/game.ini", /*recursive=*/false);
cfgWatcher.OnChanged = [](const NkFileChangeEvent& e) {
    // lambda SANS capture obligatoire (CallbackFunc = pointeur de fonction)
};
cfgWatcher.Start();   // OnChanged doit être assigné AVANT Start()
```

---

[← Index NKFileSystem](README.md) · [Récap NKFileSystem](../NKFileSystem.md) · [Couche System](../README.md)
