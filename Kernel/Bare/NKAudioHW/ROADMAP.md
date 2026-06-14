# NKAudioHW — Roadmap

> La sortie son (Phase 4). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — premier son (Phase 4)
- ⬜ Initialiser le contrôleur audio (sortie QEMU pour commencer).
- ⬜ Jouer un tampon d'échantillons simple (un bip / une onde sinus).
- 🎯 On entend un son depuis le métal.

## Jalon 2 — flux continu
- ⬜ Double buffering réarmé par IRQ (NKInterrupt).
- ⬜ Rythme stable ; silence propre en cas de sous-alimentation.
- ⬜ Fréquence / format configurables.

## Jalon 3 — intégration NKAudio
- ⬜ Brancher **NKAudio** sur ce tampon de sortie (le mixage vient du moteur).
- 🎯 Un jeu joue ses effets et sa musique.

## Plus tard
- ⬜ Latence réduite (tampons courts).
- ⬜ Contrôleur audio de la vraie carte.
- ⬜ Entrée audio (micro) si pertinent.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
