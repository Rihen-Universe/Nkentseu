# Bare — Architecture de la couche bas niveau (OS from scratch)

> ⚠️ **Statut : squelette documentaire.** Cette couche ne contient pour l'instant **aucun
> code** — uniquement l'architecture, et un `README` + une `ROADMAP` par module. On la
> construit **from scratch, pour apprendre en grandissant avec**.

---

## 1. Pourquoi cette couche existe

Nkentseu, jusqu'ici, suppose qu'un **système d'exploitation hôte** existe sous lui (Windows,
Linux, macOS, Android…). Les couches `Foundation`, `System` et `Runtime` s'appuient sur cet
OS pour la mémoire, les threads, les fichiers, la fenêtre, le GPU.

La couche **Bare** retire cette hypothèse. Son but : faire tourner Nkentseu **directement sur
le matériel**, sans OS hôte — pour notre **console maison**. C'est elle qui démarre la
machine, prend la main sur le processeur, organise la mémoire physique, pilote les
périphériques (écran, manette, stockage, audio), et finit par lancer le **dashboard** et les
**jeux** (Nkoung) au-dessus du moteur.

Règle du projet : **pas de portage Linux, pas de dépendance externe**. On n'utilise que
**Nkentseu et son kernel**. Tout ce qui manque, on l'écrit nous-mêmes. L'objectif n'est pas
d'aller vite, c'est de **comprendre toute la pile**, du métal au jeu, en la construisant.

---

## 2. Place dans la pile Nkentseu

```
┌──────────────────────────────────────────────────────────────┐
│  Application   Nkoung (dashboard + jeux 2D), SDK              │
├──────────────────────────────────────────────────────────────┤
│  Runtime       NKCanvas (renderer LOGICIEL → framebuffer),    │
│                NKUI, NKAudio, NKFont, NKImage…                │
├──────────────────────────────────────────────────────────────┤
│  System        NKWindow, NKEvent, NKFileSystem, NKTime…       │
│                (← reçoivent des BACKENDS « Bare »)            │
├──────────────────────────────────────────────────────────────┤
│  Foundation    NKMemory, NKContainers, NKMath, NKCore         │
│                (zéro-STL, zéro-dépendance → freestanding OK)  │
├══════════════════════════════════════════════════════════════┤
│  Bare (CETTE COUCHE)  l'OS from scratch :                     │
│     NKBoot · NKArch · NKInterrupt · NKPMM · NKScheduler ·     │
│     NKDriver · NKSerial · NKTimerHW · NKDisplay · NKInput ·   │
│     NKStorage · NKAudioHW · NKFSBare · NKConsoleRT            │
├──────────────────────────────────────────────────────────────┤
│  Matériel      CPU · RAM · framebuffer/GPU · manette ·        │
│                stockage · DAC audio · timers                  │
└──────────────────────────────────────────────────────────────┘
```

Le point clé : **Foundation tourne déjà sans OS** (zéro-STL, zéro-dépendance), il suffit de
lui fournir une zone mémoire et des I/O bas niveau — c'est précisément ce que Bare apporte.
Et le **renderer logiciel** de NKCanvas dessine directement dans un **framebuffer**, sans
pilote GPU : c'est notre voie d'affichage par défaut sur la console.

### Le lien Bare ↔ System

`System` (NKWindow, NKEvent, NKFileSystem, NKTime) garde son **interface publique**, mais
reçoit de **nouveaux backends** qui parlent à Bare au lieu de parler à Win32/POSIX :
- `NKWindow` → backend « Bare » : une « fenêtre » = le framebuffer plein écran (`NKDisplay`).
- `NKEvent` → backend « Bare » : les entrées viennent de `NKInput`.
- `NKFileSystem` → backend « Bare » : les fichiers viennent de `NKFSBare`.
- `NKTime` → backend « Bare » : le temps vient de `NKTimerHW`.

Ainsi, **tout le code au-dessus (Runtime, jeux) ne change pas** : il croit parler à un OS
normal, alors qu'il parle à notre propre OS. C'est toute la force du design multi-backend.

---

## 3. Les modules

