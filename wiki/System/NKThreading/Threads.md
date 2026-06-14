# Les threads

> Couche **System** · NKThreading · Faire tourner du code **en parallèle** : le thread OS
> brut `NkThread`, le stockage par-thread `NkThreadLocal`, et le pool de workers
> `NkThreadPool` avec son `ParallelFor`.

Un jeu, un éditeur ou une simulation moderne n'a pas le luxe de tout faire sur **un seul fil
d'exécution**. Pendant que le thread principal prépare la frame, on voudrait charger une
texture depuis le disque, simuler la physique, mixer l'audio, ou découper une grosse boucle
sur tous les cœurs du CPU — *en même temps*. La question n'est jamais « est-ce que je peux
lancer un thread » (le système d'exploitation sait faire), mais « **quel outil pour quel
besoin** » : un thread dédié et long ? un bout de données privé à chaque fil ? ou un essaim de
petites tâches courtes ? Tout le compromis tient en une phrase : **un thread brut donne le
contrôle total mais coûte cher à créer et impose une discipline rigoureuse ; un pool amortit
ce coût et absorbe des milliers de petites tâches, mais cède le contrôle de l'ordonnancement.**
Cette page vous apprend à choisir.

Les trois outils vivent dans le même namespace et s'incluent par leurs headers individuels
(pas de header parapluie ici). `NkThread` et `NkThreadPool` sont exportés (classes lourdes,
implémentation en `.cpp`) ; `NkThreadLocal<T>` est un template **header-only**. Toute la
mémoire interne (le contexte de démarrage d'un thread, l'implémentation cachée du pool) passe
par NKMemory — jamais par `new`/`delete` brut.

- **Namespace** : `nkentseu::threading`
- **Headers** : `#include "NKThreading/NkThread.h"`, `#include "NKThreading/NkThreadLocal.h"`,
  `#include "NKThreading/NkThreadPool.h"`

---

## Le thread OS brut : `NkThread`

C'est l'outil **bas niveau**, le wrapper RAII *move-only* directement posé sur un thread du
système (`CreateThread` sous Windows, `pthread` sous POSIX). On le prend quand on a besoin
d'un **fil d'exécution dédié et durable** : une boucle de chargement de ressources en tâche de
fond, un thread audio temps-réel, un fil réseau qui écoute en permanence. La fonction exécutée
a une signature précise — elle reçoit un `void* userData` — et le thread démarre dès qu'on
appelle `Start` (ou directement via le constructeur qui prend une fonction).

```cpp
NkThread loader;
loader.Start(NkThread::ThreadFunc([](void* userData) {
    // ... charge des assets sur ce fil, en parallèle de la boucle principale ...
}));
loader.SetName("AssetLoader");      // visible dans le débogueur
loader.Join();                      // bloque jusqu'à la fin du fil
```

Le point **le plus important** à intérioriser tient au destructeur : `~NkThread()` appelle
`Detach()`, **pas** `Join()`. Autrement dit, si vous laissez un `NkThread` sortir de sa portée
sans l'avoir joint, le fil OS **continue de tourner** — mais le code C++ qui l'a lancé a
peut-être déjà détruit les données que ce fil utilisait. Ce n'est **pas** le comportement d'un
`std::thread` (qui, lui, appelle `std::terminate` si on oublie de join/detach). Ici, la règle
de survie est simple : **capturez tout par valeur**, ou bien **Join()** explicitement avant la
fin de portée.

`NkThread` est **move-only** : on ne peut pas le copier (un thread OS n'a qu'un propriétaire),
mais on peut le **déplacer**. Le déplacement ne stoppe pas le fil — il transfère seulement le
*handle* C++ ; le thread OS continue, et la source devient non-joinable. C'est ce qui permet de
stocker un `NkThread` dans un conteneur ou de le renvoyer d'une fonction.

> **En résumé.** `NkThread` = un fil OS dédié, move-only, RAII. `Start`/constructeur le lance,
> `Join()` attend sa fin, `Detach()` le laisse vivre seul. **Piège majeur** : le destructeur
> fait `Detach()`, pas `Join()` — capturez par valeur ou joignez explicitement, sinon le fil
> survit avec des données mortes.

---

## Join, Detach et l'identification

Une fois lancé, un thread mène **deux vies possibles**. Soit on le **joint** (`Join()`) : on
bloque le fil appelant jusqu'à ce que le thread cible termine, puis ses ressources OS sont
libérées proprement. Soit on le **détache** (`Detach()`) : il poursuit sa route en toute
indépendance, et ses ressources seront récupérées quand *lui* finira. Après l'un ou l'autre, le
`NkThread` n'est plus *joinable* — `Joinable()` le confirme en `O(1)`, sans bloquer.

Deux pièges de `Join()` valent qu'on les répète : ne **jamais** joindre un thread déjà détaché
ou non initialisé, et ne **jamais** joindre un thread depuis lui-même (deadlock garanti — il
s'attendrait éternellement). Pour `Detach()`, le danger est symétrique : une fois détaché, vous
n'avez plus aucune prise sur le fil, donc plus aucune garantie sur la durée de vie des données
locales que vous lui aviez confiées.

Côté **identité**, `GetID()` donne l'identifiant porté par l'instance (0 si non-joinable), et le
statique `GetCurrentThreadId()` répond « sur quel fil suis-je en train de m'exécuter ? » sans
avoir besoin d'une instance — précieux pour tracer des logs par thread ou affirmer dans un
`assert` qu'une fonction tourne bien sur le thread principal.

> **En résumé.** `Join()` attend et nettoie (jamais sur soi-même, jamais deux fois) ;
> `Detach()` libère le fil de toute tutelle ; `Joinable()` dit où on en est. `GetID()` /
> `GetCurrentThreadId()` identifient le fil — le second sans instance, pour les asserts « bon
> thread » et le logging par fil.

---

## Affinité, priorité, nom : la configuration fine

Sur un thread destiné à un travail soutenu, le placement et le rang comptent. `SetAffinity(core)`
**épingle** le fil sur un cœur précis — utile pour un thread audio qu'on veut isoler des
hoquets, ou pour répartir manuellement des workers sur les cœurs physiques. `SetPriority(p)`
règle l'importance du fil dans `[-2, +2]` (−2 = le plus bas, 0 = normal, +2 = le plus haut),
mappée vers les constantes natives de l'OS. `SetName(name)` attache une étiquette UTF-8 qui
**apparaît dans le débogueur** — un confort énorme quand on a dix fils et qu'on cherche lequel
plante (Linux tronque le nom à 15 caractères).

Toutes ces fonctions sont `noexcept` et **tolérantes à l'échec** : si la plateforme ne supporte
pas l'opération, ou si les paramètres sont hors limites, l'appel est simplement ignoré sans rien
casser. Sous Linux, fixer une priorité temps-réel peut exiger la capacité `CAP_SYS_NICE`.
Enfin, `GetCurrentCore()` renvoie l'index (0-based) du cœur sur lequel **le fil appelant** tourne
en ce moment — attention, c'est bien le fil *courant*, pas forcément l'instance `*this`, malgré
la nature non statique de la méthode.

> **En résumé.** `SetAffinity` épingle un cœur, `SetPriority` règle le rang `[-2,+2]`,
> `SetName` étiquette pour le débogueur ; tout est `noexcept` et ignoré en silence si
> non-supporté. `GetCurrentCore()` indique le cœur **du fil appelant**, pas de l'instance.

---

## Le stockage par-thread : `NkThreadLocal`

Certaines données ne doivent **jamais** être partagées entre fils : un compteur de statistiques
par worker, un buffer scratch réutilisé à chaque frame, un générateur de nombres aléatoires, le
contexte de rendu courant. Les protéger par un mutex serait un gâchis — chaque thread veut sa
**copie privée**, point. C'est exactement ce que fournit `NkThreadLocal<T>` : un wrapper
**lock-free** au-dessus du `thread_local` natif du C++, avec valeur initiale optionnelle et
initialisation paresseuse au premier `Get()` dans chaque fil.

```cpp
NkThreadLocal<nk_uint64> tasksDone{ 0 };   // chaque thread démarre à 0
// ... dans un worker, sans aucun verrou :
tasksDone.Set(tasksDone.Get() + 1);
// accès pointer-like aussi :
NkThreadLocal<RenderScratch> scratch;
scratch->Reset();                          // operator-> délègue à Get()
```

Ce n'est **pas** un canal de communication entre threads : chaque fil ne voit *que* sa propre
copie, totalement isolée des autres. Si vous voulez transmettre une valeur d'un thread à un
autre, ce n'est pas l'outil (utilisez une file, un mutex, un futur). Et il y a une **limite
réelle de l'implémentation** à connaître : le stockage `thread_local` est attaché à la *fonction*
`Get()`, pas à l'instance — donc plusieurs `NkThreadLocal<T>` du **même type `T`** peuvent
finir par partager le même emplacement par thread. Réservez-le aux cas où vous avez *une* donnée
de ce type par fil.

> **En résumé.** `NkThreadLocal<T>` = une copie privée par thread, sans verrou, init paresseuse.
> `Get()`/`Set()` + accès pointer-like (`->`, `*`). **Jamais** pour communiquer entre fils
> (chacun voit sa copie). Attention : deux instances du même `T` peuvent partager le stockage.

---

## Le pool de workers : `NkThreadPool`

Quand on a non pas *un* gros travail durable mais **une multitude de petits travaux courts** —
mettre à jour 10 000 entités, raycaster 500 rayons, redimensionner 64 imagettes — créer un
thread par tâche serait absurde (la création d'un thread OS coûte cher). La bonne structure est
le **pool** : un petit nombre de fils workers, créés une fois, qui piochent les tâches dans une
file commune. On **soumet** du travail, on n'attend pas individuellement, et le pool répartit.

```cpp
NkThreadPool pool;                  // 0 => autant de workers que de cœurs logiques
pool.Enqueue([] { BakeLightmap(0); });
pool.Enqueue([] { BakeLightmap(1); });
pool.Join();                        // attend que TOUT soit fini
```

Une `Task` est un `NkFunction<void()>` — sans argument ni retour. La soumission de base est
`Enqueue`, asynchrone et *fire-and-forget*. Il existe aussi `EnqueuePriority` et
`EnqueueAffinity`, mais soyez prévenu : **ce sont des stubs dans l'implémentation actuelle** —
la priorité et le cœur demandés sont **ignorés**, la tâche part dans la file générale comme un
`Enqueue` ordinaire. Ne construisez pas de logique qui dépende de leur effet.

Le pool est **non-copiable et non-déplaçable** (il possède ses workers via un *Pimpl*). Son
destructeur appelle `Shutdown()`, qui attend la fin des tâches en cours, refuse les nouvelles, et
rend le pool inutilisable ensuite. Pour récupérer des résultats, on appelle `Join()` **avant** de
lire la mémoire partagée — sinon on lit pendant que les workers écrivent encore.

> **En résumé.** `NkThreadPool` = N workers persistants qui consomment une file de `Task`
> (`NkFunction<void()>`). `Enqueue` soumet, `Join()` attend tout, `Shutdown()` ferme
> définitivement. `EnqueuePriority`/`EnqueueAffinity` sont des **stubs sans effet réel**. Pool
> non-copiable, non-déplaçable.

---

## Paralléliser une boucle : `ParallelFor`

Le cas le plus fréquent du pool, c'est de **découper une boucle** sur tous les cœurs.
`ParallelFor(count, func, grainSize)` s'en charge : il fend l'intervalle `[0, count)` en
*chunks* de taille `grainSize`, et soumet chaque chunk comme une tâche au pool. `func` reçoit
**un index** et ne renvoie rien.

```cpp
pool.ParallelFor(entities.Size(), [&](nk_size i) {
    entities[i].Update(dt);          // chaque indice est traité indépendamment
}, 256);                             // grainSize = 256 entités par tâche
pool.Join();                         // ParallelFor ne joint PAS lui-même
```

Deux règles vitales. D'abord, **`func` doit être thread-safe et indépendant de l'ordre** : deux
chunks tournent en même temps sur des workers différents, donc chaque itération doit écrire dans
*sa* zone et ne pas dépendre de la précédente. Ensuite, **`ParallelFor` ne joint pas** : il
soumet et rend la main immédiatement — il faut appeler `Join()` ensuite pour être sûr que tout
est terminé avant de lire les résultats.

Le `grainSize` est le bouton de réglage. Trop petit (`grainSize = 1` sur une grande boucle), et
le coût de soumission de chaque micro-tâche écrase le gain du parallélisme. Trop grand, et
certains workers chôment pendant que d'autres finissent un énorme chunk. La règle empirique
documentée est `max(1, count / (numWorkers * 4))` : assez de chunks pour équilibrer la charge,
assez gros pour amortir l'overhead.

> **En résumé.** `ParallelFor(count, func, grainSize)` découpe `[0,count)` en chunks soumis au
> pool ; `func(index)` doit être **indépendant de l'ordre** et **thread-safe**. Il **ne joint
> pas** — `Join()` ensuite. Réglez `grainSize ≈ count / (numWorkers*4)`.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé (comportement,
cas d'usage) dans la « Référence complète ». Complexités / `noexcept` entre crochets si utile.

### `NkThread` — thread OS brut (`NkThread.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias | `ThreadFunc = NkFunction<void(void*)>`, `ThreadId = nk_uint64` | Signature de la fonction du fil (reçoit `userData`) / identifiant opaque |
| Construction | `NkThread()`, `explicit NkThread(Func&&)`, `~NkThread()` | Non-initialisé / lance immédiatement / **Detach() auto** au destructeur |
| Move-only | `NkThread(NkThread&&)`, `operator=(NkThread&&)` `[noexcept]`, copie `= delete` | Transfère le handle (le fil OS continue) ; copie interdite |
| Démarrage | `Start(ThreadFunc&&)` `[noexcept]` | Lance l'exécution (Detach du courant si déjà joinable) |
| Contrôle | `Join()`, `Detach()` `[noexcept]`, `Joinable()` `[noexcept, O(1)]` | Attendre la fin / laisser vivre seul / est-il joinable ? |
| Identité | `GetID()` `[noexcept]`, `static GetCurrentThreadId()` `[noexcept]` | ID de l'instance (0 si non-joinable) / ID du fil appelant |
| Configuration | `SetName(const nk_char*)`, `SetAffinity(nk_uint32)`, `SetPriority(nk_int32)` `[noexcept]` | Nom débogueur / épingler un cœur / priorité `[-2,+2]` |
| Configuration | `GetCurrentCore()` `[noexcept]` | Index 0-based du cœur du fil **appelant** |

### `NkThreadLocal<T>` — stockage par-thread (`NkThreadLocal.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkThreadLocal()`, `explicit NkThreadLocal(const T&)`, `explicit NkThreadLocal(T&&)` | Sans valeur initiale (default) / copie / déplacement de la valeur initiale |
| Accès | `Get()` `[nodiscard, noexcept]`, `Set(const T&)`, `Set(T&&)` `[noexcept]` | Référence vers la copie du fil (init paresseuse) / écrire (copie / move) |
| Pointer-like | `operator->`, `operator*` (mutable et const) | Délèguent à `Get()` |
| Alias legacy | `nkentseu::NkThreadLocal<T>` (`using`) | **Déprécié** ; zéro overhead (résolu à la compilation) |

### `NkThreadPool` — pool de workers (`NkThreadPool.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias | `Task = NkFunction<void()>` | Tâche sans argument ni retour |
| Construction | `explicit NkThreadPool(nk_uint32 = 0)` `[noexcept]`, `~NkThreadPool()` `[noexcept]` | `0` ⇒ cœurs logiques ; destructeur appelle `Shutdown()` |
| Copie/déplacement | copie + déplacement `= delete` | **Non-copiable, non-déplaçable** (possède ses workers) |
| Soumission | `Enqueue(Task)` `[noexcept]` | Soumission asynchrone (ignorée si en shutdown) |
| Soumission | `EnqueuePriority(Task, nk_int32 = 0)` `[noexcept]` | Priorité `[-2,+2]` — **STUB, ignorée** |
| Soumission | `EnqueueAffinity(Task, nk_uint32)` `[noexcept]` | Cœur préféré — **STUB, ignoré** |
| Soumission | `ParallelFor(count, func, grainSize = 1)` `[noexcept]` | Découpe `[0,count)` en chunks ; `func(index)` ; **ne joint pas** |
| Contrôle | `Join()`, `Shutdown()` `[noexcept]` | Attendre que tout finisse / fermer définitivement |
| Requêtes | `GetNumWorkers()` `[nodiscard]`, `GetQueueSize()` `[nodiscard]`, `GetTasksCompleted()` `[nodiscard]` | Nb workers / taille file (monitoring) / compteur cumulatif |
| Singleton | `static GetGlobal()` `[noexcept]` | Pool global paresseux (cœurs logiques), persiste jusqu'à fin du programme |

---

## Référence complète

Chaque élément est repris ici en détail : complexité, comportement, et usages dans les
différents domaines du temps réel — rendu, ECS, physique, animation, gameplay/IA, audio, UI/2D,
IO/réseau, GPU, outils. Les éléments triviaux sont décrits brièvement ; les opérations centrales
le sont **à fond**.

### Choisir : thread, thread-local ou pool ?

Le seul vrai critère est **la forme du travail** :

- **Un fil dédié et durable, avec son propre rythme** (boucle de chargement, thread audio,
  écoute réseau) → `NkThread`.
- **Une donnée privée à chaque fil, sans partage** (scratch buffer, RNG, stats par worker) →
  `NkThreadLocal`.
- **Beaucoup de petites tâches courtes, ou une boucle à découper** (update ECS, raycasts,
  *bake* de lightmaps, traitement par tuiles) → `NkThreadPool` (+ `ParallelFor`).

### `NkThread` à fond

**Move-only, RAII, et le piège du destructeur.** Un `NkThread` possède *un* fil OS : pas de
copie, mais un **déplacement** qui transfère le handle sans interrompre le fil (la source
devient non-joinable). Le `~NkThread()` appelle `Detach()` — conséquence concrète : un thread
non joint **survit** à la portée qui l'a créé, et continue d'utiliser ce qu'il a capturé, même
si ces données sont détruites. Capturez par valeur, ou enveloppez le thread dans un wrapper qui
`Join()` à son propre destructeur.

**Start et le transfert de la fonction.** `Start(ThreadFunc&&)` alloue un petit contexte sur le
heap (`ThreadStartData`) pour transmettre la `ThreadFunc` au point d'entrée natif, qui l'extrait,
libère le contexte, puis exécute. Si la création OS échoue, l'instance est marquée non-joinable
(échec silencieux, `noexcept`). La `ThreadFunc` reçoit un `void* userData` — la signature réelle,
même si un commentaire interne dit `void()`.

Usages, par domaine :
- **IO / réseau** — un fil dédié qui lit le disque ou écoute un socket en boucle pendant que le
  jeu tourne ; on le `SetName("Net")` pour le repérer au débogueur.
- **Audio** — un thread temps-réel épinglé (`SetAffinity`) et priorisé (`SetPriority(+2)`) pour
  remplir le buffer du *device* sans hoquet.
- **Outils / éditeur** — un *watcher* de fichiers, un thread de compilation de shaders en
  arrière-plan, joints proprement à la fermeture.
- **GPU** — un fil de transfert (upload de textures/buffers) découplé du fil de rendu principal.

### `Join`, `Detach`, `Joinable` à fond

`Join()` bloque le fil appelant jusqu'à la terminaison du fil cible, puis libère les ressources
OS — **jamais** sur un thread détaché/non-init, **jamais** depuis le thread lui-même (deadlock).
`Detach()` coupe le lien : le fil vit et meurt seul, ses ressources récupérées à sa fin — après
quoi vous ne devez plus toucher aux données locales du créateur. `Joinable()` est un test `O(1)`,
thread-safe et non-bloquant : true uniquement si le fil a été créé et ni joint ni détaché. Le
schéma sûr classique : `if (t.Joinable()) t.Join();` avant de jeter l'objet.

### `GetID`, `GetCurrentThreadId` à fond

`GetID()` rend l'identifiant du fil porté par l'instance (0 si non-joinable). Le statique
`GetCurrentThreadId()` répond sans instance : « quel est l'ID du fil qui exécute *cette* ligne ? »

- **Threading / debug** — préfixer chaque ligne de log par l'ID du fil pour démêler des traces
  entrelacées.
- **Outils / asserts** — `NKENTSEU_ASSERT(GetCurrentThreadId() == mainThreadId)` au début d'une
  API non thread-safe (création de ressources GPU, mutation de la scène) garantit qu'on est bien
  sur le bon fil.
- **ECS / systèmes** — router un travail vers le bon fil selon l'identité courante.

### `SetName`, `SetAffinity`, `SetPriority`, `GetCurrentCore` à fond

`SetName` attache un libellé UTF-8 visible au débogueur (Windows `SetThreadDescription`,
Linux/macOS `pthread_setname_np`, tronqué à 15 caractères sous Linux) — ignoré si null, vide ou
non-supporté. `SetAffinity(core)` épingle le fil sur un cœur logique (hors-limites ⇒ ignoré),
ce qui réduit les migrations et les pertes de cache pour un travail soutenu. `SetPriority(p)`
place le fil dans `[-2, +2]` (−2 le plus bas … +2 le plus haut), interpolé vers les constantes
natives ; les priorités temps-réel Linux peuvent demander `CAP_SYS_NICE`. `GetCurrentCore()`
donne le cœur (0-based) du **fil appelant** — utile pour vérifier qu'une affinité a bien pris,
ou pour des stats de répartition.

- **Audio** — épingler + prioriser le fil de mixage pour tenir l'échéance du *callback*.
- **Threading** — étaler ses propres workers sur les cœurs physiques via l'affinité.
- **Outils** — baisser la priorité d'un thread de fond (indexation, compilation) pour ne pas
  voler de temps au rendu.

### `NkThreadLocal<T>` à fond

**Construction et init paresseuse.** Trois constructeurs : par défaut (la valeur sera
default-construite au premier accès — `T` doit être *default-constructible*), par copie d'une
valeur initiale, ou par déplacement de celle-ci. La première fois que `Get()` est appelé **dans
un fil donné**, la copie de ce fil est initialisée (avec la valeur initiale si fournie, sinon
`T{}`). Aucun verrou : c'est du `thread_local` natif, donc lecture/écriture lock-free.

**Accès.** `Get()` renvoie une **référence mutable** vers la copie du fil — et reste `const` sur
l'instance, exprès, pour qu'on puisse modifier la valeur par-thread même via une instance
`const`. `Set(const T&)` / `Set(T&&)` copient ou déplacent dans cette copie. Les opérateurs
`->` et `*` (mutables et const) délèguent à `Get()` pour un usage pointer-like naturel.

- **Threading** — un compteur ou un accumulateur par worker, réduit à la fin (somme des copies)
  sans contention.
- **Rendu / GPU** — un buffer de commandes scratch propre à chaque fil d'enregistrement.
- **Gameplay / IA** — un générateur pseudo-aléatoire par fil (le partage corromprait l'état).
- **Outils** — un tampon temporaire réutilisé par appel sur un fil de traitement.

**Limites à respecter.** Jamais pour **communiquer** entre fils — chaque fil ne voit que sa copie.
Pas de *cleanup* explicite : la destruction se fait par le runtime à la fin du fil, dans un ordre
non spécifié. Et le stockage étant lié à la *fonction* `Get()` (et non à l'instance), deux
`NkThreadLocal<T>` du **même `T`** peuvent partager l'emplacement par-thread — gardez une seule
donnée par type et par fil.

### Alias legacy `nkentseu::NkThreadLocal`

Un `using` expose `NkThreadLocal<T>` directement dans `namespace nkentseu` (sans
`::threading::`). Il est **déprécié** mais à coût nul (résolu à la compilation) ; le code neuf
qualifie via `nkentseu::threading`.

### `NkThreadPool` à fond

**Construction et cycle de vie.** Le constructeur prend un nombre de workers (`0` ⇒ nombre de
cœurs logiques détectés) et démarre les fils **immédiatement**. Le pool est un *Pimpl* (toute
l'implémentation est cachée dans le `.cpp` via `NkUniquePtr<NkThreadPoolImpl>`), **non-copiable
et non-déplaçable** — un pool a une identité unique. Son destructeur appelle `Shutdown()`.

**Soumettre.** `Enqueue(Task)` pousse une tâche (`NkFunction<void()>`) dans la file ; c'est
asynchrone et silencieusement ignoré si le pool est en shutdown. `EnqueuePriority` et
`EnqueueAffinity` existent mais sont **des stubs** : la priorité et l'affinité demandées sont
**ignorées**, la tâche rejoint la file générale. N'écrivez aucune logique qui suppose qu'elles
agissent.

- **ECS / systèmes** — soumettre la mise à jour d'archétypes ou de tuiles de la scène.
- **Rendu** — *bake* de lightmaps, génération de mipmaps, compression de textures en parallèle.
- **Physique** — résoudre des îlots de contraintes indépendants sur plusieurs fils.
- **Animation** — évaluer en masse des poses de squelettes.
- **IO / réseau** — décompresser/parser des paquets ou des assets reçus.
- **Outils** — import par lot, génération de thumbnails.

**Attendre et fermer.** `Join()` bloque jusqu'à ce que la file soit vide **et** tous les workers
oisifs — il tient compte des tâches créées *pendant* l'attente (une tâche qui en soumet d'autres).
`Shutdown()` marque l'arrêt (nouvelles tâches ignorées), réveille et attend les workers, libère
les ressources ; le pool n'est **plus réutilisable** ensuite. Appelez `Join()` **avant** de lire
toute donnée que les tâches ont produite, sous peine de lire pendant l'écriture.

**Requêtes.** `GetNumWorkers()` est immuable et lu sans verrou. `GetQueueSize()` (protégé par
mutex) donne la taille instantanée de la file — pour du **monitoring** seulement, jamais comme
condition de synchronisation. `GetTasksCompleted()` est un compteur cumulatif monotone des tâches
exécutées (utile pour une barre de progression : comparer au nombre soumis).

**Le singleton global.** `GetGlobal()` renvoie un pool partagé, créé paresseusement à la première
demande (*magic static* C++11, thread-safe), dimensionné aux cœurs logiques. Il persiste jusqu'à
la fin du programme — pratique pour un `ParallelFor` ponctuel sans gérer son propre pool, mais à
**éviter dans les DLL chargées dynamiquement** (ordre de destruction incertain).

### `ParallelFor` à fond

Défini *inline* dans le header, `ParallelFor(count, func, grainSize)` découpe `[0, count)` en
chunks de `grainSize` : pour chaque chunk, il calcule `end = min(i + grainSize, count)`, capture
`func`/`i`/`end`, et soumet via `Enqueue` un lambda `mutable` qui exécute
`for (idx = i; idx < end; ++idx) func(idx);`. Il *return* tôt si `count == 0 || grainSize == 0`.
`func` reçoit un index (`nk_size`) et renvoie void ; il **doit** être thread-safe et indépendant
de l'ordre, car les chunks tournent simultanément. **`ParallelFor` ne joint pas** — appelez
`Join()` après. Réglez `grainSize ≈ max(1, count / (numWorkers * 4))` : assez de chunks pour
équilibrer, assez gros pour amortir l'overhead de soumission (`grainSize = 1` sur une grande
boucle est un anti-pattern coûteux).

- **ECS** — `ParallelFor(entities.Size(), updateEntity, 256)` répartit une passe de système.
- **Rendu / GPU** — traiter une grille de tuiles d'écran, ou des faces d'un mesh, en parallèle.
- **Physique / animation** — intégrer N particules ou évaluer N os indépendamment.
- **UI/2D** — disposer en masse des éléments d'une liste hors écran.

### Les pièges à retenir

- **`~NkThread()` fait `Detach()`, pas `Join()`** — un fil non joint survit avec des captures
  potentiellement mortes ; capturez par valeur ou joignez.
- **Une exception non rattrapée dans une `Task` ⇒ `std::terminate`** — enveloppez le corps de la
  tâche dans un `try/catch` au niveau tâche.
- **`EnqueuePriority` / `EnqueueAffinity` n'ont aucun effet réel** (stubs).
- **`grainSize = 1` sur une grosse boucle** = overhead massif de soumission.
- **Toujours `Join()` avant de lire les résultats partagés** d'un pool.
- **Tâches ignorées en silence après `Shutdown()`** — ne soumettez plus rien à un pool fermé.

---

### Exemple

```cpp
#include "NKThreading/NkThread.h"
#include "NKThreading/NkThreadLocal.h"
#include "NKThreading/NkThreadPool.h"
using namespace nkentseu::threading;

// 1) Un fil OS dédié, nommé et joint proprement.
NkThread net;
net.Start(NkThread::ThreadFunc([](void* /*userData*/) {
    // ... boucle réseau en arrière-plan ...
}));
net.SetName("Net");
// ... plus tard ...
if (net.Joinable()) net.Join();

// 2) Stockage par-thread : un compteur privé par worker, sans verrou.
NkThreadLocal<nk_uint64> localCount{ 0 };

// 3) Pool : paralléliser une mise à jour d'entités sur tous les cœurs.
NkThreadPool& pool = NkThreadPool::GetGlobal();
pool.ParallelFor(entities.Size(), [&](nk_size i) {
    entities[i].Update(dt);          // indépendant de l'ordre, thread-safe
    localCount.Set(localCount.Get() + 1);
}, 256);                             // grainSize ≈ count / (numWorkers*4)
pool.Join();                         // ParallelFor ne joint pas : on attend ici
```

---

[← Index NKThreading](README.md) · [Récap NKThreading](../NKThreading.md) · [Couche System](../README.md)
