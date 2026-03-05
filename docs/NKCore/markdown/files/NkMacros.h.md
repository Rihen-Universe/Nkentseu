# 📄 NkMacros.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Macros utilitaires générales

**Auteur:** Rihen

**Chemin:** `src\NKCore\NkMacros.h`

### 📦 Fichiers Inclus

- `NkCompilerDetect.h`
- [`NkVersion.h`](./NkVersion.h.md)
- [`NkTypes.h`](./NkTypes.h.md)

### 🔗 Inclus Par

- [`NkConfig.h`](./NkConfig.h.md)
- [`NkVersion.h`](./NkVersion.h.md)

## 🎯 Éléments (44)

### 🔣 Macros (44)

<a name="nkentseu_block_begin"></a>

#### 🔣 `NKENTSEU_BLOCK_BEGIN`

```cpp
#define NKENTSEU_BLOCK_BEGIN do {
```

**Wrapper do-while(0) pour macros multi-statements**

> 📝 **Note:** Permet d'utiliser des macros comme des statements

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:378`*


---

<a name="nkentseu_fixme"></a>

#### 🔣 `NKENTSEU_FIXME`

```cpp
#define NKENTSEU_FIXME NKENTSEU_COMPILE_MESSAGE("FIXME: " msg)
```

**FIXME avec message**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:513`*


---

<a name="nkentseu_sizeof_member"></a>

#### 🔣 `NKENTSEU_SIZEOF_MEMBER`

```cpp
#define NKENTSEU_SIZEOF_MEMBER sizeof(((type*)0)->member)
```

**Taille d'un membre de structure**

**Retour:** Taille du membre en octets

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:449`*


---

<a name="nkentseu_sizeof_type"></a>

#### 🔣 `NKENTSEU_SIZEOF_TYPE`

```cpp
#define NKENTSEU_SIZEOF_TYPE sizeof(type)
```

**Taille d'un type**

**Retour:** Taille en octets

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:441`*


---

<a name="nkentseu_todo"></a>

#### 🔣 `NKENTSEU_TODO`

```cpp
#define NKENTSEU_TODO NKENTSEU_COMPILE_MESSAGE("TODO: " msg)
```

**TODO avec message**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:506`*


---

<a name="nkentseu_unused"></a>

#### 🔣 `NKENTSEU_UNUSED`

```cpp
#define NKENTSEU_UNUSED (void)(x)
```

**Marque une variable/paramètre comme intentionnellement inutilisé**

> 📝 **Note:** Évite les warnings compilateur

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:294`*


---

<a name="nkentseu_unused2"></a>

#### 🔣 `NKENTSEU_UNUSED2`

```cpp
#define NKENTSEU_UNUSED2 NKENTSEU_UNUSED(x); NKENTSEU_UNUSED(y)
```

**Marque deux variables comme inutilisées**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:302`*


---

<a name="nkentseu_unused3"></a>

#### 🔣 `NKENTSEU_UNUSED3`

```cpp
#define NKENTSEU_UNUSED3 NKENTSEU_UNUSED2(x, y); NKENTSEU_UNUSED(z)
```

**Marque trois variables comme inutilisées**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:310`*


---

<a name="nkentseu_unused4"></a>

#### 🔣 `NKENTSEU_UNUSED4`

```cpp
#define NKENTSEU_UNUSED4 NKENTSEU_UNUSED3(x, y, z); NKENTSEU_UNUSED(w)
```

**Marque quatre variables comme inutilisées**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:319`*


---

<a name="nkentseu_will_add_overflow"></a>

#### 🔣 `NKENTSEU_WILL_ADD_OVERFLOW`

```cpp
#define NKENTSEU_WILL_ADD_OVERFLOW \
```

**Vérifie si une addition déborde**

**Retour:** true si a + b > max, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:462`*


---

<a name="nkentseu_will_mul_overflow"></a>

#### 🔣 `NKENTSEU_WILL_MUL_OVERFLOW`

```cpp
#define NKENTSEU_WILL_MUL_OVERFLOW \
```

**Vérifie si une multiplication déborde**

**Retour:** true si a * b > max, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:473`*


---

<a name="nkabs"></a>

#### 🔣 `NkAbs`

```cpp
#define NkAbs (((x) < 0) ? -(x) : (x))
```

**Retourne la valeur absolue**

**Retour:** Valeur absolue

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:259`*


