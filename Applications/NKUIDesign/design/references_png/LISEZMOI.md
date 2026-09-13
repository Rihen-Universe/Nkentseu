# Références pixel — export Banani du 2026-08-30

28 PNG 1440×900, un par écran de l'export (`design/banani_export_2026-08-30.txt`),
nommés `ecranNN_<displayName>`. **Ce sont les références du remandat du 31/08** :
l'application se code écran par écran pour coïncider avec ces images, à
l'identique (thème, design, police, tout), même quand la fonction n'existe pas
encore.

## Fabrication (mécanique, rejouable)

`fabrique_reference.js` (Node) : parse l'export JSON, transpile le JSX de chaque
écran (Babel preset-react), résout les `@components/*` depuis `sharedFiles`,
`t()` = identité, `ReactDOMServer.renderToStaticMarkup` → HTML statique, zéro
logique ajoutée. Le CSS utilitaire est généré par Tailwind v4 (`@import
"tailwindcss"` + le bloc `@theme` du `/style.css` de l'export, balayage des 28
HTML — les 76 classes utilisées sont toutes couvertes, mesuré). Police **Inter**
locale (`@fontsource/inter`, graisses 400/500/600/700, latin). Rendu :

```
msedge --headless=new --disable-gpu --allow-file-access-from-files \
       --window-size=1440,900 --screenshot=<png> <file:///...html>
```

## Écarts nommés entre le JSX brut et ces rendus (rien d'autre)

1. **Écran 1** : le wrapper de la toile est `className="flex-1 relative"` (un
   BLOC) ; `DesignCanvasV2` (racine `flex-1`, enfants absolus) s'y effondre à
   0 px — toile invisible. Banani étire ses previews. Correctif minimal posé
   dans la fabrique : ce wrapper devient un conteneur `flex` (la toile remplit
   la zone). Aucune autre retouche de JSX.
2. **Cadre du document** : `html,body{margin:0;overflow:hidden;background:#0d1117}`
   — sans quoi l'ascenseur du navigateur (absent de l'application) mange 15 px
   et décale tout.
3. **Hauteur fixe 900** : les écrans plus hauts que 900 px (menus déroulés,
   assemblage complet) sont coupés à la fenêtre, comme dans l'application.
