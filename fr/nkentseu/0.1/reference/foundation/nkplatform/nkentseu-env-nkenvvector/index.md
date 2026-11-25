# nkentseu::env::NkEnvVector

Conteneur vectoriel dynamique générique (sans STL)

`#include "NKPlatform/NkEnv.h"`

## Hiérarchie

- Classes de base : aucune
- Classes dérivées : aucune

## Membres

### NkEnvVector

```cpp
NkEnvVector() noexcept
```

Constructeur par défaut (vecteur vide)

### NkEnvVector

```cpp
NkEnvVector(const NkEnvVector &other)
```

Constructeur de copie

### NkEnvVector

```cpp
NkEnvVector(NkEnvVector &&other) noexcept
```

Constructeur de déplacement

### ~NkEnvVector

```cpp
~NkEnvVector()
```

Destructeur (libère la mémoire et détruit les éléments)

### operator=

```cpp
NkEnvVector &operator=(const NkEnvVector &other)
```

Opérateur d'affectation par copie

### operator=

```cpp
NkEnvVector &operator=(NkEnvVector &&other) noexcept
```

Opérateur d'affectation par déplacement

### PushBack

```cpp
void PushBack(const T &value)
```

Ajouter un élément par copie à la fin

### PushBack

```cpp
void PushBack(T &&value)
```

Ajouter un élément par déplacement à la fin

### PopBack

```cpp
void PopBack() noexcept
```

Retirer le dernier élément (détruit l'élément)

### Clear

```cpp
void Clear() noexcept
```

Vider le conteneur (détruit tous les éléments)

### Reserve

```cpp
void Reserve(NkSize capacity)
```

Réserver de la capacité pour éviter les réallocations

### At

```cpp
T &At(NkSize index)
```

Accéder à un élément par indice (version mutable)

### At

```cpp
const T &At(NkSize index) const
```

Accéder à un élément par indice (version const)

### operator[]

```cpp
T &operator[](NkSize index)
```

Opérateur d'accès par indice (version mutable)

### operator[]

```cpp
const T &operator[](NkSize index) const
```

Opérateur d'accès par indice (version const)

### Size

```cpp
NkSize Size() const noexcept
```

Obtenir le nombre d'éléments stockés

### Capacity

```cpp
NkSize Capacity() const noexcept
```

Obtenir la capacité allouée

### Empty

```cpp
bool Empty() const noexcept
```

Vérifier si le conteneur est vide

### Begin

```cpp
T *Begin() noexcept
```

Obtenir un pointeur vers le premier élément (mutable)

### Begin

```cpp
const T *Begin() const noexcept
```

Obtenir un pointeur vers le premier élément (const)

### End

```cpp
T *End() noexcept
```

Obtenir un pointeur vers après le dernier élément (mutable)

### End

```cpp
const T *End() const noexcept
```

Obtenir un pointeur vers après le dernier élément (const)
