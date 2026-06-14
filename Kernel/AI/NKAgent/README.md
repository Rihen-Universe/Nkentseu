# NKAgent — agent cognitif

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKAgent` est **l'être** : l'unité qui perçoit, se souvient, réfléchit et décide. C'est lui qui
donne corps à ton idée d'un individu avec un **passé**, un **présent** et un **futur**. Là où NKRL
fournit une politique brute (état → action), NKAgent construit une **vie intérieure** plus riche :

- **Passé** — une **mémoire** : ce que l'agent a vécu, qu'il peut rappeler et dont il tire des leçons.
- **Présent** — une **perception** : comment il lit l'état du monde autour de lui maintenant.
- **Futur** — des **buts** et une **planification** : ce qu'il veut, et comment il choisit d'agir
  pour y arriver.

Sa **décision** combine ces trois temps : une politique apprise (NKRL), éventuellement un
raisonnement par modèle de langage (NKInfer), des besoins internes, et une part d'aléatoire. Le
résultat est un comportement **émergent** — qui *paraît* autonome parce qu'il naît de l'état
interne et des interactions, non de règles scriptées (cf. [architecture §1](../ARCHITECTURE.md#1-pourquoi-cette-couche-existe)).
C'est l'inspiration des *generative agents* : des êtres qui se souviennent, réfléchissent et
planifient. Assemblés en grand nombre, ils forment la civilisation.

## Responsabilités

- **Mémoire** (passé) : stockage, rappel, oubli, importance des souvenirs.
- **Perception** (présent) : transformer l'état du monde en observation exploitable.
- **Buts & planification** (futur) : désirs, besoins, choix d'actions vers un objectif.
- **Décision** : combiner politique (NKRL), raisonnement (NKInfer) et état interne.
- Personnalité / traits (variabilité entre agents).

## Place dans la couche

- **Dépend de** : [NKRL](../NKRL/README.md), [NKInfer](../NKInfer/README.md).
- **Utilisé par** : [NKCivilization](../NKCivilization/README.md), [NKEmbodied](../NKEmbodied/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
