# Guide 1 — NKMemory : la mémoire sans `new`/`delete`

> [← Retour au sommaire des guides](README.md) · Guide suivant : [NKWindow](02-NKWindow.md)
>
> Couche : **Foundation**. C'est le tout premier module à comprendre : toutes les
> couches au-dessus (NKWindow, NKCanvas, NKAudio…) allouent leur mémoire avec lui.
> Style SFML : on part du plus simple, on ajoute une brique à la fois, code réel et
> compilable.

---

## 1. Introduction : pourquoi un allocateur maison ?

Nkentseu est un moteur **zero-STL** et **haute performance**. Il n'utilise ni
`std::malloc`, ni `new`/`delete` bruts dans son code. À la place, tout passe par le
module **NKMemory**, qui apporte trois choses qu'on n'a pas avec le tas du système :

1. **Le tracking** : qui a alloué quoi, où (fichier, ligne, fonction), combien
   d'octets sont vivants, où sont les fuites.
2. **Des stratégies adaptées** : un allocateur linéaire pour les données « par frame »,
   un pool pour les objets identiques, une arène pour un niveau de jeu… chacun beaucoup
   plus rapide que le `malloc` généraliste dans son cas d'usage.
3. **La cohérence** : un objet alloué par NKMemory doit être libéré par NKMemory. C'est
   la seule façon d'éviter la corruption de tas (voir §11).

### La règle d'or

> **On n'écrit JAMAIS `new`, `delete`, `malloc`, `free` bruts dans le code Nkentseu.**
> On alloue et on libère **uniquement** via NKMemory.

Mélanger les deux mondes — par exemple allouer avec NKMemory puis libérer avec
`delete[]` — provoque une **corruption de tas** (sous Windows : crash `0xc0000374`).
Ce guide vous montre comment faire les choses correctement.

Tout NKMemory vit dans le namespace `nkentseu::memory`. Dans les exemples qui suivent,
on suppose souvent en tête de fichier :

```cpp
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkUniquePtr.h"

using namespace nkentseu::memory;
```

---

## 2. Les concepts de base

### 2.1 L'allocateur (`NkAllocator`)

Un **allocateur** est un objet qui sait donner de la mémoire et la reprendre. Tous les
allocateurs de NKMemory dérivent de la même interface, `NkAllocator`, qui offre :

| Méthode | Rôle |
|---------|------|
| `Allocate(size, alignment)` | Donne un bloc brut de `size` octets |
| `Deallocate(ptr)` | Rend un bloc obtenu par `Allocate` |
| `Calloc(size, alignment)` | Comme `Allocate` mais le bloc est mis à zéro |
| `Reallocate(ptr, oldSize, newSize, ...)` | Redimensionne un bloc |
| `Reset()` | Libère **tout** d'un coup (pour les allocateurs qui le permettent) |
| `New<T>(args...)` / `Delete(ptr)` | Crée / détruit un **objet C++** typé (§4) |
| `NewArray<T>(n, ...)` / `DeleteArray(ptr)` | Crée / détruit un **tableau** d'objets |

`alignment` doit être une puissance de 2 ; par défaut l'alignement naturel suffit
(constante `NK_MEMORY_DEFAULT_ALIGNMENT`).

### 2.2 L'allocateur par défaut

Vous n'êtes pas obligé de créer un allocateur : il en existe un **par défaut**, prêt à
l'emploi, récupérable avec `NkGetDefaultAllocator()` :

```cpp
NkAllocator& alloc = NkGetDefaultAllocator();
```

C'est lui qu'utilisent toutes les fonctions globales (`NkAlloc`, `NkFree`,
`NkMakeUnique`…) quand on ne précise rien. Par défaut c'est un allocateur basé sur
`malloc`/`free` (`NkMallocAllocator`), fiable et portable.

On peut le **remplacer** au tout début de l'application (avant tout usage mémoire) :

```cpp
// À faire UNE fois, au démarrage, AVANT d'allouer quoi que ce soit.
static NkMallocAllocator monAlloc;
NkSetDefaultAllocator(&monAlloc);   // passer nullptr remet le défaut d'origine
```

Pour la majorité des projets, vous ne toucherez jamais à ça : l'allocateur par défaut
fait très bien le travail.

---

## 3. Allouer et libérer un buffer brut

