# Cours — NkCanvas & NKGui

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
| 5 | NKImage | charger, écrire, redimensionner, afficher dans l'interface |
| 6 | NKFont | atlas, métriques, les dix polices embarquées |
| 7 | NKAudio | `AudioEngine`, formats, bus, jouer un son au clic |
| 8 | NKMedia | sonde, lecture et écriture vidéo — périmètre honnête |
| 9 | NKNetwork | sockets, UDP fiable, `NkBitStream`, `NkNetWorld` |
| 10 | Projet final | tout assembler en une application complète |

## Formats

- **PDF** : `Cours_NkCanvas_NKGui.pdf` (196 pages) — la version composée, à lire.
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
