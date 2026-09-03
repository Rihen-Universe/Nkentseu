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
# doit lister exactement les 9 fichiers ci-dessus, et rien d'autre
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
