# Bancs de mesure NKGraph — sérialisation, groupes, robustesse

> Écrits le 2026-08-22 pour répondre à **une** question qui bloquait la
> spécification visuelle : *« que fait `Deserialize` d'une directive qu'il ne
> comprend pas ? »* — parce que **les trois réponses possibles menaient à trois
> conclusions opposées**, et qu'aucune ne se lit dans le code sans l'exécuter.
>
> Leurs résultats sont repris dans `../SPECIFICATION_VISUELLE.md` § 16 et § 7.6.
> Les `.sortie.txt` sont les sorties **réelles** de l'exécution du 22/08, gardées
> pour qu'on puisse comparer après une modification du modèle.

## Comment les rejouer

`NkString` et `NkVector` **ne sont pas header-only** — il faut lier les trois
modules de Foundation. Depuis la racine du dépôt :

```sh
g++ -std=c++17 -O0 -w -include new \
  -IKernel/Runtime/NKGraph/src \
  -IKernel/Foundation/NKContainers/src -IKernel/Foundation/NKContainers/pch \
  -IKernel/Foundation/NKCore/src       -IKernel/Foundation/NKCore/pch \
  -IKernel/Foundation/NKMemory/src     -IKernel/Foundation/NKMemory/pch \
  -IKernel/Foundation/NKPlatform/src   -IKernel/Foundation/NKMath/src \
  -o mesure.exe Kernel/Runtime/NKGraph/tests/mesure_groupes.cpp \
  $(find Kernel/Foundation/NKCore/src Kernel/Foundation/NKMemory/src \
         Kernel/Foundation/NKContainers/src -name '*.cpp')
./mesure.exe
```

⚠️ **`-include new` n'est pas décoratif** : plusieurs fichiers de NKMemory
utilisent le placement `new` sans inclure `<new>`. **g++ le tolère, clang++ le
refuse** — si tu compiles avec clang, tu verras des erreurs qui n'ont rien à voir
avec ces bancs.

⚠️ **Ne lis pas le code de retour après un tuyau.** `g++ … | head` te rend le
statut de `head`, qui vaut toujours 0. Redirige vers un fichier, ou lis
`${PIPESTATUS[0]}`.

## Ce que chacun mesure

| banc | question | réponse mesurée |
|---|---|---|
| `mesure_directive_inconnue.cpp` | une directive **inconnue** casse-t-elle la relecture ? | **Non — ignorée**, et le graphe se resérialise identique octet pour octet. ⚠️ Mais elle est **perdue au premier annuler** : l'historique réécrit depuis le modèle |
| `mesure_malformee.cpp` | et une directive **connue mais mal formée** ? | ⚠️ **corruption silencieuse** : nœud d'identifiant `0`, lien pendant conservé, identifiants dupliqués acceptés, prise orpheline avalée. `Deserialize` rend `true` dans tous les cas |
| `mesure_groupes.cpp` | le modèle porte-t-il le **groupe** (définition + instances) ? | ✅ **oui, en entier** — et un type enregistré à l'exécution survit à l'aller-retour **avec le même identifiant** |

## Les deux trouvailles à retenir

1. **`sock` avant son nœud est le pire des cas.** La prise est perdue en silence
   — et comme **l'ordre des prises EST leur index**, elle ne perd pas *une*
   prise : elle **décale toutes les suivantes**, et les liens pointent alors sur
   la mauvaise. Un graphe qui s'ouvre, s'affiche et se calcule **faux**.
2. **`NkEvalPlan::errorDetail` est vide sur `UnknownSubgraph`**, alors que
   l'en-tête promet qu'il est « rempli en cas d'échec » et que `InterfaceMismatch`
   le remplit bien. C'est le cas où le détail servirait le plus — dire *quel*
   groupe est introuvable — qui n'en a pas.

## Ce qu'ils ne mesurent PAS

- **le regroupement lui-même** — il vit dans NKEditorKit (couche 2), qui n'existe
  pas encore. Le critère d'acceptation est écrit (`SPECIFICATION_VISUELLE.md`
  § 7.6 : *grouper puis dégrouper rend le graphe identique, octet pour octet, aux
  identifiants près*) mais **il n'est pas exécuté** ;
- **les consommateurs** (`NkMatGraphCheck`), qui portent peut-être déjà des
  conventions pour une partie des notions sans foyer.
