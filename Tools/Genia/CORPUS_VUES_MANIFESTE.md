# Manifeste du corpus de vues — Ilyana-3DG

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
**État : SPÉCIFIÉ, NON PRODUIT.** Aucune vue n'est rendue à ce jour.

> ⚠️ **Ce fichier doit voyager avec les poids de tout modèle entraîné sur ce corpus.**
> Un modèle qui sort sans sa condition de provenance est un modèle qu'on ne saura plus
> qualifier dans six mois.

---

## 1. ⚠️ LA FRONTIÈRE DE PROVENANCE — À LIRE AVANT TOUTE DIFFUSION

**Décision de Rodolf, 17/08/2026**, consignée dans
`D:/Projets/2026/NKGenCorpus/brut/mes-assets/LICENCE.txt` :

> « Oublie les licences, utilise-les tous dans ce cadre privé. »

**Portée exacte de cette décision, telle qu'elle est écrite :**

- ✅ **Autorisé** : entraînement, expérimentation privée, **R&D interne**. Les 62 packs de
  `mes-assets` sont utilisables, y compris ceux à provenance non identifiée. Le tri par
  licence reste dans le fichier **comme information de provenance, plus comme barrière**.
- ⚠️ **La frontière, notée par Rodolf lui-même** : *« le jour où un modèle entraîné sur des
  données de provenance inconnue alimente un **PRODUIT DIFFUSÉ** (NKCivilisation, vente,
  publication), re-poser la question sur ce sous-ensemble. D'ici là, aucune restriction. »*

**Donc : ce corpus est un corpus de R&D interne. Un modèle qui en sort ne peut pas être
diffusé sans rouvrir la question de provenance.** Ce n'est pas une opinion d'agent : c'est la
condition que Rodolf a posée en même temps que l'autorisation.

**Ce qui reste exclu, et qu'aucune décision ne couvre :**

| source | entrées | motif de l'exclusion |
|---|---|---|
| `UE_5.7/Engine` | 73 | contenu sous licence **Epic Games**. La décision portait sur la récolte de Rodolf, pas sur le contenu d'un éditeur. |
| `Rodolf/Inspi` | 269 | des **inspirations** — travaux d'autrui rassemblés pour regarder, pas pour apprendre. |

---

## 2. LA SOURCE : `D:/Projets/2026/NKGenCorpus/brut/`

**5 703 fichiers 3D** (obj / glb / gltf / fbx). **0 échec de chargement** sur un échantillon
aléatoire de 40 (graine 4242).

⚠️ **`brut/kenney/` n'apporte AUCUN modèle** : il ne contient que des **archives `.zip` non
décompressées** (52 fichiers). Le corpus repose donc en fait sur deux sources, pas trois.
**Condition de réouverture : décompresser les archives kenney, et ce manifeste sera à refaire.**

### Répartition réelle, par pack (et non par nom de fichier — voir §3)

| pack | fichiers |
|---|---|
| KayKit Platformer Pack | 1 498 |
| KayKit Dungeon Remastered | 849 |
| KayKit Restaurant Bits | 582 |
| KayKit Resource Bits | 304 |
| KayKit Prototype Bits | 293 |
| KayKit Furniture Bits | 212 |
| KayKit City Builder Bits | 184 |
| **total KayKit** | **3 922 — 68,8 %** |
| Quaternius (`ultimate*`, `survival`, `piratekit`…) | ~1 310 |
| fichiers à nom de hachage (provenance inconnue) | **34 seulement** |

> ⚠️ **LE VRAI DÉFAUT DE CE CORPUS N'EST PAS LA LICENCE, C'EST LE DÉSÉQUILIBRE.**
> **68,8 % vient d'un seul créateur, dans un seul style** (lowpoly cartoon, 589 triangles en
> moyenne). Un modèle entraîné là-dessus apprendra ce style-là et le rendra partout.
> Ce n'est pas une objection à produire le corpus — c'est une propriété à connaître avant
> d'interpréter ce que le modèle aura appris, et à corriger par un rééquilibrage ou une
> pondération à l'échantillonnage.

---

## 3. ⚠️ POURQUOI LA COUVERTURE N'EST PAS CLASSÉE PAR NOM DE FICHIER

J'ai d'abord classé les 5 703 fichiers en familles morphologiques par mots-clés du chemin.
**Le contrôle a montré que ce classement est faux :**

    « Streetlight_Double.fbx » etait classe VEGETATION
      -> parce que « S-t-r-e-e-t » CONTIENT « tree ».

