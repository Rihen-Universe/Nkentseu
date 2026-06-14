# Les vues

> Couche **Foundation** · NKContainers · Regarder une séquence **sans la posséder** : la vue
> légère et non-propriétaire `NkSpan`, équivalent maison de `std::span`.

Il arrive constamment qu'une fonction doive **travailler sur une suite d'éléments contigus sans se
soucier d'où ils viennent**. Trier des sommets, sommer des échantillons audio, hacher un bloc
d'octets, parcourir des composants ECS : le code utile ne regarde jamais que **deux choses** — *où
commence* la séquence et *combien* d'éléments elle contient. Et pourtant, sans outil dédié, on finit
par dupliquer la même fonction pour un `NkVector`, puis pour un tableau C, puis pour un pointeur+taille
reçu d'une API externe. La vue résout exactement ce problème : elle **emballe ce couple
(pointeur, taille)** dans un petit objet uniforme, que toutes ces sources savent produire.

Une vue n'est **pas** un conteneur. Elle ne possède rien, n'alloue rien, ne libère rien — elle
*pointe* vers une mémoire qui appartient à quelqu'un d'autre. La copier ne copie que les
**métadonnées** (un pointeur et une taille), jamais les données. C'est sa force (zéro coût, zéro
allocation) et son seul danger (si la source meurt avant la vue, la vue pend dans le vide). Cette
page vous apprend à vous en servir et à éviter ce piège.

`NkSpan<T>` est **templaté** sur le type d'élément. `T` peut être `const` — `NkSpan<const float>`
est une vue **lecture seule**, idéale comme paramètre de fonction qui ne doit rien modifier. L'objet
fait la taille d'un pointeur plus celle d'un `usize`, et **toutes** ses opérations sont en `O(1)`.

- **Namespace** : `nkentseu` (le type est déclaré directement dans `nkentseu`, **pas** dans
  `nkentseu::containers`)
