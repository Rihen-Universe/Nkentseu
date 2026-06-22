# Blueprint — le langage nodal façon Unreal

> ⚠️ Squelette : pas encore de code. Voir l'[architecture](../../../ARCHITECTURE.md) et la
> [roadmap](../../../ROADMAP.md).

## Rôle
`Blueprint` est le langage visuel **pour la logique sérieuse**, calqué sur les Blueprints d'Unreal.
Sur le substrat [Graph](../Graph/README.md), il ajoute ce qui en fait un vrai langage :
- des **broches typées** (int, float, bool, string, objets…) avec des **règles de connexion** (on
  ne relie pas n'importe quoi à n'importe quoi) ;
- deux flux distincts : le **flux d'exécution** (les broches blanches qui ordonnancent « fais ceci
  puis cela ») et le **flux de données** (les broches typées qui transportent des valeurs) ;
- une **palette de nœuds** : variables, branches (`if`), boucles, appels de fonction, événements.

Le levier décisif : la palette peut être **auto-générée depuis NKReflection** — exactement comme
Unreal expose les `UFUNCTION` en nœuds. Chaque fonction/type du moteur réfléchi devient un nœud
utilisable, sans le câbler à la main. (D'où l'importance de maturer NKReflection.)

## Responsabilités
- **Broches typées** + règles/validation de connexion.
- **Flux d'exécution** + **flux de données** ; nœuds de contrôle (branch, loop, sequence).
- **Palette de nœuds**, idéalement **générée depuis NKReflection** (API du moteur).
- Variables, événements, fonctions définies par l'utilisateur.

## Dépend de
[Graph](../Graph/README.md), **NKReflection** (palette auto), NKContainers ; alimente
[Codegen](../Codegen/README.md).

[Architecture](../../../ARCHITECTURE.md) · [Roadmap](../../../ROADMAP.md)
