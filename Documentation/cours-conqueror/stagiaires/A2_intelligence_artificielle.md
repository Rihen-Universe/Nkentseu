# A2 — Écrire une IA, et **prouver** qu'elle est un bon adversaire

Document de travail personnel. Le cours (`Cours_ConquerorLab.pdf`, chapitres 3
et 8) explique *comment* écrire un module d'IA ; celui-ci explique **quoi
mesurer** et **comment défendre** que votre IA est un bon adversaire.

---

## 0. Ce qu'on attend de vous, en une phrase

Pas « une IA qui gagne ». **Une IA contre laquelle un humain a envie de
rejouer.**

Une IA imbattable et une IA stupide font fuir exactement de la même façon : dans
les deux cas, ce que fait le joueur n'a pas d'importance. Votre travail est
l'espace entre les deux — et il se mesure.

> **Vous ne travaillez pas seul, et vous ne dépendez pas d'A1**
>
> Votre IA reçoit les règles par table de pointeurs. Vous démarrez dès le premier
> jour contre `ConquerorRulesV2 (interne)`, sans attendre que le moteur d'A1
> existe. Quand A1 change son code, vous ne recompilez pas.

---

## 1. Mettre en place votre système

### 1.1 Le premier jour

```
1. décompressez le kit, installez le compilateur (LISEZMOI.txt)
2. lancez ConquerorLab.exe
3. panneau Modules : vérifiez qu'il a trouvé un compilateur
4. jouez UNE partie contre AIRef, sur mini_3x3.json
```

Jouez avant de coder. Vous devez savoir ce que ça fait de **perdre** contre la
machine avant d'en écrire une.

### 1.2 L'échelle, à monter dans l'ordre

```
copier  exemples/ai/IAFacile.cpp  ->  travail/ai/mon_ia.cpp
```

| Étape | Fichier de référence | Ce que vous apprenez |
|---|---|---|
| 1 | `IAMinimale.cpp` | la forme d'un module — neuf fonctions, la plupart vides |
| 2 | `IAFacile.cpp` | **évaluer une position** : c'est là qu'est le vrai travail |
| 3 | `IANegamax.cpp` | négamax, alpha-bêta, approfondissement itératif |

> **⚠️ Ne commencez pas par l'étape 3.** `IANegamax.cpp` fait cinq cents lignes.
> Il en fait cent pour qui a d'abord écrit sa propre évaluation, et mille pour
> qui ne l'a pas fait.

### 1.3 Le cycle de travail

```
       ┌────────────────────────────────────────────────┐
       │  1. modifier UNE chose dans mon_ia.cpp         │
       │  2. sauvegarder → l'atelier recompile          │
       │  3. duel 200 parties contre la version d'avant │
       │  4. NOTER le chiffre dans mon carnet           │
       └────────────────────────────────────────────────┘
```

**Gardez vos anciennes versions.** `mon_ia_v1.cpp`, `mon_ia_v2.cpp` : elles
restent chargeables, et c'est votre seul moyen de prouver que vous avez
progressé.

---

## 2. Les données à récolter

### 2.1 Les six obligatoires

| # | Indicateur | Où le lire | Valeur saine | Ce qu'un écart signifie |
|---|---|---|---|---|
| 1 | **Coups illégaux** | Métriques, tuile rouge | **0** | vous *construisez* des coups au lieu de les *choisir*, ou il manque un `memset`. Rien d'autre ne compte tant que ce n'est pas 0. |
| 2 | **Budget dépassé** | Métriques / `NkcAIResult::elapsedMs` | **0** | vous figez l'interface. C'est un échec, pas un détail. |
| 3 | **Déterminisme** | même graine, même position → même coup | identique | sans lui, aucun bug n'est reproductible. |
| 4 | **Force contre l'aléatoire** | duel 500 parties | **> 85 %** | en dessous, votre évaluation ne mesure rien d'utile. |
| 5 | **Progression** | v(n) contre v(n−1) | **> 55 %** | si la nouvelle version ne bat pas l'ancienne, vous avez tourné en rond. |
| 6 | **Cas témoin** | votre IA contre elle-même | **50 % ± bruit** | si ce n'est pas 50 %, votre banc est cassé — pas votre IA. |

> **⚠️ Le cas témoin d'abord**
>
> Avant de croire le moindre chiffre : faites jouer votre IA **contre
> elle-même**, 200 parties. Vous devez obtenir ~50 %. Si vous obtenez 60/40,
> il y a un biais de siège ou un bug de comptage, et **toutes** vos autres
> mesures sont fausses.

### 2.2 Les cinq qui font un bon adversaire

