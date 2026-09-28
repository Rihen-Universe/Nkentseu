# NKCraft — vêtements, cheveux et éléments secondaires

### Document 5 — Spécification : la forme dans NKCraft, le mouvement physique dans NkAnima

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Rédigé le 28/09/2026. Origine : Rodolf, le 28/09 —
> *« on a oublié d'intégrer la modélisation des vêtements à part, qui sera intégrée du
> côté de NKCraft, mais animée physiquement et réalistement du côté de
> l'animation »*, puis *« même les cheveux et autres éléments comme queue, corde,
> etc. »*.
>
> **Ce document applique la décision D4 de NKCraft** (01 §3.1, F2 et F3 : « forme dans
> NKCraft, dynamique dans NkAnima ») et **prolonge le document 4** (générateur de
> créatures) : tout ce qui est décrit ici **s'attache** aux créatures du document 4 par
> leurs **adresses stables** (04 §3.2) et suit les **mêmes règles de coexistence
> humain ↔ IA** (04 §10).
>
> **Frontière** :
>
> | application | rôle |
> |---|---|
> | **NKCraft** | la **forme au repos** et **tout ce dont la simulation a besoin** : patrons, coutures, matières, épingles, guides des cheveux, attaches, volumes de collision, réglages physiques par défaut, versions allégées |
> | **NkAnima** | le **mouvement physique et réaliste** : tissu, cheveux, cordes, queues, chair, vent — cuit, déterministe (07-produit-nkanima, famille D « Déformables ») |
> | **Nogee** | la version **temps réel** en jeu, à partir des versions allégées préparées ici |

---

## 0. En une phrase

**Dans NKCraft, on coupe et coud un vêtement sur n'importe quelle créature, on coiffe
ses cheveux (y compris tresses, locks et cheveux crépus), on prépare ses queues, cordes,
chaînes et parures ; chacun arrive dans NkAnima prêt à être simulé — et chaque
retouche humaine survit quand le corps, le patron ou l'IA changent.**

---

## 1. Ce qui existe au 28/09 — vérifié dans le dépôt

| sujet | état | où |
|---|---|---|
| **tissu** en dynamique par positions (XPBD), sous-pas, collisions, auto-collision par hachage spatial, épingles à cible ; convergence **mesurée** (défaut 16 sous-pas × 2 itérations) | ✅ CPU | `NKPhysics/NkCloth.*` (1 775 lignes), `tests/test_cloth.cpp` |
| **7 vêtements procéduraux** : cape, foulard, jupe, t-shirt, chemise, robe, pantalon (+ chapeau rigide) ; plusieurs **panneaux** dans un même tissu ; **coutures** = contraintes de distance entre bords | ✅, mais **humanoïdes seulement** (mesures lues sur un squelette humain) ; pas de **vrai patron**, pas de pinces, pas d'aisance par zone, pas de matières (écrit dans le fichier) | `NKPhysics/NkGarment.*`, `tests/test_garment.cpp` |
| **champ de distance du corps** skinné : le tissu voit le maillage, pas seulement des capsules | ✅ | `NKPhysics/NkBodySDF.*` |
| mesures du corps (`NkBodyMeasures`) | ✅ humanoïde (épaules, taille, jambes…) | `NkGarment.h` |
| **rendu des cheveux** (shaders brins, 4 API) | ✅ shaders ; ⚠️ pas de toilettage, pas de simulation | `NKRenderer/Shaders/Hair/` |
| corps rigides, liaisons, ragdoll | ✅ | `NKPhysics` |
| cordes, chaînes, tiges élastiques | ❌ pas de solveur dédié | — |
| spécifications NkAnima : famille « Déformables » (tissu, vêtement, cheveux, fourrure, chair, corde), vent qui agit sur tout | 📝 spécifié | `NkAnimaEditor/design/07-produit-nkanima.md` |

---

## 2. Le principe commun — l'« élément secondaire »

Tout ce qui est **porté** par un personnage et **bouge par la physique** est un
**élément secondaire**. Cinq familles :

