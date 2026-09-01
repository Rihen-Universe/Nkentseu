# Unkeny — moteur de jeu 2D sur NKCanvas

**Créé le 2026-09-01.** Décision de Rodolf : le nom `Unkeny` — ancien nom de
travail de Noge, jamais employé (mesuré : **zéro** occurrence dans le code, deux
`.md` à corriger) — est réattribué au moteur **2D**.

## Pourquoi un nom distinct, et pas « Noge2D »

`NKCanvas` et `NKRenderer` sont **exclusifs** par règle du dépôt : une fenêtre
utilise l'un **ou** l'autre, jamais les deux. Un nom dérivé de Noge suggérerait
une parenté — une variante, un sous-ensemble, une version allégée — qui n'existe
pas. **Unkeny est une autre pile de rendu, pas un Noge diminué.**

| | Noge | Unkeny |
|---|---|---|
| rendu | NKRenderer (3D, RHI) | NKCanvas (2D) |
| destiné à | grosses applications, façon Unreal | jeux 2D, outils légers |
| coquille | la sienne | `NkCanvasApp` |

## Ce qu'il contient, et POURQUOI exactement ça

Rien n'a été mis ici par anticipation. Chaque brique a été **mesurée** comme
répétée dans les trois jeux de plateau écrits juste avant lui (`NkDames`,
`NkEchecs`, `NkLudo`) — c'est la seule protection contre un cinquième système
dormant.

| brique | présent dans |
|---|---|
| mise en page ancrée sur la **zone sûre** (plateau carré + bandeau + pied) | 3/3 |
| **sièges** humain/IA, modes, écran de menu | 3/3 |
| `NkDansRect` et les tests de zone | 3/3 |
| **boutons** et bascules dessinés | 3/3 |
| interpolation d'animation **multi-segments** | 2/3 |

Les aides de texte, elles, ne sont **pas** ici : elles vivent un étage plus bas,
dans `NKCanvas/App/NkCanvasTexte.h`, parce qu'elles servent aussi aux
applications qui n'utilisent pas Unkeny. Unkeny les re-exporte simplement.

## Ce qu'il N'EST PAS, et ce que ce n'est pas la peine d'y écrire

⚠️ **Unkeny ne réécrit rien de ce que le dépôt porte déjà.** Il les
**compose** :

| besoin | où il vit DÉJÀ |
|---|---|
| entités et systèmes | `Kernel/Runtime/NKECS` |
| collision 2D | `Kernel/Runtime/NKCollision` (2D+3D, 13 vagues, 107 cas) |
| physique 2D | `Kernel/Runtime/NKPhysics` (2D+3D, complet M0→M13) |
| son | `Kernel/Runtime/NKAudio` |
| images, polices | `NKImage`, `NKFont` |
| fenêtre, boucle, cycle de vie mobile | `NKCanvas/App/NkCanvasApp` |
| graphe de nœuds | `Kernel/Runtime/NKGraph` |

Écrire un second système de collision 2D dans Unkeny serait la faute que ce
dépôt a déjà payée trois fois. **La règle : avant d'écrire un mécanisme ici,
chercher qui le porte déjà — et commencer par la couche du dessous.**

## Sur les `.cpp`

Unkeny **a** des `.cpp`, contrairement à `NkCanvasGuiApp.h` et
`NkCanvasTexte.h`. Ce n'est pas une incohérence : la règle de l'en-tête seul
protège un module du **Kernel** dont quatorze dépendants ne veulent pas payer
NKGui. Unkeny est un module d'**Engine** : on n'en dépend que si on le veut,
donc un `.cpp` n'impose rien à personne.

## Son éditeur

`Applications/UnkenyEditor` — il vit dans `Applications/`, jamais ici. Un moteur
ne contient pas son outil ; l'outil consomme le moteur.
