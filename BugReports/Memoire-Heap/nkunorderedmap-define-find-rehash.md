# SIGSEGV intermittent : deref null après insert+find (NkUnorderedMap rehash)

- **Catégorie** : Memoire-Heap
- **Sévérité** : majeur
- **Date** : 2026-06-04
- **Statut** : contourné (cause racine NkUnorderedMap à confirmer)

## Symptôme

Crash **intermittent** (~1 lancement sur 3) `SIGSEGV` / `0xC0000005`. Backtrace :
```
NkVector<NkSLFunctionOverload>::PushBack (this=0x78, ...)
  NkSLSemantic::AnalyzeProgram  (existing=0x0)  -> existing->overloads.PushBack(ov)
```
ou la variante dans `NkSLSymbolTable::RegisterBuiltinFunctions`. `this=0x78` =
`nullptr + offset(overloads)` → on déréférence un pointeur **null**.

## Contexte

- Compilateur NkSL (`NkSLSemantic` / `NkSLSymbolTable`), n'importe quelle cible.
- Intermittent : le même shader compile correctement la plupart du temps.

## Cause racine

Pattern dangereux à plusieurs endroits :
```cpp
NkSLSymbol* existing = symbols.Resolve(name);   // pointeur DANS la map (ou null)
if (!existing) {
    Define(sym);                                 // symbols[name]=sym -> peut REHASHER
    existing = symbols.Resolve(name);            // re-Find
}
existing->overloads.PushBack(ov);                // <- existing peut etre null
```
`NkSLScope::symbols` est un `NkUnorderedMap<NkString, NkSLSymbol>`. `Define` fait
`symbols[name] = sym` (`operator[]` qui peut **rehasher**). Le `Resolve` (Find) qui
suit retourne **intermittemment null** après cet insert → deref null → crash. Le
caractère intermittent dépend du nombre de buckets / du timing de rehash. (Cause
racine probable : incohérence insert→find de `NkUnorderedMap` lors d'un rehash —
à confirmer/corriger dans le conteneur Foundation.)

## Solution (contournement au call-site)

Ne **jamais** faire `Define()` puis re-`Resolve()`+deref. Construire le symbole
**complet** (avec son overload) et l'insérer une seule fois ; sinon ajouter au
symbole existant (pointeur frais, sans insert après) :
```cpp
NkSLFunctionOverload ov = /* ... */;
NkSLSymbol* existing = symbols.Resolve(name);
if (existing) {
    existing->overloads.PushBack(ov);            // pointeur frais, pas d'insert apres
} else {
    NkSLSymbol sym; sym.name = name; sym.kind = ...;
    sym.overloads.PushBack(ov);                  // overload AVANT l'insert
    Define(sym);                                 // insert unique, pas de re-find/deref
}
```
Appliqué à `NkSLSemantic.cpp` (fonctions user) et `NkSLSymbolTable.cpp`
(`RegisterBuiltinFunctions`, builtins).

## Vérification

- Avant : ~1/3 des lancements crashent. Après : **6/6 OK**.

## À faire (cause racine)

Auditer `NKContainers/Associative/NkUnorderedMap.h` : vérifier que `operator[]`
(insertion) + `Find` sont cohérents après un rehash, et que `Find` ne retourne pas
un pointeur invalidé. C'est un conteneur Foundation utilisé partout → un vrai bug
ici aurait d'autres symptômes latents.

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/SL/NkSLSemantic.cpp`, `NkSLSymbolTable.cpp`
- `Kernel/Foundation/NKContainers/src/NKContainers/Associative/NkUnorderedMap.h`
