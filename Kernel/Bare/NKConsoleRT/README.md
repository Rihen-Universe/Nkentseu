# NKConsoleRT — runtime console

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKConsoleRT` est le **chef d'orchestre** de la console. Tous les autres modules de `Bare`
fournissent une capacité (mémoire, temps, écran, entrées, stockage, son, tâches) ; NKConsoleRT
les **assemble** dans le bon ordre, vérifie que chacun est prêt, et fait démarrer le système
jusqu'à son but final : **lancer le dashboard et les jeux**.

C'est lui qui branche les **backends Bare** sur la couche `System` du moteur — il dit à `NKWindow`
« ta fenêtre, c'est NKDisplay », à `NKEvent` « tes entrées viennent de NKInput », à
`NKFileSystem` « tes fichiers viennent de NKFSBare », à `NKTime` « ton horloge, c'est NKTimerHW ».
Une fois ces ponts en place, **tout le moteur fonctionne sans savoir qu'il tourne sur le métal** :
NKConsoleRT lance alors **Nkoung** (le menu de plateforme + les jeux), et la machine devient une
console qui démarre directement sur ses jeux.

## Responsabilités

- **Séquence d'initialisation** ordonnée de tous les sous-systèmes Bare (cf. [architecture §4](../ARCHITECTURE.md#4-séquence-de-démarrage-vision)).
- **Brancher les backends Bare** sur `System` (NKWindow/NKEvent/NKFileSystem/NKTime).
- Démarrer les tâches (via NKScheduler) et lancer **Nkoung**.
- Gestion globale : panique propre, redémarrage, arrêt.

## Place dans la couche

- **Dépend de** : **tous** les modules Bare.
- **Lance** : la couche `System` (via backends) puis l'application `Nkoung`.

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
