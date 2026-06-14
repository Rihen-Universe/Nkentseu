# NKInfer — inférence

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKInfer` fait **tourner** un modèle déjà entraîné — pas pour apprendre, mais pour **produire**.
Une fois un réseau entraîné par NKTrain, on veut l'utiliser : classer une image, choisir l'action
d'un agent, générer du texte. L'inférence n'a besoin que du passage « avant » (forward), sans
gradients : elle peut donc être **plus rapide et plus légère** que l'entraînement, et c'est là que
les optimisations comptent.

C'est aussi le **point d'entrée des LLM** dans Nkentseu : charger un modèle de type transformer et
générer du texte token par token (avec cache d'attention pour la vitesse). NKInfer gère le
chargement des poids, l'exécution efficace (sur CPU ou GPU), et la **quantization** (réduire la
précision des poids pour faire tenir un modèle en mémoire et l'accélérer). C'est le module que les
agents (NKAgent) appellent pour **réfléchir** et **décider**.

## Responsabilités

- Charger un modèle entraîné ; exécution **forward** optimisée (sans gradient).
- Inférence par **lots** ; choix du device (CPU/GPU).
- **Quantization** des poids (fp16/int8…).
- **LLM** : génération token par token, cache d'attention (KV-cache), échantillonnage.

## Place dans la couche

- **Dépend de** : [NKNN](../NKNN/README.md), [NKTensor](../NKTensor/README.md).
- **Utilisé par** : [NKAgent](../NKAgent/README.md), [NKGen](../NKGen/README.md), et l'application.

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
