# Exemples `.nkgui` — écrits à la main

Jusqu'au 2026-08-23, **il n'existait aucun fichier `.nkgui` dans le dépôt.**
`--roundtrip` sur la racine rendait `0 / 0` : le corpus était **généré par le
banc**, en mémoire, et disparaissait avec lui. Un format sans un seul fichier
d'exemple est un format que personne ne peut ouvrir, lire, casser ni
comprendre — y compris son auteur.

Ces fichiers sont écrits **à la main**, et délibérément variés. C'est la règle
qui a rendu leur écriture utile :

> **Un corpus de dix fichiers du même moule mesure le moule, pas le format.**

Chacun exerce quelque chose de *différent*, et de préférence quelque chose qui
a réellement cassé pendant la bascule vers `NkArchive`.

## Comment les passer

```sh
# le corpus entier, sous-dossiers compris — depuis le 2026-08-27
NKUIDesign.exe --roundtrip=Applications/NKUIDesign/exemples
NKUIDesign.exe --valider=Applications/NKUIDesign/exemples

# ou un dossier précis, si on veut un relevé qui ne mélange pas les intentions
NKUIDesign.exe --roundtrip=Applications/NKUIDesign/exemples/valides
```

`--roundtrip` mesure *analyser → réémettre → réanalyser*. `--valider` juge les
rôles et les types. **Ce sont deux gestes différents**, et un fichier peut
réussir l'un en échouant l'autre.

⚠️ **Le balayage descend dans les sous-dossiers.** Jusqu'au 2026-08-27 il ne
descendait pas : viser la racine — *le geste que ce README invitait à faire* —
rendait « TOTAL : 0 erreur(s) » pour quatorze fichiers dont **pas un n'avait été
ouvert**. Un message d'échec avait été ajouté le 23/08 ; il expliquait comment
contourner l'outil, dossier par dossier. **Un diagnostic qui remplace une
capacité est une dette, pas une parade.**

⚠️ **Et viser la racine rend `1`, pas `0` — c'est correct.** Le corpus
*contient* cinq fichiers faits pour être refusés. Relevé du 2026-08-27 sur la
racine : **20 fichiers lus, 15 / 20 à l'octet** (les 5 manquants sont
`refuses_a_la_lecture/`), **11 erreurs** et 1 avertissement voulu. Pour un
relevé qui doit être vert, viser `valides/`.

---

## `valides/` — doivent passer les deux

Relevé du 2026-08-27 (après la fermeture de `appearance(État)`) : **10 / 10
octet pour octet**, **0 erreur**.

| fichier | ce qu'il exerce | attendu |
|---|---|---|
| `01_panneau_reglages.nkgui` | un document ordinaire et réaliste : huit rôles, commentaires en tête de membre, imbrication | 0 erreur |
| `02_bloc_sur_une_ligne.nkgui` | bloc **plein sur une seule ligne**, membre sur la ligne du `{`, accolade fermante sur la ligne du dernier membre, bloc vide | 0 erreur |
| `03_virgule_vecteur_couleur.nkgui` | la **virgule qui sépare deux membres**, les `vec2`, les couleurs, les listes | 0 erreur |
| `04_echappements_utf8.nkgui` | les **trois** échappements du document 2 §2 (`\n`, `\"`, `\\`) et de l'UTF-8 réel — grec, cyrillique, japonais, arabe, symboles | 0 erreur |
| `05_animation_comportement.nkgui` | trois sections sur les huit connues, chemins pointés `n1.value` et `Enum.Haut` | 0 erreur |
| `06_version_0_2_alias.nkgui` | un fichier d'une **version antérieure** avec un rôle renommé depuis | **1 avertissement voulu** : `W-ROLE-ALIAS` |
| `07_apparence.nkgui` | l'**apparence** du document 9 §3 : `appearance`, la typographie, `fill`, `shadow` nommé, et trois surcharges d'état | 0 erreur |
| `08_indentation_mixte.nkgui` | une **tranche brute indentée autrement** que le bloc qui la précède | 0 erreur |
| `09_indentation_a_la_main.nkgui` | **sept largeurs d'indentation différentes** dans un seul document — 3 espaces, un tabulateur, 6, 9, 0, une accolade fermante à 5, une tranche brute à 7 | 0 erreur |
| `10_etats_apparence.nkgui` | les **six états**, **les deux graphies du repos** (`appearance` nu / `appearance(Normal)`) et **les deux focus** — anneau sur `FocusVisible` pour le bouton, sur `Focus` pour le champ de saisie | **1 avertissement voulu** : `W-FOCUS-ANNEAU` |

Les deux avertissements du dossier sont **le comportement correct** :

- `06` — un vieux fichier doit rester ouvrable, sinon on ne peut pas le mettre à
  jour ;
