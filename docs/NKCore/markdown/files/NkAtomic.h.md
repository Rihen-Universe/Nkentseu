# 📄 NkAtomic.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Opérations atomiques multi-plateforme

**Auteur:** Rihen

**Chemin:** `src\NKCore\NkAtomic.h`

### 📦 Fichiers Inclus

- [`Platform/NkTypes.h`](./NkTypes.h.md)
- `Platform/NkCompiler.h`
- [`Platform/NkInline.h`](./NkInline.h.md)
- [`NkExport.h`](./NkExport.h.md)
- `atomic`
- `intrin.h`
- `windows.h`
- `emmintrin.h`

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`core`](../namespaces/core.md)

## 🎯 Éléments (3)

### 🏛️ Classs (3)

<a name="nkmemoryorder-nkspinlock-nkspinlock-nkscopedspinlock-nkscopedspinlock"></a>

#### 🏛️ `NkScopedSpinLock`

```cpp
class NkScopedSpinLock
```

**Scope guard pour spinlock**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkAtomic.h:513`*


---

<a name="nkmemoryorder-nkspinlock-nkspinlock"></a>

#### 🏛️ `NkSpinLock`

```cpp
template<typename T> class NkSpinLock
```

**Soustrait atomiquement et retourne la nouvelle valeur**

**Paramètres Template:**

- `typename T`

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkAtomic.h:458`*


---

<a name="nkmemoryorder-nkspinlock-nkspinlock-nkspinlock"></a>

#### 🏛️ `NkSpinLock`

```cpp
class NkSpinLock
```

**Spinlock avec backoff exponentiel**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkAtomic.h:470`*


---

