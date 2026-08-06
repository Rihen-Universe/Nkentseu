# Image de version — l'écran d'accueil

Ce dossier porte **l'image de la version courante**, à la manière du *splash
screen* de Blender : une œuvre réalisée **avec** le logiciel, créditée à son
auteur, renouvelée à chaque version.

## Ce qu'il faut y déposer

| fichier | rôle |
|---|---|
| `splash.png` | l'image (PNG ou JPEG). Format paysage recommandé, **16:9 ou plus large** — la bande est couchée en haut des projets récents. 1920×1080 suffit largement. |
| `splash.txt` | deux lignes : **1.** le titre de l'œuvre · **2.** le nom de son auteur |

Exemple de `splash.txt` :

```
Château Médiéval
TSATA TAKO
```

## Comportement

- **Sans `splash.png`, la bande ne s'affiche pas du tout** — pas de cadre vide,
  pas d'image d'emprunt. Un écran qui montre un trou est pire qu'un écran qui
  ne montre rien.
- `splash.txt` est **facultatif** : une image sans crédit s'affiche quand même,
  mais un crédit ne s'invente pas.
- La version (`NK3DModeler 0.1.0`) et le crédit se posent **sur** l'image, dans
  un bandeau sombre en pied — lisible quelle que soit l'œuvre.
- L'image est **bornée à un quart de la hauteur** de la fenêtre : au-delà, elle
  repousserait les projets récents sous la ligne de flottaison, et ce sont eux
  qu'on vient chercher.
- Elle n'est décodée **qu'une fois**, et seulement si l'écran d'accueil
  s'affiche.

## Pourquoi ce contenu-là et pas les autres

C'est le **seul contenu éditorial livrable sans serveur** : il voyage avec le
binaire. Les nouveautés téléchargeables, les vidéos de la communauté, le
marketplace et les statistiques cloud supposent tous un service qui n'existe
pas — ils restent donc hors de l'écran tant qu'ils ne fonctionnent pas
(`PRINCIPES_CONCEPTION.private.md` : une fonctionnalité naît avec ses outils).

## À la prochaine version

Remplacer les deux fichiers, et mettre à jour **`kAppVersion`** dans
`src/NK3DModeler/Shell/NkModelerWelcome.h` **en même temps que**
`appversion(...)` dans `NK3DModeler.jenga` — deux numéros différents sur le
même binaire feraient mentir l'un des deux.
