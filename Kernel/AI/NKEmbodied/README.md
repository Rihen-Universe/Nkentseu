# NKEmbodied — incarnation (robotique & objets intelligents)

> ✅ Jalon 1 livré : abstraction capteurs/actionneurs + boucle perception → décision
> (NKAgent) → action dans un corps simulé (grille), prouvée par `Applications/
> NKEmbodiedTest` (100% de réussite/200 épisodes).
> ✅ Jalon 2 livré : contrôle robuste — bruit capteur (dégradation réelle mesurée : 100%
> à σ=0 → 22,6% à σ=0,5), limites actionneur (saturation d'amplitude + limite de
> fréquence de commande, dégradation mesurée à N=128 ticks), boucle à fréquence fixe
> (découplage décision/simulation vérifié), sécurité (watchdog déclenché sur 2 scénarios
> construits exprès). Voir la [ROADMAP](ROADMAP.md) (détail + chiffres + limites
> honnêtes) et l'[architecture de la couche](../ARCHITECTURE.md).

## Rôle

`NKEmbodied` donne un **corps** à une intelligence. Un agent (NKAgent) raisonne dans l'abstrait ;
pour agir dans un **monde physique** — simulé ou réel — il lui faut des **capteurs** (yeux,
contacts, position) et des **actionneurs** (moteurs, roues, articulations). NKEmbodied fait le
pont entre la décision et le matériel : il transforme les observations des capteurs en perception
pour l'agent, et les actions de l'agent en commandes pour les actionneurs.

C'est ce qui ouvre la **robotique** et les **objets intelligents**. La même politique peut être
testée dans un corps **simulé** (sûr, rapide, répétable) puis transférée vers un robot **réel** —
qui, grâce à [Kernel/Bare](../../Bare/README.md), peut faire tourner l'IA **directement sur
l'appareil**, sans OS hôte. NKEmbodied gère cette boucle capteur→cerveau→actionneur et le passage
délicat du simulé au réel.

## Responsabilités

- Abstraction **capteurs** (vision, contact, position…) → perception pour NKAgent.
- Abstraction **actionneurs** (moteurs, articulations) ← actions de NKAgent.
- Boucle de contrôle temps réel (perception → décision → action).
- Corps **simulé** (dans le moteur) puis **réel** (via Kernel/Bare) ; transfert sim→réel.

## Place dans la couche

- **Dépend de** : [NKAgent](../NKAgent/README.md) ; côté réel, [Kernel/Bare](../../Bare/README.md) (NKDriver, NKInput…).
- **Utilisé par** : applications robotiques / objets intelligents.

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
