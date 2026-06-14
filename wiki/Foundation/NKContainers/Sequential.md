# Les conteneurs séquentiels

> Couche **Foundation** · NKContainers · Ranger des éléments dans un **ordre** : le tableau
> dynamique `NkVector`, les listes chaînées `NkList` / `NkDoubleList`, et la file à deux bouts
> `NkDeque`.

Dès qu'on a **plusieurs choses du même type à garder dans un ordre** — les sommets d'un maillage,
les entités d'une scène, les messages d'une file, les caractères d'un texte — il faut un *conteneur
séquentiel*. La question n'est jamais « lequel marche » (ils marchent tous), mais « lequel colle à
la façon dont je vais **ajouter, retirer et parcourir** ». Tout le compromis tient en une phrase :
**la contiguïté en mémoire rend le parcours rapide mais l'insertion au milieu coûteuse ; le
chaînage rend l'insertion gratuite mais le parcours lent.** Cette page vous apprend à choisir.

Les quatre types sont **templatés** sur le type d'élément et conscients de l'**allocateur**
(`NkVector<T, Allocator = memory::NkAllocator>`) : toute la mémoire passe par NKMemory, jamais par
`new`/`delete`. Chaque conteneur expose son API en **deux casses** : le style maison `PascalCase`
(`PushBack`, `Begin`) **et** le style STL minuscule (`push_back`, `begin`) pour le `range-based for`
et l'interopérabilité avec les algorithmes.

- **Namespace** : `nkentseu::containers`
- **Header parapluie** : `#include "NKContainers/NKContainers.h"`

---

## Le tableau dynamique : `NkVector`

C'est le conteneur **par défaut**, celui qu'on prend quand on ne sait pas encore. Il range ses
éléments dans **un seul bloc de mémoire contigu**, exactement comme un tableau C — mais il
**grandit tout seul**. On y accède par indice en temps constant (`v[i]`), on ajoute à la fin en
temps **amorti** constant, et le CPU l'adore : comme les éléments se suivent en mémoire, le
parcours profite à plein du cache et de la prélecture matérielle.

