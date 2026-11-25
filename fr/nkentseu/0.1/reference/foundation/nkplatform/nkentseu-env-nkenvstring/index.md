# nkentseu::env::NkEnvString

Chaîne de caractères allouée dynamiquement (sans STL)

`#include "NKPlatform/NkEnv.h"`

## Hiérarchie

- Classes de base : aucune
- Classes dérivées : aucune

## Membres

### NkEnvString

```cpp
NkEnvString() noexcept
```

Constructeur par défaut (chaîne vide)

### NkEnvString

```cpp
explicit NkEnvString(const char *str)
```

Constructeur depuis une chaîne C

### NkEnvString

```cpp
explicit NkEnvString(char ch)
```

Constructeur depuis un caractère unique

### NkEnvString

```cpp
NkEnvString(const NkEnvString &other)
```

Constructeur de copie

### NkEnvString

```cpp
NkEnvString(NkEnvString &&other) noexcept
```

Constructeur de déplacement

### ~NkEnvString

```cpp
~NkEnvString()
```

Destructeur (libère la mémoire allouée)

### operator=

```cpp
NkEnvString &operator=(const NkEnvString &other)
```

Opérateur d'affectation par copie

### operator=

```cpp
NkEnvString &operator=(NkEnvString &&other) noexcept
```

Opérateur d'affectation par déplacement

### CStr

```cpp
const char *CStr() const noexcept
```

Obtenir la chaîne C sous-jacente (null-terminée)

### Size

```cpp
NkSize Size() const noexcept
```

Obtenir la longueur de la chaîne (sans le null terminator)

### Empty

```cpp
bool Empty() const noexcept
```

Vérifier si la chaîne est vide

### Clear

```cpp
void Clear() noexcept
```

Vider la chaîne et libérer la mémoire

### operator==

```cpp
bool operator==(const NkEnvString &other) const noexcept
```

Comparaison d'égalité

### operator!=

```cpp
bool operator!=(const NkEnvString &other) const noexcept
```

Comparaison d'inégalité

### operator<

```cpp
bool operator<(const NkEnvString &other) const noexcept
```

Comparaison d'ordre lexicographique
