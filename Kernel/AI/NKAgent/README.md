# NKAgent — agent cognitif

> ✅ Jalon 4 COMPLET (2026-07-26) : **personnalité** (`NkAgentPersonality` — boldness/curiosity/
> patience, influencent RÉELLEMENT la décision via `NkAgent::StepWithPersonality` ; prouvé par
> ablation, même politique entraînée : profil Prudent = 0.97 % de pas risqués/1 chute de trou
> contre profil Audacieux = 29.08 % de pas risqués/84 chutes) + **raisonnement par LLM**
> (`NkAgentLLMReasoning` — pont réel vers NKInfer/Qwen2.5 7B Instruct, forward complet 28 couches
> réelles, poids GGUF réels ; 3 décisions réelles mesurées ~149 s/décision, `Applications/
> NKAgentLLMTest`). Jalon 3 buts & planification COMPLET (2026-07-25) : `NkAgentGoal`/
> `NkGoalStack` (pile de sous-buts) + `NkAgent::StepWithGoals` (politique dédiée par but) — prouvé
> sur un monde clé-puis-porte (`NkKeyDoorGridWorld`) : agent SANS but = **0 %** de réussite (échec
> structurel) contre agent AVEC pile de buts = **100 %**. Jalon 2 mémoire COMPLET (2026-07-25) :
> importance des souvenirs (|erreur TD|, oubli par moindre importance) + **memory replay** (prouvé
> par ablation : 20 % → 100 % de réussite à budget d'épisodes réduit). Voir l'état détaillé dans la
> [ROADMAP](ROADMAP.md) et l'[architecture de la couche](../ARCHITECTURE.md).

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
- **Décision** : combiner politique (NKRL) et état interne (`StepWithPersonality`) ; raisonnement
  par LLM (NKInfer) disponible comme pont explicite pour les décisions ambiguës
  (`NkAgentLLMReasoning`, cf ROADMAP Jalon 4b — pas encore intégré à la boucle de décision par
  défaut, vu son coût).
- Personnalité / traits (variabilité entre agents) — `NkAgentPersonality` (boldness/curiosity/
  patience), cf ROADMAP Jalon 4a.

## Place dans la couche

- **Dépend de** : [NKRL](../NKRL/README.md), [NKInfer](../NKInfer/README.md).
- **Utilisé par** : [NKCivilization](../NKCivilization/README.md), [NKEmbodied](../NKEmbodied/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