Le secret est la distinction entre **taille** (`Size()`, le nombre d'éléments réels) et
**capacité** (`Capacity()`, la place réservée). Quand on dépasse la capacité, le vecteur en
réserve une plus grande (typiquement le double), recopie les éléments, et libère l'ancienne.
Ce *réagencement* est l'unique opération coûteuse — et on l'évite en appelant `Reserve(n)` quand on
connaît la taille à l'avance.

```cpp
NkVector<NkVertex> mesh;
mesh.Reserve(1024);            // une seule allocation, pas de réagencement ensuite
for (const auto& tri : triangles) {
    mesh.PushBack(tri.a);
    mesh.PushBack(tri.b);
    mesh.PushBack(tri.c);
}
renderer.Upload(mesh.Data(), mesh.Size());   // Data() = pointeur brut, prêt pour le GPU
```

Ce n'est **pas** une liste chaînée : insérer ou supprimer **au milieu** force à décaler tout ce qui
suit (`O(n)`). Si votre charge de travail est faite d'insertions au milieu, ce n'est pas le bon
outil — lisez la suite.

> **En résumé.** `NkVector` = mémoire contiguë, accès indexé `O(1)`, ajout en fin amorti `O(1)`,
> parcours ami du cache. Le choix par défaut pour **stocker et itérer**. `Reserve` si la taille est
> connue ; évitez les insertions au milieu.

---

## Les listes chaînées : `NkList` et `NkDoubleList`

Quand le motif d'usage est fait d'**insertions et de suppressions un peu partout**, la contiguïté
devient un fardeau. Une **liste chaînée** range chaque élément dans son propre **nœud**, relié au
suivant par un pointeur. Insérer ou retirer ne déplace plus rien : on rebranche deux pointeurs,
en temps constant. Le prix à payer : plus d'accès indexé (`liste[i]` n'existe pas — il faudrait
marcher jusqu'au i-ème), un nœud = une petite allocation, et un parcours qui saute en mémoire
(donc moins ami du cache que le vecteur).

`NkList` est **simplement chaînée** : chaque nœud ne connaît que le suivant. C'est la plus légère
(un seul pointeur par nœud) et elle excelle quand on travaille **par la tête** — `PushFront`,
`PopFront` en `O(1)`. Mais on ne peut pas reculer, et il n'y a pas de `PopBack` efficace.

`NkDoubleList` est **doublement chaînée** : chaque nœud connaît le suivant **et** le précédent.
Elle coûte un pointeur de plus par nœud, mais offre `PushFront` / `PushBack` / `PopFront` /
`PopBack` **tous en `O(1)`**, le parcours dans les deux sens, et la suppression en `O(1)` quand on
tient déjà un itérateur sur l'élément. C'est la liste « complète ».

```cpp
NkDoubleList<Task> queue;
queue.PushBack(taskA);            // O(1)
queue.PushFront(urgentTask);      // O(1) — passe devant
Task next = queue.Front();
queue.PopFront();                 // O(1)
```

> **En résumé.** Les listes échangent l'accès indexé et l'amitié du cache contre des
> insertions/suppressions `O(1)` n'importe où. `NkList` (simple) = la plus légère, travail par la
> tête. `NkDoubleList` (double) = les deux bouts + parcours bidirectionnel + suppression `O(1)` sur
> itérateur. Préférez-les au vecteur **seulement** si vous insérez/retirez vraiment au milieu.

---

## La file à deux bouts : `NkDeque`

`NkDeque` (*double-ended queue*) répond à un besoin précis : ajouter et retirer **efficacement aux
deux extrémités**, *tout en gardant* un accès indexé rapide. Le vecteur est `O(1)` en fin mais
`O(n)` en tête (il faut tout décaler) ; la deque est `O(1)` **aux deux bouts**. Elle y parvient en
rangeant ses éléments par **segments** (des petits blocs contigus reliés entre eux) plutôt qu'en un
seul bloc : on peut donc pousser devant sans recopier tout le reste.

C'est l'outil des **files** et des **tampons glissants** : une file d'événements où l'on enfile à
un bout et défile à l'autre, un historique borné, une fenêtre de frames récentes. On garde `[i]` et
`Size()`, mais la mémoire n'est **pas** un unique bloc contigu — donc, contrairement au vecteur, on
ne peut pas passer `Data()` à une API qui attend un tableau C.

> **En résumé.** `NkDeque` = `O(1)` aux **deux** extrémités **et** accès indexé, au prix d'une
> mémoire segmentée (pas de pointeur contigu unique). L'outil des files et des tampons à deux bouts.

---

## Aperçu de l'API

Tous exposent en plus les variantes STL minuscules (`size`, `push_back`, `begin`, `empty`…) pour le
`range-based for` et les algorithmes. Complexités entre crochets.

### `NkVector<T, Allocator>` — tableau dynamique

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkVector()`, `NkVector(n)`, `NkVector(n, val)`, `NkVector(initList)`, copie, déplacement | Vide / taille / taille+remplissage / liste / copie profonde / transfert `O(1)` |
| Accès | `operator[]` `[O(1)]`, `At` `[O(1)]` (vérifié), `Front`, `Back`, `Data` | Indexé non vérifié / vérifié / extrémités / pointeur brut contigu |
| Capacité | `Size`, `Capacity`, `IsEmpty`/`Empty`, `Reserve`, `ShrinkToFit` | Compte / place réservée / vide ? / pré-réserver / rendre la mémoire |
| Modification | `PushBack`, `EmplaceBack`, `PopBack`, `Insert`, `Erase`, `Assign`, `Resize`, `Clear`, `Reverse`, `Swap` | Ajout/retrait en fin `[O(1)*]`, insertion/suppression `[O(n)]`, (ré)affectation, redimensionnement, vidage, inversion, échange `O(1)` |
| Itération | `Begin`/`End`, `CBegin`/`CEnd`, `RBegin`/`REnd` | Itérateurs mutables / constants / inverses |
| Libre | `NkSwap(a, b)` | Échange `O(1)` de deux vecteurs |

### `NkList<T>` — liste simplement chaînée

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Modification | `PushFront` `[O(1)]`, `PushBack`, `PopFront` `[O(1)]`, `Reverse`, `Clear` | Travail par la **tête** ; inversion ; vidage |
| Accès | `Front`, `IsEmpty`, `Size` | Premier élément ; vide ? ; compte |
| Itération | `Begin`/`End` (avant uniquement) | Parcours **unidirectionnel** |

### `NkDoubleList<T>` — liste doublement chaînée

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Modification | `PushFront`/`PushBack` `[O(1)]`, `PopFront`/`PopBack` `[O(1)]`, `Insert`/`Erase` (sur itérateur) `[O(1)]`, `Reverse`, `Clear` | Les **deux bouts** ; insertion/suppression `O(1)` où l'on tient un itérateur |
| Accès | `Front`, `Back`, `IsEmpty`, `Size` | Extrémités ; vide ? ; compte |
| Itération | `Begin`/`End`, `RBegin`/`REnd` | Parcours **bidirectionnel** |

### `NkDeque<T>` — file à deux bouts

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Modification | `PushBack`/`PushFront` `[O(1)]`, `PopBack`/`PopFront` `[O(1)]`, `Clear` | Enfiler/défiler aux **deux** extrémités |
| Accès | `operator[]` `[O(1)]`, `Front`, `Back`, `Size`, `IsEmpty` | Indexé + extrémités (mémoire **segmentée**, pas de `Data()` contigu) |
| Itération | `Begin`/`End` | Parcours |

---

## Référence complète

### Choisir : le tableau de décision

Le seul vrai critère est **comment vous modifiez la séquence** :

- **Vous ajoutez surtout à la fin et vous parcourez beaucoup** → `NkVector`. C'est 90 % des cas.
- **Vous insérez/retirez souvent au milieu, en tenant un itérateur** → `NkDoubleList`.
- **Vous travaillez uniquement par la tête, et la mémoire est précieuse** → `NkList`.
- **Vous enfilez à un bout et défilez à l'autre** → `NkDeque`.

| Opération | `NkVector` | `NkList` | `NkDoubleList` | `NkDeque` |
|-----------|-----------|----------|----------------|-----------|
| Accès `[i]` | **O(1)** | — | — | **O(1)** |
| Ajout en fin | O(1)* | O(n) | **O(1)** | **O(1)** |
| Ajout en tête | O(n) | **O(1)** | **O(1)** | **O(1)** |
| Insertion au milieu (itérateur) | O(n) | O(1) | **O(1)** | O(n) |
| Parcours / cache | **excellent** | médiocre | médiocre | bon |
| Mémoire par élément | minimale | +1 ptr | +2 ptr | minimale |

(*) amorti : `O(1)` en moyenne, `O(n)` lors d'un réagencement.

### `NkVector` à fond

**Taille contre capacité.** `Size()` est ce que vous voyez ; `Capacity()` est ce qui est réservé.
La croissance géométrique (doublement) garantit que `PushBack` est `O(1)` **amorti** : sur `n`
ajouts, le coût total des recopies reste `O(n)`. `Reserve(n)` supprime tous les réagencements
intermédiaires ; `ShrinkToFit()` rend la mémoire excédentaire.

**Accès sûr ou rapide.** `operator[]` ne vérifie rien (rapide, comportement indéfini hors bornes
en release) ; `At(i)` vérifie et déclenche `NKENTSEU_CONTAINERS_THROW_OUT_OF_RANGE` (cf.
[Utilitaires](Utilities.md) pour la politique d'erreur). `Data()` donne le **pointeur brut
contigu**, indispensable pour parler aux API C et au GPU.

Cas d'usage, par domaine :
- **Rendu** — tampons de sommets/indices construits chaque frame puis envoyés au GPU via `Data()` ;
  contiguïté + `Reserve` = zéro réagencement dans la boucle chaude.
- **ECS / scène** — tableaux de composants d'un même type, parcourus en masse par les systèmes :
  la contiguïté maximise le débit (data-oriented design).
- **Gameplay / IA** — listes d'entités visibles, voisins d'un agent, résultats d'un *raycast* :
  on remplit, on trie, on parcourt.
- **Texte / IO** — accumuler des octets lus, des tokens, avant traitement (voir aussi
  [StringBuilder](Strings.md)).

### `NkList` et `NkDoubleList` à fond

Le gain des listes est l'**insertion/suppression `O(1)` sans rien déplacer**, et la **stabilité des
éléments** : ajouter ailleurs n'invalide pas les références/itérateurs existants (contrairement au
vecteur, dont un réagencement invalide tout). En revanche, chaque nœud est une allocation et le
parcours saute en mémoire.

- `NkList` (simple) — `PushFront`/`PopFront` `O(1)`, un seul pointeur par nœud. Idéale pour une
  **pile de tâches**, une *free-list* d'objets recyclés, une chaîne où l'on n'ajoute qu'en tête.
- `NkDoubleList` (double) — les deux bouts en `O(1)`, parcours avant **et** arrière, et surtout
  `Erase` en `O(1)` quand on tient déjà l'itérateur. C'est la structure des **files d'attente**
  ordonnancées, des **listes d'affichage** où l'on retire un élément au milieu sans tout décaler,
  des systèmes où l'on garde des itérateurs stables (animations en cours, sons actifs, abonnés à un
  événement).

### `NkDeque` à fond

La deque brille là où **les deux extrémités sont vivantes** : une **file d'événements** (production
à un bout, consommation à l'autre), un **historique borné** (on pousse une frame devant, on jette
la plus vieille derrière), une **fenêtre glissante** pour lisser une mesure (FPS, latence). Elle
garde l'accès indexé `O(1)`, donc on peut aussi l'indexer comme un tableau — mais sa mémoire
segmentée interdit le pointeur contigu unique : pour parler au GPU ou à une API C, recopiez d'abord
dans un `NkVector`.

### Le socle commun

- **Allocateur conscient.** Tous prennent un `Allocator` (par défaut `memory::NkAllocator`) : la
  mémoire vient de NKMemory, jamais de `new`. Voir [NKMemory](../NKMemory.md).
- **Double casse d'API.** `PushBack` *et* `push_back`, `Begin` *et* `begin` : le second active le
  `range-based for` (`for (auto& x : v)`) et les algorithmes génériques.
- **Sémantique de déplacement.** La construction/affectation par déplacement transfère le contenu
  en `O(1)` (le conteneur source devient vide) — d'où l'intérêt de `NKENTSEU_MOVE`/`NkMove`.
- **Politique d'erreur.** Les accès vérifiés (`At`) signalent les débordements de façon centralisée
  (`NkVectorError.h`), cohérente avec [Utilities](Utilities.md).

---

### Exemple

```cpp
#include "NKContainers/NKContainers.h"
using namespace nkentseu::containers;

// Vector : on construit un tampon, on le parcourt à plein débit.
NkVector<float> samples;
samples.Reserve(buffer.count);
for (auto s : buffer) samples.PushBack(s);
float sum = 0.f;
for (float s : samples) sum += s;          // range-based for (API STL)

// DoubleList : une file de sons actifs, on en retire un au milieu en O(1).
NkDoubleList<Sound> active;
active.PushBack(music);
active.PushFront(uiClick);
// ... quand un son finit, Erase sur son itérateur, sans décaler les autres.

// Deque : une fenêtre glissante des 60 derniers temps de frame.
NkDeque<float> frameTimes;
frameTimes.PushBack(dt);
if (frameTimes.Size() > 60) frameTimes.PopFront();
```

---

[← Index NKContainers](README.md) · [Récap NKContainers](../NKContainers.md) · [CacheFriendly →](CacheFriendly.md)