---

<a name="nkalignptr"></a>

#### 🔣 `NkAlignPtr`

```cpp
#define NkAlignPtr \
```

**Aligne un pointeur**

**Retour:** Pointeur aligné

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:210`*


---

<a name="nkarraysize"></a>

#### 🔣 `NkArraySize`

```cpp
#define NkArraySize (sizeof(arr) / sizeof((arr)[0]))
```

**Taille d'un tableau statique**

**Retour:** Nombre d'éléments dans le tableau

> ⚠️ **Attention:** Ne fonctionne QUE sur tableaux statiques, pas pointeurs

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:82`*


---

<a name="nkbit"></a>

#### 🔣 `NkBit`

```cpp
#define NkBit (1U << (x))
```

**Crée un masque avec le bit N à 1 (32-bit par défaut)**

**Retour:** Masque avec le bit x à 1

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:110`*


---

<a name="nkbit64"></a>

#### 🔣 `NkBit64`

```cpp
#define NkBit64 (1ULL << (x))
```

**Crée un masque 64-bit**

**Retour:** Masque 64-bit avec le bit x à 1

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:119`*


---

<a name="nkbitclear"></a>

#### 🔣 `NkBitClear`

```cpp
#define NkBitClear ((value) &= ~NkBit(bit))
```

**Met un bit à 0**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:144`*


---

<a name="nkbitset"></a>

#### 🔣 `NkBitSet`

```cpp
#define NkBitSet ((value) |= NkBit(bit))
```

**Met un bit à 1**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:136`*


---

<a name="nkbittest"></a>

#### 🔣 `NkBitTest`

```cpp
#define NkBitTest (((value) & NkBit(bit)) != 0)
```

**Teste si un bit est à 1**

**Retour:** true si le bit est à 1, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:127`*


---

<a name="nkbittoggle"></a>

#### 🔣 `NkBitToggle`

```cpp
#define NkBitToggle ((value) ^= NkBit(bit))
```

**Inverse un bit**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:152`*


---

<a name="nkclamp"></a>

#### 🔣 `NkClamp`

```cpp
#define NkClamp \
```

**Limite une valeur entre min et max**

**Retour:** Valeur clampée dans [min, max]

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:248`*


---

<a name="nkconcat"></a>

#### 🔣 `NkConcat`

```cpp
#define NkConcat NkConcatHelper(a, b)
```

**Concatène deux tokens**

**Retour:** Token concaténé

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:41`*


---

<a name="nkconcat3"></a>

#### 🔣 `NkConcat3`

```cpp
#define NkConcat3 NkConcat(NkConcat(a, b), c)
```

**Concatène trois tokens**

**Retour:** Token concaténé

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:52`*


---

<a name="nkconcat4"></a>

#### 🔣 `NkConcat4`

```cpp
#define NkConcat4 NkConcat(NkConcat3(a, b, c), d)
```

**Concatène quatre tokens**

**Retour:** Token concaténé

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:62`*


---

<a name="nkcontainerof"></a>

#### 🔣 `NkContainerOf`

```cpp
#define NkContainerOf \
```

**Récupère le pointeur du conteneur depuis un pointeur membre**

**Retour:** Pointeur vers le conteneur

> 📝 **Note:** Technique utilisée dans le kernel Linux

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:346`*


---

<a name="nkdegreestoradians"></a>

#### 🔣 `NkDegreesToRadians`

```cpp
#define NkDegreesToRadians ((degrees) * 3.14159265358979323846 / 180.0)
```

**Convertit degrés en radians**

**Retour:** Valeur en radians

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:614`*


---

<a name="nkforeach"></a>

#### 🔣 `NkForEach`

```cpp
#define NkForEach \
```

**Applique une fonction à chaque argument variadique**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:574`*


---

<a name="nkforeachhelper"></a>

#### 🔣 `NkForEachHelper`

```cpp
#define NkForEachHelper NkForEachHelper##count(func, __VA_ARGS__)
```

**Helper pour NkForEach**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:583`*


---

<a name="nkgigabytes"></a>

#### 🔣 `NkGigabytes`