- `10` — un **champ de saisie** doit se voir focalisé quelle que soit l'origine,
  contrairement à un bouton. `W-FOCUS-ANNEAU` **pose la question, il ne tranche
  pas** : le fichier l'assume en connaissance de cause, et le dit dans son
  en-tête. Un avertissement qu'on ne peut pas assumer serait un refus déguisé —
  c'est ce que mesure le contrôle 26l.

⚠️ **Les trois derniers arrivent de `limites/`, et c'est le dossier qui dit
quoi.** `07` y était parce que la validation rendait trois `E-ROLE-INCONNU`
dessus ; `08` et `09` parce que l'écrivain **normalisait leur indentation**.
Aucun des trois n'a changé d'un octet : c'est l'outil qui a changé. Un fichier
ne reste dans `limites/` que tant que l'outil le traite mal — sinon le dossier
deviendrait un cimetière de défauts corrigés, et plus personne ne le lirait
comme une liste de choses à faire.

⚠️ **`09` mérite d'être lu avant d'être copié : il est laid exprès.** Sept
largeurs d'indentation dans un document de quinze lignes, ça ne ressemble à
rien de ce qu'on écrit. C'est le point. Il a été écrit le 2026-08-27 **avant**
le correctif, et il était **rouge** — premier écart à l'octet 1903. Sans lui, le
témoin aurait été vert des deux côtés et n'aurait rien prouvé :

> **Un corpus de dix fichiers du même moule mesure le moule, pas le format.**

---

## `fautifs/` — doivent être refusés, et il y a **deux façons de refuser**

La séparation en deux sous-dossiers n'est pas cosmétique. Elle porte une règle :

> **Un fichier dont la validation signale la faute doit rester LISIBLE.** Un
> éditeur qui refuse d'ouvrir le fichier dont il signale la faute rend cette
> faute incorrigible.

### `refuses_a_la_lecture/` — la couche ne sait pas les **représenter**

Le lecteur s'arrête. Il n'y a pas de document à réparer, donc rien à ouvrir.

| fichier | diagnostic exact attendu |
|---|---|
| `r1_echappement_inconnu.nkgui` | `E-PARSE ligne 3 colonne 19 : echappement inconnu dans une chaine : \z` |
| `r2_accolade_jamais_fermee.nkgui` | `E-PARSE ligne 2 colonne 1 : bloc jamais ferme : widgets` |
| `r4_entete_nkgui_manquant.nkgui` | `E-PARSE ligne 1 colonne 1 : le fichier doit commencer par` `nkgui <majeure>.<mineure>` |
| `r5_commentaire_jamais_ferme.nkgui` | `E-PARSE ligne 2 colonne 1 : commentaire de bloc jamais ferme` |
| `r7_valeur_manquante.nkgui` | `E-PARSE ligne 3 colonne 10 : propriete sans valeur : p` |

La numérotation saute `r3`, `r6`, `r8`, `r9` **exprès** : ces quatre-là ont
**changé de domicile** lors de la bascule. Un lecteur purement syntaxique les
lit — ce sont des jetons nus bien formés pour lui — et c'est désormais la
validation qui les juge. Ils sont ci-dessous.

### `signales_par_la_validation/` — le fichier se **lit**, la faute est **nommée**

Relevé : **6 / 6 octet pour octet** (donc parfaitement réparables), **6 erreurs**.

| fichier | diagnostic exact attendu |
|---|---|
| `v1_couleur_cinq_chiffres.nkgui` | `E-VALEUR ligne 3` : `'#12345' n'est aucune des formes de valeur du format` |
| `v2_virgule_finale_dans_liste.nkgui` | `E-VALEUR ligne 3` : `'["a",]'` |
| `v3_cle_de_dictionnaire_invalide.nkgui` | `E-VALEUR ligne 3` : `'{ 1 = 2 }'` |
| `v4_section_inconnue.nkgui` | `E-SECTION-INCONNUE ligne 2` : `'inconnue'` |
| `v5_valeur_bien_formee_mauvais_type.nkgui` | `E-TYPE ligne 5` : `un nombre attendu, lu une chaine` |
| `v6_role_inconnu.nkgui` | `E-ROLE-INCONNU ligne 3` : `'Mystere'` |

`v5` mérite une note : c'est **l'autre porte** vers un diagnostic de propriété.
Une valeur *mal formée* (`v1`) et une valeur *bien formée du mauvais type*
(`v5`) sortent par deux chemins différents du code. Une mutation a survécu
parce qu'un seul des deux était mesuré.

---

## `limites/` — **le dossier n'existe plus, et c'est le relevé qui l'a vidé**

Ce troisième dossier a existé du 2026-08-23 au 2026-08-27. Il portait les
fichiers **ni valides ni fautifs** : légaux, et pourtant mal traités — la faute
du côté de l'outil, pas du fichier. Les ranger avec les fautifs aurait menti sur
qui a tort ; les ranger avec les valides aurait masqué un défaut connu.

