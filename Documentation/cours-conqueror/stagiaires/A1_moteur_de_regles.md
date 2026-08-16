# A1 — Écrire un moteur de règles, et **prouver** que la règle est bonne

Document de travail personnel. Le cours (`Cours_ConquerorLab.pdf`) explique
*comment* écrire un module ; celui-ci explique **quoi mesurer** et **comment
défendre** une règle que vous proposez.

---

## 0. Ce qu'on attend de vous, en une phrase

Pas « une règle qui marche ». **Une règle dont vous pouvez montrer, chiffres à
l'appui, qu'elle rend le jeu meilleur pour un joueur** — plus intéressant à
jouer, équilibré, et qui donne envie de relancer une partie plutôt que de
désinstaller.

> **La règle de travail du projet**
>
> « Une règle n'est pas défendue par argument : elle est réfutée ou confirmée par
> simulation de masse. »
> — `REGLES_COMPLETES_v2.md`
>
> Personne ne vous demandera « pourquoi tu penses que c'est bien ». On vous
> demandera **« montre »**.

---

## 1. Mettre en place votre système

### 1.1 Le premier jour

```
1. décompressez le kit, installez le compilateur (LISEZMOI.txt)
2. lancez ConquerorLab.exe
3. Fichier > Langue si besoin
4. panneau Modules : vérifiez qu'il a trouvé un compilateur
5. jouez UNE partie humain contre IA, sur mini_3x3.json
```

Ne codez rien avant d'avoir joué. Vous ne pouvez pas améliorer ce que vous
n'avez pas ressenti.

### 1.2 Votre premier module

```
copier  exemples/rules/RegleFacile.cpp  ->  travail/rules/mes_regles.cpp
```

Changez le nom dans la macro finale — sinon vous aurez deux entrées identiques
dans le menu et vous ne saurez plus laquelle vous testez :

```cpp
NKC_REGLES(RegleFacile, "Regles de PRENOM", "0.1.0", "Votre nom")
```

Sauvegardez. L'atelier compile et charge. En cas d'erreur, la sortie du
compilateur est dans le panneau **Modules** — lisez-la, elle est en anglais mais
la première ligne suffit presque toujours.

### 1.3 Le cycle de travail

```
       ┌─────────────────────────────────────────────┐
       │  1. modifier UNE chose dans mes_regles.cpp  │
       │  2. sauvegarder → l'atelier recompile       │
       │  3. Nouvelle partie, jouer 2 minutes        │
       │  4. Métriques : lancer 500 parties IA vs IA │
       │  5. NOTER le chiffre dans mon carnet        │
       └─────────────────────────────────────────────┘
```

**Une chose à la fois.** Deux changements entre deux mesures donnent un résultat
que rien ne permet d'attribuer.

---

## 2. Les données à récolter

Voici les onze indicateurs. Les six premiers sont **obligatoires** dans tout
rapport ; les cinq derniers servent à défendre une règle contestée.

### 2.1 Les six obligatoires

| # | Indicateur | Où le lire | Valeur saine | Ce qu'un écart signifie |
|---|---|---|---|---|
| 1 | **Coups illégaux** | Métriques, tuile rouge | **0** | votre moteur accepte ce qu'il refuse (ou l'inverse). Rien d'autre n'est interprétable tant que ce n'est pas 0. |
| 2 | **Parties coupées** (`max_tours`) | Métriques, tuile rouge | **0** | des parties ne se terminent pas d'elles-mêmes. **C'est une pathologie des règles, pas un réglage à monter.** |
| 3 | **Avantage du siège 1** | Métriques, tuile « Siège 1 gagne » | 47–53 % | au-delà, celui qui commence gagne *par sa place*. C'est **la** question du palier 0. |
| 4 | **Durée médiane** | Métriques, histogramme | 25–60 coups | trop court = pas de jeu ; trop long = on s'ennuie avant la fin. |
| 5 | **Usage des actions** | Métriques, histogramme | chaque action > 5 % | une action jamais jouée est **dominée** : supprimez-la, ou rendez-la attractive. |
| 6 | **Rejeu déterministe** | Journal, empreinte | identique | sans lui, aucune mesure n'est reproductible, donc aucune n'est une preuve. |

> **⚠️ L'ordre compte**
>
> Tant que #1 et #2 ne sont pas à zéro, **ne regardez aucun autre chiffre**. Vous
> mesureriez le bruit d'un moteur cassé.

### 2.2 Les cinq qui défendent une règle

