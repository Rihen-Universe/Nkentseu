# Licences tierces

Registre des œuvres tierces incorporées à Nkentseu, conformément aux règles de
`docs/SOURCES_TIERCES.md`. Chaque entrée note : quoi, où, d'où, sous quelle
licence, et ce que la licence exige.

---

## Modèle de ciel Hosek-Wilkie (tables de coefficients + algorithme de cuisson)

- **Œuvre** : « An Analytic Model for Full Spectral Sky-Dome Radiance »
  (SIGGRAPH 2012) — implémentation d'exemple des auteurs, version **1.4a**
  (22 février 2013).
- **Auteurs** : Lukas Hosek et Alexander Wilkie, Université Charles, Prague.
- **Licence** : **BSD 3 clauses** — usage commercial et redistribution permis ;
  obligation de conserver la mention de copyright ; interdiction d'utiliser le
  nom des auteurs pour promouvoir le produit.
- **Ce qui est incorporé** :
  - `Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Environment/ArHosekSkyModelData_RGB.h`
    — tables de coefficients RGB, **copie VERBATIM** de la distribution
    officielle (empreinte SHA-256 vérifiée à la copie :
    `ED5B2C94674CA2249ACA289178DE72821768E6B9F152A16835B944F80FE6A196`).
    L'en-tête de licence d'origine est intact dans le fichier.
  - La cuisson des coefficients (`NkHosekCookRGB`, `NkEnvironmentSystem.cpp`)
    est **adaptée** de `ArHosekSkyModel.c` de la même distribution.
- **Provenance** : distribution source officielle des auteurs
  (`HosekWilkie_SkylightModel_C_Source.1.4a`), page du projet :
  https://cgg.mff.cuni.cz/projects/SkylightModelling/
- **Texte de licence** : reproduit intégralement en tête de
  `ArHosekSkyModelData_RGB.h`.

---

## Modèle de ciel de Prague — variante sol/XYZ (code + jeu de données)

- **Œuvre** : « A Fitted Radiance and Attenuation Model for Realistic
  Atmospheres » (SIGGRAPH 2021) — variante « ground level XYZ » du 2.6.2021.
- **Auteurs** : les auteurs de l'article (équipe d'Alexander Wilkie, Université
  Charles, Prague).
- **Licence** : **BSD 3 clauses** — mêmes conditions que Hosek-Wilkie ci-dessus.
- **Ce qui est incorporé** (copies **VERBATIM**, SHA-256 vérifiées à la copie) :
  - `Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Environment/ArPragueSkyModelGroundXYZ.inl`
    — le `.c` officiel à l'octet près (seul le nom de fichier change, pour
    l'inclure dans une unité C++) — `015B82133887E8C9…` ;
  - `…/ArPragueSkyModelGroundXYZ.h` — `87A8C548031141F3…` ;
  - `…/ArPragueSkyModelGroundXYZ_LICENSE.txt` — le texte de licence d'origine ;
  - `Resources/NKRenderer/Sky/SkyModelDataset.dat` (18,4 Mo) —
    `86BF9B864A778994…`.
- **Liaison** : `NkPragueEnsure` / `NkPragueSample` dans
  `NkEnvironmentSystem.cpp` (code Nkentseu, pas une adaptation).
- **Provenance** : distribution publique accompagnant SkyGAN (dossier
  `ArPragueSkyModelGroundXYZ` avec sa licence propre).