Le cas le plus simple : un bloc d'octets, sans constructeur ni destructeur (un buffer
de pixels, un tampon de fichier, etc.). On utilise les **fonctions globales** :

```cpp
// Allouer 1024 octets via l'allocateur par défaut.
void* buffer = NkAlloc(1024);
if (buffer) {
    // ... utilisation ...
    NkFree(buffer);          // toujours libérer avec NkFree
}
```

Variante mise à zéro (l'équivalent de `calloc`) — `NkAllocZero(count, size)` :

```cpp
// 256 entiers, tous initialisés à 0.
nk_int32* tab = static_cast<nk_int32*>(NkAllocZero(256, sizeof(nk_int32)));
// ...
NkFree(tab);
```

Redimensionner un bloc avec `NkRealloc` (il faut fournir l'ancienne taille pour la
copie) :

```cpp
void* p = NkAlloc(64);
// besoin de plus de place :
void* grown = NkRealloc(p, 64, 256);   // 64 -> 256 octets
if (grown) p = grown;                  // si échec, p reste valide
NkFree(p);
```

> Toutes ces fonctions acceptent un allocateur explicite en argument optionnel, par
> exemple `NkAlloc(1024, &monAlloc)`. Sans argument, c'est l'allocateur par défaut.

**À retenir :** un bloc obtenu par `NkAlloc`/`NkAllocZero`/`NkRealloc` se libère
**toujours** avec `NkFree` — jamais avec `delete` ni `free`.

---

## 4. Allouer et libérer un objet C++ : `New<T>` / `Delete`

`NkAlloc` donne des octets bruts ; il n'appelle pas de constructeur. Pour un **objet
C++** (qui a un constructeur et un destructeur), on utilise les helpers typés de
l'allocateur : `New<T>(...)` et `Delete(ptr)`.

```cpp
struct Joueur {
    int   vie;
    float x, y;
    Joueur(int v) : vie(v), x(0.f), y(0.f) {}
};

NkAllocator& alloc = NkGetDefaultAllocator();

// Allocation + construction (le constructeur Joueur(int) est appelé) :
Joueur* j = alloc.New<Joueur>(100);
if (j) {
    j->x = 42.f;
    // ...
    alloc.Delete(j);   // appelle ~Joueur() PUIS libère la mémoire
}
```

- `New<T>(args...)` alloue la mémoire **et** construit l'objet (les arguments sont
  transmis au constructeur de `T`). Il renvoie `nullptr` si l'allocation échoue.
- `Delete(ptr)` appelle le destructeur **puis** libère. Il est sûr sur `nullptr`
  (ne fait rien).

C'est exactement le motif utilisé dans le moteur. Extrait réel d'une *factory* de jeux
(`Applications/Mou/src/Mou/Games/Common/GameFactory.cpp`) :

```cpp
if (!allocator) allocator = &NkGetDefaultAllocator();

MouGame* game = allocator->New<CouleursGame>();   // crée le jeu
// ...
if (game && !game->Init()) {
    allocator->Delete(game);                       // échec -> on détruit proprement
    return {};
}
```

### Tableaux d'objets : `NewArray` / `DeleteArray`

Pour un tableau d'objets construits (et non un simple buffer), utilisez la paire
dédiée — elle construit chaque élément et, à la destruction, appelle chaque destructeur :

```cpp
// 64 Joueur, chacun construit avec Joueur(100)
Joueur* equipe = alloc.NewArray<Joueur>(64, 100);
// ...
alloc.DeleteArray(equipe);   // détruit les 64 éléments puis libère
```

> **Ne mélangez pas les paires.** Un `New<T>` se ferme avec `Delete`, un `NewArray<T>`
> avec `DeleteArray`, un `NkAlloc` avec `NkFree`. Chaque allocation a sa libération
> jumelle.

---

## 5. Les smart pointers : `NkUniquePtr`

Appeler `Delete` à la main, c'est risqué : un `return` au mauvais endroit et la mémoire
fuit. La solution idiomatique est le **pointeur unique** `NkUniquePtr<T>`, qui détruit
automatiquement l'objet quand il sort de portée. C'est l'équivalent maison de
`std::unique_ptr`.

La façon recommandée de le créer est la fabrique `NkMakeUnique<T>(args...)` :

