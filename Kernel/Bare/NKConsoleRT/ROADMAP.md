# NKConsoleRT — Roadmap

> Le chef d'orchestre qui fait de la machine une console (Phase 5).
> ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — séquence d'init (Phase 5)
- ⬜ Orchestrer l'init ordonnée : NKArch → … → NKAudioHW (cf. architecture §4).
- ⬜ Vérifier l'état de chaque sous-système ; panique claire si l'un manque.
- ⬜ Bannière de boot complète (modules OK, mémoire, résolution).

## Jalon 2 — branchement des backends System
- ⬜ Backend Bare de **NKWindow** ← NKDisplay.
- ⬜ Backend Bare de **NKEvent** ← NKInput.
- ⬜ Backend Bare de **NKFileSystem** ← NKFSBare.
- ⬜ Backend Bare de **NKTime** ← NKTimerHW.
- 🎯 Le moteur tourne sur le métal sans modification (Runtime opérationnel).

## Jalon 3 — lancer la console
- ⬜ Démarrer les tâches (NKScheduler) : boucle moteur, audio, fond.
- ⬜ Lancer **Nkoung** (dashboard + jeux).
- 🎯 **Jalon Phase 5** : la console boote directement sur le menu Nkoung et ses jeux.

## Jalon 4 — exploitation
- ⬜ Panique propre (écran d'erreur + dump série).
- ⬜ Redémarrage / extinction (via NKPower si ajouté).
- ⬜ Mode maintenance / diagnostic.

## Plus tard
- ⬜ Mise à jour système (depuis le stockage).
- ⬜ Profils utilisateur / sauvegardes.
- ⬜ Généralisation à d'autres équipements (même runtime, autres backends).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
