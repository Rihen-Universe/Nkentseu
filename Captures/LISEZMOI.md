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

⚠️ **Ce ne sont pas des rendus GPU** : marche de rayon **CPU**
(`NkFluidGridRaymarch`), aucune fenêtre, aucun device — comme les deux du 05/09.

Elles sont citées par `Engine/Noge/DECISIONS_RODOLF.md`, bloc « 06→07/09 (nuit) —
LE CONFINEMENT DE VORTICITÉ, ET LA GRILLE BRANCHÉE ».