Avec **48,9 % de fichiers non classés** en plus, ce classement ne dit rien. **Il est écarté.**
La répartition par **pack** (§2) est la seule information fiable tirée des chemins ; une vraie
grille morphologique demanderait de classer **sur la géométrie** (proportions, symétrie,
nombre de composantes), pas sur les noms. **Non fait.**

---

## 4. LES POSES — CE QUI GARANTIT QU'ELLES NE MENTENT PAS

Rendu par `Tools/Genia/rendre_vues_corpus.py` (`rendre_vectorise`), poses écrites en JSON.

- **Témoin de reprojection obligatoire avant toute écriture** : la pose est **relue du disque**
  et **reconstruite depuis ses angles**, puis le tampon de positions est reprojeté. Critère :
  **p99 < 0,5 px**.
- **Sensibilité publiée par modèle** (de 0,140 à 1,442 px/degré selon la profondeur du sujet) :
  un objet **plat** est mesuré avec une sensibilité proche de zéro et **doit être écarté ou
  contrôlé autrement**.
- Le rasteriseur vectorisé est **identique aux octets** à la version de référence (12 rendus,
  masque + positions + normales), gain ×26,4.

---

## 5. LE NOMBRE DE VUES — CHIFFRÉ SUR UNE MESURE, PAS SUR UNE EXTRAPOLATION

**Débit mesuré : 0,092 s/vue** (échantillon aléatoire de 40 modèles, graine 4242).

| azimuts × élévations | vues/modèle | vues totales | durée |
|---|---|---|---|
| 8 × 1 | 8 | 45 624 | **1,2 h** |
| 16 × 1 | 16 | 91 248 | **2,3 h** |
| 12 × 2 | 24 | 136 872 | **3,5 h** |
| **16 × 3** | **48** | **273 744** | **7,0 h** |
| 24 × 3 | 72 | 410 616 | 10,5 h |

**Choix recommandé : 16 azimuts × 3 élévations = 48 vues, 7,0 heures.**

**Justification, et elle tient sur l'asymétrie** : sous-échantillonner est gratuit, re-rendre
coûte. 48 vues permettent d'en tirer 24, 16, 8 ou 4 **sans rien relancer** ; 8 vues ne
donneront jamais 48. Les 3 élévations sont nécessaires parce qu'un modèle entraîné sur une
seule hauteur d'œil n'apprend pas ce qu'est le dessus d'un objet — et c'est précisément ce que
la reconstruction mono-vue rate aujourd'hui.

**7 heures, une seule fois, contre une reprise complète si le nombre est trop petit.**

---

## 6. 🔴 LE DISQUE — MESURÉ, ET IL INTERDIT LE FORMAT ÉVIDENT

**Espace libre : 55 Go sur `D:` (déjà à 90 %), 409 Go sur `C:`.**

| format | par modèle | corpus (5 703) | fichiers | verdict |
|---|---|---|---|---|
| tampons bruts (masque + positions + normales, float64) | 331 Mo | **1 842 Go** | 820 000 | 🔴 **IMPOSSIBLE — ×33 l'espace libre** |
| **`.npz` compressé par modèle** (RGBA + profondeur 16 bits + poses) | **2,39 Mo** | **13,3 Go** | **5 703** | ✅ retenu |
| PNG RGBA seuls, sans profondeur | 0,12 Mo | 0,7 Go | 273 744 | possible, mais jette la profondeur |

**C'est ce que j'allais écrire sans mesurer : 1,8 To sur un disque qui en a 55 Go de libre.**

**Deux décisions de format, et la seconde applique le principe asymétrique :**
1. **On stocke la PROFONDEUR, pas les POSITIONS.** Les positions se reconstruisent exactement
   depuis la profondeur et la pose : les garder, c'est payer trois canaux en float64 pour une
   information qu'on possède déjà. Facteur 12 avant même la compression.
2. **On garde la profondeur plutôt que les PNG seuls** (13,3 Go contre 0,7 Go) parce que
   **la profondeur ne se recrée pas sans re-rendre** — le même argument qui impose 48 vues.
   12,6 Go d'écart contre 7 heures de reprise : l'échange est bon.

**Un fichier par modèle, pas un par vue** : 5 703 fichiers au lieu de 820 000.

---

## 7. 🔴 LA COULEUR — CE QUI BLOQUE LE LANCEMENT, ET C'EST MESURÉ

**Mon rasteriseur rend un lambert GRIS. Il ne lit ni texture ni couleur.**

