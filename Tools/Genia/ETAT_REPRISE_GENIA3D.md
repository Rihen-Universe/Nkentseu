# GenIA 3D — état de reprise

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
Écrit le 2026-09-18 à 06 h 15, avant l'extinction de 07 h 00.
**Pour quelqu'un qui n'a aucune mémoire de cette session.** Branche `feat/genia-3d`.

---

## 0A. ⚠️ L'ETALON MESHY A DEPLACE LE PROBLEME (19/09) — A LIRE EN PREMIER

**Le probleme des coutures UV n'etait pas le bon probleme.** Mesure sur l'etalon commercial
`D:/L'Enfant des Bois/Ressources Externes/Perso Remesh/Meshy_AI_...fbx` :

| | topologie (valence 4) | ilots | coutures | distorsion p99 |
|---|---|---|---|---|
| **Meshy (etalon)** | **98,3 %** | 174 | **4,5 %** | **1,213** |
| notre grammaire quads (`personnage_quads.obj`) | **99,5 %** | **9** | **0,0 %** | 4,064 ❌ |
| `p512_allege_repare.obj` (TripoSR + marching cubes) | **7,5 %** ❌ | 272 ❌ | 34,3 % ❌ | (bornee 1,707) |
| notre retopologie `p512_rho1.obj` | **26,9 %** ❌ | — | — | — |

**Ce que ca commande, et c'est le cap de la reprise :**

1. **Meshy ne pose pas mieux ses coutures : il RETOPOLOGIE D'ABORD.** 98,3 % de valence 4.
   Quatre methodes de coutures ont ete refutees sur une etape qui, dans le bon ordre,
   **ne se presente jamais**.
2. ✅ **FAIT le 19/09 (commit `3298d9739`) : les UV equi-aires.** p99 **4,064 -> 1,414**,
   >1,732 **46,4 % -> 0,0 %**, ecart de densite entre ilots **x111,12 -> x1,00**, et **1,000
   exactement** sur la chaise (faces planes). Mesure avant/apres **sur le meme maillage** grace
   a `NK_UV_MUTE=3`, qui restaure le comportement d'origine au chiffre pres. 9 ilots, 0 couture
   et la valence 99,8 % **n'ont pas bouge**.
   ⚠️ **LE COMPROMIS DE GAUSS EST MESURE, PAS DEVINE** : `NK_UV_ALPHA` (pourcentage ENTIER --
   `atof` + virgule decimale en fr-FR) donne alpha 0 -> aire 20,242 / conforme 4,512 ;
   alpha 1 -> aire 2,001 / conforme 27,927. Defaut 1,00 choisi sur la courbe. La conformite
   **s'ameliore** en mediane (3,835 -> 1,878) et p90, et ne se degrade **qu'aux poles**.
   ✅ **ET LE RECOUVREMENT EST FERME le 19/09 (commit `16af61a88`)** : 93 376 texels
   partages (44,7 %) -> **0**. Le critere `mesurer_atlas.py` a ete ecrit **AVANT** le code,
   et il prouve son zero (atlas etale -> 0) et son refus (deux ilots superposes -> rouge).
   Densite, distorsion, ilots et coutures **n'ont pas bouge** : on translate les ilots d'un
   bloc et on applique UN SEUL facteur commun -- un facteur par ilot detruirait le x1,00.
   ⚠️ **L'OCCUPATION SEULE EST UN CRITERE QUI SE RETOURNE** : l'atlas superpose affichait
   79,7 % contre 38,2 % pour l'atlas etale. L'optimiser aurait AGGRAVE le defaut.
   ⚠️ **RESTE OUVERT** : `mesurer_atlas.py` ne voit pas le REPLI d'un ilot sur lui-meme
   (les faces voisines partagent legitimement les texels de leur arete). Un ilot replie
   passerait. **Reouverture : des qu'une texture reelle sera appliquee.**

