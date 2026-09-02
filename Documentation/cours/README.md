# Construire avec Nkentseu

> De la première ligne de C à dix applications complètes.

Construire des applications et des jeux avec le moteur Nkentseu.
Cours complet pour débutants, en français, du dépôt jusqu'à l'application finie.

## Contenu

| | Chapitre | Sujet |
|---|---|---|
| 0 | Avant-propos | le dépôt, les outils de base, la compilation Jenga |
| 1 | Le décor | la pile complète : fenêtre → événements → GPU → NkCanvas → NKGui |
| 2 | NkCanvas | le rendu 2D : batching, textures, scissor — et ce qu'il n'a pas |
| 3 | NKGui | l'interface immédiate : identité, contexte, occlusion, widgets |
| 4 | Application | de zéro à l'exécutable : squelette, boucle, `.jenga` |
| **5** | **La coquille** | **`NkCanvasApp` : ce que vous n'écrivez plus — boucle qui cède la main, zone sûre, pointeur unifié, écran d'ouverture** |
| 6 | NKImage | charger, écrire, redimensionner, afficher dans l'interface |
| 7 | NKFont | atlas, métriques, les dix polices embarquées |
| 8 | NKAudio | `AudioEngine`, formats, bus, jouer un son au clic |
| 9 | NKMedia | sonde, lecture et écriture vidéo — périmètre honnête |
| 10 | NKNetwork | sockets, UDP fiable, `NkBitStream`, `NkNetWorld` |
| 11 | Projet final | tout assembler en une application complète |

## ⚠️ Ce cours se verifie contre le CODE, jamais contre la documentation

Rodolf, 2026-09-02 : « le cours ne doit pas se baser sur le wiki ou les docs
écrits, car le code a évolué depuis et on n'a pas mis ça à jour. »

Un audit du 2 septembre 2026 l'a confirmé, en mesurant plutôt qu'en relisant :

- le cours ne citait **nulle part** `NkCanvasApp` ni `NkCanvasGuiApp` --- toute la
  coquille d'application était absente, alors qu'elle est aujourd'hui le chemin
  normal. C'est ce qu'a comblé le chapitre 5 ;
- il ne citait ni `NkPointer`, ni `NkLayoutInfo`, ni `safeArea` : rien du support
  mobile ;
- il montrait encore un exemple `NKUI`, **module déprécié**.

⚠️ **Un audit par NOMS ne suffit pas.** L'outil employé vérifie qu'un symbole
existe encore, pas qu'une **signature** est restée la même : un `DrawRect` qui
gagne un paramètre reste « présent » et le cours reste faux. Sa sortie est un
plancher de travail, jamais un quitus.

**La règle, pour qui écrit ici** : chaque `\begin{nkcode}` cite un fichier du
dépôt, et c'est du **code source** --- pas un `Guides/`, pas le wiki, pas ce
README.

## Formats

- **PDF** : `Construire_avec_Nkentseu.pdf` (203 pages) — la version composée, à lire.
- **Markdown** : `md/` — la même matière, lisible sur GitHub et modifiable.
- **Source** : `tex/` — LaTeX (XeLaTeX), un fichier par chapitre dans
  `tex/chapitres/`.

## Recompiler le PDF

```powershell
cd Documentation/cours
./build.ps1          # compilation complète (2 passes XeLaTeX)
./build.ps1 -Quick   # une passe, pour un aperçu
./build.ps1 -Clean   # nettoyage
```

MiKTeX est requis (`xelatex` dans le PATH). Les polices utilisées — Segoe UI et
Consolas — sont livrées avec Windows.

## Règles d'édition

- Chaque bloc de code porte sa **provenance** (`chemin:ligne` du dépôt) : un
  exemple sans provenance ne peut pas être vérifié, il n'entre pas.
- Ce que le moteur **n'a pas** se dit noir sur blanc ; aucun exemple qui
  promet.
- Pas d'onglets dans les sources `.tex` : le corps des listings passe par un
  fichier temporaire où TeX note la tabulation « `^^I` ». Espaces uniquement.
- Les chapitres se numérotent depuis **zéro** (`\setcounter{chapter}{-1}` dans
  `main.tex`) : les renvois du texte et les noms de fichiers disent la même
  chose que la numérotation imprimée.
