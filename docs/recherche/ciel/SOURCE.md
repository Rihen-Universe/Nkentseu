# Ciel — sources et provenance

> Fiche prescrite par `docs/SOURCES_TIERCES.md`. Dossier de collecte local de
> Rihen : `C:\Users\Rihen\Documents\revu` (hors dépôt).

## Exploité

- **Hosek & Wilkie, « An Analytic Model for Full Spectral Sky-Dome Radiance »,
  SIGGRAPH 2012** — préprint + supplément dans le dossier de collecte.
  Distribution source officielle **1.4a** (BSD 3-clauses). Tables RGB copiées
  verbatim dans le moteur (SHA-256 vérifiée), cuisson adaptée de
  `ArHosekSkyModel.c`. → entrée « Hosek-Wilkie (mesuré) » du combo Modèle.
  Attribution : `THIRD_PARTY_LICENSES.md`.
- **Preetham et al. 1999** — implémenté depuis l'article (aucune obligation).
- **Diffusion Rayleigh + Mie** — écrite depuis la physique.

## Repéré pour plus tard

- **Modèle de Prague** (`pragueskymodel-master`) — successeur de Hosek par la
  même équipe : géré jusqu'à **−4,2° sous l'horizon**, donc les couchants
  *mesurés*. Réserve : son jeu de données est volumineux et chargé à part.
  ⚠ le LICENSE trouvé en premier dans ce dépôt est celui d'une dépendance
  embarquée (Dear ImGui) — vérifier la licence du modèle lui-même avant usage.
- **« Adding a Solar Radiance Function... » + `sccg_2013_alien_sun`** — soleils
  d'autres températures/spectres (« Alien World », même distribution 1.4a BSD) :
  directement utile aux films de science-fiction.
- **SkyGAN (CGF/EGSR 2025, dépôt BSD 3-clauses)** — ciels HDR nuageux générés ;
  exploitation réaliste au niveau de l'article (le dépôt exige une pile ML).
- **`vevoda_2022_infrared_sky`** — ciel infrarouge.
- **`clear-sky-models-master`** (Bruneton) — banc de comparaison de nombreux
  modèles de ciel : sert de référence croisée pour valider les nôtres.

## Sans licence affichée (article oui, code non)

`code/`, `Fitting-comparisons/`, `Paper-images-in-EXR-form/` — compagnons
d'articles : lecture et référence seulement.