Il est vide parce que ses trois occupants sont partis, chacun le jour où l'outil
a cessé de les maltraiter :

| fichier | ce qui n'allait pas | parti le | devenu |
|---|---|---|---|
| `01_apparence_faux_positifs` | trois `E-ROLE-INCONNU` sur des constructions du document 9 §3 | 2026-08-23 | `valides/07_apparence` |
| `02_indentation_mixte` | tranche brute réémise avec la largeur déduite du document, écart à l'octet 65 | 2026-08-27 | `valides/08_indentation_mixte` |
| `03_indentation_a_la_main` | sept largeurs d'indentation normalisées, écart à l'octet 1903 | 2026-08-27 | `valides/09_indentation_a_la_main` |

**Le recréer est un geste normal**, pas un aveu : dès qu'un fichier légal est mal
traité et qu'on ne corrige pas tout de suite, il va là. Ce qui serait anormal,
c'est qu'un fichier y **reste** après correction.

Ce que `01` avait montré en passant mérite d'être gardé, parce que c'est plus
général que le défaut lui-même : `appearance(Hover)` — l'en-tête de bloc avec
parenthèses, la **limite déclarée** — ne produisait **aucune** erreur, parce
qu'il reste une tranche verbatim que la validation ne regarde pas.

> **La partie modélisée crie, la partie non modélisée se tait.** Une limite qui
> protège d'un faux positif ne protège de rien : elle cache que c'est le faux
> positif qu'il fallait traiter.

---

## Les limites déclarées, et ce que ce corpus en dit

### Levée le 2026-08-27 : `appearance(État)` est modélisé, et la liste est close

**Six états, et l'ordre EST la priorité** :
`Disabled > Pressed > Hover > FocusVisible > Focus > Normal`. Quand plusieurs
sont vrais, **un seul s'applique** — parce que *le cumul permet de produire un
rendu que personne n'a dessiné*. Deux noms de focus, parce que *CSS a essayé un
seul nom et n'a pas pu s'y tenir*.

L'en-tête de bloc avec parenthèses **n'est plus une tranche verbatim**. Son
contenu est jugé exactement comme celui d'un `appearance` nu, la liste des états
est fermée (`Normal · Hover · Pressed · Focus · Disabled`), et le chemin des
diagnostics porte l'état — sans quoi trois `appearance` sur un widget rendraient
trois diagnostics au chemin identique.

⚠️ **Deux graphies, un seul état, et l'aller-retour tient parce que le modèle ne
canonise pas.** `appearance { }` et `appearance(Normal) { }` sont synonymes ;
l'archive retient *laquelle a été écrite*. Les cumuler est signalé
(`W-ÉTAT-DOUBLE`). `10_etats_apparence.nkgui` exerce les deux.

### Les limites déclarées encore ouvertes

- ~~**`appearance(Hover)`**~~ — **fermée le 2026-08-27**, voir ci-dessus. Ce
  qu'elle a appris reste vrai et vaut plus que le défaut :
  **la partie modélisée crie, la partie non modélisée se tait.**
- **Commentaire inline** (`a /* c */ = 1`) — non conservé. **Aucun usage
  n'apparaît.** En écrivant six fichiers à la main, le commentaire est allé
  chaque fois sur sa propre ligne, jamais entre deux jetons d'une même
  construction. La limite **reste déclarée**, et il n'y a pas lieu de la fermer.
- **`items = [a, b]` reste un jeton nu** — `KindOf` dit que c'est une liste,
  personne ne peut itérer ses éléments. `03` et `v2` l'exercent ; rien n'en a
  eu besoin.

### Levée le 2026-08-27 : la normalisation de l'indentation

L'écrivain régénérait l'indentation à `profondeur × largeur déduite`. Il
conserve désormais le verbatim, et **ne génère que pour le neuf** :

> **Une ligne qui vient du fichier garde ses octets ; une ligne créée par
> l'éditeur reçoit une indentation générée.**

⚠️ **Les deux moitiés sont indissociables.** Un écrivain qui n'indenterait
*plus jamais rien* rend le corpus 9 / 9 à l'octet et passe **60 des 61**
contrôles — mesuré, mutation M1. Seul le contrôle 24i tient la seconde moitié.

⚠️ **Ce que ce corpus, lui, ne peut pas mesurer : le terminateur de ligne.** Il
est figé en LF (`.gitattributes`), pour une bonne raison — ses positions en
octets font foi. Le prix est qu'il est aveugle : une mutation qui perd le `\r`
d'un fichier CRLF laisse les 20 fichiers **exactement au même relevé**. Seuls
les contrôles 24j et 24k du banc le voient. **Un corpus figé mesure ce qu'on a
figé.**