```cpp
#define NkGigabytes (NkMegabytes(x) * 1024LL)
```

**Convertit en gigabytes**

**Retour:** Taille en octets

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:185`*


---

<a name="nkkilobytes"></a>

#### 🔣 `NkKilobytes`

```cpp
#define NkKilobytes ((x) * 1024LL)
```

**Convertit en kilobytes**

**Retour:** Taille en octets

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:169`*


---

<a name="nkmaskaddress"></a>

#### 🔣 `NkMaskAddress`

```cpp
#define NkMaskAddress ((void*)((uintptr_t)(ptr) & 0xFFFFF000))
```

**Masque une adresse mémoire (pour debug)**

**Retour:** Pointeur masqué

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:593`*


---

<a name="nkmax"></a>

#### 🔣 `NkMax`

```cpp
#define NkMax (((a) > (b)) ? (a) : (b))
```

**Retourne le maximum de deux valeurs**

**Retour:** La plus grande des deux valeurs

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:239`*


---

<a name="nkmegabytes"></a>

#### 🔣 `NkMegabytes`

```cpp
#define NkMegabytes (NkKilobytes(x) * 1024LL)
```

**Convertit en megabytes**

**Retour:** Taille en octets

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:177`*


---

<a name="nkmin"></a>

#### 🔣 `NkMin`

```cpp
#define NkMin (((a) < (b)) ? (a) : (b))
```

**Retourne le minimum de deux valeurs**

**Retour:** La plus petite des deux valeurs

> ⚠️ **Attention:** Évalue les arguments plusieurs fois

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:229`*


---

<a name="nkpointerdistance"></a>

#### 🔣 `NkPointerDistance`

```cpp
#define NkPointerDistance ((intptr_t)((const char*)(ptr2) - (const char*)(ptr1)))
```

**Calcule la distance entre deux pointeurs en octets**

**Retour:** Distance en octets (ptr2 - ptr1)

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:601`*


---

<a name="nkradianstodegrees"></a>

#### 🔣 `NkRadiansToDegrees`

```cpp
#define NkRadiansToDegrees ((radians) * 180.0 / 3.14159265358979323846)
```

**Convertit radians en degrés**

**Retour:** Valeur en degrés

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:622`*


---

<a name="nkstringify"></a>

#### 🔣 `NkStringify`

```cpp
#define NkStringify NkStringifyHelper(x)
```

**Convertit un token en chaîne de caractères**

**Retour:** Chaîne de caractères représentant le token

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:31`*


---

<a name="nkswap"></a>

#### 🔣 `NkSwap`

```cpp
#define NkSwap \
```

**Échange deux valeurs (nécessite temporaire)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:276`*


---

<a name="nkterabytes"></a>

#### 🔣 `NkTerabytes`

```cpp
#define NkTerabytes (NkGigabytes(x) * 1024LL)
```

**Convertit en terabytes**

**Retour:** Taille en octets

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:193`*


---

<a name="nkvaargscount"></a>

#### 🔣 `NkVaArgsCount`

```cpp
#define NkVaArgsCount \
```

**Nombre d'arguments variadiques (max 16)**

**Retour:** Nombre d'arguments passés

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:91`*


---

<a name="nkversionencode"></a>

#### 🔣 `NkVersionEncode`

```cpp
#define NkVersionEncode \
```

**Encode une version en entier 32-bit**

**Retour:** Version encodée (format: 0xMMmmpppp)

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:529`*


---

<a name="nkversionmajor"></a>

#### 🔣 `NkVersionMajor`

```cpp
#define NkVersionMajor (((version) >> 24) & 0xFF)
```

**Extrait le major d'une version encodée**

**Retour:** Numéro de version majeure

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:541`*


---

<a name="nkversionminor"></a>

#### 🔣 `NkVersionMinor`

```cpp
#define NkVersionMinor (((version) >> 16) & 0xFF)
```

**Extrait le minor d'une version encodée**

**Retour:** Numéro de version mineure

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:549`*


---

<a name="nkversionpatch"></a>

#### 🔣 `NkVersionPatch`

```cpp
#define NkVersionPatch ((version) & 0xFFFF)
```

**Extrait le patch d'une version encodée**

**Retour:** Numéro de version patch

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkMacros.h:557`*


---

