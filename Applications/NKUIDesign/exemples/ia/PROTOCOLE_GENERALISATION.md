# PROTOCOLE DE GENERALISATION — le 12/12 est-il un resultat ou un ajustement ?

AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

Ecrit le 2026-09-20, **avant** que la moindre demande neuve existe.

---

## 0. CE QU'IL FAUT SAVOIR AVANT DE LIRE LE RESTE

Le 20/09, la forme `parent` a rendu **12/12** la ou la forme `enfants` rendait
**6 et 7 sur 12**, plages disjointes, sur les douze demandes de
`demandes.txt`. **Ces douze-la ont servi a ECRIRE la regle d'invite.** Elles sont
donc optimistes, et le chiffre n'est pas opposable tant qu'il n'a pas ete rejoue
sur des demandes que personne de ce chantier n'a vues.

⚠️ **Je ne peux pas administrer ce test seul : j'ai lu les reponses.** Des
demandes que j'ecrirais maintenant porteraient ce que je sais. **L'auteur des
demandes et celui de la mesure doivent etre deux personnes.**

### CE QUE LES DOUZE SONT, MESURE PLUTOT QUE SUPPOSE

    11 / 12 emploient au moins un mot de NOTRE vocabulaire
            (bouton, champ, carte, libelle, colonnes, largeur, explorateur)
     8 / 12 suivent le meme moule : « <nom d'ecran> : <enumeration des parties> »
            parties enumerees : mediane 4, de 1 a 7

**La seconde ligne compte plus que la premiere.** « Un titre, deux champs avec
libelles, un bouton » **EST** la liste des noeuds. La demande fait la moitie du
travail de structure ; c'est ce qui rend un 12/12 facile.

---

## 1. COMBIEN, ET DE QUELLES NATURES

**N = 30 demandes.** Plancher de la mesure : **1/30 = 3,3 points**. En dessous de
30, un ecart de deux demandes vaut deja 17 points et rien ne serait separable.

**Huit natures, trois demandes chacune (24), puis six libres (6).** Les natures
sont choisies pour couvrir **ce que les douze ne couvrent pas** :

| # | Nature | Pourquoi elle manque |
|---|--------|----------------------|
| 1 | **Sans enumeration** — un but, pas une liste de parties | Les 12 donnent la liste des noeuds ; c'est leur biais principal |
| 2 | **Tres petite** — un ou deux elements | La plus petite des 12 en a 1 enumere, aucune n'est triviale |
| 3 | **Tres grande** — huit zones ou plus | Les 12 plafonnent a 5 zones (d10, d11) |
| 4 | **Imbrication profonde** — une zone dans une zone dans une zone | Aucune des 12 ne descend au-dela de 2 niveaux explicites |
| 5 | **Hors bureau** — telephone, borne, ecran de vehicule, televiseur | Les 12 sont toutes des ecrans de bureau ou de navigateur |
| 6 | **Hors domaine outil** — medical, jeu, commerce, administratif, contexte local | Les 12 sont toutes des interfaces de logiciel pour developpeur ou de site |
| 7 | **Ambigue** — volontairement sous-specifiee | Les 12 sont toutes completes et ordonnees |
| 8 | **Hors de portee** — ce que l'outil ne sait pas faire | Les 12 en ont 3, declarees d'avance ; il en faut assez pour juger la QUALITE DU REFUS |

⚠️ **La nature 8 n'est pas du remplissage.** Un outil se juge aussi sur ce qu'il
refuse et sur le motif qu'il donne. Un taux calcule en jetant les demandes
impossibles mesurerait notre habilete a les choisir.

---

## 2. CE QUI EST INTERDIT A L'AUTEUR

### 2.1 Les interdits durs — ils annulent le lot

1. **Ne lire aucun document de ce chantier** : ni `demandes.txt`, ni
   `contrat_outils.txt`, ni les canaux `echanges/`, ni une reponse de modele.
2. **N'avoir touche a aucun des lots du 20/09.**
3. **Ne demander a personne « quels composants existent ».**

*Si l'un des trois est enfreint, le lot est nul et refait par un autre auteur.
Ca se declare, ca ne se rattrape pas.*

### 2.2 ⚠️ CE QUE JE N'INTERDIS PAS, ET POURQUOI C'EST DELIBERE

**Je n'interdis PAS les mots « bouton », « champ », « carte », « colonne ».**

Deux raisons, et la seconde est la vraie :

- **Les interdire rendrait les demandes artificielles.** « Bouton » est du
  francais ordinaire pour une interface ; un auteur a qui on l'interdit ecrit
  une langue que personne n'emploie, et on mesurerait cette langue-la.
- **Et surtout : donner la liste des mots interdits, C'EST ENSEIGNER LE
  VOCABULAIRE.** Un auteur a qui l'on dit « n'ecris jamais `tree_view` » vient
  d'apprendre que `tree_view` existe. *Une consigne d'ignorance qui nomme ce
  qu'il faut ignorer se detruit en s'enoncant.*

**Donc : rien de tout cela n'apparait dans la consigne remise a l'auteur.** La
contamination est **MESUREE APRES**, pas interdite avant — et elle devient une
variable de l'analyse au lieu d'un filtre.

### 2.3 ⚠️ ET SURTOUT : ON NE FILTRE PAS LES DEMANDES APRES COUP

Ecarter les demandes « mal ecrites » apres les avoir lues reviendrait a choisir
l'epreuve en connaissant la copie. **Les 30 demandes rendues sont mesurees,
toutes les 30, quelles qu'elles soient.** La seule sortie est l'annulation du lot
entier au titre du 2.1.

---

