# Critères du dépliage UV — ÉCRITS AVANT LA PREMIÈRE MESURE

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
**Écrit le 25/09/2026, avant d'avoir lancé le déplieur une seule fois sur un
objet généré.** Ce fichier existe pour qu'on ne puisse pas m'accuser — ni que je
puisse me permettre — de choisir le seuil après avoir vu le chiffre.

## Imposés par le lot (Rodolf / le plan)
1. **Recouvrement** : aire de recouvrement < **0,5 %** de l'aire UV totale.
2. **Un damier posé dessus se lit** sans étirement visible.
3. **Négatif** : un maillage sans UV fait **rougir la porte**. Jamais de texture
   grise par défaut, jamais de repli muet.

## Que je fixe, et pourquoi (le « distorsion bornée » du lot)
4. **Distorsion d'aire** — `areaMean` dans **[0,80 ; 1,25]**, `areaMax` ≤ **4,0**.
   La mesure est normalisée par le rapport global : 1 = fidèle. Un facteur 4 en
   aire vaut un facteur 2 en longueur — c'est le point où un carreau de damier
   paraît deux fois plus grand qu'un autre : visible, pas encore illisible.
5. **Distorsion d'angle** — `angleMean` ≤ **10°**, `angleMax` ≤ **45°**.
   LSCM minimise l'écart d'angle par construction ; 10° de moyenne est large,
   et 45° au pire laisse passer quelques triangles dégénérés sans les cacher.
6. **Temps** ≤ **2 000 ms** pour le maillage de Rodolf. Au-delà, le dépliage
   quitte le fil d'affichage — le précédent est écrit : l'ajustement sur image,
   12,3 s, en a été sorti pour la même raison.

## Ce que je refuse d'avance
- Déclarer un succès si le déplieur **refuse** : un refus nommé
  (`IlotNonDisque`, `CoinsSoudes`, `AreteNonManifold`…) est un résultat, pas un
  échec de mesure, et il se rapporte tel quel avec son chiffre d'Euler.
- Contourner un refus en posant des UV planaires « pour voir » : ce serait le
  repli muet que le négatif interdit.
