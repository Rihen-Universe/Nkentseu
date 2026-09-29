# Mentions des tiers — Nkentseu / Rihen

**Dernière vérification : 26/09/2026.** Ce fichier est **engendré depuis une mesure**
(`echanges/ICONES_PROVENANCE.md`) : les listes ne sont pas recopiées à la main.

---

## vscode-codicons — 62 icônes de `Applications/NK3DModeler/data/icons/`

| élément exigé par CC BY 4.0 | valeur |
|---|---|
| **Titre** | vscode-codicons |
| **Auteur** | Microsoft Corporation |
| **Source (dépôt)** | https://github.com/microsoft/vscode-codicons |
| **Source (licence)** | https://github.com/microsoft/vscode-codicons/blob/main/LICENSE |
| **Licence** | Creative Commons Attribution 4.0 International (**CC BY 4.0**) |
| **Texte de la licence** | https://creativecommons.org/licenses/by/4.0/ |
| **Modifications** | **OUI — le matériel a été modifié** (voir ci-dessous) |

### Les modifications apportées, nommées

CC BY 4.0 exige d'indiquer si le matériel a été modifié. Il l'a été, de deux façons :

1. **Teinte au rendu.** Les fichiers sont monochromes et l'application les colore à
   l'affichage selon le rôle de thème de leur contexte (`NkModelerIcons.h`) ; la couleur
   affichée n'est pas celle du fichier d'origine.
2. **Rastérisation.** Ils sont décodés par notre propre codec SVG à 2× la taille cible puis
   réduits d'un cran, ce qui produit une image qui n'est pas la sortie d'un rendu SVG direct.

Aucune donnée de tracé (`d=`) n'a été retouchée : les 62 fichiers ci-dessous sont
**identiques au caractère près** aux originaux de la révision `a114bce`.

### Les 62 fichiers concernés

  `add` `arrow-circle-down` `arrow-circle-left` `arrow-circle-right` `arrow-circle-up` `arrow-down` `arrow-left` `arrow-right`
  `arrow-up` `book` `check` `checklist` `chevron-down` `chevron-left` `chevron-right` `chevron-up`
  `chrome-close` `chrome-maximize` `chrome-minimize` `chrome-restore` `circle-filled` `color-mode` `compass` `desktop-download`
  `device-camera` `edit` `eye` `eye-closed` `folder` `folder-opened` `gear` `globe`
  `law` `layers` `layout` `list-tree` `location` `lock` `menu` `milestone`
  `move` `output` `package` `paintcan` `primitive-square` `record-small` `refresh` `save`
  `screen-normal` `search` `split-horizontal` `symbol-color` `symbol-method` `symbol-numeric` `symbol-operator` `symbol-ruler`
  `table` `target` `terminal` `trash` `unlock` `zoom-in`

---

## ⚠️ CE QUI N'EST PAS ATTRIBUÉ ICI, ET POURQUOI

Un fichier d'attribution qui ratisse trop large est faux, exactement comme un fichier qui
en oublie. Les 42 autres icônes du dossier **ne viennent pas de vscode-codicons**.

### 36 icônes propres au projet

Ni leur nom ni leurs données de tracé ne figurent dans les 655 codicons officiels.

  `Vector` `brush` `capsule` `circle-edge` `cloth` `cone` `cube-3d` `curve-bezier`
  `cylinder` `device-camera-off` `empty-axes` `hair` `hand` `ico-sphere` `image-ref` `link-2`
  `liquid` `media-pause` `media-play` `media-record` `media-stop` `metaball` `minus-circle` `monkey`
  `ortho-cube` `pipette` `plane-3d` `plus-circle` `proportional` `sliders-horizontal` `square-check` `sun`
  `surface-patch` `text-3d` `torus` `uv-sphere`

### 6 icônes à provenance INDÉTERMINÉE — non attribuées, et à remplacer en premier

  `close` `lightbulb` `magnet` `pin` `preview` `tag`

Ces 6 fichiers portent **le nom** d'un codicon mais **un dessin différent** : aucune de
leurs données de tracé ne correspond à l'un des 655 de la révision mesurée. Deux explications
restent possibles — une **révision antérieure** du jeu, ou un **redessin local** — et la
comparaison à l'historique complet du dépôt amont n'a pas été faite.

**Elles ne sont donc pas attribuées** : affirmer une origine qu'on n'a pas établie est aussi
faux que d'en taire une qu'on a établie. **Elles sont en tête de la file de remplacement**
(décision du 26/09) ; six dessins suffisent à supprimer cette zone grise.

---

## Ce qui est prévu

Le remplacement complet des icônes empruntées par un jeu dessiné pour Rihen est décidé, et
**reporté après le 1er novembre 2026**. Ce fichier disparaîtra alors — ou ne gardera que ce
qui reste effectivement emprunté.

**Condition de retrait de chaque ligne** : le jour où une icône est redessinée, elle sort de
la liste ci-dessus. La liste étant engendrée depuis la mesure, il suffit de relancer la mesure.
