# NKReflection — documentation détaillée

Le module **NKReflection**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKReflection.md](../NKReflection.md).

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec ses pièges
concrets (ownership des pointeurs, thread-safety, capacités fixes, écarts spec/impl).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Types.md](Types.md) | Décrire un type (`NkType`, `NkTypeOf<T>()`), une classe (`NkClass` : héritage, instanciation), et les retrouver dans le registre central `NkRegistry` (+ macros d'auto-enregistrement). | `NkType.h`, `NkClass.h`, `NkRegistry.h` |
| [Members.md](Members.md) | Réfléchir les membres : `NkProperty` (lire/écrire un champ par offset ou accesseur, flags) et `NkMethod` (invocation réflexive type-erased, paramètres). | `NkProperty.h`, `NkMethod.h` |

[← Récap NKReflection](../NKReflection.md) · [← Couche System](../README.md)