- **Header** : `#include "NKContainers/Views/NkSpan.h"` (le parapluie `NKContainers.h`
  n'inclut **pas** `NkSpan` — il faut inclure ce header directement)

---

## Passer une séquence sans en imposer le type

Le premier réflexe à acquérir : dès qu'une fonction n'a besoin que de *lire ou modifier sur place*
une suite contiguë, **prenez un `NkSpan`** en paramètre, jamais un `NkVector` concret. Le
constructeur depuis conteneur étant **implicite**, l'appelant passe ce qu'il a sous la main — un
`NkVector`, un tableau C, ou un span construit à la volée — sans aucune conversion explicite.

```cpp
void ProcessData(NkSpan<float> data) {
    for (float& v : data) v *= 2.f;        // on parcourt comme un tableau
}

NkVector<float> samples = LoadSamples();
ProcessData(samples);                       // NkVector → NkSpan : implicite
float raw[100];
ProcessData(raw);                           // tableau C → NkSpan : taille déduite
ProcessData(NkSpan<float>(buffer, count));  // pointeur + taille brut
```

La même fonction sert désormais **toutes** les sources contiguës du moteur. Ce n'est **pas** un
template à recompiler pour chaque conteneur, et ce n'est **pas** une copie : `data` partage la
mémoire de `samples`, modifier `data[i]` modifie `samples[i]`.

Quand la fonction ne doit **rien modifier**, déclarez le paramètre `NkSpan<const T>`. Un
`NkVector<int>` se convertit implicitement vers `NkSpan<const int>`, et `data[i]` renvoie alors un
`const int&` : le compilateur garantit la lecture seule.

> **En résumé.** Prenez `NkSpan<T>` (ou `NkSpan<const T>` en lecture seule) en paramètre plutôt
> qu'un conteneur concret. La conversion depuis `NkVector`, tableau C ou pointeur+taille est
> **implicite** et **sans copie** — une seule fonction couvre toutes les sources contiguës.

---

## Découper sans copier

L'autre usage majeur est le **sous-découpage**. À partir d'une vue, `First(n)`, `Last(n)` et
`Subspan(offset, count)` produisent une nouvelle vue **qui partage la même mémoire** — aucune
allocation, aucune recopie. C'est l'outil pour isoler un en-tête, un corps, un bloc, tout en
restant connecté aux données d'origine.

```cpp
NkSpan<nk_uint8> packet = ReceiveBytes();
NkSpan<nk_uint8> header  = packet.First(4);        // les 4 premiers octets
NkSpan<nk_uint8> footer  = packet.Last(2);         // les 2 derniers
NkSpan<nk_uint8> payload = packet.Subspan(4, packet.Size() - 6);
```

Comme les trois sous-vues pointent dans le **même tampon**, écrire `header[0]` écrit aussi
`packet[0]`. Ce n'est **pas** un découpage défensif qui isolerait une copie : c'est une fenêtre sur
les données réelles. Le traitement par blocs en profite directement — on avance d'un pas régulier et
on regarde chaque tranche sans jamais allouer :

```cpp
for (usize i = 0; i < data.Size(); i += 16) {
    usize chunkSize = data.Size() - i < 16 ? data.Size() - i : 16;
    Process(data.Subspan(i, chunkSize));            // une fenêtre, zéro alloc
}
```

> **En résumé.** `First`, `Last` et `Subspan` taillent des **sous-vues partageant la mémoire
> source** en `O(1)` — pour isoler un en-tête/corps/bloc ou traiter par tranches sans la moindre
> allocation. Modifier une sous-vue modifie la source.

---

## Le danger : une vue ne possède pas ses données

Toute la légèreté de `NkSpan` repose sur le fait qu'elle **ne possède rien**. C'est aussi son unique
piège sérieux : si la source disparaît pendant que la vue survit, la vue pointe vers de la mémoire
morte (*dangling*) et tout accès est un comportement indéfini.

```cpp
NkSpan<int> Mauvais() {
    NkVector<int> local = { 1, 2, 3 };
    return local;                 // DANGER : 'local' meurt à la sortie,
}                                 // la vue retournée pend dans le vide
```

La règle est simple et sans exception : **ne jamais retourner ni stocker un `NkSpan` au-delà de la
durée de vie de sa source.** Une vue est faite pour traverser un appel, pas pour survivre à ce
qu'elle regarde. Deux corollaires utiles :

- Pour partager une vue entre threads, copiez la **vue** (`NkSpan<T> local = shared;` — ce sont des
  métadonnées triviales), jamais les données ; `NkSpan` n'est **pas** thread-safe et n'offre aucune
  synchronisation.
- Les bornes sont vérifiées par `NKENTSEU_ASSERT`, donc **uniquement en debug**. En release, un
  accès hors-borne est un UB silencieux — testez `Empty()` avant `Front()`/`Back()`.

> **En résumé.** `NkSpan` ne possède pas ses données : ne la faites jamais survivre à sa source
> (dangling = UB). Les bornes sont vérifiées **en debug seulement**. Aucune allocation n'est en jeu,
> donc aucun risque de heap corruption — mais aucune libération automatique non plus.

---

## Aperçu de l'API

La liste de **tous** les éléments publics de `NkSpan<T>`, en un coup d'œil. Toutes les opérations
sont `O(1)` (l'objet ne stocke qu'un pointeur et une taille). Chacun est détaillé dans la
« Référence complète » qui suit.

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias | `ElementType`, `ValueType`, `SizeType` | Type d'élément (avec/sans cv), type des tailles (`usize`). |
| Alias | `Reference`, `ConstReference`, `Pointer`, `ConstPointer` | Réf./pointeur mutables et constants. |
| Alias | `Iterator`, `ConstIterator` | Itérateur = pointeur brut (`T*` / `const T*`). |
| Construction | `NkSpan()` | Vue vide (`nullptr`, taille 0). |
| Construction | `NkSpan(data, size)` | Depuis pointeur + taille (UB si `nullptr` & `size > 0`). |
| Construction | `NkSpan(arr[N])` | Depuis tableau C, taille `N` déduite. |
| Construction | `NkSpan(container)` | Depuis tout conteneur à `Data()`/`Size()` — **implicite**. |
| Construction | copie, `operator=` | Copie superficielle des métadonnées. |
| Accès | `operator[](i)` `[O(1)]` | Accès indexé (assert debug). |
| Accès | `Front()`, `Back()` `[O(1)]` | Premier / dernier élément (assert si vide). |
| Accès | `Data()` | Pointeur brut sous-jacent (interop C/GPU). |
| Itération | `begin()`, `end()` | Itérateurs (minuscules) pour le `range-based for`. |
| Capacité | `Size()` | Nombre d'éléments. |
| Capacité | `SizeBytes()` | Taille totale en octets (`Size * sizeof(T)`). |
| Capacité | `Empty()` | Vue vide ? |
| Sous-vues | `First(count)` `[O(1)]` | Les `count` premiers éléments. |
| Sous-vues | `Last(count)` `[O(1)]` | Les `count` derniers éléments. |
| Sous-vues | `Subspan(offset, count)` `[O(1)]` | Fenêtre arbitraire (partage la mémoire). |
| CTAD | guides de déduction | `NkSpan(arr)` / `NkSpan(container)` sans paramètre template. |
| Libre | `NkMakeSpan(data, size)` | Fabrique depuis pointeur + taille. |
| Libre | `NkMakeSpan(arr[N])` | Fabrique depuis tableau C. |
| Libre | `NkMakeSpan(container)` | Fabrique depuis conteneur. |

---

## Référence complète

Chaque élément est repris ici en détail. Les éléments triviaux (alias, construction) sont décrits
brièvement ; les opérations qui structurent l'usage réel (paramètre de fonction, sous-vues, interop)
le sont **à fond**, avec leurs emplois dans les différents domaines du temps réel — rendu, ECS,
physique, animation, gameplay/IA, audio, UI/2D, IO, GPU.

### La structure : pourquoi tout est `O(1)`

`NkSpan` ne contient que deux champs : `mData` (un `T*` vers le premier élément, possiblement
`nullptr`) et `mSize` (un `usize`). Il n'y a **pas** de tampon interne, pas de nœuds, pas de
capacité réservée — d'où le fait que **toute** opération soit en temps constant et que l'objet ne
fasse jamais d'allocation. C'est une *poignée* sur de la mémoire détenue ailleurs. Copier un span,
c'est copier ces deux valeurs ; c'est pourquoi le passer par valeur est gratuit, et pourquoi le
partager entre threads se fait en copiant la poignée, pas les données.

### Alias publics

`NkSpan` expose les types imbriqués usuels pour le code générique. `ElementType` est `T` (il
conserve les qualifiers `const`/`volatile`), tandis que `ValueType` est le type « nu »
(`traits::NkRemoveCV<T>`). `SizeType` est `usize` — le type de toutes les tailles et indices.
`Reference`/`ConstReference` et `Pointer`/`ConstPointer` sont les réf./pointeurs mutables et
constants. Enfin, `Iterator` et `ConstIterator` sont **de simples pointeurs bruts** (`T*` et
`const T*`) : l'itération sur un span n'est donc rien d'autre que de l'arithmétique de pointeurs, ce
qui la rend triviale à optimiser pour le compilateur et compatible avec les algorithmes qui attendent
des itérateurs contigus.

### Construction

Un span se construit de quatre façons, toutes en `O(1)` :

- **`NkSpan()`** — vue vide (`mData = nullptr`, `mSize = 0`). Sert d'état par défaut (paramètre
  optionnel, membre non encore initialisé, valeur de retour « rien à voir »).
