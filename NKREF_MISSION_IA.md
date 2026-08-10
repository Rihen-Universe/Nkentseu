# MISSION IA — NkRef : le tableau de références (type PureRef) sur NKCanvas + NKGui

> Document de passation destiné à **une IA quelconque**. Demandé par Rihen le
> 10 août 2026. Dépôt principal : `D:\Projets\2026\Nkentseu\Nkentseu` — mais
> CET AGENT TRAVAILLE DANS UN WORKTREE (voir §2, règle absolue).
> Objectif : une application **exactement dans l'esprit de PureRef** — le
> tableau de références d'images que tout artiste garde ouvert à côté de son
> logiciel 3D — construite sur les briques maison **NKCanvas** (2D SFML-like)
> et **NKGui** (interface immédiate), zéro STL, zéro bibliothèque externe.

---

## 1. CE QU'EST PUREREF (le modèle à égaler)

PureRef est une fenêtre minimaliste où l'on JETTE des images de référence :

- **Un canevas infini** : on dépose des images (glisser-déposer, coller depuis
  le presse-papiers), on les déplace, on **panne** et on **zoome** sans limite.
- **Fenêtre discrète** : sans bordure, redimensionnable, **toujours au
  premier plan** en option, opacité réglable, mode « transparent aux clics »
  pour dessiner par-dessus dans un autre logiciel.
- **Manipulation d'images** : déplacer, échelle, rotation, **recadrer**,
  retourner (miroir), niveaux de gris, notes de texte posées sur le canevas.
- **Rangement automatique** : « Pack » (rangement compact des images),
  alignements, grilles, tout au raccourci clavier.
- **Sauvegarde d'une planche** en un seul fichier (chez nous : `.nkref`,
  images EMBARQUÉES — une planche doit se rouvrir sur une autre machine).
- Philosophie : AUCUN chrome inutile. Le canevas est l'application ; le menu
  vit dans le clic droit et les raccourcis.

## 2. RÈGLES ABSOLUES (le dépôt les a payées cher)

1. **WORKTREE OBLIGATOIRE** — tu ne travailles JAMAIS dans le dossier
   principal :
   ```
   git worktree add ../Nkentseu-nkref -b feat/nkref origin/main
   ```
   Deux `jenga build` simultanés dans le MÊME arbre corrompent `Build/Obj`
   (binaires qui crashent absurdement — incident vécu deux fois, dont un
   rebase emmêlé le 10 août). Ton worktree a SON Build/.
2. Cycle TEST → VALIDATION → INTÉGRATION du `CLAUDE.md` racine ; commits via
   `./gitcommit.sh "msg" <chemins explicites>` (commit par pathspec).
3. Français partout ; **zéro STL** (NkString, NkVector, allocateurs NKMemory) ;
   commentaires qui expliquent le POURQUOI.
4. **Carnet de bord** : `Applications/NkRef/CARNET.private.md`, au fil de
   l'eau, jamais poussé.
5. Lire avant de commencer : `ARCHITECTURE.md`, `CLAUDE.md` (racine),
   `Kernel/Runtime/NKCanvas/ROADMAP.md` + **`NKCanvas/USAGE.md`** (les
   exemples SFML-like), `Kernel/System/NKWindow` (fenêtres), et
   `Applications/NK3DModeler/src/NK3DModeler/Shell/` comme RÉFÉRENCE d'usage
   de NKGui en vraie grandeur (painter, hit zones, menus contextuels).
6. **Jamais réécrire une source en PowerShell** (Get/Set-Content double-encode
   les accents UTF-8).

## 3. CE QUE LE MOTEUR T'OFFRE DÉJÀ

