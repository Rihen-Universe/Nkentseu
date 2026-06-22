# NkSparkHAL — ROADMAP

> ✅ fait · 🔶 partiel · ⏳ en cours · ❌ à faire · 🚫 bloqué.

## Synthèse

| Composant | Statut | Détail |
|-----------|:------:|--------|
| Modèle périphérique/registre | ❌ | Adresses, champs, reset |
| Import CMSIS-SVD | ❌ | XML → bindings NkSpark |
| Cartes mémoire par cible | ❌ | Flash/RAM/vecteurs |
| Profil RV32I (QEMU virt) | ❌ | Cible pilote M0 |
| Profils STM32 / AVR / ESP32 | ❌ | Ajout incrémental |

## Livré
- (rien — scaffold)

## En cours
- (rien)

## À venir
- ❌ Profil minimal RV32I QEMU `virt` + UART/GPIO pour M0/M1.
- ❌ Générateur de bindings depuis SVD.
- ❌ Profils matériels supplémentaires.

## Bugs
- (aucun)

## Dépendances
- NkSparkCore ; consommé par NkSparkSema, NkSparkLink, NkSparkRT.