| # | Indicateur | Comment l'obtenir | Pourquoi il compte pour le joueur |
|---|---|---|---|
| 7 | **Paliers réellement distincts** | chaque palier contre le précédent, 300 parties | Un palier doit gagner **60–75 %** contre celui du dessous. Cinq paliers identiques, c'est un menu qui ment. |
| 8 | **Temps de réponse ressenti** | `elapsedMs` médian | **150–800 ms**. En dessous, l'IA paraît ne pas réfléchir ; au-dessus, le joueur attend. |
| 9 | **Régularité** | `elapsedMs` max / médian | **< 3**. Une IA qui répond en 100 ms puis en 2 s donne l'impression d'un bug. |
| 10 | **Erreurs plausibles** | aux paliers faibles, observer les coups | Une IA faible doit jouer des coups **compréhensibles mais sous-optimaux**, pas des coups absurdes. Un débutant doit pouvoir dire « ah, il a raté ça » — pas « il joue n'importe comment ». |
| 11 | **Absence de coup fétiche** | histogramme des actions | Si votre IA joue toujours la même chose, le joueur trouve la parade et s'ennuie. |

> **✅ Le point le plus difficile, et le plus important : #10**
>
> Rendre une IA plus faible est facile — on lui donne moins de temps. Mais une IA
> à 10 ms joue des coups **aberrants**, et le joueur ne progresse pas contre du
> hasard : il n'apprend rien, il gagne sans comprendre, il s'ennuie.
>
> Une bonne IA facile joue **bien à courte vue** : elle voit le coup immédiat,
> pas la réponse. C'est exactement ce que fait un débutant humain — et c'est
> pourquoi `IAFacile.cpp` (un coup d'avance) est un bien meilleur palier
> « Facile » qu'un négamax bridé.

---

## 3. Le protocole de mesure

### 3.1 Le duel

Panneau **Métriques** : votre IA d'un côté, la référence de l'autre, 500 parties,
côtés inversés une sur deux (automatique).

| Objectif | Parties |
|---|---|
| voir si ça tourne | 20 |
| une tendance | 200 |
| une conclusion | 1 000 |

**Mesurez votre bruit de fond** : deux fois 200 parties, mêmes réglages, graines
différentes. L'écart est le seuil en dessous duquel un résultat ne veut rien
dire. Notez-le, rappelez-le dans chaque rapport.

### 3.2 La règle qui vous évitera trois jours perdus

> **Pour mesurer une IA, la RÈGLE ne doit pas bouger.**
>
> Utilisez toujours `ConquerorRulesV2 (interne)` pendant vos mesures. Le jour où
> vous comparez deux IA sur deux moteurs différents, vous ne mesurez plus rien.
>
> C'est le pendant exact de la consigne d'A1 : pour mesurer une règle, l'IA ne
> doit pas bouger.

### 3.3 Ce qu'il faut noter, systématiquement

```
Date        : 2026-08-12
IA          : mon_ia.cpp 0.4.0
Changement  : ajout du terme de mobilité (et RIEN d'autre)
Moteur      : ConquerorRulesV2 (interne)
Plateau     : hexagone_6x7.json
Adversaire  : mon_ia 0.3.0
Parties     : 500     Budget : 100 ms     Graine : 4242

Résultats   : v0.4 gagne 58.4 %   illégaux = 0   budget dépassé = 0
              elapsedMs médian 82, max 210  (ratio 2.6)
              profondeur atteinte : 3 (médiane), 5 (max)
              nœuds/s : 1.4 M
Bruit mesuré: ±3.2 points (deux runs de 200)

Conclusion  : +8.4 points, au-dessus du bruit. La mobilité aide.
              MAIS le ratio de régularité est passé de 1.8 à 2.6 : à surveiller.
```

---

## 4. Comment défendre votre IA

Quatre questions, dans cet ordre.

### 4.1 « Est-elle correcte ? »

Indicateurs #1 à #3 et #6. Sans ça, rien d'autre ne se discute.

### 4.2 « Est-elle forte ? »

Indicateurs #4 et #5. Une courbe de progression version par version vaut mieux
qu'un chiffre isolé :

| Version | Contre l'aléatoire | Contre la version précédente |
|---|---|---|
| v0.1 (hasard) | 50 % | — |
| v0.2 (glouton) | 96 % | 96 % |
| v0.3 (négamax p2) | 99 % | 71 % |
| v0.4 (+ mobilité) | 99 % | 58 % |

### 4.3 « Est-elle un bon adversaire ? »

Indicateurs #7 à #11. C'est ici qu'on distingue une IA de compétition d'une IA
de jeu.

> **✅ Le test des trois parties**
>
> Faites jouer trois parties à quelqu'un qui ne connaît pas le jeu, contre votre
> palier « Normal ». Regardez, ne l'aidez pas. Notez :
>
> - a-t-il gagné au moins une fois ?
> - a-t-il compris **pourquoi** il a perdu les autres ?
> - a-t-il demandé à rejouer ?
>
> Trois « oui » valent plus que dix points de winrate.

### 4.4 « Qu'est-ce que je n'ai pas pu vérifier ? »

**Section obligatoire.** L'atelier mesure la force, la vitesse et la régularité.
Il ne mesure **pas** le plaisir de jouer contre. Écrivez franchement ce qui reste
ouvert :

> *« Mon palier Expert gagne 92 % contre Normal. Je ne sais pas s'il est
> frustrant : il faudrait le faire jouer à cinq personnes et compter combien
> relancent une partie. »*

---

## 5. Ce qui fait fuir un joueur

| Le joueur part parce que… | Indicateur | Seuil d'alerte |
|---|---|---|
| **l'IA réfléchit trop longtemps** | temps médian (#8) | > 1 s |
| **l'IA a des à-coups** | régularité (#9) | ratio > 3 |
| **l'IA est imbattable** | force du palier bas | Facile gagne > 60 % contre un humain débutant |
| **l'IA joue n'importe quoi** | erreurs plausibles (#10) | observation directe |
| **l'IA joue toujours pareil** | coup fétiche (#11) | une action > 60 % |
| **les paliers ne changent rien** | paliers distincts (#7) | écart < 55 % entre deux paliers voisins |

> **⚠️ Le piège du budget**
>
> Il est tentant de régler la difficulté uniquement par le temps. Ça marche pour
> les paliers hauts et **pas** pour les paliers bas : à 10 ms, votre IA ne joue
> pas « moins bien », elle joue **au hasard**. Le joueur ne peut rien apprendre
> contre du hasard.
>
> Pour les paliers bas, réduisez la **profondeur** ou **appauvrissez
> l'évaluation** — pas seulement le temps.

---

## 6. Erreurs de méthode qui invalident un rapport

- **Ne pas avoir fait tourner le cas témoin** (#6). Sans lui, aucun chiffre.
- **Changer le moteur de règles entre deux mesures.** Vous mesurez le moteur.
- **Conclure sur 40 parties.**
- **Journaliser dans une boucle chaude.** `NKC_LOG_*` formate avant de savoir si
  quelqu'un écoute : dans un rollout, c'est un désastre, et le tampon de 4096
  lignes vous fera perdre justement ce que vous cherchiez. **Une trace par coup,
  jamais par nœud.**
- **Muter l'état reçu.** Le contrat l'interdit : clonez. L'atelier fait tourner
  plusieurs parties en parallèle et compte dessus.
- **Ne rapporter que les versions qui ont marché.** Une piste abandonnée pour
  une bonne raison, documentée, vaut mieux qu'un chiffre sans histoire.

---

## 7. Livrables

| Pièce | Contenu |
|---|---|
| `mon_ia.cpp` | votre IA, commentée |
| Versions intermédiaires | v1, v2, v3… — elles prouvent la progression |
| `CARNET.md` | daté, avec les pistes **abandonnées** et les erreurs |
| `RAPPORT.md` | une page, les quatre questions du §4 |
| Traces | journaux copiés qui appuient vos chiffres |

> **Le carnet pèse plus que le code.** Une erreur trouvée et racontée vaut mieux
> qu'un code propre sans histoire. L'exemple à suivre est dans le chapitre 8 du
> cours : l'IA de référence a d'abord **perdu 16–20 contre un joueur aléatoire**
> à cause d'une erreur de signe. Rien ne plantait, aucun test ne tombait. Cette
> erreur-là est plus instructive que tout le reste du fichier.

---

## 8. Aide-mémoire

```
Modules      → mon fichier est-il vu ? compilé ? l'erreur est là.
Sortie       → mes NKC_LOG_INFO. Une trace par coup, jamais par nœud.
Joueurs      → qui tient quel siège, à quel palier, avec quel budget.
Métriques    → duel 500 parties, MÊME moteur de règles des deux côtés.
mini_3x3.json→ le plateau sur lequel on débogue à l'œil.
```

> **Après avoir modifié un en-tête de `include/Conqueror/`** : si l'atelier se
> met à planter au démarrage, c'est une reconstruction incomplète. Supprimez
> `Build/Obj/` et recompilez entièrement. Constaté le 2026-08-12.