| Brique | Où | Usage NkRef |
|---|---|---|
| NKCanvas Renderer2D | `Kernel/Runtime/NKCanvas` | le canevas : NkSprite/NkTexture/NkTransform, primitives, texte (NkText + NKFont) — couche SFML-like, voir USAGE.md |
| NKGui + pont canvas | `NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h` | le peu d'interface : menu clic droit, champs, curseurs |
| NKImage | `Kernel/Runtime/NKImage` | décodage PNG/JPG/EXR..., `Resize` (ATTENTION : retourne une NOUVELLE image — piège vécu), écriture |
| NKWindow | `Kernel/System/NKWindow` | fenêtre sans bordure, toujours-devant, opacité, glisser-déposer de fichiers, presse-papiers |
| NKSerialization | `Kernel/Runtime/NKSerialization` | le `.nkref` (NkArchive JSON + blobs d'images embarqués) |
| NKFileSystem | `Kernel/System/NKFileSystem` | fichiers récents, chemins |

Vérifie CE QUI EXISTE VRAIMENT dans NKWindow (drag-drop de fichiers ?
presse-papiers image ? opacité de fenêtre ? toujours-devant ?) : chaque
manque devient un PETIT chantier NKWindow documenté dans sa ROADMAP, pas un
contournement — c'est comme ça que le moteur grandit (règle de la maison).

## 4. LE PLAN PAR ÉTAPES — chaque étape est un livrable seul

### Étape 0 — l'application `Applications/NkRef` + le canevas nu
Projet jenga (copier la structure d'une app existante, ex. ConquerorLab),
fenêtre, boucle, canevas : **panner** (clic milieu ou espace+glisser),
**zoomer à la molette CENTRÉ SOUS LE CURSEUR** (la règle d'or d'un canevas
infini), grille discrète en fond. Livrable : on se promène dans le vide.

### Étape 1 — des images sur la planche
Glisser-déposer un fichier image (ou Ctrl+V presse-papiers si NKWindow le
permet, sinon dialogue d'ouverture) → NkSprite sur le canevas. Sélection au
clic, déplacement au glisser, poignées d'échelle aux coins, rotation
(poignée ou R), miroir (X/Y), suppression. Multi-sélection au rectangle.
L'ORDRE de profondeur suit l'ordre d'ajout, Ctrl+molette ou PgUp/PgDn
réordonne.

### Étape 2 — le fichier `.nkref`
Enregistrer/rouvrir la planche : transformations + images EMBARQUÉES
(octets du fichier source dans l'archive — pas des chemins qui meurent).
Récents. Titre de fenêtre = nom de la planche + indicateur non-enregistré
(même langage visuel que le modeleur).

### Étape 3 — le confort PureRef
Recadrage (double-clic → poignées de crop), notes de texte, niveaux de gris
par image, opacité par image, **Pack** (rangement compact — un simple
bin-packing par hauteurs triées suffit en V1), alignements, tout au clavier.
Menu CLIC DROIT complet (le canevas n'a AUCUNE barre de menus).

### Étape 4 — la fenêtre discrète
Sans bordure (le canevas jusqu'au pixel), déplacement par glisser du fond
(quand rien n'est sous le curseur), toujours-devant, opacité globale,
« transparent aux clics » si NKWindow l'offre. Chaque option dans le menu
clic droit → Fenêtre.

## 5. TESTS — l'agent se vérifie SEUL
- Crochets d'environnement à poser dès l'étape 0 (imiter la boucle du
  modeleur : NK_AGENT_SHOT/NK_AGENT_EXIT, captures numérotées + diff pixel).
- Round-trip `.nkref` : enregistrer → rouvrir → réenregistrer → octets
  identiques (le test qui a validé les scènes du modeleur).
- Une planche de test avec 3 images générées (PNG System.Drawing ou NKImage).

## 6. LE PROMPT — à copier tel quel pour l'agent

```
Tu travailles sur le moteur Nkentseu (C++ maison, zéro STL). Ta mission :
NkRef, le tableau de références d'images type PureRef, construit sur
NKCanvas (2D SFML-like) et NKGui. LIS D'ABORD NKREF_MISSION_IA.md à la
racine du dépôt principal — il décrit PureRef (le modèle), les briques
disponibles, le plan par étapes et les règles absolues.

RÈGLE N°1 : crée ton WORKTREE (git worktree add ../Nkentseu-nkref -b
feat/nkref origin/main) et n'en sors JAMAIS — deux builds dans le même
arbre corrompent les objets (incident vécu). Travaille et réponds en
FRANÇAIS, zéro STL, commentaires qui disent le POURQUOI, carnet de bord
Applications/NkRef/CARNET.private.md au fil de l'eau.

COMMENCE PAR L'ÉTAPE 0 : l'application Applications/NkRef (projet jenga
calqué sur une app existante), fenêtre + canevas infini — panner, zoomer
à la molette CENTRÉ SOUS LE CURSEUR, grille de fond. Pose dès ce stade
les crochets de test NK_AGENT_SHOT/NK_AGENT_EXIT (imite la boucle
d'agent du modeleur) et vérifie par captures. Étape suivante seulement
après validation de Rihen. Si une capacité manque à NKWindow
(glisser-déposer, presse-papiers image, opacité, toujours-devant),
ajoute-la PROPREMENT dans NKWindow avec sa note de ROADMAP — pas de
contournement. Ne jamais surévaluer : chaque étape est annoncée pour ce
qu'elle est.
```
