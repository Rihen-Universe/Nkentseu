# SOURCE — Guidage de Dwivedi piloté par les données (EGSR 2025)

- **Article** : Darryl Gouder, Jiří Vorba, Marc Droske, Alexander Wilkie,
  « A Data-Driven Approach to Analytical Dwivedi Guiding »,
  *Computer Graphics Forum* 44(4), Eurographics Symposium on Rendering 2025.
- **Affiliations** : Université Charles (Prague) et Wētā FX.
- **Licence** : article en accès ouvert sous **Creative Commons Attribution (CC-BY)**
  (mention explicite en première page). Aucun code source ni jeu de données publié
  avec l'article (implémentation des auteurs dans Mitsuba, non distribuée).
- **Fichier local** : `C:\Users\Rihen\Documents\revu\EGSR2025___A_Data_Driven_Approach_to_Dwivedi_Guiding.pdf`
- **Date de récupération / lecture** : 2026-08-03.

## Ce que propose l'article

L'article s'attaque au coût du **subsurface scattering en path tracing** : simuler
la diffusion sous la surface (peau, lait, marbre) demande de longues marches
aléatoires dans le volume, très bruitées. Deux familles de réduction de variance
existaient, toutes deux issues de la théorie « zéro-variance » : l'échantillonnage
analytique de **Dwivedi** (léger — le volume est assimilé à une dalle homogène
semi-infinie, et une « normale de dalle » oriente directions et distances de vol
libre vers la sortie — mais aveugle à l'éclairage réel), et le **path guiding
volumétrique** (données apprises dans tout le volume, précis mais lourd à
entraîner et à cacher en 3D).

L'idée centrale : garder les noyaux analytiques de Dwivedi, mais **choisir la
normale de dalle d'après le champ lumineux appris uniquement à la frontière** du
volume (les caches de surface path guiding, déjà répandus en production). La
frontière est partitionnée par une grille creuse ; pendant un entraînement
forward, chaque région accumule irradiance moyenne, normale moyenne et point
moyen (région « illuminée » à partir de 32 échantillons). Quand un chemin entre
dans le volume, on tire une région illuminée selon une CDF dont les poids
combinent le **profil de diffusion de Burley** R_d(t), la transmittance Tr(t) et
un cutoff d'orientation des normales (éq. 7-8) — Burley ne sert qu'à
l'échantillonnage, l'estimateur reste **non biaisé**. Deux variantes : **BONO**
(la normale moyenne de la région tirée devient la normale de dalle, fixe pour
toute la marche) et **BILL** (normale tirée d'une distribution directionnelle
vMF d'illumination incidente). La PDF des normales échantillonnées est évaluée
par un estimateur SMIS à un échantillon, qui se réduit à la PDF de Dwivedi
conditionnée sur la seule normale tirée. Résultats (Mitsuba, scènes type
production : Buddha, buste, orc en éclairage de plateau) : BONO et BILL battent
Classical, Dwivedi-PoE, Dwivedi-II et parfois le volume path guiding, avec un
surcoût mémoire quasi nul (frontière seulement, ~2 % d'une grille 40³ occupés).
BONO est le plus robuste et le plus simple ; la méthode fonctionne mieux **sans**
combinaison MIS avec Classical. Limites : milieux **homogènes, isotropes,
bornés** uniquement ; hétérogène, fonctions de phase anisotropes, frontières
réfractives et Russian Roulette guidée sont laissés en travaux futurs.

## Ce qu'on peut en tirer pour Nkentseu

**Temps réel : rien de directement exploitable.** C'est une technique de
réduction de variance pour l'intégration Monte Carlo de marches aléatoires
volumiques. Le terme subsurface actuel du PBR (wrap lighting via
`uObj.subsurface` dans `Resources/NKRenderer/Shaders/PBR/NkSL/pbr.frag.nksl`)
et le shader Skin dédié (`Resources/NKRenderer/Shaders/Skin/`) ne tracent aucun
chemin : il n'y a ni variance à réduire, ni noyau de Dwivedi à guider. Aucune
retombée sur la rasterisation.

**Retombée indirecte à court terme (modeste)** : l'article s'appuie sur le
**profil de diffusion de Burley** (Christensen-Burley, réf. [Chr15] de
l'article) et valide qu'il capture bien la forme du transport volumique. C'est
ce même profil qui fonde le SSS écran (separable / diffusion screen-space)
utilisé partout en temps réel. Si on veut faire progresser le shader Skin
au-delà du wrap lighting, la piste temps réel n'est pas cet article mais
Christensen-Burley 2015 — l'article Gouder et al. sert alors seulement de
confirmation que ce profil est un bon modèle du transport.

**Futur path tracer : cible naturelle et bien adaptée.** Le jour où Nkentseu a
un path tracer (offline ou preview progressive GPU) avec du SSS en marche
aléatoire, cette méthode est un excellent candidat : surcoût mémoire négligeable
(caches à la frontière seulement, pas dans le volume 3D), s'ajoute par-dessus un
surface path guiding existant ou même sans (BONO n'a pas besoin de
distributions directionnelles), particulièrement efficace pour la **peau** —
homogène et quasi isotrope, exactement le domaine de validité revendiqué. La
variante BONO est celle à retenir (plus simple, plus robuste, pas de fitting
vMF). À classer « futur » dans la roadmap rendu : dépend entièrement de
l'existence d'un path tracer volumétrique.

## Ce qu'il faut et comment faire

Chaîne de dépendances, dans l'ordre :

1. **Path tracer avec marche aléatoire volumique** (préalable absolu, hors du
   périmètre de l'article) : intégrateur Monte Carlo, échantillonnage Classical
   (distance ∝ transmittance, direction ∝ fonction de phase), next-event
   estimation à la frontière. **Effort : gros** — c'est un sous-système entier
   du moteur, pas une étape de cet article.
2. **Noyaux de Dwivedi analytiques (PoE)** : longueur de diffusion ν₀
   (approximation de d'Eon, éq. 6, depuis l'albédo de diffusion simple α et le
   libre parcours moyen), tirage de direction (éq. 4-5), étirement des
   distances de vol libre via σt' = σt·(1 − cos θ/ν₀). Quelques centaines de
   lignes, purement analytique. **Effort : petit** (une fois le point 1 acquis).
3. **Cache de frontière** : grille creuse (hachage de voxels) restreinte à la
   frontière du volume ; passe d'entraînement forward qui accumule par région
   irradiance moyenne, normale moyenne, point moyen ; seuil de 32 échantillons
   pour marquer une région « illuminée ». **Effort : moyen**.
4. **Construction des CDF** : profil de Burley R_d(t) + transmittance Tr(t)
   entre points moyens de régions, cutoff d'orientation (cos π/4), mise en
   cache des CDF par région (coût O(n²) amorti), cas des régions non
   illuminées. **Effort : petit/moyen**.
5. **BONO** : à l'entrée du volume, tirer une région via la CDF, prendre sa
   normale moyenne comme normale de dalle pour toute la marche ; PDF via la
   simplification SMIS (PDF de Dwivedi conditionnée sur la normale tirée) ;
   **ne pas** combiner avec Classical par MIS. **Effort : petit** une fois 2-4
   en place.
6. **BILL (optionnel, à ne pas faire d'abord)** : mixtures vMF ajustées sur
   l'illumination incidente par région, tirage de la normale dans la mixture.
   N'apporte un gain que dans des cas de rétro-éclairage précis ; le fitting
   vMF est le plus gros morceau. **Effort : moyen**, gain marginal.

Total pour le guidage lui-même (points 2-5) : **moyen**. Le vrai coût est le
préalable (point 1). Aucune dépendance externe obligatoire : tout est
réimplémentable depuis les équations du papier ; pas de table de données à
récupérer.

## Régime juridique

- **Article : régime 1 de `docs/SOURCES_TIERCES.md` (libre)**, renforcé par la
  licence **CC-BY** de l'open access : la méthode, les équations (4-8) et le
  pseudo-déroulé sont librement réimplémentables. On cite les auteurs par
  honnêteté intellectuelle (et l'attribution CC-BY couvre toute reprise de
  texte ou de figure, à éviter de toute façon dans le code).
- **Code : aucun code publié** avec l'article. L'implémentation des auteurs vit
  dans Mitsuba chez eux et n'est pas distribuée ; le renderer Mitsuba 0.x
  lui-même est **GPL** — régime 4 : ne jamais lire ni adapter son code pour
  Nkentseu. Réimplémentation depuis le papier uniquement.
- **Données : aucune** table ni LUT fournie ; rien à placer dans `Resources/`.
- Conclusion : voie par défaut du projet — implémenter depuis l'article, citer
  Gouder, Vorba, Droske, Wilkie (EGSR 2025 / CGF 44(4)) dans ce dossier et dans
  les crédits du moteur le moment venu. Aucun risque viral, aucune obligation
  au-delà de la mention.
