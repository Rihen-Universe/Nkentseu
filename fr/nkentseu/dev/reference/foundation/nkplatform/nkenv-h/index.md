# NkEnv.h

`#include "NKPlatform/NkEnv.h"`

## Types

- [nkentseu::env::NkEnvString](../nkentseu-env-nkenvstring/index.md)
- [nkentseu::env::NkEnvPair](../nkentseu-env-nkenvpair/index.md)
- [nkentseu::env::NkEnvVector](../nkentseu-env-nkenvvector/index.md)
- [nkentseu::env::NkEnvUMap](../nkentseu-env-nkenvumap/index.md)

## Fonctions

### NkMove

```cpp
template <typename T> constexpr T &&NkMove(T &t) noexcept
```

Déplacement générique d'une valeur (équivalent à std::move)

### operator+

```cpp
NkEnvString operator+(const NkEnvString &lhs, const NkEnvString &rhs)
```

Concaténation de deux NkEnvString

### NkGet

```cpp
NkEnvString NkGet(const NkEnvString &name, NkEnvString &result) noexcept
```

Récupérer la valeur d'une variable d'environnement

### GetEnvVar

```cpp
const char *GetEnvVar(const char *name) noexcept
```

Lit une variable d'environnement : un pointeur vers sa valeur (en UTF-8), ou nullptr si elle n'existe pas.

### SetEnvVar

```cpp
bool SetEnvVar(const char *name, const char *value) noexcept
```

Écrit une variable d'environnement du processus courant, là où GetEnvVar la lit (l'environnement du runtime C).

### UnsetEnvVar

```cpp
bool UnsetEnvVar(const char *name) noexcept
```

Retire une variable d'environnement du processus courant ; GetEnvVar rend ensuite nullptr.

### NkSet

```cpp
bool NkSet(const NkEnvString &name, const NkEnvString &value, bool overwrite = true) noexcept
```

Définir une variable d'environnement

### NkUnset

```cpp
bool NkUnset(const NkEnvString &name) noexcept
```

Supprimer une variable d'environnement

### NkExists

```cpp
bool NkExists(const NkEnvString &name) noexcept
```

Vérifier l'existence d'une variable d'environnement

### NkGetAll

```cpp
NkEnvUMap<NkEnvString, NkEnvString> NkGetAll() noexcept
```

Obtenir une copie de toutes les variables d'environnement

### NkGetPathSeparator

```cpp
char NkGetPathSeparator() noexcept
```

Obtenir le séparateur de chemin spécifique à la plateforme

### NkPrependToPath

```cpp
bool NkPrependToPath(const NkEnvString &directory, const NkEnvString &varName = NkEnvString("PATH"))
```

Ajouter un répertoire au début d'une variable de chemin

### NkAppendToPath

```cpp
bool NkAppendToPath(const NkEnvString &directory, const NkEnvString &varName = NkEnvString("PATH"))
```

Ajouter un répertoire à la fin d'une variable de chemin

## Alias

### NkSize

```cpp
using NkSize = unsigned int
```

Type entier non signé pour les tailles et indices

