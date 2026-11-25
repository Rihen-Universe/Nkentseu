# nkentseu::platform::ExtendedFeatures

Fonctionnalités CPU supplémentaires (sécurité, mémoire, performance)

`#include "NKPlatform/NkCPUFeatures.h"`

## Hiérarchie

- Classes de base : aucune
- Classes dérivées : aucune

## Membres

### hasAES

```cpp
bool hasAES
```

Support de AES-NI (chiffrement AES matériel)

### hasSHA

```cpp
bool hasSHA
```

Support des extensions SHA matérielles

### hasRDRAND

```cpp
bool hasRDRAND
```

Support de RDRAND (générateur aléatoire matériel)

### hasRDSEED

```cpp
bool hasRDSEED
```

Support de RDSEED (graine pour PRNG)

### hasCLFLUSH

```cpp
bool hasCLFLUSH
```

Support de CLFLUSH (flush de ligne de cache)

### hasCLFLUSHOPT

```cpp
bool hasCLFLUSHOPT
```

Support de CLFLUSHOPT (version optimisée)

### hasPREFETCHWT1

```cpp
bool hasPREFETCHWT1
```

Support de PREFETCHWT1 (préchargement vers L2)

### hasMOVBE

```cpp
bool hasMOVBE
```

Support de MOVBE (move avec swap d'octets)

### hasPOPCNT

```cpp
bool hasPOPCNT
```

Support de POPCNT (comptage de bits à 1)

### hasLZCNT

```cpp
bool hasLZCNT
```

Support de LZCNT (comptage de zéros en tête)

### hasBMI1

```cpp
bool hasBMI1
```

Support de BMI1 (Bit Manipulation Instruction Set 1)

### hasBMI2

```cpp
bool hasBMI2
```

Support de BMI2 (Bit Manipulation Instruction Set 2)

### hasADX

```cpp
bool hasADX
```

Support de ADX (addition multi-précision avec carry)

### hasVMX

```cpp
bool hasVMX
```

Support de VMX (Intel VT-x)

### hasSVM

```cpp
bool hasSVM
```

Support de SVM (AMD-V)

### ExtendedFeatures

```cpp
ExtendedFeatures()
```

Constructeur par défaut avec toutes les fonctionnalités désactivées
