# NKSerial — Roadmap

> Le log avant l'écran (Phase 1). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — sortie minimale (Phase 1)
- ⬜ Initialiser l'UART (16550 sur QEMU x86 / PL011 sur ARM).
- ⬜ Émettre un caractère en **polling**.
- ⬜ Émettre une chaîne → afficher « Hello, bare metal ».
- 🎯 **Jalon Phase 1 atteint** : message visible au port série.

## Jalon 2 — intégration log
- ⬜ Formatage minimal (entiers, hexa, pointeurs) sans libc.
- ⬜ Brancher comme **sink de NKLogger** → tous les `NK_LOG_*` sortent sur le série.
- ⬜ Sortie de **panique** (dump d'exception lisible, appelé par NKInterrupt).

## Jalon 3 — entrée + IRQ
- ⬜ Lecture (RX) en polling → console de debug précoce.
- ⬜ Passage en mode **interruptions** une fois NKInterrupt + NKDriver prêts.
- ⬜ Tampons circulaires TX/RX.

## Plus tard
- ⬜ Plusieurs ports.
- ⬜ UART de la vraie carte (vs QEMU).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