Or, sur un échantillon de 50 modèles tirés au hasard :

    modeles lisibles par trimesh (obj/glb/gltf) : 14
      avec TEXTURE image      : 14   -> 100 %
      avec couleur par sommet :  0
      sans couleur du tout    :  0
    non lus : 36, dont les FBX (mon lecteur ne lit ni UV ni materiaux)

> **100 % des modèles lisibles portent une texture, et le rendu actuel la jette.**

**Pourquoi c'est bloquant et pas une amélioration à faire plus tard** : c'est exactement le
principe qui a fixé 48 vues. **Sous-échantillonner est gratuit, re-rendre coûte.** Un corpus
rendu en gris ne deviendra jamais couleur sans refaire les 7 heures — et la couleur est
précisément ce qui manquait au dos de `p512`, donc elle sera demandée.

**Ce qu'il faut ajouter** : interpoler les UV comme on interpole déjà les positions (les
coordonnées barycentriques sont là), puis échantillonner l'image du matériau. Le coût est
faible ; le refaire après coup coûte tout.

⚠️ **Et une asymétrie à assumer ou à corriger** : les **FBX (2 242 fichiers, 39 % du corpus)**
resteraient **gris**, car `lire_fbx.py` ne lit ni UV ni matériaux. Le manifeste devra porter,
par fichier, **s'il est texturé ou non** — sans quoi un modèle apprendrait que 39 % du monde
est gris.

**→ Je ne lance pas la production dans cet état, et j'attends l'arbitrage :** rendre en couleur
d'abord (le coût de rendu ne change pas, seul le développement s'ajoute), ou assumer un premier
corpus géométrique gris en sachant qu'il faudra le refaire.

---

## 8. KENNEY EXTRAIT — ET ⚠️ LE DÉSÉQUILIBRE A CHANGÉ DE CAMP, IL N'A PAS BAISSÉ

**50 archives sur 50 extraites, aucun échec. +13 786 modèles pour 556 Mo** (espace libre :
55 Go → 54 Go). Le corpus passe de **5 703 à 19 489 modèles**, et de **deux sources à trois**.

| source | fichiers | part |
|---|---|---|
| **kenney** (CC0, manifeste présent) | **13 786** | **70,7 %** |
| `mes-assets` (dont KayKit 3 922) | 4 393 | 22,5 % |
| quaternius (CC0, manifeste présent) | 1 310 | 6,7 % |

> ⚠️ **L'extraction n'a PAS corrigé le déséquilibre : elle l'a DÉPLACÉ.** KayKit dominait à
> 68,8 % ; kenney domine maintenant à 70,7 %. **Annoncer « le déséquilibre est réglé » serait
> faux**, et c'est exactement ce que la mesure était là pour empêcher.

**Ce qu'elle a réellement apporté, et qui compte** : une **troisième** source, toutes trois
documentées. Un tirage équilibré devient possible — **1 310 par source, soit 3 930 modèles à
parts égales** — et il se fait **à l'entraînement**, par lecture de ce manifeste, sans rien
re-rendre. C'est le même arbitrage que pour les vues, appliqué aux sources :
**on rend tout, on équilibre au tirage.**

**La neutralité de la chaîne l'exige** (Rodolf, 19/09) : *« un studio qui utilisera Nkentseu
aura son propre style — rien dans le moteur, les formats ou les algorithmes ne doit
l'orienter. »* Un modèle dont 70 % de l'apprentissage vient d'un seul créateur **impose un
goût**. Le corpus porte donc la part de chaque source, pour que l'équilibrage soit **un réglage
et non une reprise**.

## 9. 🔴 LE PRIX A TRIPLÉ — LE CHIFFRE À ARBITRER

Débit mesuré sur kenney : **0,086 s/vue** (23 modèles, 425 triangles en moyenne).

| | vues/modèle | vues totales | durée |
|---|---|---|---|
| 16 az × 1 él. | 16 | 311 824 | **7,6 h** |
| 12 az × 2 él. | 24 | 467 736 | **11,4 h** |
| **16 az × 3 él.** | **48** | **935 472** | **22,9 h** |

**Les 7,0 h validées portaient sur 5 703 modèles. Avec 19 489, le même choix coûte 22,9 h** —
presque une journée entière, pas une nuit. **Le principe asymétrique ne change pas** (re-rendre
coûterait ces 22,9 h), **mais l'engagement n'est plus du même ordre, et je ne le prends pas
seul.** Disque en `.npz` : 13,3 Go × 3,4 ≈ **45 Go**, contre **54 Go libres** — ça passe, mais
de justesse ; `C:` (409 Go libres) serait plus sûr.
