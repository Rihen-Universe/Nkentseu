# NkRef — version de test (préversion)

Tableau de références d'images (dans l'esprit de PureRef), construit sur le
moteur Nkentseu de Rihen. **Préversion pour retours** : dites-nous tout ce qui
coince, plante ou manque — c'est exactement ce qu'on cherche.

## Lancer

- **Windows** : dézippez, double-cliquez `NkRef.exe` (gardez les `.dll` à côté).
  **Si Windows bloque le lancement** (« Windows a protégé votre ordinateur ») :
  c'est SmartScreen qui se méfie de tout programme récent non signé — pas un
  virus (le binaire vient du code public github.com/Rihen-Universe/Nkentseu).
  Deux façons de passer : cliquez « Informations complémentaires » puis
  « Exécuter quand même » ; ou, AVANT de dézipper : clic droit sur le zip >
  Propriétés > cochez « Débloquer » > OK.
- **Linux** : `tar xzf`, puis `./NkRef` (nécessite X11 + OpenGL — présents sur
  toute distro de bureau ; sous Wayland, XWayland fait l'affaire).
- **macOS** : `tar xzf`, puis `./NkRef` depuis un terminal. Binaire non signé :
  macOS peut le bloquer → clic droit > Ouvrir, ou
  `xattr -d com.apple.quarantine NkRef`. Version la moins éprouvée : les
  retours macOS sont les plus précieux.

## L'essentiel

- **Déposez des images** (glisser-déposer) ou **Ctrl+V** (coller une capture).
- **Molette** : zoom centré sous le curseur · **clic milieu** ou
  **Espace+glisser** : déplacer la planche.
- **Clic droit** : LE menu (tout y est).
- **Clic gauche** : sélectionner ; glisser une image : la déplacer ; coins :
  échelle ; rond au-dessus : rotation (Maj = crans de 15°).
- **Glisser le fond** : déplace la FENÊTRE (elle n'a pas de bordure — approchez
  le curseur du haut pour l'en-tête). **Ctrl+glisser** : rectangle de sélection.
- **D** : crayon (annotations colorées) · **Ctrl+P** : rangement automatique ·
  **X/Y** : miroir · **Suppr** : supprimer · **Début** : revenir à l'origine.
- **T** : fenêtre toujours devant · **1..9, 0** : opacité de la fenêtre.
- Chevron au bord droit : tiroir de propriétés. Menu > Réglages : préférences,
  thèmes (sombre GitHub / clair), liste des raccourcis.

## Limites connues de cette préversion

- **Pas encore d'enregistrement de planche** (le fichier .nkref arrive) : tout
  est perdu à la fermeture.
- Formats **WebP/AVIF non lus** (refusés proprement, comptés dans l'en-tête).
- Les grandes images sont réduites à 4096 px à l'import (désactivable dans
  Réglages > Préférences).
- Pendant le déplacement/redimensionnement de la fenêtre, le contenu ne se
  redessine pas (bref gel visuel, sans gravité).

Merci ! — Rihen (github.com/Rihen-Universe/Nkentseu)