**Instruments (reutilisables sur n'importe quelle source, y compris pour juger la retopologie) :**
`mesurer_uv_fbx.py` (lecteur FBX binaire 7400 ecrit ici : rien dans le venv ne lit le FBX),
`mesurer_uv_obj.py`, `mesurer_valence.py`. Chacun porte **son attendu en tete** et
`mesurer_uv_fbx.py` porte **son negatif** — dont la premiere version, une PERMUTATION, etait
**incapable de refuter** (une bijection preserve l'incidence) ; le negatif correct tire chaque
indice **independamment**.

**Corpus `D:/Projets/2026/NKGenCorpus/`** : lu, rien ecrit. `brut/` (mes-assets, kenney,
quaternius **CC0**), `normalise/` (« ne rien y mettre a la main »), `outils/recolte.ps1`
(reprend sans retelecharger). **Licences verifiees par `LICENCE.txt` + `MANIFESTE.csv` par
createur -> utilisable pour l'entrainement d'Ilyana-3DG sans reprise juridique.**
**Regle du lieu : « Pas de commit ici : ce sont des donnees. »**

## 0ter. LE LOT ① A LIVRE (19/09, nuit) : PRIMITIVES EN QUADS, NOMMEES, AVEC UV

    NKTexte3D --scene <doc.nkscene> --out <f.obj> --primitives [--anneaux 16] [--segments 24]

                    sommets   faces                 quads    UV    normales
  champ (avant)      36 596   73 180 triangles          0     0    moyennees
  analytique          3 027   2 400 quads + 336 tri  87,7 %  3027  exactes (1,2e-07)

Fidelite : ecart maximal a la surface analytique **4,6e-08** -- l'erreur du flottant. Un TORE
rend **384 quads, ZERO triangle**. Les 7 parties sortent NOMMEES et relues par NkOBJLoader.
L'ANTENNE est la (le chemin du champ la perdait : rayon 0,009 contre un pas de 0,0203).

⚠️ LIMITE : les BOOLEENS et le LISSAGE exigent un champ -- refus nomme, garder l'ancien chemin.
⚠️ NON REGRESSION : le banc a 9 cas passe toujours par le champ, comptes INCHANGES.
⚠️ PIEGE PAYE : sous echelle anisotrope, une normale se DIVISE par l'echelle, elle ne s'y
multiplie pas. Faux sur toute forme aplatie, et invisible dans un compteur.

## 0quater. LES QUATRE IDEES DE RODOLF (19/09) ET LE CAP REVISE

**CAP** : ① document + deformation **FAIT** · **② rendre p512 exploitable = LOT ACTIF** ·
③ « decolle les bras » comme OPERATION · ④ gabarit ajuste aux silhouettes.

**③ « DECOLLE LES BRAS » EST UNE OPERATION, PAS UNE CHAINE.** Le panneau IA du modeleur
fonctionne (24 verbes, annulation prouvee) : `decoller:bras` est un verbe de plus. Une
operation LOCALE suffit -- couper, reboucher. Pas besoin de rigger tout le personnage.
⚠️ **Le BOUCHAGE est PROUVE** (19/09) : coupe par plan INCLINE (boucle NON PLANE), zero arete
de bord apres, chi retrouve sa valeur. Reserve : n-gon non plane de 26 sommets, triangulation
en eventail a ameliorer un jour.

**④ LE GABARIT AJUSTE AUX SILHOUETTES** -- valide par Rodolf (« ca marche »). On garde le
modele structure comme GABARIT et on le DEFORME jusqu'a ce que ses silhouettes coincident avec
ses dessins. Il reutilise l'etage de deformation deja ecrit, pilote non plus par des valeurs du
document mais par un ecart mesure aux silhouettes.
⚠️ **LE PIEGE DE LA CIRCULARITE** : rendre notre propre modele puis le reconstruire donne des
vues parfaitement alignees, mais on reconstruirait NOTRE forme, pas son dessin. **La
ressemblance ne peut pas augmenter toute seule.** Elle n'augmente que si de l'information
NOUVELLE entre -- les dessins de Rodolf, ou ses corrections.

**LA SPIRALE : DEUX COUCHES, ET C'EST LA CONCEPTION DU R32.**
    document -> BASE regeneree + PILE D'OPERATIONS rejouee par-dessus
⚠️ **MON FORMAT NE SAIT PAS decrire un maillage corrige a la main** : regenerer ECRASE
aujourd'hui toute correction. Et il ne faut PAS chercher a remonter la geometrie dans la
grammaire -- impossible en general. Les corrections sont des GESTES REJOUABLES, pas des sommets.
⚠️ **CONDITION QUI DECIDE DE TOUT : designer par NOM ou par CRITERE, JAMAIS par indice.**
« biseaute l'arete 417 » biseautera n'importe quoi des que la base bouge. Les parties nommees
sont la moitie de la reponse ; la SELECTION PAR CRITERE manque et c'est l'autre moitie.
⚠️ Criteres ecrits (R32.7) : pile vide = base AU BIT · determinisme · cible introuvable = REFUS
NOMME (appliquer ailleurs est PIRE que ne rien faire) · une orpheline n'annule pas les autres.

⚠️ **LA SCULPTURE N'EXISTE PAS** : `PixolSculpt` et `Noge/NkSculpting` sont deux squelettes,
ZERO appelant, brosses reduites a des enumerations. Ne rien construire qui la suppose.

## 0bis. LES TROIS CHANTIERS, DANS L'ORDRE VALIDE PAR RODOLF (18/09 au soir)

**① LE DOCUMENT DE SCENE DEPUIS L'IMAGE — ACTIF.**
Ma moitie est PROUVEE : un document declarant sept parties NOMMEES rend
**12 tranches consecutives a 3 regions** (torse + deux bras) la ou TripoSR n'en montre qu'une,
et **3 composantes** contre 1. *C'est le seul des trois chemins qui donne les bras SANS RIEN
COUPER.* Il manque le modele qui voit, pour l'etape image -> document.
⚠️ Verifier sa presence par `ollama list`, JAMAIS par le code de sortie : `ollama pull` rend 0
sans installer, et `ollama run` relance un telechargement au lieu d'echouer.

**② LA FUSION MULTI-VUES — en attente, PAS abandonnee.**
`right.png` montre le bras **de profil, detache** : l'information existe dans les images de
Rodolf. TripoSR est mono-vue (`Nv=1` en dur). Les trois vues ne sont pas alignees (tailles et
cadrages differents, alpha plein) — il faut les refaire avec meme echelle et meme centre.

**③ LE SQUELETTE — en attente, PAS abandonnee.**
Etat des lieux au R26 : **la repose est deja resolue** (LBS GPU, 4 appelants) ; quatre briques
manquent (ajuster un gabarit, poids, couper, boucher) ; criteres et **cas defavorables** ecrits.

**④ LE DECOUPAGE GEOMETRIQUE — GELE derriere ①, sur ma recommandation.**
Rodolf : « le decoupage actuel est mal fait ». Analyse au R28. Mes criteres etaient invariants
par le defaut. L'outil de qualite (`qualite_decoupe.py`) est livre avec **son zero ROUGE** : il
mesure un contour rasterise, pas une aire de section (106 contre 52 sur un cylindre uniforme).

⚠️ **CE QUI FAIT QUE ① ET ③ SE REJOIGNENT** : un document qui NOMME les parties peut aussi
PORTER leur squelette, et il fournit l'a priori anatomique qui manque au decoupage. Un document
qui dit « bras_gauche, capsule, attachee au torse a telle hauteur » dit OU L'OS DOIT ALLER.

## 0aa. NOMENCLATURE : ILYANA-3DG (nomme par Rodolf le 19/09)

**Ilyana-3DG** = ce qui APPREND : la generation de geometrie (TripoSR est son **dorsal
d'aujourd'hui**, remplacable), la generation des vues manquantes, tout ce qui produit de la 3D
par apprentissage.
⚠️ **Ne porte PAS ce nom** : le format de scene, la grammaire de primitives, les deformations,
la segmentation, la retopologie, le depliage UV. **Ce sont du calcul deterministe.** La
frontiere dit d'un coup d'oeil ce qui est remplacable par un meilleur modele et ce qui ne l'est
pas.
⚠️ **Trois Ilyana coexistent** : **Ilyana** (texte, poids detruits le 07/09) · **Ilyana
Collect** (14B envisage, distillation de poids OUVERTS seulement) · **Ilyana-3DG** (celui-ci).

## 0ab. LES UV : LE DEPLIAGE EXISTE, ET J'AVAIS ANNONCE SON ABSENCE (19/09)

`NkUVUnwrap` : **1 045 lignes**, banc dedie **19 tests / 0 echec**, refus NOMMES, et
`NkUVMeasureDistortion` calcule DEJA deux de mes quatre criteres (aire et angle).

**Sur un maillage reel il REFUSE, et les deux refus sont EXACTS :**
  - p512 allege -> `AreteNonManifold` : il reste **1 arete** (la dette du R14.9, dont la
    condition de reouverture disait « le jour ou un depliage sera demande ». Ce jour est arrive) ;
  - corps analytique -> `IlotNonDisque, euler=2` : une surface FERMEE ne se deplie pas sans
    coutures.

**CE QUI MANQUE, ET C'EST PETIT** : aucun calcul automatique de COUTURES (`MarkSeam`,
`ComputeSeams` : zero occurrence).

**ET POUR LE DOCUMENT LA QUESTION NE SE POSE PAS** : les primitives analytiques portent deja
**3 027 UV dans [0,1]**. La parametrisation EST le depliage.

**Prochain geste** : (a) reparer la derniere arete non-manifold ; (b) poser les coutures --
deux sources existent deja (parties nommees + aretes vives de NkMeshRetopo), **non mesurees**.

## 0. LE PREMIER GESTE DE LA PROCHAINE SESSION

```
cd D:/Projets/2026/Nkentseu/Nkentseu-genia3d
git log --oneline -6                       # 6 commits, rien poussé
Build/Bin/Release-Windows/NKTexte3D/NKTexte3D.exe --scene Tools/Genia/scenes/chaise.nkscene --out /tmp/c.obj --verifier
```
Si ce dernier rend **0**, tout ce qui est décrit ici tient encore. S'il rougit, **la
première hypothèse est la machine, pas le code** — et il faut le dire ainsi.

Puis lire, dans cet ordre : `Tools/Genia/FORMAT_SCENE.md`, puis `echanges/genia-3d.questions.md`
à partir du **R13**.

---

## 1. Ce qui est ACQUIS et mesuré

| fait | par quoi |
|---|---|
| **41 864 → 997 sommets**, silhouette à **99,11 %**, objet éditable | deux instruments sans code commun (R14) |
| le maillage de TripoSR **n'est pas manifold** : χ = 313 impair, 279 arêtes | R14.2 |
| `DecimateQEM` **répare** 277 de ces 279 arêtes avant même de décimer | R14.3, trouvé par « prouver le zéro d'abord » |
| χ **invariant sur 41 459 contractions** | R14.4 |
| le **document de scène** existe, avec placement, échelle par axe, rotation, booléens | `FORMAT_SCENE.md`, commit `18fab6963` |
| la géométrie **se vérifie contre le document** (`--verifier`) | 3 critères, chacun avec sa condition |
| **une chaise sort correcte** — ce que la grammaire de mots déclarait « IMPOSSIBLE » | `logs_genia3d/scene/apercus/chaise_corrigee.png` |
| les 9 cas du banc historique **n'ont pas bougé d'un sommet** | attendu sur ce qui ne doit pas bouger |
| **rembg ne télécharge plus rien** : refus nommé, chemin durable, taille vérifiée | commit `7d5578b83` |

**Mutations vivantes** : `NK_TEXTE3D_MUTE=1|2|3`, `NK_SCENE_MUTE=1|2|3`, `NK_REDUC_MUTE=1|2|3`,
`NK_SURFACENETS_NAIF=1`, `--axe-up brut`. **Aucune ne doit rendre 0.**

---

## 1bis. ⚠️ TRANCHÉ LE 18/09 AU SOIR — LA RÉSOLUTION EST EXCLUE

Mesuré sur **l'image de Rodolf** (`D:/Rodolf/Livre/front.png`), pas sur un substitut :

    l'ecart bras/corps mesure 14 a 18 px sur l'image SOURCE (8 lignes sur 8)
    le personnage fait 570 px de haut -> 1 cellule a 256 = 2,23 px
    l'ecart dispose donc de 7,2 CELLULES a 256, et 14,4 a 512

    256 : 20 tranches sur 30 a UNE region, maximum 4 a 22 % de la hauteur
    512 : 20 tranches sur 30 a UNE region, maximum 4 a 22 % -- MEME VALEUR, MEME ENDROIT

**Sept cellules suffisent largement a marching cubes.** L'ecart ne sort ni a 256 ni a 512 :
**l'echantillonnage est EXCLU**, le modele n'a pas mis l'ecart dans son champ. Monter a 1024
offrirait 29 cellules la ou 7 ne suffisent deja pas -- **ce n'est pas une question de finesse.**

**Ce qui marche, livre et mesure** : la segmentation (`Tools/Genia/segmenter_parties.py`) rend
**6 parties** sur ce personnage -- tete+antenne, torse, **les deux jambes separees**, deux
bottes -- somme de faces exacte, et les cinq criteres du R22.5 tenus.
**Ce qu'elle ne peut pas** : separer les BRAS, fondus dans le volume. Aucun decoupage a
posteriori ne recupere ce que la reconstruction a rempli.

**Les trois vues de Rodolf** (`D:/Rodolf/Livre/`) : `right.png` montre le bras **de profil,
detache**. L'information existe dans ses images ; elle n'atteint pas le modele, qui n'en lit
qu'une (`Nv=1` code en dur, `tsr/system.py`). ⚠️ Elles ne sont **pas detourees** (alpha plein),
le fond est une **grille**, et elles n'ont **ni la meme taille ni le meme cadrage** : une
fusion multi-vues demande qu'elles partagent echelle et centre.

## 1ter. SQUELETTE ET PEAU — ETAT DES LIEUX DU 18/09 AU SOIR (R26 du canal)

**CE QUI EXISTE ET TOURNE, avec appelants de production** (la colonne qui compte) :
lire un squelette (glTF/FBX, JOINTS_0/WEIGHTS_0), la structure `NkSkeletonDef` (238 l.,
**6 appelants**), evaluer une pose `EvaluateGLTFPose` (**4 appelants**), deformer la peau en
**LBS sur GPU** (`Resources/.../Shaders/Skin/`, via `SubmitSkinned`, **4 appelants**), l'IK
(`NkIKSolver.cpp`, 240 l., 2 appelants), et `NkVertexSkinned`.

> **« Poser un maillage en T ou incline » EST DEJA RESOLU.** Aucune brique neuve : il faut
> seulement que le maillage AIT un squelette et des poids.

⚠️ **Orphelin trouve** : `NkAnimationSystem::ApplySkinnedMesh` n'a AUCUN appelant.

**CE QUI MANQUE — zero occurrence dans tout le depot** : `AutoRig`, `FitSkeleton`,
`ComputeSkinWeights`, `BoneHeat`, `FillHole`. Soit quatre briques :
  1. ajuster un squelette GABARIT humanoide (tractable : on n'extrait pas, on ajuste) ;
  2. calculer les poids (le plus proche os suffit pour couper) ;
  3. couper suivant la frontiere des poids ;
  4. BOUCHER les deux ouvertures -- `MakeFaceFromSelected` existe, son aptitude a fermer une
     grande boucle non plane N'EST PAS MESUREE.

⚠️ **L'ORDRE EST CONTRAINT** : ajuster, calculer, couper, boucher, reposer. Reposer avant de
couper produit une PALMURE ; couper sans boucher ouvre le maillage. **Les deux fautes sont
silencieuses** -- le compte de faces ne les voit pas.

**Les criteres et leurs CAS DEFAVORABLES sont ecrits au R26.4 du canal**, avant tout code.

## 2. LES TROIS PROBLÈMES QUE RODOLF A VUS DANS BLENDER — ET ILS SONT DISTINCTS

Références : `D:/Rihen/Livraisons/Reference_Blender/`.

### (1) « Les vertices sont en désordre » — ce n'est PAS le modèle

Le modèle rend un **champ de densité**. Les sommets sont posés par **notre** étage
d'extraction, sur une **grille régulière** — d'où le fil de fer dense, uniforme et isotrope.
**Aucune intelligence du modèle ne changera ça : c'est notre code.** C'est l'objet du chantier
de retopologie quadrangulaire, qui tourne dans `Nkentseu-retopo`.

### (2) « Pas d'espacement entre les bras et le corps » — DEUX causes, une expérience les sépare

**(a) notre échantillonnage.** À `--mc-resolution 256` sur un personnage d'environ 0,97 m,
une cellule vaut **~3,8 mm**. Un écart bras/corps de 2 mm **ne peut pas** être représenté : il
est plus fin que la maille. Ce n'est pas une opinion, c'est une borne.

**(b) la connaissance du modèle.** Depuis une seule vue, s'il n'a jamais « vu » l'écart,
aucune résolution ne l'inventera.

> **CETTE EXPERIENCE A ETE LANCEE LE 18/09 AU SOIR. REPONSE : LA RESOLUTION N'EST PAS LA
> CAUSE.** Sur `horse.png`, 512 n'ouvre AUCUNE separation que 256 ne montrait deja -- maximum
> de regions IDENTIQUE (6, meme hauteur), ecarts sous la dispersion de l'instrument -- pour un
> cout de **x6,4 en temps** (76 s -> 486 s) et 4 composantes parasites de plus.
> **Limite dite** : sur ce sujet les pattes etaient DEJA separees a 256 ; le cas « parties
> franchement collees » demande **l'image de Rodolf**, toujours introuvable.
> ⚠️ Et le critere « composantes connexes » du protocole ci-dessous **ne pouvait pas voir le
> defaut** : un bras decolle reste attache a l'epaule. Le critere qui distingue est le nombre
> de **regions par coupe horizontale** (`Tools/Genia/mesure_separation.py`).
> ⚠️ Cet instrument **publie sa propre dispersion** (5 tirages) : un ecart entre deux maillages
> ne se lit que s'il la depasse.

> **PROTOCOLE ÉCRIT AVANT LA COURSE — conserve pour memoire.**

```
# La MEME image, la MEME graine, seule la resolution change.
PY=D:/Projets/2026/Nkentseu/genia-tools/venv/Scripts/python.exe
$PY Tools/Genia/genia_triposr.py --image <IMAGE> --out r256.glb --device cpu --mc-resolution 256
$PY Tools/Genia/genia_triposr.py --image <IMAGE> --out r512.glb --device cpu --mc-resolution 512
# Le critere : le nombre de COMPOSANTES CONNEXES. Derive, pas a l'oeil.
Build/Bin/Release-Windows/NKGeniaTemoin/NKGeniaTemoin.exe --reduire r256.glb --faces 4000 --out r256.obj
Build/Bin/Release-Windows/NKGeniaTemoin/NKGeniaTemoin.exe --reduire r512.glb --faces 4000 --out r512.obj
```

| issue | conclusion | ce qu'on fait ensuite |
|---|---|---|
| **512 ouvre l'écart** (plus de composantes qu'à 256) | la cause est **notre échantillonnage** | monter la résolution par défaut, et mesurer son coût |
| **512 n'ouvre rien** (même compte) | la cause est **le modèle** | une seule vue ne suffit pas ; il faut plusieurs vues ou un autre modèle |

**Les deux issues sont informatives** — c'est ce qui rend l'expérience non stérile.
⚠️ **Coût attendu : 512 coûte environ 8 fois 256** (la grille est cubique). Sur processeur,
compter **plusieurs minutes** et une empreinte mémoire nettement plus grande. **Relever la
carte avant**, et rapporter temps ET mémoire à côté du résultat.
⚠️ **Le compte de composantes se lit sur le maillage BRUT autant que sur l'allégé** : la
décimation ne crée ni ne détruit de composante, mais le vérifier coûte une ligne.

### (3) « Séparer pour animer » — c'est une TROISIÈME chose, et la plus grosse

Même avec une surface parfaite et des écarts nets, un personnage animable demande des
**parties nommées** (segmentation) et un **squelette** — pas seulement des trous. Ça touche
**NKAnima**. **Non attaqué, et il ne faut pas l'attaquer sans arbitrage** : c'est un chantier
de plusieurs semaines (segmentation sémantique, pose d'un rig, poids de peau), et il vient
**après** la topologie.

---

## 3. UV, TEXTURES, NORMALES — LA PRÉMISSE EST À CORRIGER

Rodolf demande que « l'UV édition et les textures soient bien calculées, pas à peu près ».
**Mesuré sur le fichier produit par la chaîne, par deux instruments indépendants :**

```
attributs declares : POSITION, COLOR_0
                     PAS de TEXCOORD_0   PAS de NORMAL
images embarquees  : 0        materiaux : 0
```

> **Il n'y a pas d'UV approximatives : il n'y a AUCUNE UV, AUCUNE texture, et pas même de
> normales.** La couleur vit **sur les sommets**, donc sa résolution est celle du maillage.
> C'est la cause directe de l'aspect délavé, et ça n'a rien à voir avec la qualité du modèle.

Le dire ainsi lui évite de chercher un réglage qui n'existe pas.

**L'ORDRE DE DÉPENDANCE, et il n'est pas négociable :**

1. **La topologie d'abord.** Déplier une soupe de triangles issue d'une grille ne donne rien
   d'éditable : les coutures tombent n'importe où, l'atlas est illisible. → le chantier de
   retopologie est **en amont**.
2. **Le dépliage ensuite** (coutures, îlots, empaquetage), avec des critères **dérivés** —
   chacun avec ce qui le ferait rougir :
   - **distorsion d'angle** (conformité) — rougit au-delà d'un facteur mesuré sur un cas connu ;
   - **distorsion d'aire** — le rapport aire 3D / aire UV doit rester borné ;
   - **longueur totale de couture** — une couture plus longue que le périmètre est un échec ;
   - **taux d'occupation de l'atlas** — un atlas à moitié vide gâche la moitié de la texture.
3. **La cuisson enfin.** ⚠️ **NE PAS cuire les couleurs de sommets dans la texture.** Ce
   serait **graver la pauvreté actuelle** dans un fichier de 2048². Il faut rééchantillonner
   **la source** — le champ de couleur du modèle, ou l'image d'entrée par projection — pour
   que la résolution de la couleur **cesse d'être liée à la densité du maillage**. C'est toute
   la raison d'être d'une texture.

**Et un défaut à part, petit et réparable : l'absence de NORMAL.** Sans elles, chaque logiciel
les recalcule à sa façon et l'objet ne s'affiche pas pareil d'un outil à l'autre.

---

## 4. CE QUI RATE, NOMMÉ, AVEC SA CONDITION DE RÉOUVERTURE

1. **`op difference` produit un maillage non-manifold — MAIS L'OPERATEUR EST DISCULPE (18/09 soir).**
   Trois cas, dont un **temoin de controle connu sain** :
   | cas | ce qu'il isole | non-manifold | chi |
   |---|---|---|---|
   | sphere moins sphere **interieure** | soustraction **sans arete vive** | **0** | 4 (derive : 2 surfaces imbriquees) |
   | sphere moins sphere **qui la coupe** | entaille, **avec** arete vive | **8** | 38 |
   | « un cube perce » (grammaire) | trou **traversant**, vert depuis le 17/09 | **0** | 0 |

   Le troisieme utilise **la meme operation** `max(d, -dq)` et il est sain : mon soupcon
   « le champ n'est plus 1-Lipschitz » est donc **refute par un temoin qui existait deja**.
   **Nature du defaut, mesuree** : 45 composantes = 1 objet (36 820 sommets) + **44 eclats**
   de 12 a 32 sommets. Des ilots parasites le long de l'entaille, pas une topologie ruinee.
   → *Le domaine de recherche est reduit : la surface soustraite qui COUPE la surface
   d'accueil, ni traversante ni interieure.*
   ⚠️ **Piege nomme** : supprimer les petites composantes rendrait l'objet utilisable et
   MASQUERAIT le defaut — « un correctif qui eteint l'alarme ». Ne pas le faire.

1bis. **(ancienne formulation, conservee)** `op difference` produit un maillage non-manifold. Cas minimal (sphère moins sphère) :
   8 arêtes non-manifold, χ = 38. **La résolution n'y change rien** (128, 192, 256 testés) —
   l'hypothèse « parties trop fines » est **réfutée**. Mon second test (le lissage) **ne
   prouvait rien** : le lissage ne s'applique qu'aux unions dans mon code, donc la
   soustraction restait vive — un négatif mal construit. **Le garde-fou refuse d'annoncer ce
   maillage valide (code 1), donc rien de faux ne sort.**
   → *Se rouvre dès qu'un document aura besoin de creuser. Première piste à tester : le champ
   `max(d, -dq)` n'est plus 1-Lipschitz, et SurfaceNets interpole linéairement.*
2. **L'écrivain OBJ perd les couleurs de sommets.**
3. **Le validateur du pont recopie le vocabulaire de `NKTexte3D`** — deux autorités.
   → *Se rouvre quand `NKTexte3D --vocabulaire` existera.*
4. **Tout est mesuré sur un seul objet** (`chair.glb`) côté décimation.
5. **`SurfaceNetsQuads` porte toujours le défaut non-manifold d'origine**, non corrigé.
6. **Aucune mesure de temps** sur la décimation.

---

## 5. CE QU'IL FAUT DEMANDER À RODOLF

1. **L'IMAGE QU'IL A UTILISÉE.** Elle est introuvable : la seule sortie récente est un
   `test.glb`. **Un résultat sans son entrée n'est pas reproductible.**
   → *Correctif à faire : que le lanceur COPIE l'image d'entrée à côté de sa sortie.* Une
   ligne, et elle évite de reperdre chaque essai.
2. **L'arbitrage sur le branchement de la décimation** : dans le générateur, ou une question
   à l'import (R14.8) ? Le second vit dans `Applications/NK3DModeler/`, hors périmètre.
3. **Le chantier « séparer pour animer »** (§2.3) : c'est lui qui décide s'il vaut ses semaines.

---

## 6. ÉTAT GIT

**6 commits** sur `feat/genia-3d`, **rien poussé**. `transit` était à `265c3f7e4` et **n'est
plus ancêtre** de cette branche : une refusion sera nécessaire, et *une prédiction ne vaut que
contre l'état contre lequel elle est faite* — la refaire au moment de fusionner.

**Aucune construction de l'espace complet lancée** : uniquement des cibles isolées
(`--target NKTexte3D`, `--target NKGeniaTemoin`), chacune avec sa ligne `Status: ✓ SUCCESS`.
