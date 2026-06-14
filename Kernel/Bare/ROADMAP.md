# Bare — Roadmap globale

> Feuille de route de la couche bas niveau. Esprit : **from scratch, incrémental, un jalon
> observable par phase**. On préfère un système simple qui boote à un système complet qui ne
> démarre pas. Cible d'itération initiale : **x86_64 sous QEMU** (rapide à tester), puis
> portage **ARM64** (console) via `NKArch`.

Légende : ⬜ à faire · 🟡 en cours · ✅ fait.

---

## Phase 0 — Mise en place (avant tout code)

- ⬜ Décider l'architecture d'amorçage (x86_64/QEMU recommandé pour commencer).
- ⬜ Toolchain freestanding (cross-compilateur, pas de libc) + intégration Jenga (`.jenga` par module).
- ⬜ Script de boot QEMU + format d'image (ELF multiboot ou équivalent).
- ⬜ Linker script (placement des sections, point d'entrée).

## Phase 1 — « Hello, bare metal » (boot + log)

**Modules : NKArch, NKBoot, NKSerial.**
- ⬜ NKBoot : point d'entrée, init pile + sections BSS/data, saut vers le code C++.
- ⬜ NKArch : primitives CPU minimales (halt, désactiver/activer IRQ, accès registres).
- ⬜ NKSerial : sortie UART → afficher un message au port série depuis QEMU.
- 🎯 **Jalon : la machine boote et écrit « Hello, bare metal » sur le série.**

## Phase 2 — Mémoire + temps (Foundation opérationnelle)

**Modules : NKInterrupt, NKPMM, NKTimerHW.**
- ⬜ NKInterrupt : table des vecteurs, handlers d'exception (fautes CPU → log lisible).
- ⬜ NKPMM : cartographie de la RAM physique + pagination ; exposer une grande zone à NKMemory.
- ⬜ NKTimerHW : timer périodique + base de temps.
- 🎯 **Jalon : NKMemory/NKContainers/NKMath tournent sur le métal ; un tick d'horloge avance.**

## Phase 3 — Image + entrées (le visuel)

**Modules : NKDriver, NKDisplay, NKInput.**
- ⬜ NKDriver : modèle de pilote + enregistrement + découverte de périphériques.
- ⬜ NKDisplay : framebuffer accessible ; effacer l'écran, dessiner un pixel/rectangle.
- ⬜ Brancher le **renderer logiciel NKCanvas** sur le framebuffer.
- ⬜ NKInput : lire la manette/les boutons → produire des events.
- 🎯 **Jalon : une scène 2D NKCanvas animée à l'écran, qu'on pilote à la manette.**

## Phase 4 — Système de fichiers + son

**Modules : NKStorage, NKFSBare, NKAudioHW.**
- ⬜ NKStorage : lecture/écriture de blocs (SD/flash).
- ⬜ NKFSBare : monter un volume, ouvrir/lire un fichier (assets des jeux).
- ⬜ NKAudioHW : sortie DAC ; brancher NKAudio (un son qui joue).
- 🎯 **Jalon : un jeu charge ses assets depuis le stockage et joue du son.**

## Phase 5 — Multitâche + console

**Modules : NKScheduler, NKConsoleRT.**
- ⬜ NKScheduler : tâches + changement de contexte (coopératif → préemptif via le timer).
- ⬜ NKConsoleRT : init ordonnée de tous les sous-systèmes + branchement des **backends System**
  (NKWindow/NKEvent/NKFileSystem/NKTime → Bare) + lancement de **Nkoung**.
- 🎯 **Jalon : la console boote directement sur le dashboard Nkoung et ses jeux.**

## Phase 6 — Matériel réel & généralisation

- ⬜ Porter de QEMU vers la **vraie carte** (nouveau backend `NKArch` + pilotes `NKDriver`).
- ⬜ Stabiliser, mesurer, documenter.
- ⬜ **Généraliser aux autres équipements** : chaque nouvel appareil = nouveaux backends
  (display, input, audio) au-dessus du même cœur Bare.

---

[Architecture](ARCHITECTURE.md) · [Modules](README.md)
