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
NKUIDesign.exe --roundtrip=Applications/NKUIDesign/exemples/valides
NKUIDesign.exe --valider=Applications/NKUIDesign/exemples/valides
```

`--roundtrip` mesure *analyser → réémettre → réanalyser*. `--valider` juge les
rôles et les types. **Ce sont deux gestes différents**, et un fichier peut
réussir l'un en échouant l'autre — le dossier `limites/` existe entièrement
pour ça.

---

## `valides/` — doivent passer les deux

Relevé du 2026-08-23 (après le correctif d'apparence) : **7 / 7 octet pour
octet**, **0 erreur**.

| fichier | ce qu'il exerce | attendu |
|---|---|---|
| `01_panneau_reglages.nkgui` | un document ordinaire et réaliste : huit rôles, commentaires en tête de membre, imbrication | 0 erreur |
| `02_bloc_sur_une_ligne.nkgui` | bloc **plein sur une seule ligne**, membre sur la ligne du `{`, accolade fermante sur la ligne du dernier membre, bloc vide | 0 erreur |
| `03_virgule_vecteur_couleur.nkgui` | la **virgule qui sépare deux membres**, les `vec2`, les couleurs, les listes | 0 erreur |
| `04_echappements_utf8.nkgui` | les **trois** échappements du document 2 §2 (`\n`, `\"`, `\\`) et de l'UTF-8 réel — grec, cyrillique, japonais, arabe, symboles | 0 erreur |
| `05_animation_comportement.nkgui` | trois sections sur les huit connues, chemins pointés `n1.value` et `Enum.Haut` | 0 erreur |
| `06_version_0_2_alias.nkgui` | un fichier d'une **version antérieure** avec un rôle renommé depuis | **1 avertissement voulu** : `W-ROLE-ALIAS` |
| `07_apparence.nkgui` | l'**apparence** du document 9 §3 : `appearance`, la typographie, `fill`, `shadow` nommé, et trois surcharges d'état | 0 erreur |

Le seul avertissement du dossier est **le comportement correct** : un vieux
fichier doit rester ouvrable, sinon on ne peut pas le mettre à jour.

⚠️ **`07_apparence.nkgui` arrive de `limites/`, et c'est le dossier qui dit
quoi.** Il y était parce que la validation rendait trois `E-ROLE-INCONNU`
dessus. Elle ne les rend plus : le fichier n'a pas changé, l'outil si. Un
fichier ne reste dans `limites/` que tant que l'outil le traite mal — sinon le
dossier deviendrait un cimetière de défauts corrigés, et plus personne ne le
lirait comme une liste de choses à faire.

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

## `limites/` — **légaux**, et pourtant l'outil ne les traite pas correctement

Ce troisième dossier existe parce que ces fichiers ne sont **ni valides ni
fautifs** : la faute est du côté de l'outil, pas du fichier. Les ranger avec les
fautifs serait mentir sur qui a tort ; les ranger avec les valides masquerait
un défaut connu.

**Les deux ont été trouvés au premier essai d'écriture manuelle**, ce que dix
fichiers du même moule n'avaient jamais montré. **Il en reste un.**

| fichier | ce qui ne va pas | qui a tort |
|---|---|---|
| `02_indentation_mixte.nkgui` | une tranche brute indentée autrement que le bloc de la section précédente est réémise avec la largeur déduite du document. Premier écart à l'octet 533. | **l'écrivain** |

### Ce qui est parti d'ici, et pourquoi

`01_apparence_faux_positifs.nkgui` est devenu `valides/07_apparence.nkgui` le
2026-08-23 : les trois `E-ROLE-INCONNU` étaient un **défaut de la validation**,
pas du fichier, et l'apparence a désormais son propre vocabulaire.

Ce qu'il avait montré en passant mérite d'être gardé, parce que c'est plus
général que le défaut lui-même : `appearance(Hover)` — l'en-tête de bloc avec
parenthèses, la **limite déclarée** — ne produisait **aucune** erreur, parce
qu'il reste une tranche verbatim que la validation ne regarde pas.

> **La partie modélisée crie, la partie non modélisée se tait.** Une limite qui
> protège d'un faux positif ne protège de rien : elle cache que c'est le faux
> positif qu'il fallait traiter.

---

## Les limites déclarées, et ce que ce corpus en dit

- **`appearance(Hover)`** — en-tête de bloc avec parenthèses, conservé verbatim,
  enfants absents de l'archive. **L'usage est réel** : il s'écrit naturellement
  dès qu'un bouton a un état survolé (`valides/07`). La limite **reste
  ouverte**, et elle a une conséquence désormais nommée : la validation est
  **asymétrique**, la même faute est vue dans `appearance` et tue dans
  `appearance(Hover)`. Ce n'est plus seulement écrit — c'est **mesuré** par le
  contrôle 23e. La fermer demande d'abord la **liste fermée des états**, que le
  document 9 §3.2 marque explicitement « à trancher » et que personne n'a
  écrite : c'est une décision de vocabulaire, pas un travail de code.
- **Commentaire inline** (`a /* c */ = 1`) — non conservé. **Aucun usage
  n'apparaît.** En écrivant six fichiers à la main, le commentaire est allé
  chaque fois sur sa propre ligne, jamais entre deux jetons d'une même
  construction. La limite **reste déclarée**, et il n'y a pas lieu de la fermer.
- **`items = [a, b]` reste un jeton nu** — `KindOf` dit que c'est une liste,
  personne ne peut itérer ses éléments. `03` et `v2` l'exercent ; rien n'en a
  eu besoin.
