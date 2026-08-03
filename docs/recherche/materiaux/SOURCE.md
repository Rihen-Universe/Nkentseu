# SOURCE — « From Words to Wood: Text-to-Procedurally Generated Wood Materials »

- **Auteurs** : Mohcen Hafidi, Alexander Wilkie (Charles University, Prague).
- **Publication** : Eurographics 2025 — *Computer Graphics Forum*, vol. 44 (2025), n° 2, 14 pages.
- **Licence de l'article** : open access, **Creative Commons Attribution-NonCommercial (CC BY-NC)** — mention explicite en pied de première page.
- **Fichier local** : `C:\Users\Rihen\Documents\revu\Hafidi2025_EG2025_From_words_to_wood.pdf` (lu intégralement le 2026-08-03).

## Ce que propose l'article

Deux contributions couplées : un **modèle de bois procédural (PGWM)** et une **interface texte (TUI)** qui le pilote.

Le PGWM est un modèle **volumétrique 3D** : chaque point P(x,y,z) de l'espace reçoit une valeur, évaluée **par pixel dans un programme GPU**. Toutes les équations sont données dans le papier :
- **Cernes de croissance** : fonction d'onde en **dent de scie** sur la distance radiale, découpée en segments de longueurs S = {s1..sn} (un segment = un cerne), avec un facteur d'adoucissement *e* qui règle la transition bois de printemps → bois d'été (éq. 1–6). Masques dérivés pour **duramen** (cœur sombre), **aubier** et **moelle** (éq. 4–5).
- **Vaisseaux/pores** : bruit de **Voronoï** dont le diamètre est piloté par des **fonctions linéaires par morceaux** selon la porosité (à pores en anneau, semi-anneau, diffuse — Table 1 donne les points de contrôle).
- **Rayons ligneux** : couches de **cylindres elliptiques** partant de la moelle vers l'écorce (éq. 7–8), répétées par modulo sur Z ; masqués hors des cernes et des vaisseaux par les valeurs de distance.
- **Nœuds** : **cônes courbés** (fonction de courbure appliquée à la distance radiale, éq. 9), fusionnés avec les cernes via une **courbe de Bézier à 3 points** donnée explicitement (éq. 13–14) ; assombrissement aléatoire des nœuds.
- **Distorsion** : (a) **points d'influence** répulsifs qui plient le grain (ajustables en position/force/rayon/nombre, permettent de coller à une photo réelle) ; (b) **« brushiness »** : bruits de Perlin imbriqués étirés en Z (haute fréquence) pour l'aspect brossé.
- **Sorties** : les valeurs de distance servent de **carte de hauteur** → **normales** par dérivées partielles (éq. 10–12) ; **rugosité** mappée sur [0.3, 0.7] (bois de printemps plus rugueux que bois d'été, valeurs moyennes tirées d'études citées) ; couleur **RGBA**. Le tout est **cuisable en textures PBR à n'importe quelle résolution** pour le temps réel.

La TUI : un modèle **NER spaCy** (`en_core_web_lg` affiné ~51 min sur **500 phrases annotées** maison) reconnaît intention/paramètre/valeur (« raise the vessels' scale by 12% ») ; un script Python à branchements met à jour un **JSON de paramètres** ; un **jeu de configurations prédéfinies par essence** (JSON, paramètres seuls, pas d'images) sert de point de départ aux novices. Environ 1 image par instruction (pas temps réel en édition), temps réel une fois les textures cuites.

## Ce qu'on peut en tirer pour Nkentseu