- **`NkSpan(data, size)`** — depuis un pointeur brut et un nombre d'éléments. Aucune vérification
  n'est faite : `NkSpan(nullptr, n)` avec `n > 0` est un **comportement indéfini**. C'est la forme
  qu'on emploie pour emballer un buffer reçu d'une API C (`malloc`, lecture fichier, retour driver).
- **`NkSpan(arr[N])`** — depuis un tableau C statique, la taille `N` étant **déduite** à la
  compilation : on ne peut pas se tromper de taille.
- **`NkSpan(container)`** — depuis tout conteneur exposant `Data()` et `Size()` (typiquement
  `NkVector`). Ce constructeur est **non `explicit`** : c'est lui qui permet de passer directement un
  `NkVector` à une fonction prenant un `NkSpan`, sans rien écrire.

La copie et `operator=` ne copient que les **métadonnées** (pointeur + taille) — une vue copiée
regarde exactement la même mémoire que l'originale.

### Accès aux éléments

`operator[](i)` donne l'accès indexé en `O(1)`, vérifié par `NKENTSEU_ASSERT(index < mSize)` en
debug. Détail à connaître : la méthode est `const` mais renvoie une **référence modifiable**
(`T&`) — c'est cohérent avec la nature non-possessive de la vue (modifier l'élément modifie la
source, pas le span). `Front()` et `Back()` renvoient le premier (`mData[0]`) et le dernier
(`mData[mSize-1]`) élément, chacun protégé par un assert exigeant `mSize > 0` : **testez `Empty()`
d'abord** sur une vue dont vous ne connaissez pas la taille. `Data()` rend le **pointeur brut**
sous-jacent (`nullptr` si vide) — c'est la porte vers l'interop. Quelques emplois par domaine :