## 3. LA CONSIGNE A REMETTRE A L'AUTEUR — a recopier telle quelle

> Ecris **30 demandes** d'interface, une par ligne, en francais, comme tu les
> dirais a quelqu'un qui va te la dessiner. Une ou deux phrases chacune.
>
> Varie-les volontairement : certaines minuscules, certaines enormes ; certaines
> qui disent precisement ce qu'il faut, d'autres qui disent seulement **a quoi ca
> doit servir** ; des ecrans de telephone, de borne, de tableau de bord de
> voiture, de television ; des domaines varies — sante, jeu, commerce,
> administration, agriculture, ce que tu veux ; **trois au moins volontairement
> impossibles ou floues.**
>
> **Ne regarde aucun document de ce projet, aucun exemple existant, et ne demande
> a personne ce que l'outil sait faire.** Ecris comme si tu ne savais rien de
> lui : c'est exactement ce qu'on mesure.
>
> Numerote de `e01` a `e30`, format `e01 | ta phrase`.

*Elle ne contient ni nom de composant, ni nom de champ, ni tournure des douze.*

---

## 4. CE QUI EST MESURE, ET CONTRE QUOI

### 4.1 Le montage — DECLARE NON MESURE

**Ce protocole mesure N2 : le document se LIT.** Il ne mesure ni N3 (le document
s'ouvre en `.nkgui`) ni le rendu. *C'est ecrit ici pour que ca ne se perde pas
entre le rapport et la conclusion.*

### 4.2 Le dispositif

    les DEUX formes dans le MEME binaire   `--structure=enfants` / defaut
    les MEMES 30 demandes pour les deux    appariement
    temperature 0, meme modele, meme hote  NK_OLLAMA_TEMP=0
    catalogue BREF des deux cotes

⚠️ **Sans « le meme binaire », la comparaison porterait sur la forme ET sur tout
ce qui a change entre deux constructions.** C'est verifiable : le drapeau
regenere le contrat d'avant le 20/09 a l'octet pres (7 168).

### 4.3 Froid contre froid, et l'ordre neutralise

Une invite ne se lit **froide qu'une fois** : la paire froide est donc unique, et
c'est elle qui fait foi — *c'est le seul regime qu'un utilisateur rencontre.*

⚠️ **Le cache de prefixe d'Ollama rend l'ordre des deux formes influent.** Il est
neutralise par construction : **demande par demande, en alternant** —
`e01` forme `enfants` puis `parent` ; `e02` forme `parent` puis `enfants` ; et
ainsi de suite. Un biais d'ordre s'appliquerait alors a moitie a chacune.

Puis **deux paires CHAUDES** pour la dispersion. Tout taux publie porte son
regime, son rang de course et l'heure de chargement du runner.

### 4.4 Les deux mesures

    (1) TAUX N2      acceptees / 30, par forme
    (2) PART DU DEFAUT DE STRUCTURE
        forme `enfants` : reponses portant au moins un ORPHELIN
        forme `parent`  : reponses portant au moins une SECONDE RACINE

**La mesure (2) est celle qui attribue.** Un taux qui monte sans qu'elle baisse
veut dire que le gain vient d'ailleurs — *et qu'on ne sait pas d'ou.*

### 4.5 Les covariables notees pour chaque demande, AVANT de voir les reponses

    enumeration  la demande liste-t-elle les parties ? (oui / non)
    taille       nombre de parties nommees
    nature       1 a 8, ou « libre »
    contamination  nombre de mots de notre vocabulaire (mesure, pas filtre)

**Elles servent a une seule chose** : si l'ecart existe seulement sur les
demandes qui enumerent, `parent` n'aide que la ou la demande faisait deja le
travail — **et ce serait un resultat, pas un echec.**

---

## 5. L'ATTENDU, ET SON POUVOIR DE DIRE NON

Soient `E30` et `D30` les taux N2 de la paire FROIDE, et
`Δ = D30 − E30` en points.

**Reference tunee du 20/09** : `E12 ∈ {50 %, 58 %}`, `D12 = 100 %`, `Δ12 ≈ +46`.

| Verdict | Condition, decidee d'avance |
|---|---|
| ✅ **GENERALISE** | `Δ ≥ +20 points` **ET** part des secondes racines **< moitie** de la part d'orphelins |
| 🔴 **NE GENERALISE PAS** | `Δ ≤ +7 points` — soit 2 demandes sur 30, a peine le double du plancher |
| 🟠 **INDECIS** | entre les deux : on le DIT, on ne conclut pas, et on double N |

### ⚠️ LA GARDE ABSOLUE, qui prime sur Δ

**Si `D30 < 50 %`, on ne conclut pas au succes, meme avec un Δ enorme.** Un ecart
large entre deux formes qui echouent toutes les deux ne dit pas que la bonne
forme porte des demandes neuves ; il dit que l'autre est pire. *Un rapport qui
publierait « +30 points » sur 40 % contre 10 % aurait raison sur l'ecart et
mentirait sur l'outil.*

### CE QUI INVALIDE LA MESURE ELLE-MEME

- **Un refus du dorsal sur une demande** → le denominateur reel est publie, pas 30.
- **Un interdit du 2.1 enfreint** → lot nul, refait par un autre auteur.
- **Moins de deux paires chaudes** → pas de dispersion, donc pas de taux publiable.

### CE QUE CE PROTOCOLE NE TRANCHERA PAS

Un modele, une machine, une nuit, un jeu de 30. Il dit si le 12/12 survit a des
demandes qu'on n'a pas choisies. **Il ne dit rien** du montage, du rendu, d'un
autre modele, ni de ce que vaut l'outil pour un utilisateur reel.