| famille | exemples | la forme (NKCraft) | le mouvement (NkAnima) |
|---|---|---|---|
| **A. Tissu** | vêtements, capes, pagnes, voiles, drapeaux, bannières, membranes molles | patrons, coutures, drapé au repos | tissu XPBD, puis contact précis (§4.10) |
| **B. Brins** | cheveux, barbe, sourcils, poils, fourrure, crinière, moustaches d'animaux, plumes souples | guides, coupe, densité, frisure, tresses | brins guides simulés, brins de rendu interpolés |
| **C. Chaînes** | **queues**, **cordes**, chaînes, colliers, ceintures pendantes, lianes, laisses, tentacules secondaires, oreilles souples, antennes, trompes | tubes ou maillons, points d'attache, raideur de repos | tiges élastiques (§6) |
| **D. Volumes mous** | ventre, joues, poitrine, fesses, bajoues, crête molle | zones de souplesse peintes | rebond (*jiggle*), chair |
| **E. Rigides attachés** | armures, casques, bijoux, carquois, armes au fourreau, sacs | pièces rigides | suivent un os ; collisions ; ballottement par liaison |

**Quatre règles valent pour toutes les familles** :

1. **Attaché par adresse** : un élément secondaire s'attache à la **peau** (04 §5.9) par
   des **adresses stables** (04 §3.2 : os, position le long de l'os, angle autour). Si le
   corps change (bras plus long, créature plus grosse), l'élément **suit**.
2. **Une couche à lui** : chaque élément est une **couche** du personnage, avec son
   auteur (humain, auto, IA) — il se régénère sans toucher au corps, et inversement.
3. **Prêt à simuler** : NKCraft livre à NkAnima la forme **et** ses réglages physiques
   par défaut (matière, attaches, collisions), que NkAnima peut ajuster.
4. **Deux qualités** : une version **film** (fine, simulée complète) et une version
   **jeu** (allégée : moins de particules, cartes de cheveux, chaînes courtes), tirées
   de la même source.

---

## 3. Les volumes de collision — fournis par le générateur

La simulation a besoin de savoir **où est le corps**. Le générateur de créatures
(document 4) le sait **par construction** :

- **capsules par os** (rapides, pour le jeu) : chaque os de C0 avec le rayon de son
  profil — **n'importe quelle créature**, pas seulement un humain ;
- **les volumes du blockout** (04 §5.8) : ellipsoïdes, capsules, superquadriques —
  des collisions simples **et fidèles** (un gros ventre, une épaule massive), gratuites
  puisque l'artiste les a déjà posés ;
- **champ de distance** du maillage skinné (`NkBodySDF`, existant) pour le film ;
- **zones sans collision** nommées (entre les doigts, sous les aisselles quand c'est
  voulu) et **zones de friction** (épaules pour une bretelle, hanches pour une
  ceinture).

📌 `NkBodyMeasures` (humanoïde) devient un cas particulier : les **mesures se lisent
sur le squelette C0 par noms et par rôles** (épaule, taille, cou, base de queue, base
d'aile…). Un vêtement pour un personnage à quatre bras, un harnais de cheval, une cape
de dragon qui laisse passer les ailes : même mécanisme.

---

## 4. Les vêtements

### 4.1 Le patron — comme un vrai tailleur

Un vêtement se conçoit **à plat**, en **panneaux** (les pièces de tissu), comme chez
un tailleur ou dans Marvelous Designer :

- chaque panneau : un **contour** (droites et courbes), des **pinces**, des **ourlets**,
  des **crans** de repère, le **droit-fil** (le sens du tissu) ;
- dimensions **en centimètres réels** ;
- **paramétrique** : un patron est une recette qui lit les **mesures** du corps (§3) —
  changer la créature regradue le patron (la **gradation** des tailleurs).

### 4.2 Les coutures

- Une couture relie **deux bords** de panneaux (ou deux bords d'un même panneau), avec
  un **sens** (endroit / envers) et une **longueur égale** — l'outil prévient si elles
  diffèrent (fronces volontaires : option).
- Types : couture simple, **fronces**, plis, **élastique** (taille, poignet), boutons et
  pressions (attaches ouvrables), fermeture.
- Aujourd'hui (`NkGarment`) : une couture = contraintes de distance entre bords ; on
  garde ce principe, **dessiné** au lieu d'être codé.

### 4.3 Le placement sur le corps

- Chaque panneau est posé **autour d'un os** (ou d'une zone) par des **points
  d'arrangement** — des adresses : « dos, entre les omoplates », « autour de la cuisse
  gauche ».
- Puis une **simulation d'assemblage** tire les coutures jusqu'à les fermer, sur le
  corps au repos (pose de référence).

### 4.4 Le drapé au repos — dans NKCraft

- NKCraft **simule** le vêtement **jusqu'à l'équilibre** (le `NkCloth` existant) : c'est
  la **forme de repos** livrée à NkAnima.
- Le drapé est **déterministe** : même patron, même corps → même forme au bit près.
- Pas d'animation ici : NKCraft s'arrête au vêtement **posé**.

### 4.5 Le maillage d'un vêtement — des quads par construction

- Chaque panneau est maillé en **grille de quads alignée sur le droit-fil** : la
  topologie est **propre par nature** (100 % quads), et le sens du tissu est connu de
  la simulation (étirement différent en chaîne et en trame).
- Les **coutures** apparient les bords avec le **même nombre de segments** (règle de
  parité du document 4 §5.4) : aucune jonction en T.
- **Épaisseur** : ajoutée à l'export (film) ou au rendu (jeu), jamais dans la
  simulation.
- **Adresse d'un point du vêtement** = `(vêtement, panneau, u, v)` en **coordonnées du
  patron** : c'est l'adresse stable qui garde les retouches (§8).

### 4.6 Les couches de vêtements

Sous-vêtement → chemise → gilet → veste → manteau → écharpe : un **ordre de couches**
est déclaré ; chaque couche entre en collision avec les couches en dessous. Un
vêtement peut être **ouvert** ou **fermé** (veste, pagne noué).

### 4.7 Les matières

Une **bibliothèque de tissus** (reliée à la bibliothèque de matières de NKCraft,
01 §4.4, qui compte déjà **wax, kente, ndop, bogolan**) : chaque tissu porte son
apparence **et** ses paramètres physiques :

| paramètre | sens |
|---|---|
| masse surfacique (g/m²) | lourd ou léger |
| souplesse en étirement (chaîne, trame), en cisaillement, en flexion | les compliances de `NkCloth` |
| frottement, épaisseur | contact |
| amortissement | le tissu « vit » ou s'arrête vite |

Tissus de départ : coton, lin, **wax**, **kente**, **ndop**, **bogolan**, **raphia**,
soie, jean, cuir, laine, toile de voile, filet. Les valeurs sont des **préréglages
mesurés** au banc (§10), pas des devinettes ; NkAnima peut les ajuster plan par plan.

### 4.8 Les gabarits de vêtements

Les 7 vêtements procéduraux existants deviennent des **gabarits de patrons** (cape,
foulard, jupe, t-shirt, chemise, robe, pantalon), et la bibliothèque s'élargit :

- **vêtements d'Afrique** : **boubou**, **kaba**, **agbada**, **pagne** noué, **gandoura**,
  **toghu**, **dashiki**, foulard de tête (**attaché** en nœud) ;
- **costumes et armures souples** : tunique, cape à capuche, toge, kimono, tablier,
  harnais ;
- **pour créatures** : caparaçon de cheval, cape de dragon (ouvertures pour les ailes),
  vêtement à quatre manches, voile de sirène.

Chaque gabarit est une **recette paramétrique** (longueurs, amples, ouvertures), lue sur
les mesures : il s'ajoute sans toucher au code (modèle `NkMeshFamilles`).

### 4.9 L'IA et les vêtements

- **Description → patron** : « un boubou brodé long, manches larges » → gabarit +
  paramètres + tissu (l'IA de langage déjà branchée, comme au document 4 §11.1 A).
- **Image → patron** : une photo ou une planche → paramètres du gabarit le plus proche.
  La recherche publiée montre que c'est faisable avec des patrons **paramétriques** :
  *GarmentCode* (Korosteleva & Sorkine-Hornung, SIGGRAPH Asia 2023) et le jeu de données
  *GarmentCodeData* (ECCV 2024) — **licences à vérifier** avant tout usage de leur code
  ou de leurs données ; leur **méthode** (patron = programme paramétrique) est
  exactement la nôtre.
- **Données** : comme au document 4, surtout **synthétiques** — nos gabarits, tirés au
  hasard, drapés sur nos créatures, rendus sous plusieurs vues.

### 4.10 Ce que NkAnima en fait

- **Tissu XPBD** existant pour le temps réel et la prévisualisation.
- Pour le **film** (contact exact, tissus superposés sans traversée), la voie de
  référence publiée est le **contact par potentiel incrémental** (*Codimensional
  Incremental Potential Contact*, Li et al., SIGGRAPH 2021) — plus lent, garanti sans
  interpénétration ; à étudier côté NkAnima (**licence du code à vérifier**, article
  seulement tant qu'elle ne l'est pas).
- Le **vent** de NkAnima agit sur tout (07-produit-nkanima).

---

## 5. Les cheveux, poils et fourrures

### 5.1 La forme — le toilettage dans NKCraft

- **Zones d'implantation** : cuir chevelu, sourcils, barbe, pelage — peintes sur le
  corps **par adresses** (elles suivent le corps s'il change).
- **Guides** : quelques centaines de courbes, que l'artiste **peigne**, **coupe**,
  **allonge**, **frise**, **groupe en mèches** ; les brins de rendu (des dizaines de
  milliers) sont **interpolés** entre les guides.
- **Cartes de densité, de longueur, de couleur** : peintes, par adresses.

### 5.2 Toutes les textures de cheveux — d'office

Un outil qui ne sait faire que des cheveux lisses est un outil faux (même principe que
le 01 §4.6 pour les visages). Types de départ, **paramétriques** :

| type | paramètres |
|---|---|
| lisse, ondulé, bouclé | amplitude, fréquence, torsion |
| **crépu** (coils 4A–4C), **afro** | frisure très serrée, volume, densité, rétrécissement |
| **tresses**, **nattes collées** (*cornrows*), **vanilles** (*twists*), **locks** | un **tressage** procédural le long d'une courbe guide : nombre de brins, serrage, taille |
| **coiffures tenues** : chignon, bantu knots, perruques, attaches | un guide + un attachement (élastique, épingle) |
| **fourrure** d'animal, **crinière**, plumes souples | longueur par zone, sens du poil, touffes |

Une tresse ou une lock est à la fois **brins** (rendu) et **chaîne** (simulation, §6) :
elle bouge comme un tout.

### 5.3 Deux qualités

- **Film** : brins interpolés, rendus par les shaders cheveux existants (`NKRenderer`).
- **Jeu** : **cartes de cheveux** (bandes de quads texturées, alignées sur les mèches) ou
  mèches en tubes — générées **depuis les mêmes guides**.

### 5.4 Ce que NkAnima en fait

Les **guides** sont simulés (tiges élastiques, §6), les brins de rendu suivent ; collision
avec la tête et les épaules (capsules et champ de distance du §3) ; vent.

---

## 6. Queues, cordes, chaînes et autres « tiges »

### 6.1 Ce que c'est

Tout ce qui est **long et fin** et **ploie** : queue (de chat, de lion, de dragon quand
elle suit l'inertie), corde, lasso, chaîne, collier, ceinture qui pend, laisse,
tentacule secondaire, oreilles souples, antennes, trompe, liane, fouet, cordon, **tresse**
et **lock**.

### 6.2 La forme dans NKCraft

- **Une chaîne de segments** avec une **forme de repos** (courbe) et un **profil** : tube
  (corde, queue — la topologie du document 4 §5.1), ou **maillons instanciés** (chaîne
  de métal : chaque maillon est un objet rigide).
- **Attaches** : une extrémité (queue, pendentif), les deux (corde tendue, hamac), ou
  des points intermédiaires (ceinture passée dans des passants) — par adresses.
- **Réglages physiques par défaut** : masse, raideur en flexion et en torsion,
  étirement (une corde ne s'allonge presque pas, un élastique si), amortissement,
  rayon de collision.
- **Pour une queue** du document 4 : la même chaîne d'os reçoit un **drapeau
  « secondaire »** — l'animateur l'anime à la main, et la physique **ajoute** l'inertie
  par-dessus (mélange réglable dans NkAnima).

### 6.3 Ce que NkAnima en fait

Simulation de **tiges élastiques** (position **et** orientation de chaque segment, pour
la torsion) : la méthode de référence en dynamique par positions est *Position and
Orientation Based Cosserat Rods* (Kugelstadt & Schömer, SCA 2016), cohérente avec le
XPBD de `NkCloth`. Les **maillons** d'une chaîne rigide passent par les corps rigides et
liaisons existants.

---

## 7. Chair, rebond et pièces rigides

- **Volumes mous** : l'artiste **peint** la souplesse (ventre, joues, poitrine,
  bajoues) ; NKCraft livre ces poids ; NkAnima ajoute le **rebond** secondaire.
- **Pièces rigides** (armure, casque, bijoux, armes) : attachées à un os par adresse,
  avec leur **proxy de collision** ; une pièce pendante (médaillon, boucle d'oreille)
  est une chaîne courte (§6).

---

## 8. Humain ↔ automatique ↔ IA — les mêmes garanties

Les règles du document 4 §10 s'appliquent telles quelles ; les **adresses** propres à
chaque famille les rendent possibles :

| élément | adresse stable | ce qui survit |
|---|---|---|
| vêtement | `(vêtement, panneau, u, v)` en coordonnées **du patron** | les **plis sculptés à la main** (couche de sculpt du vêtement, en repère local) survivent à un nouveau drapé, à une gradation, à un changement de tissu |
| patron | noms des panneaux et des bords | une pince ajoutée, un ourlet raccourci : opérations rejouées par nom |
| cheveux | `(zone, u, v)` sur le corps + indice **nommé** du guide | les guides peignés, coupés, frisés à la main restent quand l'IA change la coiffure ailleurs ; un guide dont la zone disparaît devient **orphelin**, gardé et montré |
| chaîne, queue, corde | `(chaîne, s)` le long de la tige | la forme de repos retouchée, les attaches déplacées |

- **L'IA propose, l'humain dispose** : « rends la cape plus longue » regradue le patron,
  **rejoue** les retouches et **reporte** les plis sculptés ; retirer une manche retouchée
  passe par un aperçu qui liste ce qui sera orphelin (04 §10.5).
- **Intégrer / détacher** (04 §10.6) : un pli sculpté peut être **intégré** en
  paramètre (plus d'ampleur), ou un drapé **figé** en sculpt pour être retouché
  librement.
- **NkAnima n'écrase pas NKCraft** : ce qui est réglé dans NkAnima (matière d'un plan,
  vent) vit dans la scène d'animation ; la **forme de repos** reste dans NKCraft.

---

## 9. Ce que NKCraft livre à NkAnima

Un **paquet par personnage** (format à décider avec NkAnima, D19) :

| contenu | pour |
|---|---|
| le corps riggé (document 4 : squelette, poids, blendshapes) | l'animation |
| les **volumes de collision** (capsules par os, champ de distance) | toutes les simulations |
| les **vêtements** : maillage de repos, patrons, coutures, couches, matières, épingles | tissu |
| les **cheveux** : zones, guides, cartes, types ; versions film et jeu | brins |
| les **chaînes** : formes de repos, attaches, réglages | tiges |
| les **zones molles** et les **pièces rigides** attachées | rebond, rigides |
| les **réglages physiques par défaut** de chaque élément | départ de NkAnima |

---

## 10. Critères — mesurés

| critère | cible |
|---|---|
| panneaux de vêtement | 100 % quads, droit-fil aligné, coutures sans jonction en T |
| drapé au repos | aucune particule sous la peau (champ de distance : pénétration 0), coutures fermées (écart ≤ 1 mm), déterministe au bit |
| gradation | le même patron habille le catalogue du document 4 (humain, trapu, cartoon, centaure, 4 bras) sans traversée |
| matières | chaque préréglage est **mesuré** au banc (test de drapé de Cusick sur un disque, test de flexion en porte-à-faux) et documenté |
| cheveux | toutes les textures du §5.2 produites, versions film et jeu depuis les mêmes guides |
| chaînes | une corde attachée aux deux bouts, une queue, un collier : forme de repos exacte, aucune auto-traversée au repos |
| coexistence | trois tours prompt → retouche → prompt sur un personnage habillé et coiffé **sans perte de retouche** (jalon C) |
| temps réel (jeu) | budget par personnage à fixer avec Nogee (D20) |

---

## 11. Phasage

| palier | contenu | critère de fin |
|---|---|---|
| **V0** | volumes de collision tirés du générateur (§3) ; mesures lues sur C0 par rôles ; les 7 vêtements existants habillent **toutes** les créatures humanoïdes du catalogue | t-shirt, robe, pantalon sur les 4 humanoïdes du catalogue, sans traversée |
| **V1** | **patrons** : panneaux, coutures dessinées, placement par adresses, drapé au repos, maillage en quads alignés ; matières mesurées | un boubou et un pantalon dessinés en patron, drapés, livrés à NkAnima |
| **V2** | couches de vêtements ; gabarits d'Afrique et pour créatures ; gradation | une tenue en 3 couches ; une cape de dragon avec ouvertures d'ailes |
| **V3** | **cheveux** : zones, guides, peigne, coupe, frisure, types du §5.2, tresses et locks ; cartes pour le jeu | une coiffure crépue, des nattes collées, des locks — film et jeu |
| **V4** | **chaînes** : queues secondaires, cordes, chaînes à maillons, parures ; zones molles ; rigides attachés | un personnage à queue, collier et ceinture pendante, livré complet |
| **V5** | coexistence (adresses des vêtements, cheveux, chaînes ; plis sculptés ; orphelines) | le critère « trois tours sans perte » habillé et coiffé |
| **V6** | IA : description → patron / coiffure ; image → paramètres ; données synthétiques | « un boubou long et une coiffure en vanilles » produit, retouché, repris |

📌 **Côté NkAnima** (proposé, **rien n'est modifié** dans ses documents) : tiges élastiques
(Cosserat), cheveux simulés par guides, rebond de chair, et, pour le film, un contact
sans interpénétration pour les tissus superposés.

---

## 12. Sources

- Müller, Heidelberger, Hennix, Ratcliff — *Position Based Dynamics*, 2006/2007
- Macklin, Müller, Chentanez — *XPBD: Position-Based Simulation of Compliant Constrained Dynamics*, MIG 2016
- Macklin et al. — *Small Steps in Physics Simulation*, SCA 2019
- Kugelstadt, Schömer — *Position and Orientation Based Cosserat Rods*, SCA 2016
- Li et al. — *Codimensional Incremental Potential Contact*, SIGGRAPH 2021 — https://ipc-sim.github.io/C-IPC/ (code : https://github.com/ipc-sim/Codim-IPC, **licence à vérifier**)
- Korosteleva, Sorkine-Hornung — *GarmentCode: Programming Parametric Sewing Patterns*, SIGGRAPH Asia 2023 — https://github.com/maria-korosteleva/GarmentCode (**licence à vérifier**)
- Korosteleva et al. — *GarmentCodeData: A Dataset of 3D Made-to-Measure Garments With Sewing Patterns*, ECCV 2024 — https://igl.ethz.ch/projects/GarmentCodeData/ · https://arxiv.org/abs/2405.17609 (**licence des données à vérifier**)
- Umetani, Kaufman, Igarashi, Grinspun — *Sensitive Couture for Interactive Garment Modeling and Editing*, SIGGRAPH 2011 (édition du patron ↔ drapé en direct)
- Existant Rihen : `NKPhysics/NkCloth.h` (XPBD, mesures de convergence), `NkGarment.h`, `NkBodySDF.h`, `NKRenderer/Shaders/Hair/`

---

## 13. Décisions

| # | question | proposition | état |
|---|---|---|---|
| D16 | la frontière | **forme au repos** et préparation à la simulation dans NKCraft ; **mouvement physique** dans NkAnima ; version **temps réel** dans Nogee | ✅ Rodolf, 28/09 (confirme D4, F2, F3) |
| D17 | les vêtements | par **patrons** (panneaux, coutures), paramétriques et gradués sur **toute** créature, drapés au repos dans NKCraft | 🔴 proposé |
| D18 | les cheveux | toilettage par guides ; **toutes les textures d'office** (crépus, tresses, nattes collées, vanilles, locks) ; versions film et jeu depuis les mêmes guides | 🔴 proposé |
| D19 | le paquet NKCraft → NkAnima | format à définir ensemble (§9) | 🔴 à décider avec NkAnima |
| D20 | le budget temps réel par personnage (particules, guides, chaînes) | à fixer avec Nogee | 🔴 |
| D21 | les queues du document 4 | chaîne d'os avec drapeau **secondaire** : animée à la main, inertie physique **ajoutée** par-dessus dans NkAnima | 🔴 proposé |