- **GPU / rendu** — passer un tampon de sommets ou d'indices au driver, qui attend `(pointeur,
  nombre)` : `vbo.Upload(verts.Data(), verts.Size())`.
- **IO** — copier un bloc brut avec `memcpy(dest, bytes.Data(), bytes.SizeBytes())`, écrire un buffer
  dans un flux.
- **Interop C / legacy** — appeler une fonction héritée `LegacyProcess(span.Data(), span.Size())`
  sans réécrire son interface.
- **Audio** — `Front()`/`Back()` pour lire le premier/dernier échantillon d'une fenêtre, `[i]` pour
  parcourir un buffer PCM.

### Itération — `begin()` / `end()`

Seules méthodes en **minuscules** de toute la classe (par cohérence avec le `range-based for` et les
algorithmes STL), `begin()` renvoie `mData` et `end()` renvoie `mData + mSize` (sentinelle à ne
**jamais** déréférencer). Comme l'itérateur est un pointeur brut, on parcourt un span comme un
tableau, et il s'intègre directement aux algorithmes génériques. Par domaine :

- **Tous domaines** — `for (int& val : span)` modifie en place ; `for (int v : span)` lit.
- **Rendu / ECS** — parcourir une plage de composants ou de sommets sans connaître le conteneur
  d'origine, dans un système qui ne reçoit qu'une vue.
- **Gameplay / IA** — itérer la liste des voisins visibles d'un agent, des cibles d'un raycast,
  passée sous forme de span.
- **Algorithmes** — `span.begin()`/`span.end()` se branchent sur les algorithmes qui acceptent des
  itérateurs contigus (recherche, tri en place).

### Capacité — `Size`, `SizeBytes`, `Empty`

`Size()` rend le nombre d'éléments (valeur pré-calculée, `O(1)`). `SizeBytes()` rend la taille
**totale en octets** (`mSize * sizeof(T)`) — exactement le nombre attendu par `memcpy`, une
allocation, ou un upload GPU mesuré en octets. `Empty()` est `true` quand `mSize == 0` ; c'est le
**garde-fou** à placer avant `Front()`/`Back()` sur une vue de taille inconnue, pour éviter
l'assertion (debug) ou l'UB (release).

### Sous-vues — `First`, `Last`, `Subspan`

Le cœur de l'utilité de `NkSpan` : tailler des fenêtres **sans copie**, toutes en `O(1)`, toutes
partageant la mémoire source.

- **`First(count)`** — les `count` premiers éléments. Assert `count <= mSize`.
- **`Last(count)`** — les `count` derniers : `NkSpan(mData + (mSize - count), count)`. Assert
  `count <= mSize`.
- **`Subspan(offset, count)`** — une fenêtre arbitraire à partir de `offset` :
  `NkSpan(mData + offset, count)`. Deux asserts (`offset <= mSize` et `count <= mSize - offset`).

(À noter : ces trois méthodes ne sont **pas** marquées `NKENTSEU_NOEXCEPT`, contrairement au reste de
l'API.) Comme elles renvoient une vue sur le **même** tampon, modifier une sous-vue modifie la
source. Emplois par domaine :

- **IO / réseau** — séparer en-tête, corps et pied d'un paquet (`First(4)` / `Subspan(4, n)` /
  `Last(2)`) sans découper la mémoire.
- **Audio** — extraire une fenêtre glissante de `N` échantillons (`Subspan(i, N)`) pour une FFT ou
  un filtre, sans allouer à chaque frame.
- **Rendu / GPU** — découper un gros tampon en sous-plages soumises en plusieurs draw calls, ou
  traiter un mesh par lots (`Subspan(i, 16)`).
- **Physique / IA** — traiter une grande liste d'entités par tranches (par exemple pour étaler le
  coût sur plusieurs frames), chaque tranche étant un `Subspan` sans copie.
- **Algorithmes sans allocation** — un `FilterInPlace` compacte les éléments retenus au début puis
  renvoie `data.First(writePos)` : la vue résultat décrit exactement la partie valide, zéro alloc.

### Déduction de template (CTAD) et fabriques `NkMakeSpan`

Deux **guides de déduction** (C++17) évitent d'écrire le paramètre template : `NkSpan(arr)` depuis un
tableau C déduit `NkSpan<T>`, et `NkSpan(container)` depuis un conteneur exposant `ValueType`
déduit `NkSpan<typename Container::ValueType>`. En miroir, les trois fabriques libres `NkMakeSpan`
(toutes `NKENTSEU_CONSTEXPR` + `NKENTSEU_NOEXCEPT`) construisent un span depuis un pointeur+taille, un
tableau C, ou un conteneur — utiles dans un contexte où la déduction de constructeur ne s'applique
pas, ou simplement pour un style plus explicite : `auto s = NkMakeSpan(verts);`.

---

### Exemple

```cpp
#include "NKContainers/Views/NkSpan.h"
using namespace nkentseu;

// Une fonction générique qui accepte n'importe quelle source contiguë.
float Somme(NkSpan<const float> data) {
    float total = 0.f;
    for (float v : data) total += v;          // range-based for (begin/end)
    return total;
}

NkVector<float> samples = LoadSamples();
float s = Somme(samples);                      // NkVector → NkSpan : implicite

// Découpage zéro-copie : en-tête, corps, pied — tous partagent le tampon.
NkSpan<nk_uint8> packet = ReceiveBytes();
NkSpan<nk_uint8> header  = packet.First(4);
NkSpan<nk_uint8> payload = packet.Subspan(4, packet.Size() - 6);
NkSpan<nk_uint8> footer  = packet.Last(2);

// Interop C / GPU : on passe (pointeur, taille) ou (pointeur, octets).
LegacyProcess(payload.Data(), payload.Size());
memcpy(dst, payload.Data(), payload.SizeBytes());

// DANGER à ne jamais commettre : retourner une vue sur une source locale.
//   NkSpan<int> f() { NkVector<int> v = {1,2,3}; return v; } // dangling → UB
```

---

[← Index NKContainers](README.md) · [Récap NKContainers](../NKContainers.md) · [Utilities →](Utilities.md)
