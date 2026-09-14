<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

# `Captures/` — ce qui est versionné ici, et ce qui ne l'est pas

## 🔴 LA RÈGLE, en une phrase

> **Une image qui fait foi dans un document est versionnée.
> Une image de travail ne l'est pas.**

Décidée par Rodolf le **2026-09-02**, en retirant `/Captures/` du `.gitignore`
sous une seule condition : *« je dois limiter le nombre de fichiers lourds. »*

**Pourquoi cette règle existe, et pourquoi elle est écrite ici plutôt que devinée.**
Ce dossier portait **950 images** — rendus intermédiaires, essais, artefacts — et
il était **ignoré en bloc**. Conséquence mesurée le 02/09 : la **carte des
7 plateformes** de `Engine/Noge/DECISIONS_RODOLF.md` s'appuyait sur des images
qui n'étaient **dans aucun dépôt**. Un verdict dont la preuve peut disparaître
d'un coup de ménage n'est pas un verdict — c'est un souvenir.

Mais l'inverse coûtait autant : tout versionner alourdit l'historique **pour
toujours**, et un historique ne se dégonfle pas.

D'où l'**allowlist nommée** du `.gitignore` : `/Captures/*` ignore le contenu,
puis chaque image qui fait foi est ré-autorisée **une par une** (seule la famille
`plateforme_*` passe par un motif, parce qu'elle est fermée et nommée).

⚠️ **Sans cette règle écrite, le dossier redevient un fourre-tout en trois
semaines**, et quelqu'un remet l'ignore global — en emportant les preuves avec.

## ✅ Pour ajouter une image ici

Trois conditions, dans cet ordre :

1. **elle est CITÉE par un document versionné** — c'est le test, et il est
   vérifiable : `grep -rl "<nom du fichier>" --include=*.md .` doit rendre au
   moins un fichier. Une image que personne ne cite n'a rien à prouver ;
2. **on ajoute son nom au `.gitignore`**, explicitement (pas un dossier, pas un
   `*` élargi) ;
3. **on la pèse.** Une capture de HUD n'a pas besoin d'être un PNG sans perte :
   si l'image dépasse ~1 Mo, la ré-encoder en JPEG de qualité raisonnable ou
   réduire la définition **avant** de la commiter. Ce qui doit rester lisible,
   c'est le texte du HUD.

## 📋 Les 9 images versionnées au 2026-09-02 — **2 292 069 octets** au total

### La base de preuves de la carte des plateformes

| image | octets | ce qu'elle prouve |
|---|---:|---|
| `plateforme_windows.png` | 528 160 | Windows, 29/07 15h34 — scène 3D, **142,1 FPS**. Jumelle de la capture Linux, à 4 minutes d'écart |
| `plateforme_linux.png` | 522 450 | **Linux ✅**, 29/07 15h38 — `Demo 3D | API : OpenGL`, `Shadow tweak`, PBR + ombres, **81,6 FPS** |
| `nk_android_demo3d.png` | 494 385 | **Android ✅** — 18 sphères PBR, ombres douces, **59 FPS**, `VSM atlas 4096 px`. La preuve 3D la plus forte du dossier |
| `plateforme_web_2026-09-02.png` | 344 797 | **Web 🟡** — HUD lu, `Draw:1093 Tris:489586`, mais **rendu logiciel** (SwiftShader) |
| `plateforme_harmonyos_2026-09-02.jpeg` | 154 674 | **HarmonyOS ✅**, 02/09 19h31 — `Demo 3D | API : OpenGL`, `Shadow tweak`, `FPS approx : 8.3`, 17 sphères PBR + ombres. Sur le `.hap` du 10/08 |

| `noge_vehicule_2026-09-04.png` | 297 091 | **La voiture roule** — 04/09, `renderdemo` OpenGL : châssis + 4 roues sur `NkVehicle`, ombre portée, `Draw:1243` contre 1093 sans elle. Citée par `echanges_noge.md` et `CONCEPTION_VEHICULE.md` |

| `noge_particules_2026-09-04.png` | 354 667 | **Les particules rendent** — 04/09, `renderdemo` OpenGL : 502 vivantes, `Draw:1094` et `Tris:490542` contre 1093/489588 sans elles. Citée par `DECISIONS_RODOLF.md` bloc 13 |
| `noge_particules_melange_2026-09-04.png` | 307 828 | **Le mélange déclaré est celui qui rend** — 04/09, `renderdemo --demo=2`, OpenGL, frame 170, même émetteur (400/s) : à gauche `NK_ADDITIVE` (cœur blanc saturé : 2 426 px quasi blancs), à droite `NK_ALPHA` (disques orange opaques : 1 027 px, soit le fond seul ≈ 1 026). Avant, les deux rendaient pareil. Cité par `DECISIONS_RODOLF.md` bloc 13 |
| `noge_particules_texture_2026-09-04.png` | 37 981 | **La texture déclarée est celle qui rend** — 04/09, `renderdemo --demo=2`, OpenGL, une particule immobile portant un damier 2×2 magenta/vert (64×64) : quatre cellules (1 386 / 837 / 1 710 / 1 667 px), deux diagonales perpendiculaires (|cos| 0,19) qui se croisent (écart 5,7 px). Extrait 180×180 agrandi ×2. Cité par `DECISIONS_RODOLF.md` bloc 13 |
| `noge_particules_instancie_2026-09-04.png` | 202 759 | **Le quad s'expanse sur le GPU** — 04/09 soir, `renderdemo --demo=2`, OpenGL, même particule damier après instanciation (six coins statiques + un enregistrement de 24 o par particule) : quatre cellules lisibles, boîte identique à ±2 px ; à 50 000, envoi 9 277 → 1 168 Ko. Citée par `DECISIONS_RODOLF.md` (lot A) |
| `noge_fluide_dam_break_2026-09-04.png` | 205 983 | **Un fluide SPH se dessine par le stockage** — 04/09 nuit, `renderdemo --demo=2`, OpenGL, `NK_SPH_PROBE=1 NK_SPH_SCENE=dam`, image 40 : 4 096 particules bleues (quad instancié, mélange alpha). Témoin du CHEMIN, pas de la physique : repos et front sont ROUGES ce soir (`DECISIONS_RODOLF.md`, bloc SPH) |
| `noge_fluide_dam_break_dfsph_2026-09-04.png` | — | **Le fluide tient (DFSPH)** — 04/09 nuit, `renderdemo --demo=2` OpenGL **Release**, `NK_SPH_PROBE=1 NK_SPH_SCENE=dam`, image 40 : 4 096 particules + 15 040 fantômes, nappe cohérente qui avance et remonte sur le mur opposé. Repos ρ/ρ₀ = 1,001 (sol compris), vmax 0,04 m/s à 10 s ; front de dam break sous Ritter (dit dans `DECISIONS_RODOLF.md`, bloc DFSPH) |
| `noge_fluide_dam_break_morris_2026-09-04.png` | 202 048 | **Le front tient avec une viscosité physique (Morris 1997)** — 04/09 23h, `renderdemo --demo=2` OpenGL **Release** (binaire reconstruit après la coupure), `NK_SPH_PROBE=1 NK_SPH_SCENE=dam`, image 40 : 4 096 particules + 15 040 fantômes, ν = 0,02 m²/s (Re ≈ 160) avec sa condition de pas mesurée (2 sous-pas). Repos 10 s ρ/ρ₀ = 1,001 (sol 1,001), vmax 0,015 m/s ; front à 10 % de Cébron & Sigrist (12 % à h/2, même colonne), 9 % de Martin & Moyce sur sa géométrie n² = 2. Citée par `DECISIONS_RODOLF.md` (bloc Morris) |
| `noge_tissu_drape_2026-09-05.png` | 494 688 | **Le tissu XPBD drape une sphere, dans le vent** -- 05/09, `renderdemo --demo=2`, OpenGL, `NK_CLOTH_PROBE=1 NK_CLOTH_N=64 NK_CAPTURE=150` (pas fixe 1/60, image 150 = t 2,5 s) : nappe 64 x 64 de 2,4 m (orange Rihen, maillage dynamique `NkMeshSystem::UpdateVertices`) lachee sur une sphere R = 0,8 m (petrole Rihen) en l air, vent uniforme 0,4 m g par particule projete sur la normale (le pan de droite se souleve), auto-collision active, 32 sous-pas x 4 iterations ; penetration 0,000 mm, etirement max 3,3 %, 42-60 ms par pas. La physique est prouvee par `NKPhysics_Tests` (temoins (a)-(g), 83 passes), pas par cette image. Citee par `DECISIONS_RODOLF.md` bloc « tissu » |

### Les deux témoins INVALIDÉS — versionnés exprès

| image | octets | pourquoi on la garde |
|---|---:|---|
| `nk_harmony_renderdemo.jpeg` | 65 856 | La capture du **29/07** qui a fait classer HarmonyOS en **échec à tort**. Relue le 02/09 : ni `Demo 3D`, ni `Shadow tweak` — **c'est la démo 0**, elle n'a jamais exercé la 3D |
| `nk_web_headless.png` | 4 407 | Même histoire : prise à la **première image, une seule passe** (la mesure du 02/09 en donne 22). Elle testait la création du contexte, pas le rendu |

> 📌 **Une preuve réfutée reste une pièce du dossier.** Ces deux images sont
> celles qui ont produit deux verdicts faux ; on ne peut pas raconter la
> correction sans pouvoir les rouvrir. *Un témoin invalidé contamine tout ce
> qu'il a jugé* — encore faut-il pouvoir montrer le témoin.

### Les chargeurs de modèles

| image | octets | citée par |
|---|---:|---|
| `model_loaders/fbx_FuturisticCar.png` | 90 368 | `Engine/Noge/echanges_noge.md` |
| `model_loaders/usda_cube.png` | 86 972 | `Engine/Noge/echanges_noge.md` |

## 🔍 Comment vérifier que l'allowlist n'a pas dérivé

```sh
# doit lister exactement les fichiers de l allowlist, et rien d autre.
# ⚠️ LE COMPTE ETAIT ECRIT EN DUR ICI (« les 9 fichiers ») et il a VIEILLI en
# silence : deux images se sont ajoutees le 07/09. Un compte fige a cote d une
# liste est le motif que ce depot a deja paye -- la liste et son compte separes.
# On COMPTE la liste, on ne la recite pas :
#   grep -c '^!/Captures/' ../.gitignore   # les entrees de l allowlist (dont
#                                            # LISEZMOI.md et deux prefixes)
git status --porcelain --untracked-files=all Captures/

# contrôle NÉGATIF — une image de travail doit rester ignorée,
# y compris dans un sous-dossier autorisé
printf 'x' > Captures/essai.png
printf 'x' > Captures/model_loaders/essai.png
git check-ignore -v Captures/essai.png Captures/model_loaders/essai.png
rm Captures/essai.png Captures/model_loaders/essai.png
```

*Le second est le seul qui prouve quelque chose : une allowlist qui laisse tout
passer a exactement la même apparence qu'une allowlist juste, tant qu'on ne lui
présente pas un cas qu'elle doit refuser.*

- `noge_vetements_marche_2026-09-05.png` (535 494 o) — VETEMENTS SUR MANNEQUIN EN MARCHE : CesiumMan skinne (3 273 sommets, 19 os, son clip de marche de 2 s), avance 1,2 m/s poussee par le code (clip in place), cape (orange Rihen) + foulard + chapeau rigide, corps en petrole Rihen. `renderdemo --demo=2 NK_MANNEQUIN_PROBE=1 NK_MANNEQUIN_SDF_BOX=1 NK_MANNEQUIN_SDF_CELL=12 NK_CAPTURE=150` (refaite le 05/09 a 13h55 : un CHAMP DE DISTANCE PAR VETEMENT, cellule de 12 mm -- le foulard y tient son critere double, 0 particule sous la peau et 0,18 % d etirement moyen). Ma fenetre seule (relecture de la cible finale), premier plan lu avant, NKIlyana.exe (PID 29432) presente avant et apres, jamais touchee. DIT : le mannequin est petit et partiellement masque par le decor de la demo (le cube central) — trois cadrages payes ; la scene de demonstration n a pas ete concue pour cadrer un personnage.

### La fumée et le feu sur GRILLE (05/09 nuit) — `NkFluidGridProbe`

| image | octets | ce qu'elle prouve, et ce qu'elle ne prouve pas |
|---|---:|---|
| `fumee_colonne_2026-09-05.png` | 11 183 | **Le rendu de volume par marche de rayon FONCTIONNE** — 480 x 360, grille 25 x 80 x 25 (h = 2 cm), 255 pas. Mesuré DANS ces pixels : la colonne est **74 fois plus opaque au centre** qu'à ses bords (333,6 contre 4,5 sur des fenêtres de même taille, 672 et 756 px), et la bande basse est **1,34 fois** plus opaque que la bande haute (260,4 contre 194,2 sur 1 449 px chacune). Contrôle négatif au même cadrage : densité nulle -> **0 pixel** différent du fond sur 172 800. ⚠️ **Ce n'est pas un rendu GPU** : marche de rayon **CPU** (`NkFluidGridRaymarch`), aucune fenêtre, aucun device. ⚠️ La colonne est **fine et droite** parce qu'il n'y a **pas de confinement de vorticité** (Fedkiw 2001 § 4, non fait) : c'est ce qui donne aux vraies fumées leurs volutes. |
| `feu_degrade_2026-09-05.png` | 28 985 | **La température devient une couleur de corps noir** — 480 x 360, grille 32 x 64 x 32 (h = 1,25 cm), combustion réelle (carburant qui se consume), Tmax 1 691 K. Mesuré dans ces pixels, **par différence avec le même rendu sans émission** (sinon on mesure le gris de la fumée) : sur 6 456 pixels émissifs, le tiers des rayons les plus FROIDS (<= 1 183 K) rend un G/R émis de **0,207**, le tiers les plus CHAUDS (>= 1 437 K) de **0,409** — l'écart et le sens de la table de Planck. ⚠️ Rendu **CPU**, pas GPU. ⚠️ La flamme est un **jet fin** : la poussée d'Archimède à 1 700 K vaut ~46 m/s² et rien ne la fait tournoyer (même manque de confinement de vorticité). |

Les deux sont citées par `Engine/Noge/DECISIONS_RODOLF.md`, bloc « 05/09 (nuit) —
LA FUMÉE ET LE FEU SUR GRILLE ».

### Le CONFINEMENT DE VORTICITÉ (06→07/09 nuit) — `NkFluidGridProbe`

**Ces deux-là se regardent CÔTE À CÔTE, et c'est tout leur intérêt** : même scène,
même graine, même nombre de pas (255), même caméra, même rendu. **Seul `epsilon`
change.** Elles répondent à la remarque du 05/09 écrite deux lignes plus haut — *la
colonne est fine et droite parce qu'il n'y a pas de confinement de vorticité*.

| image | octets | ce qu'elle prouve, et ce qu'elle ne prouve pas |
|---|---:|---|
| `fumee_jet_sans_confinement_2026-09-07.png` | 11 183 | **Le JET** — `epsilon = 0`, l'état du 05/09. Grille 25 × 80 × 25 (h = 2 cm), 255 pas. Mesuré DANS ces pixels : la boîte du panache fait **45 px** de large ; à la ligne y = 84 (tiers supérieur de cette boîte, calée sur la boîte mesurée et non écrite à la main), le rayon de giration pondéré par l'opacité vaut **5,329 px** sur 32 pixels. |
| `fumee_panache_confinement_2026-09-07.png` | 25 043 | **Le PANACHE** — `epsilon = 8` (Fedkiw, Stam & Jensen, SIGGRAPH 2001, § 4, eq. 9-11). Boîte **72 px** de large ; à la MÊME ligne y = 84, rayon **13,124 px** sur 68 pixels — **× 2,46**. Un second instrument, qui lit le champ de densité et non les pixels, dit **× 1,85** sur le rayon à y = 0,60 m : deux chemins indépendants, le même sens. ⚠️ **Le confinement se PAIE** : sur la même scène la dérive de masse passe de −29,3 % à −42,5 % et la divergence résiduelle de 0,39 % à 6,33 %. ⚠️ **Le rayon SEUL ne départage pas le signe de la force** : avec la force *inversée* le panache est encore plus large (× 3,59). C'est la **concentration** de la vorticité qui sépare les deux (× 1,38 avec, × 0,42 contre). |

⚠️ **CE QUE L'ŒIL VOIT, ET CE QU'IL NE VOIT PAS — regarder une image est une
mesure, avec ses conditions de validité.** Les deux images ont été REGARDÉES, pas
seulement mesurées. Ce qui change est net : le jet est une colonne lisse et sans
détail ; le panache est **plus large et STRUCTURÉ**, avec des bouffées internes
visibles. ⚠️ Mais **il ne montre pas encore de grosses volutes qui s'enroulent** :
la boîte fait 0,5 m de côté et la caméra est proche, donc les tourbillons que le
confinement entretient restent de la taille de quelques cellules. « Plus large et
structuré » est ce que ces deux images prouvent ; « ça tournoie » serait dire plus
que ce qu'elles montrent.

⚠️ **Ce ne sont pas des rendus GPU** : marche de rayon **CPU**
(`NkFluidGridRaymarch`), aucune fenêtre, aucun device — comme les deux du 05/09.

Elles sont citées par `Engine/Noge/DECISIONS_RODOLF.md`, bloc « 06→07/09 (nuit) —
LE CONFINEMENT DE VORTICITÉ, ET LA GRILLE BRANCHÉE ».

- `noge_eau_eclaboussure_impact_2026-09-06.png` (12195 o) — LES ECLABOUSSURES
  (ROADMAP_PRODUITS.md §6.6 palier 1). Un contact d'impact a 4,0 m/s, oblique
  (v = (2, -4, 0)), et les **14 gouttes** que `math::NkSplashEmit` en fait naitre :
  la vitesse incidente en pointille rouge, le sol, la trajectoire balistique de
  chaque goutte sur 0,45 s (petrole Rihen) et sa position a 0,15 s (orange Rihen).
  Produite par `Build/Tests/Release-Windows/NKRenderer_Tests.exe` (temoins de
  `Kernel/Runtime/NKRenderer/tests/test_eau_sph.cpp`).
  ⚠️ **CE N'EST PAS UNE CAPTURE DU MOTEUR.** Aucun GPU, aucune fenetre : c'est un
  trace dessine par le banc a partir des chiffres qu'il vient de mesurer. La
  difference avec les autres images de ce dossier est reelle et je la dis :
  celles-la montrent ce que le rendu produit, celle-ci montre ce que la LOI
  produit. Une capture du rendu demande une scene de demonstration d'eclaboussures,
  qui n'existe pas encore. NKIlyana.exe (PID 29432) presente avant et apres,
  jamais touchee -- et pour cause : rien ici ne s'approche de la carte.

- `noge_eau_mouillage_avant_apres_2026-09-06.png` (25416 o) — LE MOUILLAGE
  (§6.6 paliers 2-3, demande explicite de Rodolf le 05/09 : « n'oublie pas l'effet
  mouille »). Trois panneaux de la MEME carte `math::NkWetnessMap` 220 x 220, en
  espace UV, sur un sable clair : **sec** | **mouille** par trois depots de contact
  (canal moyen 0,1664) | **apres 5 tau de sechage** (0,001121, tau = 5 s). La
  couleur de chaque texel est `math::NkApplyWetness` -- la formule de Sebastien
  Lagarde, « Water drop 3b - Physically based wet surfaces », 2013 : albedo x0,2 et
  rugosite x0,6 a saturation sur une matiere de porosite 1.
  ⚠️ Meme avertissement : trace du banc, pas capture du rendu. Le nuanceur PBR ne
  lit PAS encore cette carte (l'entree materiau n'est pas branchee -- nommee dans
  le rapport) ; ce que l'image montre est la valeur que le nuanceur RECEVRAIT.


### Les volutes : BOÎTE ou SCHÉMA ? (12/09) — `NkFluidGridProbe`, mode `NK_FLUID_VOLUTES=1`

| image | octets | ce qu'elle prouve, et ce qu'elle ne prouve pas |
|---|---:|---|
| `fumee_panache_h1cm_2026-09-12.png` | 23 687 | **Le même panache que `fumee_panache_confinement_2026-09-07.png`, à h = 1 cm au lieu de 2** — même boîte 0,5 × 1,6 × 0,5 m, même source, même `dt`, mêmes 255 pas, même `epsilon = 8`, **même caméra** ; 25 × 80 × 25 → **50 × 160 × 50 = 400 000 cellules**, 3,3 s par pas sur un fil. Elle se regarde À CÔTÉ de celle du 07/09 : **c'est une aide à l'œil, pas une preuve** — la preuve est la mesure, dans `PLAN_VOLUTES.md` et le bloc du 12/09 de `DECISIONS_RODOLF.md`. Mesuré sur la grille (pas sur les pixels) : l'échelle des structures tourbillonnaires vaut **2,20 cellules à 2 cm et 4,77 cellules à 1 cm** — soit **0,044 m → 0,048 m, constante en MÈTRES** (0,4 fois le diamètre de la source). ⚠️ **Ce que l'œil voit, exactement** : la colonne à 1 cm est **plus fine et plus filamentée** (des stries, des bouffées mieux découpées), **pas plus enroulée** — les structures ont la même taille en mètres, elles sont seulement mieux résolues. **Elle ne montre pas de grosses volutes non plus**, et la mesure dit pourquoi ce n'est pas la grille qui les cache : leur taille est physique, ~4-5 cm sur une source de 12 cm, à 3 pixels par centimètre. ⚠️ Rendu **CPU** (`NkFluidGridRaymarch`), aucune fenêtre, aucun device. |


### AVANT / APRÈS la GRILLE DÉCALÉE (MAC) et l'ADVECTION EN FLUX (13/09) — `NkFluidGridProbe`

**Décision de Rodolf, 13/09 : on GARDE les anciennes en les DATANT, et on ajoute
les neuves.** *Une preuve réfutée reste une pièce du dossier* — c'est déjà la
doctrine appliquée aux deux témoins invalidés plus haut. Une image qu'on écrase
emporte avec elle la possibilité de raconter ce qui a changé.

#### ① Les QUATRE images d'AVANT — inchangées à l'octet, et c'est volontaire

Elles montrent l'état du solveur **AVANT la bascule sur grille décalée (MAC) et
avant l'advection conservative en flux**. Elles ne sont pas fausses : elles sont
**datées**.

| image | octets | produite par le commit | ce qu'elle date |
|---|---:|---|---|
| `feu_degrade_2026-09-05.png` | 28 985 | `e2692098d` (06/09) | grille COLOCALISÉE, advection semi-lagrangienne seule |
| `fumee_colonne_2026-09-05.png` | 11 183 | `e2692098d` (06/09) | idem |
| `fumee_jet_sans_confinement_2026-09-07.png` | 11 183 | `105baf2ce` (07/09) | idem, avec le confinement de vorticité ajouté (ici `epsilon = 0`) |
| `fumee_panache_confinement_2026-09-07.png` | 25 043 | `105baf2ce` (07/09) | idem, `epsilon = 8` |

#### ② LES DEUX JUMELLES : le même objet git, et c'est VOULU — démontré, pas supposé

`fumee_colonne_2026-09-05.png` et `fumee_jet_sans_confinement_2026-09-07.png` sont
**le même objet git**, `2eb5b17f2`. Ce n'est pas une copie involontaire, et voici
les trois pièces qui le montrent :

1. **LE CODE.** Les deux sont rendues par **le même appel, avec les mêmes quatre
   arguments** — `ConstruirePanache(g, false, 255, 0.f)` (`rendu.cpp`, palier ② et
   `ImagesDuConfinement`), même caméra, même rendu. Le solveur est **déterministe
   et sans GPU** : deux appels identiques *doivent* rendre les mêmes octets.
2. **L'HISTORIQUE.** Le blob de `fumee_colonne` est **le même à `e2692098d` et à
   `105baf2ce`** : la colonne n'a pas bougé entre le 06 et le 07/09, et c'est
   normal — le confinement ajouté le 07/09 vaut `epsilon = 0` par défaut. L'image
   du jet, écrite par une **autre ligne de code** le 07/09, est sortie identique.
3. **LA CONTRE-ÉPREUVE D'AUJOURD'HUI, et c'est elle qui tranche.** La course du
   13/09 a **re-rendu les deux, séparément** (08h02 et 08h21, deux appels de
   `NkFluidRaymarchRender` distincts) : les deux fichiers neufs sont **de nouveau
   identiques à l'octet** (`130e9122…`). Deux rendus séparés produisant les mêmes
   octets, aujourd'hui, sur un code différent de celui de septembre : l'identité
   est une **propriété de la scène**, pas la trace d'un `cp`.

> ⚠️ **CE QUI RESTE VRAI MALGRÉ TOUT : le NOM du 07/09 fait mal croire.**
> « colonne » et « jet sans confinement » désignent la **même scène**. Le second
> nom n'a de sens que **par paire** avec `fumee_panache_confinement_*`, dont il est
> le bras `epsilon = 0`. Ce n'est pas un doublon accidentel, c'est un **doublon
> assumé** — et le dire coûte moins cher que de laisser quelqu'un le redécouvrir.

#### ③ Les QUATRE images d'AUJOURD'HUI — mêmes scènes, code de `main` (9c3fad332)

**Configuration commune**, écrite ici plutôt que devinée : binaire
`Build/Bin/Release-Windows/NkFluidGridProbe`, **Release**, **aucun GPU, aucune
fenêtre, aucun device** (marche de rayon **CPU**, un seul fil), rendu **480 × 360**,
caméra `(0 ; 0,38 ; 2,10)` visant `(0 ; 0,30 ; 0)`, champ 40°. Course complète du
**13/09, 08h07 → 08h21 — 14 min 30 s** sur cette machine (et non ~27 min : le
chiffre dépend de la machine et de la charge, il est publié avec les siens).

| image | octets | scène, et ce qui est MESURÉ DANS CES PIXELS |
|---|---:|---|
| `fumee_colonne_2026-09-13.png` | 11 701 | Palier ②. Boîte 0,5 × 1,6 × 0,5 m, `h = 2 cm` → **25 × 80 × 25**, `dt = 1/60 s`, **255 pas**, `epsilon = 0`. (2.1) centre **245,67** sur 1 680 px contre **0,00** aux deux bords (1 764 px chacun) — rapport **SATURÉ**, pas un nombre ; (2.2) bande basse **122,30** contre bande haute **89,54** → **1,37**. Contrôle négatif au même cadrage : densité nulle → **0 pixel** sur 172 800. Coût du rendu : **518,0 ms** à 480 × 360 (1 501 910 échantillons + 4 440 358 d'ombre) |
| `fumee_jet_sans_confinement_2026-09-13.png` | 11 701 | **Le MÊME appel que la précédente** — voir ② : c'est le bras `epsilon = 0` de la comparaison. Boîte du panache dans l'image : **108 px** de large ; à la ligne y = 84, rayon de giration pondéré par l'opacité **5,273 px** sur 31 px |
| `fumee_panache_confinement_2026-09-13.png` | 31 359 | `epsilon = 8` (Fedkiw, Stam & Jensen, SIGGRAPH 2001, § 4, eq. 9-11). À la MÊME ligne y = 84 : rayon **16,122 px** sur 86 px — **× 3,06** (contre × 2,46 le 07/09) |
| `feu_degrade_2026-09-13.png` | 18 452 | Palier ③, **combustion réelle** (carburant qui se consume). **Tmax de la scène 1 836 K** ; mesuré par DIFFÉRENCE avec le même rendu sans émission : sur **7 138 px** émissifs (rayons de 927 à 1 835 K), G/R émis du tiers **FROID** (≤ 1 230 K) **0,222** (1 748 px) contre tiers **CHAUD** (≥ 1 533 K) **0,538** (3 142 px). Rendu **5 389 ms** |

#### ⚠️ CE QUI A CHANGÉ ENTRE LES DEUX SÉRIES, et il faut le dire

**La boîte du jet a plus que DOUBLÉ, et la comparaison de boîtes s'est INVERSÉE.**

| | 07/09 (avant MAC) | 13/09 (après) |
|---|---:|---:|
| boîte SANS confinement | 45 px | **108 px** |
| boîte AVEC confinement | 72 px | **92 px** |
| rayon à y = 84, SANS | 5,329 px | 5,273 px |
| rayon à y = 84, AVEC | 13,124 px | **16,122 px** |

Le 07/09, la boîte disait « le panache est plus large que le jet ». **Aujourd'hui
elle dit l'inverse**, pendant que le rayon de giration, lui, dit la même chose en
**plus fort** (× 2,46 → × 3,06).

**Ce n'est pas une contradiction, c'est deux grandeurs différentes** : la *boîte*
est l'enveloppe de **tout pixel au-dessus du seuil**, donc elle suit la fumée la
plus **diluée** ; le *rayon de giration* pèse chaque pixel par son **opacité**,
donc il suit la **matière**. Le jet d'aujourd'hui s'étale davantage en voile
ténu — ce que confirme la dérive de masse mesurée sur cette scène — sans pour
autant concentrer plus de fumée loin de l'axe.

> 📌 **Le témoin (i1) juge sur le RAYON, pas sur la boîte**, et c'est pour cette
> raison-là. La boîte est publiée à côté, comme information, pas comme verdict.
> Une ligne du LISEZMOI du 07/09 (« boîte 45 px → 72 px ») est donc **datée** :
> elle décrivait un état, elle ne décrit plus celui-ci.

⚠️ **Et un témoin du confinement est ROUGE** sur le code d'aujourd'hui : **(v1)
l'enstrophie**, qui devrait au moins DOUBLER avec le confinement, vaut
**× 0,36** (19,196880 → 6,940482). C'est l'un des **7 ROUGES** des **84 contrôles**
de la course du 13/09. L'image du panache montre donc bien un panache **plus
large et plus structuré** ; elle **ne prouve pas** que le confinement réinjecte de
la vorticité sur cette scène-là — la mesure dit aujourd'hui le contraire, et c'est
un chantier ouvert, pas un détail.

⚠️ **Ce ne sont pas des rendus GPU** : marche de rayon **CPU**
(`NkFluidGridRaymarch`), aucune fenêtre, aucun device — comme toutes les images de
`NkFluidGridProbe`.
