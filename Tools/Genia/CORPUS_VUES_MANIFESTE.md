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