```cpp
#include "NKMemory/NkUniquePtr.h"
using namespace nkentseu::memory;

{
    NkUniquePtr<Joueur> j = NkMakeUnique<Joueur>(100);   // construit Joueur(100)
    if (j.IsValid()) {
        j->x = 10.f;        // accès comme un pointeur normal
        (*j).vie = 50;
    }
}   // <-- ici, fin de portée : ~Joueur() + libération automatiques
```

Propriétés clés :

- **Ownership unique** : un seul `NkUniquePtr` possède l'objet. On ne peut pas le
  **copier**, mais on peut le **déplacer** (le transférer).
- Accès : `j->membre`, `*j`, et `j.Get()` pour le pointeur brut.
- Test de validité : `j.IsValid()` ou directement `if (j) { ... }`.

### Transférer la propriété (move)

Comme on ne peut pas copier, on **déplace**. C'est parfait pour renvoyer un objet
fraîchement créé depuis une fonction :

```cpp
NkUniquePtr<Joueur> CreerJoueur() {
    return NkMakeUnique<Joueur>(100);   // la propriété sort de la fonction
}

void Quelquepart() {
    NkUniquePtr<Joueur> j = CreerJoueur();   // transfert, aucune copie
    // ...
}
```

C'est exactement ce que renvoie la *factory* du moteur :

```cpp
nkentseu::memory::NkUniquePtr<MouGame>
GameFactory::CreateGame(GameId id, ...) {
    MouGame* game = allocator->New<CouleursGame>();
    // ...
    return NkUniquePtr<MouGame>(game, NkDefaultDelete<MouGame>(allocator));
}
```

Ici l'objet est créé via un allocateur précis, puis emballé dans un `NkUniquePtr` en
lui passant un **deleter** (`NkDefaultDelete<MouGame>`) configuré avec ce même
allocateur — pour que la libération se fasse bien avec le bon allocateur (voir §5.2).

### 5.1 Reprendre / relâcher la main

```cpp
NkUniquePtr<Joueur> j = NkMakeUnique<Joueur>(100);

Joueur* brut = j.Release();   // j ne possède plus rien ; à VOUS de libérer 'brut'
// ...
NkGetDefaultAllocator().Delete(brut);   // libération manuelle

j.Reset();                    // détruit l'objet courant (s'il y en a un)
```

- `Release()` : abandonne la propriété **sans** détruire ; renvoie le pointeur brut.
  Utile pour passer la main à une API qui prend l'ownership.
- `Reset(ptr = nullptr)` : détruit l'objet courant et le remplace (par `nullptr` pour
  un simple vidage).

### 5.2 `NkDefaultDelete` et l'allocateur du pointeur

Par défaut, un `NkUniquePtr<T>` utilise le **deleter** `NkDefaultDelete<T>`, qui libère
via l'allocateur par défaut. Si votre objet a été alloué dans un allocateur
**spécifique** (un pool, une arène…), il faut que le pointeur libère dans **le même**
allocateur. Deux façons :

```cpp
// (a) fabrique avec allocateur explicite :
NkPoolAllocator pool(sizeof(Joueur), 100);
NkUniquePtr<Joueur> j = NkMakeUniqueWithAllocator<Joueur>(pool, 100);
// -> j libérera Joueur DANS le pool à la destruction.

// (b) construction manuelle avec un deleter configuré :
Joueur* raw = pool.New<Joueur>(100);
NkUniquePtr<Joueur> j2(raw, NkDefaultDelete<Joueur>(&pool));
```

### 5.3 Tableaux et deleter personnalisé

Pour un tableau, il existe la spécialisation `NkUniquePtr<T[]>`, construite avec
`NkMakeUniqueArray<T>(count)` ; l'accès se fait par `operator[]` :

```cpp
NkUniquePtr<float[]> data = NkMakeUniqueArray<float>(256);
for (nk_size i = 0; i < 256; ++i) data[i] = static_cast<float>(i);
// destruction du tableau automatique en fin de portée
```

Le deleter peut aussi gérer une ressource **non-mémoire** (le pointeur unique devient
alors un garde-fou « RAII » générique) :

```cpp
struct FileDeleter {
    void operator()(FILE* f) const noexcept { if (f) fclose(f); }
};
NkUniquePtr<FILE, FileDeleter> f(fopen("data.txt", "r"), FileDeleter());
// fclose() est appelé automatiquement en fin de portée.
```

