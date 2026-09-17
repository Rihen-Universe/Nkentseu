// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

# ATTENDU DU JEU D'EPREUVE IA — ECRIT AVANT LA PREMIERE COURSE

> Les douze demandes sont dans `demandes.txt`, ecrites avant celui-ci et non
> modifiees depuis. Ce fichier dit ce que je PREDIS, avec la **condition** que
> chaque prediction suppose — une valeur sans sa condition est ma faute
> personnelle, elle m'a coute trois fois cette semaine.

## Les trois niveaux, et pourquoi ils echouent pour des raisons differentes

| niveau | ce qu'il mesure | ce qui le fait echouer |
|---|---|---|
| **N1** le modele rend quelque chose | code 0 et fichier de sortie non vide | modele absent, VRAM prise, invite plus longue que la fenetre de contexte, plantage |
| **N2** le document se lit | `ValidateReply` : ligne `nkuidoc`, structure chargeable, composants du registre, **rejeu identique** | le modele ecrit « a peu pres » le format, invente un composant, produit un arbre incoherent |
| **N3** le document s'ouvre | greffe + mise en page + ecriture `.nkgui` + validateur 0 erreur + monteur 0 role inconnu / 0 zone non servie | le pont `.nkuidoc -> .nkgui` ne sait pas traduire ce que le modele a produit |

## Mes predictions chiffrees

**N1 : 12/12**, CONDITION : que NKDesignLLM demarre et qu'aucune autre instance
ne tienne la carte. Si l'invite depasse la fenetre de contexte (le catalogue
complet y entre), le refus sera NOMME et N1 tombera a 0 — pas a moitie.

**N2 : entre 0 et 3 sur 12.** C'est une prediction BASSE et je l'ecris avant de
voir quoi que ce soit. Raisons :
1. le format `nkuidoc` n'existe nulle part dans les donnees d'entrainement d'un
   modele generaliste : il n'a jamais vu une seule ligne de `noeud 0` ;
2. l'invite donne la GRAMMAIRE mais **aucun exemple complet** — or un modele de
   7 milliards de parametres copie beaucoup mieux qu'il ne deduit ;
3. la generation est **gloutonne** (deterministe) : pas de seconde chance ;
4. le rejeu est un verificateur dur : une seule valeur que l'ecrivain ne sait pas
   reecrire fait diverger l'aller-retour.

**N3 : inferieur ou egal a N2**, et probablement egal : une fois le document
charge et rejoue, le pont vers `.nkgui` est du code deterministe.

## Ce qui serait le resultat le plus UTILE

Un N2 a 0 ou 1 n'est pas un echec du chantier : c'est la mesure qui dit a Rodolf
que **le modele local generaliste ne suffit pas pour ce format**, et que le choix
est entre (a) donner des exemples dans l'invite, (b) entrainer un adaptateur sur
le corpus que l'outil fabrique, (c) brancher un modele plus gros. Sans le
chiffre, les trois ressemblent a des opinions.

## Ce qui invaliderait la mesure elle-meme

- une seule demande qui reussit et onze qui n'ont pas ete lancees ;
- un jeu de demandes choisi APRES avoir vu des reponses ;
- un N2 compte sur une reponse ECRITE A LA MAIN et non par le modele ;
- un `.nkgui` declare « monte » sans que le monteur l'ait lu (mon propre piege du
  17/09 : un compteur vert et une bande noire de 197 px sur l'image).
