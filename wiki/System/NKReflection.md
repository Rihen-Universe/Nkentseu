# NKReflection

> Couche **System** · La réflexion minimaliste du moteur : décrire un type, une classe,
> ses propriétés et ses méthodes à l'exécution, et les retrouver dans un registre central.

La réflexion, c'est la capacité d'un programme à **se décrire lui-même** : connaître à
l'exécution le nom d'un type, sa taille, la liste de ses champs, ses méthodes — et pouvoir
lire/écrire un champ ou appeler une méthode sans connaître le type concret à la compilation.
C'est la brique sous-jacente d'un **inspecteur d'éditeur** (afficher/éditer les propriétés
d'un objet ECS), de la **sérialisation automatique** (parcourir les champs réfléchis pour les
écrire en JSON), du **scripting** et de tout outillage générique sur les types du moteur.

NKReflection est volontairement **minimaliste et zéro-allocation** : pas de conteneurs STL,
pas de mémoire dynamique pour les registres (tableaux statiques de capacité fixe). On décrit
un type avec `NkType`, une classe avec `NkClass`, ses champs avec `NkProperty`, ses méthodes
avec `NkMethod`, et on enregistre le tout dans `NkRegistry`. La fabrique canonique de tout
est `NkTypeOf<T>()`, qui renvoie une instance `NkType` unique et stable par type.

- **Namespace** : `nkentseu::reflection` (sauf les callables, qui sont des `nkentseu::NkFunction`)
- **Header parapluie** : `#include "NKReflection/NkReflection.h"`
- **Macro d'export** : `NKENTSEU_REFLECTION_API` (sur chaque classe publique)

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Décrire un type (nom, taille, catégorie) à l'exécution | [Types & classes](NKReflection/Types.md) |
| Décrire une classe, son héritage, créer/détruire une instance | [Types & classes](NKReflection/Types.md) |
| Retrouver un type/une classe par son nom (registre central) | [Types & classes](NKReflection/Types.md) |
| Réfléchir des champs : lire/écrire une propriété par offset ou accesseur | [Propriétés & méthodes](NKReflection/Members.md) |
| Invoquer une méthode de façon réflexive (type-erased) | [Propriétés & méthodes](NKReflection/Members.md) |
| Auto-enregistrer une classe avec des macros | [Types & classes](NKReflection/Types.md) |

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec ses pièges
concrets (ownership, thread-safety, capacités fixes, écarts spec/impl).

---

## Aperçu des familles

- **Type** (`NkType.h`) — `NkType` porte les métadonnées immuables d'un type : nom, taille,
  alignement, catégorie (`NkTypeCategory` : primitifs, pointeur, classe, enum…). La fabrique
  canonique est la fonction libre `NkTypeOf<T>()`, qui renvoie une instance **unique et
  stable** par type — base de toute la réflexion. Inclut aussi un petit registre local
  `NkTypeRegistry`.
- **Classe** (`NkClass.h`) — `NkClass` décrit une classe : nom, taille, `NkType` associé,
  classe de base (héritage simple), collections fixes de **64** propriétés et **64** méthodes,
  callbacks ctor/dtor en `NkFunction`. Fournit `CreateInstance`/`DestroyInstance` et des
  fabriques `MakeFromClassType` / `RegisterMemberProperty` / `RegisterMemberMethod`.
- **Registre** (`NkRegistry.h`) — `NkRegistry` est le registre **central** singleton (types
  ET classes, capacité 512 chacune), recommandé et utilisé par les macros
  `NKENTSEU_REFLECT_CLASS` / `NKENTSEU_REGISTER_CLASS`. À ne pas confondre avec le
  `NkTypeRegistry` local de `NkType.h`.
- **Propriétés** (`NkProperty.h`) — `NkProperty` décrit un champ réfléchi : nom, type, offset,
  flags (`NkPropertyFlags` : lecture seule, statique, transient…), accesseurs get/set
  optionnels type-erased. Lecture/écriture via `GetValue<T>` / `SetValue<T>` (par offset ou
  via accesseur).
- **Méthodes** (`NkMethod.h`) — `NkMethod` décrit une méthode réfléchie : nom, type de retour,
  flags (`NkMethodFlags`), liste de types de paramètres, et un invokeur type-erased.
  Invocation via `Invoke(instance, args)`.

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NkReflection.h` | Parapluie (inclut le module). | — |
| `NkType.h` | `NkType`, `NkTypeCategory`, `NkTypeRegistry`, `NkTypeOf<T>()`. | [Types & classes](NKReflection/Types.md) |
| `NkClass.h` | `NkClass` (héritage, props/méthodes, ctor/dtor). | [Types & classes](NKReflection/Types.md) |
| `NkRegistry.h` | `NkRegistry` (registre central) + macros de réflexion. | [Types & classes](NKReflection/Types.md) |
| `NkProperty.h` | `NkProperty`, `NkPropertyFlags`. | [Propriétés & méthodes](NKReflection/Members.md) |
| `NkMethod.h` | `NkMethod`, `NkMethodFlags`, `MakeThreadEntry`. | [Propriétés & méthodes](NKReflection/Members.md) |
| `NkReflectionApi.h` | Macro d'export `NKENTSEU_REFLECTION_API`. | — |

---

[← Couche System](README.md) · [Index du wiki](../README.md)
