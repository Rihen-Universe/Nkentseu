# Sources tierces : ce qu'on peut prendre, et comment

> Règles fixées avec Rihen le 2026-08-03, à l'occasion de Hosek-Wilkie.
> Elles valent pour TOUT ce qu'on exploitera : modélisation, matériaux, rendu,
> ciel atmosphérique, animation — articles comme code.

## Le dossier de références

Chaque sujet exploité reçoit son dossier sous `docs/recherche/<sujet>/`, avec un
`SOURCE.md` qui note : URL, auteurs, **licence**, date de récupération, et ce
qu'on en a tiré. Un article ou un code dont on ne sait plus d'où il vient est
inutilisable — pas techniquement, juridiquement.

Les FICHIERS DE DONNÉES destinés au moteur (tables de coefficients, LUT...)
vont dans `Resources/` avec leur en-tête de licence INTACT, jamais dans le
dossier de recherche seulement.

## Les cinq régimes

1. **Article scientifique** (équations, méthode, pseudo-code) : LIBRE. Le droit
   d'auteur protège l'expression, pas les idées. Implémenter depuis le papier
   n'oblige à rien — on cite par honnêteté intellectuelle. C'est la voie par
   défaut : Preetham et l'atmosphère Rayleigh + Mie ont été faits ainsi.

2. **Code permissif** (BSD, MIT, Apache, zlib) : lire, adapter, et même COPIER
   TEL QUEL. Une seule condition : conserver l'en-tête de copyright et lister la
   source dans `THIRD_PARTY_LICENSES.md`. Dix lignes. La licence permissive est
   un cadeau, pas un piège — il n'y a RIEN à contourner.

3. **Données sous licence permissive** (tables de coefficients, datasets) : les
   prendre TELLES QUELLES, avec la mention. **Jamais les retaper** :
   - juridiquement, retaper ne change pas les droits — les données restent
     l'œuvre de leurs auteurs ;
   - techniquement, c'est la corruption assurée : deux lectures du fichier
     Hosek-Wilkie ont rendu les mêmes valeurs avec un décalage d'indexation de
     trois positions. Dans une table où la position porte le sens, ce décalage
     produit un résultat *plausible mais faux* — le pire défaut possible,
     puisque rien ne le signale.

4. **GPL / AGPL** : NE PAS adapter dans le moteur. Ces licences sont virales :
   du code dérivé placerait Nkentseu sous GPL. Et « réécrire à notre manière »
   ne suffit PAS — une réécriture d'après lecture proche reste une œuvre
   dérivée. Pour ces sources : lire l'ARTICLE, pas le code, et réimplémenter
   depuis la méthode.

5. **Pas de licence affichée** : tous droits réservés par défaut. L'article
   oui, le code non.

## Le piège à ne pas refaire

« Voir leur code, réécrire le nôtre à notre manière, utiliser leurs données »
ne contourne aucune licence :
- la réécriture-traduction est une œuvre dérivée (le droit suit) ;
- les données SONT la chose sous licence (les retaper n'y change rien).

Pour du permissif, la manœuvre est inutile — la mention suffit et coûte dix
lignes. Pour du GPL, elle est insuffisante — on repart du papier. Dans les deux
cas, ce qui crée le risque, c'est d'effacer l'origine, jamais de la garder.
