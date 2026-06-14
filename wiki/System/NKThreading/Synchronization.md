# Les primitives de synchronisation

> Couche **System** · NKThreading · Coordonner plusieurs threads qui partagent des données : la
> variable conditionnelle `NkConditionVariable`, le sémaphore `NkSemaphore`, la barrière de phase
> `NkBarrier`, l'événement Win32-like `NkEvent`, le compte à rebours `NkLatch`, et le verrou
> lecteur/écrivain `NkReaderWriterLock`.

Dès qu'**au moins deux threads touchent la même donnée**, le problème n'est plus « comment calculer »
mais « **qui a le droit de toucher, et quand** ». Un mutex (`NkMutex`, page voisine) répond à la
première moitié : il garantit qu'**un seul** thread entre dans une section critique. Mais il ne sait
pas répondre à la seconde : *attendre qu'une condition devienne vraie* sans brûler le CPU à
interroger en boucle. C'est exactement le rôle des primitives de cette page. Toutes reposent en
interne sur `NkMutex` + `NkConditionVariable` ; toutes sont **portables** (Windows et POSIX derrière
la même façade) ; aucune n'alloue dynamiquement (zéro-STL respecté), aucune n'est copiable ni
déplaçable (ce sont des objets d'**identité**, pas de valeur), et **tout est `noexcept`**.

La règle à graver dès le départ : **on ne *poll* jamais**. Écrire `while (!pret) {}` fait tourner un
cœur à 100 % pour rien. Les primitives ci-dessous *endorment* le thread en attente et le réveillent
au bon moment — c'est tout l'intérêt.

- **Namespace** : `nkentseu::threading`
- **Headers** : `#include "NKThreading/NkConditionVariable.h"`, `NkSemaphore.h`,
  `Synchronization/NkBarrier.h`, `Synchronization/NkEvent.h`, `Synchronization/NkLatch.h`,
  `Synchronization/NkReaderWriterLock.h`

---

## Attendre une condition : `NkConditionVariable`

C'est la primitive **fondatrice**, celle sur laquelle toutes les autres sont bâties. Le problème
qu'elle résout : un thread doit attendre qu'un *prédicat* devienne vrai (« la file n'est plus vide »,
« le travail est prêt »), mais il tient le mutex qui protège justement la donnée à tester. S'il garde
le mutex en attendant, personne ne pourra jamais la modifier — interblocage. La variable
conditionnelle dénoue le nœud : `Wait()` **relâche le mutex** pendant le sommeil, puis le
**ré-acquiert** avant de rendre la main. Le producteur peut donc modifier la donnée, et notifier.

```cpp
NkMutex mtx;
NkConditionVariable cv;
NkVector<Job> queue;

// Consommateur
NkScopedLockMutex lock(mtx);
cv.WaitUntil(lock, [&]{ return !queue.IsEmpty(); });   // dort tant que vide
Job j = queue.Back(); queue.PopBack();
```

Le piège classique tient en un mot : **`while`, pas `if`**. Un `Wait()` peut rendre la main *sans
notification* (réveil parasite, *spurious wakeup* — une réalité du matériel/OS). Il faut donc
**toujours re-tester le prédicat** au réveil. `Wait(lock)` brut vous oblige à écrire la boucle
vous-même ; `WaitUntil(lock, predicate)` l'écrit pour vous (`while(!predicate()) Wait(lock);`) — c'est
la forme à préférer. Côté notification, deux variantes : `NotifyOne()` réveille **un** waiter (le bon
choix quand un seul peut progresser, ex. une file à un consommateur), `NotifyAll()` les réveille
**tous** (au risque du *thundering herd* : tous se ruent, un seul gagne, les autres se rendorment).

Symétriquement à `WaitUntil`, on dispose d'attentes **bornées dans le temps**. `WaitFor(lock, ms)`
attend au plus `ms` millisecondes (timeout **relatif**) et renvoie `true` si une notification l'a
réveillé, `false` sur timeout. `WaitUntil(lock, deadlineMs)` attend jusqu'à un instant **absolu**,
exprimé sur la base de `GetMonotonicTimeMs()` — pratique pour respecter une échéance fixe quelle que
soit la date d'entrée dans l'attente. Ces deux-là sont `[[nodiscard]]` : le booléen de retour
*distingue* notification et timeout, il ne se jette pas.

Ce n'est **pas** un mutex et ce n'est **pas** un signal qui « reste mis » : une notification émise
quand personne n'attend est **perdue**. D'où la discipline indissociable : **modifier la condition ET
notifier sous le mutex détenu**, sinon une notification peut tomber entre le test et le sommeil d'un
waiter.

> **En résumé.** `NkConditionVariable` = attendre qu'un prédicat devienne vrai sans *polling* ; elle
> relâche/ré-acquiert le mutex autour de l'attente. Toujours `WaitUntil(lock, predicate)` (boucle
> anti-réveil-parasite) plutôt que `Wait` nu. `NotifyOne` (un waiter) vs `NotifyAll` (tous). Modifiez
> la condition **et** notifiez sous mutex, sinon notification perdue.