| # | Indicateur | Comment l'obtenir | Pourquoi il compte pour le joueur |
|---|---|---|---|
| 7 | **Expression du talent** | Négamax contre l'aléatoire, 500 parties | Si le fort ne gagne pas nettement (**> 85 %**), le jeu est du hasard déguisé. Un joueur qui progresse mais ne gagne pas plus **arrête**. |
| 8 | **Richesse de décision** | Barre du plateau : « coups légaux », relevé sur 20 tours | 5–25 coups. En dessous, le joueur n'a pas de choix ; au-dessus, il est paralysé et joue au hasard. |
| 9 | **Renversements** | Journal : le leader change-t-il après la mi-partie ? | Une partie décidée au tiers et qui dure encore trente coups est la **première cause d'abandon**. Visez ≥ 30 % de parties avec au moins un renversement. |
| 10 | **Moments spectaculaires** | Comptez les « CASCADE ×N » sur 10 parties | Le sommet émotionnel du jeu. Zéro cascade = jeu plat. C'est ce que le joueur raconte à quelqu'un d'autre — donc ce qui vous amène des joueurs. |
| 11 | **Absence de stratégie dominante** | Usage des actions + observation | Si un coup est joué plus de 60 % du temps, il n'y a plus de décision : il y a une recette. Le jeu meurt le jour où elle circule. |

---

## 3. Le protocole de mesure

### 3.1 Combien de parties ?

| Objectif | Parties |
|---|---|
| voir si ça tourne | 20 |
| une tendance | 200 |
| une conclusion | 1 000 |
| une décision de conception | 10 000 |

**Mesurez d'abord votre bruit de fond** : lancez deux fois 200 parties avec la
même configuration et des graines différentes. L'écart entre les deux est le
seuil en dessous duquel un résultat ne veut rien dire. Notez-le, et rappelez-le
dans chaque rapport.

### 3.2 La configuration de mesure

Bouton **IA vs IA** dans la barre du plateau : tous les sièges à l'IA, lecture
lancée. C'est l'état dans lequel l'atelier répond à une question.

> **✅ Toujours la même IA des deux côtés**
>
> Pour mesurer une **règle**, les deux camps doivent jouer pareil. Sinon vous
> mesurez un mélange de « ma règle » et « cette IA joue mieux », et vous ne
> saurez jamais lequel a bougé.

### 3.3 L'inversion des côtés

L'atelier inverse les sièges une partie sur deux, avec la **même graine**. C'est
cet appariement qui sépare « cette stratégie gagne » de « le premier joueur est
avantagé ». Vous n'avez rien à faire — mais sachez que ça n'a lieu **qu'à deux
joueurs**.

### 3.4 Ce qu'il faut noter, systématiquement

```
Date        : 2026-08-12
Règle       : mes_regles.cpp 0.3.0
Changement  : portée de duplication 1 -> 2 (et RIEN d'autre)
Plateau     : hexagone_6x7.json
IA          : IANegamax, palier Normal, budget 100 ms, aux deux sièges
Parties     : 1000
Graine      : 4242

Résultats   : siège 1 = 51.3 %   coupées = 0   illégaux = 0
              durée médiane = 34 coups
              actions : dupliquer 78 %, fusionner 19 %, passer 3 %
Bruit mesuré: ±2.1 points (deux runs de 200)

Conclusion  : l'écart de siège (1.3 pt) est SOUS le bruit. La portée 2 ne
              déséquilibre pas. En revanche la durée médiane passe de 34 à 21 :
              les parties sont un tiers plus courtes.
```

Le bouton **Copier** du panneau Journal vous donne une trace rejouable
(graine + modules + coups + empreinte) à coller dans votre carnet.

---

## 4. Comment défendre une règle

Votre rapport tient en une page et répond à quatre questions, dans cet ordre.

### 4.1 « Quel problème cette règle résout-elle ? »

Une règle qui ne résout rien est une règle en trop. Partez d'un **constat
mesuré**, pas d'une idée :

> *« Sur 1000 parties, 34 % se terminent par blocage avant le tour 15. Le joueur
> perd sans avoir eu le temps de comprendre pourquoi. »*

### 4.2 « Est-ce que le jeu reste équilibré ? »

Indicateurs #1 à #6. Un tableau avant/après suffit :

| | Avant | Après | Bruit |
|---|---|---|---|
| Siège 1 gagne | 58,2 % | 50,9 % | ±2,1 |
| Parties coupées | 0 | 0 | — |
| Durée médiane | 12 | 31 | — |

