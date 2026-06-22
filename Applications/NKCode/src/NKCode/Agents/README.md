# Agents — le système de développement IA (façon Claude) + sous-agents

> ⚠️ Squelette : pas encore de code. Voir l'[architecture](../../../ARCHITECTURE.md) et la
> [roadmap](../../../ROADMAP.md).

## Rôle
`Agents` apporte à NKCode un **assistant de développement IA**, comme Claude Code : on lui parle, et
il **agit sur le projet** — lit et écrit des fichiers, lance des builds Jenga, cherche dans le code,
explique, corrige, refactore. La clé : les **outils** de l'assistant **sont les capacités de
NKCode** (l'éditeur, [Project](../Project/README.md), la recherche). L'assistant n'est donc pas un
chat à côté du code — il **développe avec toi, dans l'IDE**.

Trois piliers :

1. **L'assistant** — un panneau de conversation relié à un **LLM**. Au début, le modèle est
   **externe** (appel d'API via NKNetwork). À terme, des **modèles locaux** via **NKAI/NKInfer**
   (ton propre framework ML) → un assistant **100 % local**, sans dépendance cloud.
2. **Les sous-agents** — plutôt qu'un seul assistant, on **lance des agents spécialisés** (revue,
   écriture de tests, refactor, recherche, migration) qui travaillent **en parallèle**, chacun avec
   son contexte, façon Task/Agent. Un **orchestrateur** les coordonne et fusionne les résultats.
3. **Texte OU graphique** — on définit et visualise un pipeline d'agents de deux façons :
   - **en texte** (prompts, scripts d'orchestration) ;
   - **en graphe**, en **réutilisant le substrat [Graph](../Graph/README.md)** : des nœuds
     « **agent** », « **outil** », « **condition** », « **boucle** » qu'on **câble visuellement**
     (façon ComfyUI / n8n pour l'IA). → **la programmation visuelle de NKCode orchestre ses propres
     agents.** C'est l'imbrication unique : le même éditeur de nœuds sert au gameplay *et* à l'IA.

## Responsabilités
- **Assistant** : panneau de chat + boucle outil (lire/écrire fichiers, build Jenga, rechercher),
  diff/aperçu avant application, historique.
- **Connecteurs LLM** : API externe (via NKNetwork) puis **local via NKAI/NKInfer**.
- **Sous-agents** : lancement, contexte isolé, exécution parallèle, orchestrateur + fusion.
- **Orchestration visuelle** : nœuds agent/outil/condition/boucle sur le substrat Graph ;
  exécution + suivi en direct. Définition équivalente en **texte**.
- Garde-fous : permissions (ce qu'un agent peut toucher), validation humaine, journal d'actions.

## Dépend de
NKNetwork (API LLM), **NKAI/NKInfer** (modèles locaux, à terme), [Graph](../Graph/README.md)
(orchestration visuelle), [Project](../Project/README.md) (build/run comme outils),
[Editor](../Editor/README.md) (édition), NKThreading (parallélisme).

[Architecture](../../../ARCHITECTURE.md) · [Roadmap](../../../ROADMAP.md)