---

## Compter des jetons : `NkSemaphore`

Là où le mutex dit « un seul à la fois », le sémaphore dit « **au plus N à la fois** ». C'est un
**compteur** : `Acquire()` le décrémente (et bloque s'il est à zéro), `Release()` l'incrémente (et
réveille un attendeur). On s'en sert pour **borner une ressource** — N slots dans un pool, N requêtes
réseau simultanées, N tampons GPU disponibles — ou comme **signal de comptage** entre producteur et
consommateur (chaque `Release` annonce « une unité de travail prête », chaque `Acquire` en consomme
une).

```cpp
NkSemaphore slots(0u, 4u);   // capacité 4, départ vide

// Producteur : une frame est prête
if (!slots.Release()) { /* overflow : la capacité serait dépassée */ }

// Consommateur : attendre une frame
slots.Acquire();             // bloque tant que le compteur est à 0
```

Le constructeur prend un `initialCount` (jetons disponibles au départ) et un `maxCount` (capacité,
`0xFFFFFFFF` par défaut = quasi-infini) ; si `maxCount < initialCount`, le plafond est **remonté** à
`initialCount`. L'invariant tenu est `0 <= count <= maxCount`. La conséquence importante :
**`Release(count)` est `[[nodiscard]]` et peut échouer** — il renvoie `false` si l'incrément
dépasserait `maxCount` (protection contre le débordement). Ce booléen n'est pas décoratif : l'ignorer,
c'est risquer de croire qu'on a libéré un jeton qui n'a jamais été ajouté.

Pour ne pas bloquer indéfiniment, deux tentatives non bloquantes : `TryAcquire()` prend un jeton si
disponible (`true`) ou abandonne aussitôt (`false`) ; `TryAcquireFor(ms)` attend au plus `ms`
millisecondes. `GetCount()` lit la valeur courante (sous mutex, donc potentiellement déjà périmée au
moment où vous la lisez) et `GetMaxCount()` la capacité (fixée à la construction, immuable).

Attention : `Acquire()` est **non réentrant**. Le même thread qui acquiert deux fois sans libérer
entre les deux peut s'auto-bloquer si le compteur retombe à zéro. Et le destructeur **ne réveille
pas** les waiters bloqués — c'est à l'appelant de garantir que plus personne n'attend avant de
détruire le sémaphore.

> **En résumé.** `NkSemaphore` = compteur borné `0..maxCount` ; `Acquire`/`Release` pour limiter une
> ressource ou signaler des unités de travail. `Release` est `[[nodiscard]]` (échoue sur overflow) —
> testez-le. `TryAcquire`/`TryAcquireFor` pour ne pas bloquer. Non réentrant ; le dtor ne réveille pas
> les attendeurs.

---

## Se donner rendez-vous : `NkBarrier`

Quand un travail se découpe en **phases** où *tous* les threads doivent finir l'étape *k* avant qu'*aucun*
n'attaque l'étape *k+1*, il faut un **point de rendez-vous**. C'est une barrière. On la construit pour
un nombre fixe de threads `N` ; chacun appelle `Wait()` en fin de phase et **bloque jusqu'à ce que les
`N` soient arrivés** — puis tous repartent ensemble. Et comme elle est **réutilisable**, la phase
suivante recommence sur la même barrière (un compteur de phase interne s'incrémente à chaque
*completion*).

```cpp
NkBarrier barrier(workerCount);

void Worker() {
    SimulateForces();          // phase 1
    if (barrier.Wait()) {      // le dernier arrivé renvoie true
        IntegratePositions();  // action « une seule fois » entre les phases
    }
    barrier.Wait();            // tous repartent pour la phase 2
}
```

La subtilité utile : `Wait()` est `[[nodiscard]]` et renvoie `true` **pour le tout dernier thread
arrivé**, `false` pour les autres. Cela donne un endroit naturel et sûr pour exécuter une **action
post-synchronisation unique** — agréger des résultats partiels, faire avancer l'horloge de simulation,
publier le résultat de la frame — sans avoir à désigner un « thread maître » à l'avance.

Ce n'est **pas** un `NkLatch` : la barrière se **réarme** toute seule pour la phase suivante, alors
que le latch est à usage unique. Deux pièges : appeler `Wait()` avec un nombre de threads **différent**
de `N` mène droit à l'interblocage (la barrière attend des arrivants qui ne viendront jamais) ; et
`Reset()` **force** la *completion* de la phase courante même si tous ne sont pas là (il réveille tout
le monde) — à n'utiliser que si la cohérence des données est garantie par ailleurs, typiquement pour
débloquer un arrêt propre.

> **En résumé.** `NkBarrier` = rendez-vous **réutilisable** de `N` threads par phase ; `Wait()`
> bloque jusqu'à `N` arrivants et renvoie `true` au dernier (pour une action post-sync unique).
> Toujours `N` arrivants exacts sinon deadlock. `Reset()` force la completion (débloquage d'urgence).

---

## Signaler un état : `NkEvent`

L'événement est le **drapeau** du monde Win32 : un état booléen « signalé / non signalé » qu'un thread
*lève* et qu'un autre *attend*. Sa puissance vient de ses **deux modes**, choisis à la construction.
En **ManualReset**, le signal est **persistant** : une fois `Set()`, *tous* les `Wait()` passent
jusqu'à ce qu'on `Reset()` explicitement — idéal pour un état durable du type « le moteur est
initialisé », « le chargement est terminé ». En **AutoReset** (le défaut), le signal est **consommé**
par le premier `Wait()` qui passe : `Set()` ne réveille qu'**un seul** thread, et l'événement
retombe aussitôt non signalé — parfait pour distribuer un travail unitaire à un pool.

```cpp
NkEvent loaded(/*manualReset*/ true);    // état durable

// Thread de chargement
LoadAssets();
loaded.Set();                            // reste signalé pour tous

// Threads consommateurs
loaded.Wait();                           // tous passent une fois Set()
```

`Wait(timeoutMs)` accepte un timeout **signé** (`nk_int32`) avec deux sentinelles parlantes : `-1`
(défaut) = attente **infinie**, `0` = **polling** (retour immédiat avec l'état courant). Il est
`[[nodiscard]]` : `true` = signalé, `false` = timeout — et en AutoReset, le retour `true` **consomme**
le signal. `IsSignaled()` jette un œil à l'état sans attendre (mais l'état peut changer juste après —
c'est une photo, pas une garantie).

Reste `Pulse()`, le plus subtil : il signale puis remet à zéro **immédiatement**, ne réveillant que
les threads **déjà en `Wait()` au moment précis de l'appel** (un signal *éphémère*, tracé en interne
par une génération `mPulseGeneration`). Si personne n'attend, le pulse est **perdu** — par
construction. C'est l'outil d'un *tick* horloge ou d'un *frame boundary* qu'on ne veut surtout pas voir
« mémorisé » pour un futur waiter.

> **En résumé.** `NkEvent` = drapeau signalé/non signalé. **ManualReset** = persistant (tous passent
> jusqu'à `Reset`) ; **AutoReset** (défaut) = consommé par un seul waiter. `Wait(timeoutMs)` avec
> `-1`=infini, `0`=polling, `[[nodiscard]]`. `Pulse()` ne réveille que les waiters déjà présents (sinon
> perdu).

---

## Attendre la fin de plusieurs travaux : `NkLatch`

Le latch répond à une question très fréquente et très précise : « **attends que ces `N` tâches soient
toutes finies** ». On le construit avec un compteur `N` ; chaque tâche terminée appelle `CountDown()` ;
le ou les threads en attente sur `Wait()` repartent **dès que le compteur atteint zéro**. C'est le
*fan-in* naturel d'un *fork/join* : on lance `N` jobs, on attend leur convergence, on continue.

```cpp
NkLatch latch(chunkCount);

for (auto& chunk : chunks)
    pool.Submit([&]{ Process(chunk); latch.CountDown(); });

latch.Wait();              // bloque jusqu'à ce que tous aient décompté
Merge(chunks);             // sûr : tout est fini
```

La différence capitale avec `NkBarrier` : le latch est à **usage unique**. Une fois à zéro, il y reste
— il est **irréversible** et ne se réarme pas. Tous les `Wait()` ultérieurs reviennent
**immédiatement** : c'est le comportement voulu (un « jalon » franchi une fois pour toutes, comme « le
système est prêt »). `CountDown(count)` décrémente de `count` et **clampe à zéro** si on dépasse ;
sur-décompter ne plante donc pas, mais c'est le **signe d'un bug logique** (vous comptez plus de fins
que de débuts). `Wait(timeoutMs)` partage la convention de `NkEvent` (`-1` infini, `0` polling,
`[[nodiscard]]`). Pour le monitoring, `IsReady()` dit si le compteur est à zéro et `GetCount()` donne
les unités restantes.

Un dernier garde-fou : `NkLatch(0)` est **interdit** (assert en debug) — un latch déjà accompli à la
construction n'a pas de sens.

> **En résumé.** `NkLatch` = compte à rebours `N→0` à **usage unique**, irréversible ; `CountDown()`
> côté tâches, `Wait()` côté attente — le *fan-in* d'un fork/join. Contrairement à `NkBarrier`, ne se
> réarme pas. Sur-décompter est clampé (mais c'est un bug) ; `NkLatch(0)` interdit.

---

## Lecteurs nombreux, écrivain seul : `NkReaderWriterLock`

Beaucoup de données partagées sont **lues bien plus souvent qu'écrites** : une table de ressources, un
graphe de scène, un cache de configuration. Y mettre un mutex simple sérialise *tout* — même deux
threads qui ne font que **lire** s'attendent inutilement. Le verrou lecteur/écrivain rétablit le bon
sens : **plusieurs lecteurs en parallèle** (la lecture ne corrompt rien), mais **un seul écrivain à la
fois, en exclusif** (pendant qu'il écrit, aucun lecteur).

```cpp
NkReaderWriterLock rw;

// Lecteurs : en parallèle
{ NkReadLock r(rw);  auto* tex = registry.Find(id); /* ... */ }

// Écrivain : exclusif
{ NkWriteLock w(rw); registry.Insert(id, newTex); }
```

Ce verrou est à **préférence écrivain** : `LockRead()` bloque s'il y a un écrivain actif *ou en
attente* (`mWritersWaiting > 0`). C'est volontaire — cela empêche un flot continu de lecteurs
d'**affamer** un écrivain (la *starvation* classique des RW locks naïfs). On dispose des tentatives non
bloquantes `TryLockRead()` / `TryLockWrite()` (renvoient `false` plutôt que d'attendre), et chaque
verrou pris doit être rendu par le `Unlock*` correspondant.

En pratique, on ne manipule **jamais** les `Lock/Unlock` à la main : on utilise les gardes **RAII**
`NkReadLock` et `NkWriteLock`. Construits, ils prennent le verrou (`LockRead` / `LockWrite`) ;
détruits, ils le rendent (`UnlockRead` / `UnlockWrite`) — **même en cas de retour anticipé ou
d'exception**. C'est exactement le rôle de `NkScopedLockMutex` pour le mutex : le verrou ne peut plus
« fuir ».

Deux pièges sérieux. D'abord, **ne tentez jamais de passer d'un read à un write sur le même verrou en
le tenant** : l'écrivain attend la fin de tous les lecteurs, dont vous-même — interblocage immédiat
(relâchez le read, prenez le write). Ensuite, **ne tenez pas un verrou pendant des I/O** (fichier,
réseau) : vous bloqueriez tous les autres pendant une opération lente et imprévisible — copiez la donnée
sous verrou, relâchez, puis faites l'I/O.

> **En résumé.** `NkReaderWriterLock` = lecteurs concurrents **ou** un écrivain exclusif, à
> **préférence écrivain** (anti-starvation). Utilisez les gardes RAII `NkReadLock` / `NkWriteLock`
> plutôt que les `Lock/Unlock` nus. Jamais de read→write sur le même verrou (deadlock) ; jamais d'I/O
> verrou tenu.

---

## Aperçu de l'API

Tous les types vivent dans `nkentseu::threading`, sont **non copiables** et **non déplaçables**, et
toutes leurs méthodes sont `noexcept`. Les timeouts relatifs (`milliseconds`) sont des `nk_uint32` ;
les `Wait(timeoutMs)` de `NkEvent`/`NkLatch` sont des `nk_int32` avec sentinelles `-1` (infini) / `0`
(polling). Les retours `[[nodiscard]]` sont signalés `[nd]`.

### `NkConditionVariable` — attente sur prédicat

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkConditionVariable()`, `~NkConditionVariable()` | Init/destruction (infaillible Win, marquée POSIX). |
| Attente nue | `Wait(lock)` | Attend une notification ; relâche/ré-acquiert le mutex. **Tester en `while`**. |
| Attente bornée | `WaitFor(lock, ms)` `[nd]` | Timeout **relatif** ; `true` = notifié, `false` = timeout. |
| Attente bornée | `WaitUntil(lock, deadlineMs)` `[nd]` | Timeout jusqu'à **deadline absolue** (`GetMonotonicTimeMs`). |
| Attente + prédicat | `WaitUntil(lock, predicate)` | Boucle `while(!predicate()) Wait(lock)` (anti-réveil-parasite). |
| Attente + prédicat | `WaitUntil(lock, deadlineMs, predicate)` `[nd]` | Deadline **et** prédicat combinés. |
| Notification | `NotifyOne()` | Réveille **un** waiter (sélection arbitraire). |
| Notification | `NotifyAll()` | Réveille **tous** les waiters (risque thundering herd). |
| Temps | `static GetMonotonicTimeMs()` | Horloge monotone en ms (base des deadlines). |

### `NkSemaphore` — compteur de jetons borné

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkSemaphore(initialCount=0, maxCount=0xFFFFFFFF)` | Compteur initial + plafond (remonté si `< initialCount`). |
| Acquisition | `Acquire()` | Décrémente ; bloque si compteur à 0 (non réentrant). |
| Acquisition | `TryAcquire()` `[nd]` | Non bloquant ; `true` si un jeton dispo. |
| Acquisition | `TryAcquireFor(ms)` `[nd]` | Timeout **relatif** ; `true` si acquis à temps. |
| Libération | `Release(count=1)` `[nd]` | Incrémente + notifie ; `false` si overflow `> maxCount`. |
| Lecture | `GetCount()` `[nd]`, `GetMaxCount()` `[nd]` | Valeur courante (peut être périmée) / capacité (immuable). |

### `NkBarrier` — rendez-vous de phase réutilisable

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkBarrier(numThreads)` | `numThreads > 0` (assert debug). Réutilisable. |
| Rendez-vous | `Wait()` `[nd]` | Bloque jusqu'à `numThreads` arrivants ; `true` au **dernier**. |
| Débloquage | `Reset()` | Force la completion de la phase + réveille tout le monde. |

### `NkEvent` — drapeau signalé/non signalé (Win32-like)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkEvent(manualReset=false, initialState=false)` | Défaut = AutoReset, non signalé. |
| Signal | `Set()` | Signale (persistant en manual / consommé au 1ᵉʳ Wait en auto). |
| Signal | `Reset()` | Remet non signalé. |
| Signal | `Pulse()` | Signal **éphémère** : ne réveille que les waiters déjà présents. |
| Attente | `Wait(timeoutMs=-1)` `[nd]` | `-1` infini, `0` polling ; `true`=signalé. AutoReset consomme. |
| Lecture | `IsSignaled()` `[nd]` | État courant sans attendre (volatile). |

### `NkLatch` — compte à rebours à usage unique

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkLatch(initialCount)` | `initialCount > 0` (assert debug). Irréversible. |
| Décompte | `CountDown(count=1)` | Décrémente (clampé à 0) ; réveille tous si atteint 0. |
| Attente | `Wait(timeoutMs=-1)` `[nd]` | `-1` infini, `0` polling ; `true` si compteur à 0. |
| Lecture | `IsReady()` `[nd]`, `GetCount()` `[nd]` | À zéro ? / unités restantes (monitoring). |

### `NkReaderWriterLock` — lecteurs concurrents / écrivain exclusif

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkReaderWriterLock()` | Compteurs à zéro. Préférence **écrivain**. |
| Lecture | `LockRead()`, `TryLockRead()` `[nd]`, `UnlockRead()` | Accès partagé ; bloque si écrivain actif **ou** en attente. |
| Écriture | `LockWrite()`, `TryLockWrite()` `[nd]`, `UnlockWrite()` | Accès exclusif ; bloque jusqu'à fin des lecteurs. |

### `NkReadLock` / `NkWriteLock` — gardes RAII

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Garde lecture | `NkReadLock(lock)`, `~NkReadLock()` | `LockRead` à la construction, `UnlockRead` à la destruction. |
| Garde écriture | `NkWriteLock(lock)`, `~NkWriteLock()` | `LockWrite` à la construction, `UnlockWrite` à la destruction. |

---

## Référence complète

Chaque primitive est reprise ici à fond : comportement, complexité conceptuelle et usages dans les
différents domaines du moteur. Les opérations triviales sont décrites brièvement ; les choix qui
font ou défont la justesse d'un programme concurrent le sont en détail.

### Choisir : quelle primitive pour quel besoin

La question n'est jamais « laquelle marche » mais « **quelle relation entre threads** » :

- **« Attends qu'une condition devienne vraie »** → `NkConditionVariable` (la brique de base).
- **« Au plus N en même temps » / « signaler des unités »** → `NkSemaphore`.
- **« Tout le monde finit l'étape k avant l'étape k+1 », répété** → `NkBarrier`.
- **« Lève/baisse un drapeau d'état »** → `NkEvent` (persistant ou auto-consommé).
- **« Attends que ces N tâches soient toutes finies », une fois** → `NkLatch`.
- **« Beaucoup de lectures, peu d'écritures »** → `NkReaderWriterLock`.

### `NkConditionVariable` à fond

**Le contrat du mutex.** `Wait`, `WaitFor` et `WaitUntil` exigent que le `NkScopedLockMutex` passé
soit **détenu** à l'appel. Pendant l'attente, le mutex est relâché (les autres peuvent progresser) ;
au réveil, il est ré-acquis avant le retour, de sorte que vous restez en section critique pour
re-tester la condition. C'est ce va-et-vient qui rend le motif sûr.

**Réveils parasites.** Le matériel et l'OS autorisent un retour de `Wait` sans notification. La parade
est invariable : re-tester le prédicat dans une boucle `while`. `WaitUntil(lock, predicate)` encapsule
exactement cette boucle ; la surcharge `WaitUntil(lock, deadlineMs, predicate)` y ajoute une échéance
(elle renvoie `false` si la deadline expire avant que le prédicat soit vrai).

**Deadline relative vs absolue.** `WaitFor(ms)` repart de l'instant de l'appel ; `WaitUntil(deadlineMs)`
vise un instant fixe sur l'horloge `GetMonotonicTimeMs()` (Windows `GetTickCount64`, wrap ~584 jours ;
POSIX `clock_gettime(CLOCK_MONOTONIC)`). L'absolu est plus juste quand plusieurs attentes successives
doivent respecter **une seule** échéance globale.

Usages par domaine :
- **Threading / job system** — file de travail producteur/consommateur : les workers `WaitUntil(lock,
  !queue.IsEmpty())`, le soumetteur `NotifyOne()` par job (ou `NotifyAll()` pour un *flush*).
- **Audio** — le thread mixeur attend que de nouveaux échantillons soient mis en file par le moteur de
  jeu ; `WaitFor` borné évite de geler si la source se tarit.
- **IO / réseau** — un thread d'arrière-plan dort jusqu'à ce qu'une requête arrive ; deadline absolue
  pour un *timeout* de session.
- **Outils / éditeur** — un *file watcher* réveille le thread de rechargement à chaque modification
  détectée.

### `NkSemaphore` à fond

**Compteur, pas verrou.** Rien n'impose que celui qui `Acquire` soit celui qui `Release` — c'est ce
qui en fait un bon **signal** entre threads distincts, là où un mutex exige un propriétaire unique.
L'invariant `0 <= count <= maxCount` est tenu sous mutex interne.

**`Release` faillible.** Parce que le plafond `maxCount` est réel, `Release(count)` renvoie `false`
si l'ajout dépasserait la capacité, sans rien modifier. C'est pourquoi il est `[[nodiscard]]` : sur un
sémaphore borné, ce booléen révèle un déséquilibre producteur/consommateur.

Usages par domaine :
- **Rendu / GPU** — borner le nombre de frames *in-flight* (double/triple buffering : un sémaphore à
  `maxCount=2` ou `3` régule la profondeur de la pipeline CPU↔GPU).
- **IO / réseau** — limiter les requêtes simultanées (pool de connexions, *rate limiting* d'un
  téléchargeur d'assets).
- **Threading** — un *bounded buffer* producteur/consommateur se code avec deux sémaphores (places
  libres / éléments disponibles).
- **Outils** — un pool de N handles de fichiers ou de N contextes de compilation réutilisables.

### `NkBarrier` à fond

**Phases et réutilisation.** Le compteur de phase interne s'incrémente à chaque *completion*, ce qui
réarme automatiquement la barrière pour l'itération suivante : un même `NkBarrier` sert toute une
simulation, frame après frame. `Wait()` renvoyant `true` au **dernier** arrivant offre le point
naturel pour l'action « une fois entre les phases ».

**`Reset` est une issue de secours.** Il force la completion même incomplète et réveille tout le
monde ; on l'emploie pour démanteler proprement un système (arrêt demandé) — pas dans le flot normal.

Usages par domaine :
- **Physique** — solveur en passes : *broadphase* → détection → résolution, chaque passe se termine
  pour tous avant la suivante ; le dernier thread intègre les positions.
- **Animation** — évaluer toutes les poses (phase 1) avant de calculer le *skinning* (phase 2).
- **ECS** — systèmes en étages : un *stage* parallèle se synchronise avant le *stage* suivant pour
  éviter les courses entre systèmes lecteurs/écrivains.
- **Rendu** — N threads construisent des *command buffers* en parallèle, la barrière garantit qu'ils
  sont tous prêts avant la soumission unique.

### `NkEvent` à fond

**Deux modes, deux usages distincts.** ManualReset modélise un **état durable** (« prêt »,
« en pause »), lu par un nombre quelconque de threads tant qu'on ne `Reset()` pas. AutoReset modélise
un **jeton de travail** consommé par un seul réveil — la base d'une distribution un-à-un.

**`Pulse`, le cas qui surprend.** Il signale et remet à zéro dans le même souffle, ne touchant que
les threads **déjà** en `Wait`. Le mécanisme interne (`mPulseGeneration`) sert précisément à ce qu'un
waiter entrant *après* le pulse ne le « capte » pas. Un pulse sans waiter est silencieusement perdu —
c'est le comportement voulu pour des signaux instantanés.

Usages par domaine :
- **Gameplay** — un drapeau global « jeu en pause » (ManualReset) que tous les sous-systèmes
  consultent.
- **Threading** — réveiller un seul worker endormi à l'arrivée d'un job (AutoReset).
- **Rendu** — `Pulse()` comme *frame boundary* : ne réveiller que les threads déjà en attente du début
  de frame, sans mémoriser le tick pour un retardataire.
- **Outils / éditeur** — signaler « build terminé » (ManualReset) à l'UI et aux scripts qui
  l'attendent.

### `NkLatch` à fond

**Jalon à sens unique.** Le latch ne se réarme pas : une fois à zéro, c'est définitif, et tout `Wait`
postérieur revient sans bloquer. C'est exactement ce qu'on veut d'un *milestone* franchi une bonne
fois (« assets de base chargés », « sous-systèmes initialisés »). Le `CountDown` clampé à zéro rend
l'objet robuste à une sur-décrémentation, mais celle-ci trahit un bug de comptage à corriger.

Usages par domaine :
- **Chargement / IO** — attendre que tous les chunks d'un niveau soient parsés avant de spawn la
  scène (`CountDown` par chunk fini, `Wait` côté démarrage).
- **Threading** — *fork/join* : éclater un calcul en N sous-tâches, `Wait` la convergence, fusionner.
- **Moteur / boot** — `Wait` que les N sous-systèmes (audio, rendu, réseau) aient signalé leur
  initialisation avant d'entrer dans la boucle principale.
- **Outils** — barre de progression : `GetCount()` alimente l'avancement, `Wait` la fin.

### `NkReaderWriterLock` à fond

**Le bon compromis lecture/écriture.** Plusieurs `LockRead` coexistent ; un `LockWrite` est exclusif
de tout le reste. La **préférence écrivain** (`LockRead` bloque dès qu'un écrivain attend) évite que
les lectures, souvent bien plus fréquentes, ne repoussent indéfiniment une écriture. Deux variables
conditionnelles internes (lecteurs / écrivains) permettent de ne réveiller que la bonne catégorie :
`NotifyAll` pour relancer la foule des lecteurs, `NotifyOne` pour passer la main à un écrivain.

**RAII obligatoire en pratique.** `NkReadLock` et `NkWriteLock` garantissent le `Unlock`
correspondant à la sortie du *scope*, exception comprise — un `UnlockRead` sans `LockRead` (ou
inversement) est un **comportement indéfini**, donc on ne les appelle pas à la main.

Usages par domaine :
- **Rendu / outils** — registre d'assets (textures, meshes) : lu par tous les threads de rendu,
  écrit rarement lors d'un *hot-reload* (`NkWriteLock` le temps d'insérer).
- **ECS / scène** — graphe de scène ou table de transforms consultée en masse, modifiée
  ponctuellement (ajout/retrait d'entité).
- **Gameplay / IA** — table de *blackboard* partagée : beaucoup d'agents lisent, peu écrivent.
- **Réseau** — table de sessions/joueurs lue à chaque paquet, modifiée aux connexions/déconnexions.

### Le socle commun

- **Tout sur mutex + condition variable.** Sémaphore, barrière, événement, latch et RW lock sont bâtis
  au-dessus de `NkMutex` et `NkConditionVariable` — comprendre la condition variable, c'est comprendre
  toute la page.
- **Ni copie ni déplacement.** Toutes ces classes sont des objets d'**identité** (un état partagé
  par référence) : copy ctor et `operator=` sont `= delete`, et aucun move n'est déclaré. On les
  partage **par référence**, jamais par valeur.
- **`noexcept` partout, zéro allocation.** Aucune de ces opérations ne lance ni n'alloue
  dynamiquement (zéro-STL). Les tentatives/attentes renvoient un booléen `[[nodiscard]]` plutôt que de
  signaler par exception.
- **Portabilité unifiée.** Une seule façade pour Windows (`CONDITION_VARIABLE`, `GetTickCount64`) et
  POSIX (`pthread_cond_t`, `clock_gettime`) — le code applicatif ne voit jamais la différence.

---

### Exemple

```cpp
#include "NKThreading/NkConditionVariable.h"
#include "NKThreading/NkSemaphore.h"
#include "NKThreading/Synchronization/NkLatch.h"
#include "NKThreading/Synchronization/NkReaderWriterLock.h"
using namespace nkentseu::threading;

// File de travail : attente sur prédicat, sans polling.
NkMutex mtx; NkConditionVariable cv; NkVector<Job> queue;
auto consume = [&]{
    NkScopedLockMutex lock(mtx);
    cv.WaitUntil(lock, [&]{ return !queue.IsEmpty(); });   // while interne
    Job j = queue.Back(); queue.PopBack();
    return j;
};

// Sémaphore : borner les frames in-flight (triple buffering).
NkSemaphore inFlight(3u, 3u);
inFlight.Acquire();                       // attendre un slot
// ... soumettre la frame ...
if (!inFlight.Release()) { /* déséquilibre */ }

// Latch : fork/join sur N chunks.
NkLatch latch(chunkCount);
for (auto& c : chunks) pool.Submit([&]{ Process(c); latch.CountDown(); });
latch.Wait();                             // converger
Merge(chunks);

// RW lock : registre d'assets, lecteurs concurrents / écrivain exclusif.
NkReaderWriterLock rw;
{ NkReadLock r(rw);  auto* tex = registry.Find(id); }   // lecture parallèle
{ NkWriteLock w(rw); registry.Insert(id, loaded); }     // écriture exclusive
```

---

[← Index NKThreading](README.md) · [Récap NKThreading](../NKThreading.md) · [Couche System](../README.md)