Le cœur exploitable pour le chantier **Matériaux** de NK3DModeler, c'est le PGWM comme **générateur de textures PBR de bois** : on évalue le modèle dans un shader (ou un passe de calcul) hors écran et on **cuit** le résultat en triplet **albedo / normal / ORM** — exactement le format de nos matériaux :
- **Albedo** : la couleur RGBA du modèle (duramen/aubier, cernes, vaisseaux, rayons, nœuds).
- **Normal** : dérivées partielles de la carte de hauteur (éq. 10–12) — trivial dans notre pipeline, c'est un simple filtre sur la hauteur cuite.
- **ORM** : R = la rugosité du papier (mappage [0.3, 0.7] earlywood/latewood) ; M = 0 (le bois n'est pas métallique) ; O (occlusion) n'est pas traité par l'article — mettre 1.0 ou dériver une AO faible de la hauteur.

L'**aperçu hors écran** existant du chantier Matériaux est précisément l'endroit où brancher la cuisson : même mécanique (rendu offscreen → texture), il suffit d'y ajouter un mode « génération procédurale » avant l'application au matériau. Les **emplacements multiples par model** ne sont pas concernés directement : le générateur produit un jeu de textures, qui s'affecte ensuite à un emplacement comme n'importe quelle texture utilisateur. Bonus propre au modèle volumétrique : sur un maillage avec coordonnées 3D objet, le bois reste **cohérent aux arêtes** (vrai bois massif), ce qu'aucune texture 2D ne donne.

**Implémentable depuis l'article seul** : la totalité du PGWM — cernes (dent de scie + easing), masques duramen/aubier/moelle, vaisseaux (Voronoï + fonctions par morceaux de la Table 1), rayons (cylindres elliptiques), nœuds (cônes courbés + Bézier explicite), points d'influence, brushiness, normales, rugosité. Toutes les formules et constantes nécessaires figurent dans le papier.

**Exige des données ou modèles qu'on n'a pas** : (1) le **jeu de configurations par essence** (le JSON des auteurs n'est pas publié ni licencié — il faudrait construire nos propres presets, p. ex. 5–10 essences, à partir des sources qu'ils citent ou à l'œil) ; (2) la **TUI NLP** (les 500 phrases annotées et le modèle spaCy affiné ne sont pas fournis — il faudrait créer notre propre corpus). Pour NK3DModeler, une interface à curseurs/valeurs dans le panneau Matériaux rend la TUI **facultative** : c'est l'ergonomie du papier, pas sa substance.

## Ce qu'il faut et comment faire

Étapes ordonnées (chaque étape livre quelque chose de visible) :
1. **Shader « cernes »** : dent de scie sur la distance radiale, segments + easing, masques duramen/aubier/moelle, couleurs earlywood/latewood parametrées. Aperçu dans le panneau offscreen. — *le socle, tout le reste s'y branche.*
2. **Hauteur → normal + rugosité** : cuire la valeur de distance en carte de hauteur, dériver la normale (éq. 10–12), mapper la rugosité [0.3, 0.7]. Premier triplet albedo/normal/ORM complet cuit.
3. **Vaisseaux** : Voronoï + les trois fonctions linéaires par morceaux de la Table 1 (paramètre unique pour changer de porosité ou couper les pores).
4. **Distorsions** : brushiness (Perlin imbriqués étirés en Z), puis points d'influence (liste de points, force/rayon).
5. **Rayons** puis **nœuds** (les deux morceaux les plus techniques ; les nœuds demandent le cône courbé + la Bézier de fusion).
6. **Presets d'essences** : notre propre petit JSON de configurations (pin, noyer, chêne...), réglées à la main sur photos.
7. *(Optionnel, plus tard)* interface texte — sans intérêt immédiat pour NK3DModeler, à ignorer.

**Dépendances** : aucune bibliothèque externe — Voronoï et Perlin s'écrivent en shader (ou existent déjà dans le moteur) ; GPU requis mais c'est notre contexte normal (DX11 et consorts) ; pas de données tierces obligatoires ; spaCy/Python seulement si on faisait la TUI (on ne la fait pas). La cuisson réutilise l'aperçu hors écran existant.

**Effort honnête** : étapes 1–2 = **petit-moyen** (quelques jours de shader + branchement cuisson) ; 3–4 = **moyen** ; 5 = la partie **dure** (géométrie des cônes courbés, fusion Bézier, masquage croisé rayons/vaisseaux/cernes) ; au total un **chantier moyen à gros** si on vise le modèle complet, mais découpable : un bois crédible (cernes + duramen + brushiness + PBR cuit) est atteignable dès les étapes 1–2–4a, en **petit chantier**.

## Régime juridique

- **Article** : open access **CC BY-NC**. Sans importance pour nous côté implémentation : selon nos règles (`docs/SOURCES_TIERCES.md`, régime 1), les **idées, équations et méthodes sont libres** — on implémente depuis le papier et on cite par honnêteté. Le CC BY-NC ne contraint que la reproduction du **texte et des figures** de l'article (ne pas copier ses images dans un produit commercial).
- **Code** : les auteurs **ne publient pas de code** (aucun dépôt mentionné pour leur système ; le seul dépôt cité est celui de Larsson et al. [LIY*22], travail antérieur d'autrui, non utilisé ici). Donc **réimplémentation depuis le papier uniquement** — ce qui est de toute façon notre voie par défaut.
- **Données** : le **dataset de configurations d'essences** (JSON) et le **corpus NER de 500 phrases** ne sont **ni publiés ni licenciés** → tous droits réservés par défaut, inutilisables. On construit nos propres presets ; les valeurs de rugosité par essence peuvent se retrouver dans les sources primaires citées ([GUR14], [Dav11], base « Database of scanned wood at microscopic level »).
- **Conclusion** : régime « **article seulement** » — le plus simple. Citer Hafidi & Wilkie 2025 dans nos sources, rien à ajouter à `THIRD_PARTY_LICENSES.md` tant qu'on ne copie ni code ni données.