| Module | Rôle | Dépend de |
|--------|------|-----------|
| **NKBoot** | Point d'entrée bas niveau : init CPU, pile, sections BSS/data, puis passage à `main`. | NKArch |
| **NKArch** | Abstraction d'architecture (x86_64 / ARM64) : registres, contexte, primitives CPU, MMU bas niveau. | — |
| **NKInterrupt** | Interruptions et exceptions : table des vecteurs, dispatch des IRQ, handlers. | NKArch |
| **NKPMM** | Gestion de la mémoire **physique** + pagination/MMU : c'est la mémoire qui **alimente NKMemory**. | NKArch |
| **NKScheduler** | Ordonnanceur de tâches : changement de contexte, files d'exécution, coopératif puis préemptif. | NKArch, NKInterrupt, NKTimerHW |
| **NKDriver** | Framework de pilotes : modèle de driver, enregistrement, découverte de bus/périphériques. | NKInterrupt |
| **NKSerial** | UART / port série : le **premier moyen de log**, avant même l'écran (debug bas niveau). | NKDriver |
| **NKTimerHW** | Timer matériel : base de temps, ticks périodiques (pour NKScheduler et NKTime). | NKDriver, NKInterrupt |
| **NKDisplay** | Pilote d'affichage / **framebuffer** : la sortie du renderer logiciel NKCanvas. | NKDriver, NKPMM |
| **NKInput** | Pilote d'entrées : manette, boutons (source des events). | NKDriver, NKInterrupt |
| **NKStorage** | Pilote de stockage : carte SD / flash (blocs bruts). | NKDriver |
| **NKAudioHW** | Pilote audio : DAC / sortie son (alimente NKAudio). | NKDriver, NKInterrupt |
| **NKFSBare** | Système de fichiers minimal (FAT ou maison) par-dessus NKStorage. | NKStorage |
| **NKConsoleRT** | **Runtime console** : assemble tout, initialise les sous-systèmes dans l'ordre, branche les backends System, et lance le dashboard Nkoung. | tous |

Détail de chaque module dans son dossier (`<Module>/README.md` + `<Module>/ROADMAP.md`).

---

## 4. Séquence de démarrage (vision)

```
1. NKBoot       le firmware/bootloader donne la main → init CPU, pile, BSS/data
2. NKArch       configure le mode CPU, les tables de descripteurs
3. NKSerial     on ouvre l'UART → on peut enfin logger « Hello, bare metal »
4. NKInterrupt  table des vecteurs + handlers d'exception
5. NKPMM        on cartographie la RAM physique → on donne une grande zone à NKMemory
   → à partir d'ici, Foundation (NKMemory/NKContainers/NKMath) est OPÉRATIONNEL
6. NKTimerHW    base de temps + ticks
7. NKDriver     découverte des périphériques
8. NKDisplay    framebuffer prêt → NKCanvas (software) peut dessiner
9. NKInput      manette/boutons → NKEvent reçoit des events
10. NKStorage + NKFSBare   accès aux fichiers (assets des jeux)
11. NKAudioHW   son
12. NKScheduler démarre les tâches
13. NKConsoleRT branche les backends System, lance Nkoung (dashboard + jeux)
```

Le premier jalon visible (« hello world » de la console) : étapes 1→8, afficher un rectangle
de couleur dans le framebuffer via NKCanvas software. Tout le reste se construit autour.

---

## 5. Philosophie et conventions

- **From scratch, incrémental.** Chaque module commence par le strict minimum qui marche, et
  grandit. On préfère un système simple qui tourne à un système complet qui ne boote pas.
- **Zéro Linux, zéro dépendance externe.** Uniquement Nkentseu + son kernel. Ce qui manque,
  on l'écrit.
- **Freestanding.** Pas de libc/libstdc++. On s'appuie sur la Foundation zéro-STL et, si
  besoin, on fournit le peu de runtime C requis (memcpy, etc. — déjà dans NKMemory).
- **Conventions Nkentseu.** Préfixe `NK`, `NkPascalCase`, namespace par module (probable :
  `nkentseu::bare`). Chaque module : `src/`, `README.md`, `ROADMAP.md`.
- **Une cible d'apprentissage à la fois.** On vise une architecture (probablement **ARM64**
  pour une console embarquée, ou **x86_64** sous QEMU pour itérer vite au début), puis on
  généralise via `NKArch`.

---

## 6. Trajectoire vers la console

1. **Boot + log** (QEMU) : NKBoot → NKArch → NKSerial. « Hello, bare metal » au port série.
2. **Mémoire** : NKPMM alimente NKMemory → la Foundation tourne sur le métal.
3. **Image à l'écran** : NKDisplay (framebuffer) + NKCanvas **software** → un rectangle, puis
   du texte (NKUI), puis une scène 2D animée (comme les démos NKCanvas).
4. **Interactivité** : NKInput → NKEvent → on déplace quelque chose à la manette.
5. **Système** : NKTimerHW (temps), NKStorage + NKFSBare (assets), NKAudioHW (son).
6. **Multitâche** : NKScheduler.
7. **Console** : NKConsoleRT branche les backends System et lance **Nkoung** (le dashboard +
   les jeux) sur le métal. À ce stade, on a une console maison qui boote sur nos jeux.
8. **Matériel réel** : passer de QEMU à la vraie carte (nouveau backend `NKArch`/`NKDriver`),
   puis généraliser aux autres équipements (nouveaux backends par appareil).

---

[Modules](README.md) · [Roadmap globale](ROADMAP.md)
