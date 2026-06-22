# NkSparkLink — ROADMAP

> ✅ fait · 🔶 partiel · ⏳ en cours · ❌ à faire · 🚫 bloqué.

## Synthèse

| Composant | Statut | Détail |
|-----------|:------:|--------|
| Fusion de sections | ❌ | `.text/.rodata/.data/.bss` |
| Relocations & symboles | ❌ | Résolution inter-fonctions |
| Layout mémoire | ❌ | Placement flash/RAM par cible |
| Sortie Intel HEX | ❌ | Format prioritaire |
| Sortie BIN | ❌ | Image brute |
| Sortie ELF | ❌ | Debug / objdump / gdb |

## Livré
- (rien — scaffold)

## En cours
- (rien)

## À venir
- ❌ Layout mémoire paramétrable (memory map RV32I QEMU `virt` pour M0).
- ❌ Émission **Intel HEX** (premier format).
- ❌ Relocations minimales (appels, adresses globales).
- ❌ ELF pour debug ultérieur.

## Bugs
- (aucun)

## Dépendances
- NkSparkCore, NkSparkGen, NkSparkHAL, NkSparkRT.