---

## 6. Les allocateurs spécialisés : lequel quand ?

L'allocateur par défaut convient à 90 % des cas. Mais pour des patterns particuliers,
NKMemory fournit des allocateurs dédiés, beaucoup plus rapides dans leur niche. Tous
dérivent de `NkAllocator` : on peut donc les passer partout où un `NkAllocator*` est
attendu (y compris à `NkMakeUniqueWithAllocator`).

| Cas d'usage | Allocateur | Pourquoi |
|-------------|-----------|----------|
| Allocations générales | `NkMallocAllocator` | Portable, fiable (c'est le défaut) |
| Objets C++ via `new/delete` | `NkNewAllocator` | S'appuie sur `operator new` |
| Très grandes régions, permissions | `NkVirtualAllocator` | `VirtualAlloc`/`mmap` bas niveau |
| Temporaire « par frame » | `NkLinearAllocator` | `Allocate` en O(1), tout libéré d'un `Reset()` |
| Allocations avec retour arrière | `NkArenaAllocator` | markers pour libérer partiellement |
| Scope LIFO déterministe | `NkStackAllocator` | on libère dans l'ordre inverse |
| Beaucoup d'objets de même taille | `NkPoolAllocator` | `Allocate`/`Deallocate` en O(1), cache-friendly |
| Tailles variables imprévisibles | `NkFreeListAllocator` | flexible, fusionne les blocs libres |
| Réduire la fragmentation | `NkBuddyAllocator` | blocs en puissances de 2 |

> Note d'état : `NkMallocAllocator` et `NkPoolAllocator` sont les piliers livrés et
> éprouvés du module. Les variantes arène / linéaire / stack / buddy / virtual sont
> décrites par l'interface et couvertes par des tests, mais le module continue de
> consolider leur exposition publique (voir la
> [ROADMAP du module](../Kernel/Foundation/NKMemory/ROADMAP.md)). En cas de doute,
> restez sur l'allocateur par défaut.

### 6.1 Linéaire : « tout jeter à la fin de la frame »

Un allocateur linéaire avance un simple curseur (« bump pointer ») : allouer est
quasi gratuit, mais on **ne libère pas individuellement** — on remet tout à zéro d'un
coup avec `Reset()`. Idéal pour les données temporaires recalculées chaque frame.

```cpp
NkLinearAllocator frame(1 * 1024 * 1024);   // 1 Mo de scratch

void EndFrame() {
    // ... pendant la frame, on a fait plein de frame.Allocate(...) ...
    frame.Reset();   // libère TOUT en O(1), prêt pour la frame suivante
}
```

On peut interroger l'état : `frame.Capacity()`, `frame.Used()`, `frame.Available()`.

### 6.2 Arène : libération partielle avec markers

L'arène est comme le linéaire mais on peut poser un **marker** et revenir dessus, en
libérant tout ce qui a été alloué après :

```cpp
NkArenaAllocator arena(64 * 1024);

NkArenaAllocator::Marker m = arena.CreateMarker();
void* tmp1 = arena.Allocate(256);
void* tmp2 = arena.Allocate(512);
// ...
arena.FreeToMarker(m);   // libère tmp1 et tmp2, garde ce qui précède le marker
```

### 6.3 Pool : des objets identiques, ultra-rapide

Un pool gère un nombre fixe de blocs de **taille fixe** via une free-list : allouer et
libérer sont en O(1), sans fragmentation. Parfait pour des objets créés/détruits très
souvent (particules, projectiles, nœuds…).

```cpp
// 100 blocs de la taille d'un Joueur :
NkPoolAllocator pool(sizeof(Joueur), 100);

Joueur* j = pool.New<Joueur>(100);   // pris dans le pool
// ...
pool.Delete(j);                       // rendu au pool
```

### 6.4 Stack : LIFO pour scopes imbriqués

Le `NkStackAllocator` ne libère que la **dernière** allocation (Last-In-First-Out),
ce qui colle bien aux scopes imbriqués à durée de vie déterministe.

---

## 7. Utilitaires mémoire

NKMemory fournit des opérations mémoire bas niveau, optimisées (potentiellement SIMD),
dans `NKMemory/NkUtils.h` — à préférer aux `memcpy`/`memset` de la libc pour rester
cohérent et profiter des optimisations du moteur.

```cpp
#include "NKMemory/NkUtils.h"
using namespace nkentseu::memory;

struct Data { int x, y, z; } d;

NkMemZero(&d, sizeof(d));               // met tout à 0
NkMemSet(&d, 0xFF, sizeof(d));          // remplit d'une valeur

char src[256], dst[256];
NkMemCopy(dst, src, sizeof(src));       // copie (zones NON chevauchantes)
NkMemMove(dst, src, sizeof(src));       // copie (chevauchement autorisé)

int diff = NkMemCompare(dst, src, sizeof(src));   // 0 si identiques
```

Helpers d'alignement utiles quand on gère soi-même des buffers :

```cpp
bool p2  = NkIsPowerOfTwo(64);          // true
bool ali = NkIsAlignedPtr(ptr, 16);     // ptr est-il aligné sur 16 octets ?
```

---

## 8. (Avancé) Le système mémoire global et le tracking

Pour le **debugging** et le **suivi des fuites**, NKMemory expose un singleton,
`NkMemorySystem`, et des macros pratiques (dans `NKMemory/NKMemory.h`) qui injectent
automatiquement fichier/ligne/fonction de l'appel :

```cpp
#include "NKMemory/NKMemory.h"
using namespace nkentseu::memory;

void* buf = NK_MEM_ALLOC(1024);          // alloc trackée (fichier/ligne capturés)
NK_MEM_FREE(buf);

Joueur* j = NK_MEM_NEW(Joueur, 100);     // objet tracké
NK_MEM_DELETE(j);

Joueur* tab = NK_MEM_NEW_ARRAY(Joueur, 8);
NK_MEM_DELETE_ARRAY(tab);
```

Statistiques et rapport de fuites :

```cpp
NkMemoryStats s = NK_MEMORY_SYSTEM.GetStats();
// s.liveBytes, s.peakBytes, s.liveAllocations, s.totalAllocations

NK_MEMORY_SYSTEM.DumpLeaks();            // liste les allocations non libérées
```

Cycle de vie typique d'une application instrumentée :

```cpp
NK_MEMORY_SYSTEM.Initialize();           // optionnel : appelé en lazy sinon
// ... toute l'application ...
NK_MEMORY_SYSTEM.Shutdown(true);         // true = rapporter les fuites au log
```

> En **Release**, on peut définir `NKENTSEU_DISABLE_MEMORY_TRACKING` pour que ces
> macros tombent au plus près de `malloc`/`free` (overhead ~0). Le tracking n'est
> intéressant qu'en Debug.
>
> Important : **ne mélangez pas les familles**. Ce qui est alloué avec `NK_MEM_*` se
> libère avec `NK_MEM_*` ; ce qui vient de `NkAlloc`/`New` se libère avec
> `NkFree`/`Delete`. Voir §11.

---

## 9. Mini-exemple complet

Un petit programme qui combine buffer brut, objet typé et pointeur unique :

```cpp
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKMemory/NkUtils.h"

using namespace nkentseu;
using namespace nkentseu::memory;

struct Particule {
    float x, y, vie;
    Particule() : x(0), y(0), vie(1.f) {}
};

void Demo() {
    NkAllocator& alloc = NkGetDefaultAllocator();

    // 1) Buffer brut, mis à zéro.
    float* poids = static_cast<float*>(NkAllocZero(16, sizeof(float)));
    poids[0] = 1.0f;
    NkFree(poids);

    // 2) Un objet, géré à la main.
    Particule* p = alloc.New<Particule>();
    p->vie = 0.5f;
    alloc.Delete(p);

    // 3) Le même objet, mais auto-géré (recommandé).
    NkUniquePtr<Particule> q = NkMakeUnique<Particule>();
    q->x = 42.f;
    // pas de Delete : libéré tout seul en fin de portée.

    // 4) Pool pour beaucoup de particules identiques.
    NkPoolAllocator pool(sizeof(Particule), 1000);
    NkUniquePtr<Particule> r = NkMakeUniqueWithAllocator<Particule>(pool);
    r->y = 7.f;
}   // q et r détruits ici, chacun dans le bon allocateur
```

---

## 10. Récapitulatif des paires allocation / libération

| J'ai alloué avec… | Je libère avec… |
|-------------------|-----------------|
| `NkAlloc` / `NkAllocZero` / `NkRealloc` | `NkFree` |
| `alloc.New<T>(...)` | `alloc.Delete(ptr)` |
| `alloc.NewArray<T>(n, ...)` | `alloc.DeleteArray(ptr)` |
| `NkMakeUnique<T>(...)` / `NkMakeUniqueArray<T>(n)` | automatique (fin de portée) |
| `NK_MEM_ALLOC` | `NK_MEM_FREE` |
| `NK_MEM_NEW` | `NK_MEM_DELETE` |
| `NK_MEM_NEW_ARRAY` | `NK_MEM_DELETE_ARRAY` |

---

## 11. Pièges courants

1. **Mélanger les mondes = corruption de tas (`0xc0000374` sous Windows).**
   Le piège n°1. N'utilisez jamais `delete`/`delete[]`/`free` sur quelque chose alloué
   par NKMemory, et inversement. Restez dans la **même famille** d'un bout à l'autre
   (voir le tableau §10). C'est la cause de presque tous les crashs mémoire du moteur.

2. **Toute classe avec un `Create` doit avoir un `Destroy`.**
   Règle de conception du moteur : si un objet (fenêtre, renderer, contexte GPU…)
   s'initialise via une méthode `Create(...)`, il **doit** être libéré via le `Destroy`
   correspondant — pas avec un simple `delete`. Sinon : ressources fuites ou corruption.

3. **Libérer dans le mauvais allocateur.**
   Un objet pris dans un `NkPoolAllocator` doit retourner dans **ce** pool. Avec un
   `NkUniquePtr`, configurez le deleter (`NkMakeUniqueWithAllocator` ou
   `NkDefaultDelete<T>(&pool)`), sinon il tentera de libérer dans l'allocateur par
   défaut.

4. **Oublier le `Reset()` d'un allocateur linéaire/arène.**
   Ces allocateurs ne libèrent pas bloc par bloc : sans `Reset()` (ou `FreeToMarker`),
   ils se remplissent et finissent par renvoyer `nullptr`. C'est voulu — pensez à
   réinitialiser au bon moment (typiquement en fin de frame).

5. **Ne pas vérifier le `nullptr`.**
   `New<T>`, `NkAlloc`, etc. renvoient `nullptr` en cas d'échec. Testez avant d'écrire
   dedans (`if (j) { ... }` ou `q.IsValid()`).

6. **Ne pas coller de code accidentel dans les headers.**
   Anecdote vécue dans le projet : du texte collé par erreur dans un `.h` casse la
   compilation de façon obscure. Si des erreurs de syntaxe étranges apparaissent après
   une session de copier-coller, vérifiez la fin des fichiers touchés.

---

## 12. Dépendances Jenga

Le module s'appelle **`NKMemory`**. Pour l'utiliser dans votre projet, ajoutez-le à
votre liste de dépendances Jenga (Jenga propage transitivement les includes des couches
en dessous) :

```python
# MonJeu.jenga (extrait)
with project("MonJeu"):
    kind("app")
    files(["src/**.cpp"])
    nkentseudependson(
        ["NKMemory", "NKCore", "NKPlatform"],   # NKMemory + ses couches Foundation
        extra_includes=["src"],
    )
```

`NKMemory` repose lui-même sur **`NKCore`** (types `nk_size`, `nk_char`,
`NkSpinLock`, traits…) et **`NKPlatform`** (macros d'export, alignement). En pratique
vous aurez déjà ces deux modules dès que vous utilisez le moteur.

Includes principaux côté code :

```cpp
#include "NKMemory/NkAllocator.h"   // NkAllocator, NkAlloc/NkFree, allocateurs, New/Delete
#include "NKMemory/NkUniquePtr.h"   // NkUniquePtr, NkMakeUnique, NkDefaultDelete
#include "NKMemory/NkUtils.h"       // NkMemSet/NkMemCopy/NkMemZero, alignement
#include "NKMemory/NKMemory.h"      // (avancé) NkMemorySystem + macros NK_MEM_*
```

---

> **Prochaine étape :** maintenant que la mémoire est sous contrôle, ouvrez une
> fenêtre avec le [guide NKWindow](02-NKWindow.md), puis affichez quelque chose avec
> [NKCanvas](05-NKCanvas.md). [← Retour au sommaire](README.md)
