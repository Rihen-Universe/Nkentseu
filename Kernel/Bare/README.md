# Bare — couche bas niveau (OS from scratch)

> ⚠️ **Squelette documentaire — aucun code pour l'instant.** Chaque module ne contient que son
> `README.md` et sa `ROADMAP.md`. On construit cette couche **from scratch, pour apprendre en
> grandissant avec**, **sans Linux**, en ne s'appuyant que sur **Nkentseu et son kernel**.

`Bare` est la couche qui fait tourner Nkentseu **directement sur le matériel**, sans système
d'exploitation hôte — pour notre **console maison**. Elle démarre la machine, prend la main
sur le CPU, organise la mémoire physique, pilote les périphériques, puis lance le dashboard et
les jeux (Nkoung) par-dessus le moteur.

Pour la vision d'ensemble, le découpage en couches et la séquence de démarrage : **lire d'abord
[ARCHITECTURE.md](ARCHITECTURE.md)**. La feuille de route globale : [ROADMAP.md](ROADMAP.md).

---

## Les modules

| Module | Rôle | Phase |
|--------|------|-------|
| [NKArch](NKArch/README.md) | Abstraction d'architecture CPU (x86_64 / ARM64) : registres, contexte, MMU bas niveau. | 1 |
| [NKBoot](NKBoot/README.md) | Démarrage : entrée bas niveau, init CPU/pile/BSS, passage à `main`. | 1 |
| [NKSerial](NKSerial/README.md) | UART / port série : le premier moyen de log, avant l'écran. | 1 |
| [NKInterrupt](NKInterrupt/README.md) | Interruptions et exceptions : vecteurs, dispatch IRQ. | 2 |
| [NKPMM](NKPMM/README.md) | Mémoire physique + MMU/pagination : alimente NKMemory. | 2 |
| [NKTimerHW](NKTimerHW/README.md) | Timer matériel : base de temps, ticks. | 2 |
| [NKDriver](NKDriver/README.md) | Framework de pilotes : modèle, enregistrement, découverte. | 3 |
| [NKDisplay](NKDisplay/README.md) | Affichage / framebuffer : sortie du renderer logiciel NKCanvas. | 3 |
| [NKInput](NKInput/README.md) | Entrées : manette, boutons (source des events). | 3 |
| [NKStorage](NKStorage/README.md) | Stockage : carte SD / flash (blocs bruts). | 4 |
| [NKFSBare](NKFSBare/README.md) | Système de fichiers minimal par-dessus NKStorage. | 4 |
| [NKAudioHW](NKAudioHW/README.md) | Audio : DAC / sortie son (alimente NKAudio). | 4 |
| [NKScheduler](NKScheduler/README.md) | Ordonnanceur de tâches : contexte, files, coopératif puis préemptif. | 5 |
| [NKConsoleRT](NKConsoleRT/README.md) | Runtime console : assemble tout, branche les backends System, lance Nkoung. | 5 |

Les phases correspondent à la [ROADMAP globale](ROADMAP.md). Les modules d'une même phase se
construisent ensemble ; chaque phase produit un jalon **observable**.

---

## Conventions

- Préfixe `NK`, `NkPascalCase`, namespace prévu `nkentseu::bare`.
- Chaque module, une fois démarré, aura : `src/`, `README.md`, `ROADMAP.md` (+ `.jenga` à terme).
- **Freestanding** : pas de libc/libstdc++ ; on s'appuie sur la Foundation zéro-STL.
- On vise **une architecture à la fois** (x86_64 sous QEMU pour itérer, puis ARM64 cible
  console), généralisée via `NKArch`.

[Architecture complète](ARCHITECTURE.md) · [Roadmap globale](ROADMAP.md)
