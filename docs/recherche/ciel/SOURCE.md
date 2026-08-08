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
- **Modèle de Prague** — variante `ArPragueSkyModelGroundXYZ` (BSD 3-clauses,
  prise dans la distribution SkyGAN) : couchants **mesurés** jusqu'à −4,2°,
  intégrée + validée en scène (2026-08-03). Une correction LLP64 documentée.
  → entrée « Prague (mesuré, couchants) » du combo Modèle.
- **Soleil alien** (« Alien World » de la distribution Hosek 1.4a BSD +
  `sccg_2013_alien_sun`) — cuisson spectrale 11 bandes + corps noir, intégrée +
  validée en scène (2026-08-03). → entrée « Soleil alien (température) ».

## SkyGAN (CGF 2024) — verdict de licence : ARTICLE SEUL

Examen du dépôt `SkyGAN-public_intel` (2026-08-03) :

| Morceau | Licence | Pour Nkentseu |
|---|---|---|
| `src/stylegan3` (le réseau) | **NVIDIA Source Code License** (recherche, NON commercial) | ❌ code inintégrable |
| `sky_image_generator.*` (racine) | **aucune licence affichée** | ❌ lecture seule |
| Jeu de données (39 000 ciels HDR) | **CC-BY-NC-SA 4.0** (NON commercial) | ❌ ne jamais embarquer ni entraîner dessus pour un produit — « contact us » des auteurs pour un autre accord |
| `ArPragueSkyModelGroundXYZ/` | BSD 3-clauses | ✅ déjà intégré (c'est notre source Prague) |

Ce qu'on retient de l'**article** (librement implémentable) :
- l'architecture hybride « ciel clair analytique + couche apprise de nuages »
  **valide notre découpage** (Prague/Hosek cuits + surcouche de nuages animée) ;
- le conditionnement par position du soleil et taux de couverture nuageuse —
  déjà nos contrôles Élévation/Azimut/Couverture ;
- la marginalisation d'azimut (invariance par rotation du ciel) — utile le jour
  où on apprendra nos propres nuages.

Usage privé possible : faire tourner leur réseau **hors moteur** (Python) pour
produire des `.exr` chargés par notre chemin HDRI — mais licences NC : pas pour
un rendu commercial sans accord des auteurs.

## Repéré pour plus tard

- **`vevoda_2022_infrared_sky`** — ciel infrarouge.
## Contre-validation avec `clear-sky-models` (Bruneton, BSD 3-clauses) — 2026-08-03

Le banc implémente 8 modèles de ciel + les mesures réelles de Kider
(Cornell, 2013-05-27). Utilisé comme **référence croisée** sur nos deux
portages écrits à la main :

- **Hosek RGB (cuisson moteur)** — sonde numérique contre le code officiel
  1.4a : **1 008 comparaisons** (7 élévations × 4 thêtas × 6 gammas ×
  2 turbidités × 3 canaux), écart relatif **max 2,4 × 10⁻⁶** — le bruit
  d'arrondi float32. Verdict : CONFORME.
- **Preetham (shader)** — lecture croisée coefficient par coefficient contre
  la transcription indépendante de Bruneton (annexe 2 de Preetham 1999) :
  les 15 coefficients de Perez, les polynômes de chromaticité au zénith et
  la luminance zénithale sont **identiques chiffre à chiffre** ; même forme
  de normalisation (valeur au zénith). Verdict : CONFORME.
- Prague et le soleil alien reposent sur du code officiel **verbatim**
  (SHA-256 vérifiées) : la contre-validation porte sur nos liaisons,
  couvertes par les sondes de scène (couchant orange, 5 778 K → terrestre).

## Sans licence affichée (article oui, code non)

`code/`, `Fitting-comparisons/`, `Paper-images-in-EXR-form/` — compagnons
d'articles : lecture et référence seulement.