### 4.3 « Est-ce que ça reste intéressant à jouer ? »

Indicateurs #7 à #11. C'est ici qu'on juge si la règle **ajoute une décision**
ou seulement une complication.

> **✅ Le test d'élégance**
>
> Comptez ce que la règle **coûte** — lignes de texte pour l'expliquer à un
> joueur — et ce qu'elle **rapporte** — décisions nouvelles, moments
> spectaculaires. Une règle qui demande trois phrases pour changer un chiffre de
> 2 % est une mauvaise affaire.

### 4.4 « Qu'est-ce que je n'ai pas pu vérifier ? »

**Cette section est obligatoire, et elle vous protège.**

L'atelier mesure des faits. Il ne mesure **pas** le plaisir. Il sait dire qu'une
règle est cassée, déséquilibrée ou dominée ; il ne sait pas dire qu'elle est
amusante.

Écrivez donc franchement ce qui reste ouvert :

> *« La cascade est plus fréquente (+40 %). Je suppose que c'est agréable, mais
> je ne l'ai pas vérifié : il faudrait faire jouer cinq personnes et regarder
> leur visage. »*

---

## 5. Ce qui fait fuir un joueur

Les cinq motifs d'abandon les plus courants, et l'indicateur qui les détecte
**avant** que ça n'arrive.

| Le joueur part parce que… | Indicateur | Seuil d'alerte |
|---|---|---|
| **il perd sans comprendre** | richesse de décision (#8) trop faible en début de partie | < 3 coups légaux au tour 2 |
| **la partie est jouée d'avance** | renversements (#9) | < 15 % de parties avec renversement |
| **il ne progresse pas** | expression du talent (#7) | fort vs faible < 70 % |
| **c'est toujours pareil** | stratégie dominante (#11) | une action > 60 % |
| **c'est trop long** | durée (#4) | médiane > 80 coups |

> **⚠️ Le pire de tous : la partie déjà perdue qui continue**
>
> Un joueur qui sait qu'il a perdu et doit encore jouer vingt coups ne revient
> pas. Si vos parties se décident au tiers, **ce n'est pas un problème
> d'équilibre, c'est un problème de rythme** — et la réponse est souvent une
> condition de fin anticipée, pas un rééquilibrage.

---

## 6. Erreurs de méthode qui invalident un rapport

- **Deux changements entre deux mesures.** Le résultat n'est attribuable à rien.
- **Conclure sur 40 parties.** Un écart de 10 points sur 40 parties est du bruit.
- **Oublier de noter la graine et les réglages.** Un chiffre sans son contexte
  n'est pas une mesure, c'est une anecdote.
- **Comparer deux campagnes avec des IA différentes.** Vous mesurez les IA.
- **Croire un chiffre sans avoir fait tourner le cas témoin.** Deux IA
  identiques doivent donner 50 %. Si ce n'est pas le cas, votre banc est cassé,
  pas le jeu.
- **Ne rapporter que ce qui va dans votre sens.** Un résultat qui ne bouge pas
  est un résultat : il prouve que le paramètre n'est pas un levier.

---

## 7. Livrables

| Pièce | Contenu |
|---|---|
| `mes_regles.cpp` | votre moteur, commenté |
| `CARNET.md` | daté, avec les options **rejetées** et les erreurs |
| `RAPPORT.md` | une page, les quatre questions du §4 |
| Traces | les journaux copiés qui appuient vos chiffres |

> **Le carnet pèse plus que le code.** On évalue votre capacité à défendre une
> décision, pas votre vitesse de frappe. Une règle abandonnée pour une bonne
> raison, documentée, vaut mieux qu'une règle livrée sans justification.

---

## 8. Aide-mémoire

```
Modules      → mon fichier est-il vu ? compilé ? l'erreur est là.
Sortie       → mes NKC_LOG_INFO. Une trace par coup, jamais par nœud.
Plateau      → barre d'état : elle dit ce que l'atelier attend.
Journal      → bouton Copier = trace rejouable pour un rapport de bug.
Métriques    → IA vs IA, 1000 parties, une variable à la fois.
mini_3x3.json→ le plateau sur lequel on débogue à l'œil.
```

> **Après avoir modifié un en-tête de `include/Conqueror/`** : si l'atelier se
> met à planter au démarrage, c'est une reconstruction incomplète. Supprimez
> `Build/Obj/` et recompilez entièrement. Constaté le 2026-08-12.
