# Planches — le contrat visuel de NkUIDesign

> Rangées le **2026-08-21**. Elles vivaient jusque-là dans
> `C:\Users\Rihen\Pictures\Screenshots\`, au milieu de 904 fichiers, nommées par
> un horodatage.
>
> ⚠️ **Le prompt dit ce qu'on a demandé ; la planche montre ce qui est sorti.**
> Les prompts sont archivés au §22 du `4_NkUIDesign_Brief_Banani.md` — mais un
> prompt seul ne suffit pas : il faudrait tout revalider pour savoir ce qu'il
> produit. **C'est l'image qui est le contrat.**

---

## ⚠️ Deux avertissements avant de s'en servir

**1. Le travail n'est pas fini.** Il manque au moins : le Dope Sheet et
l'éditeur de courbes, l'inspecteur en sélection multiple, l'état « rien n'est
sélectionné », et la **composition de la fenêtre complète** — qui a échoué et
dont l'échec est conservé (voir `ecartees/`).

**2. Chaque planche porte ses réserves, et elles ne se copient pas.** Une planche
validée « sauf le contraste du gris » ne doit pas être reproduite telle quelle :
le défaut relevé est un défaut, pas une décision.

---

## Validées

| fichier | §22 | réserve relevée |
|---|---|---|
| `22.0_canvas_design` | §1bis | repères magenta parasites ; étiquette du cadre mobile trop haute |
| `22.1_inspecteur_design` | §22.1 | la ligne *Cible* reperd son grisé de lecture seule |
| `22.2_inspecteur_behavior` | §22.2 | icône de restauration de la ligne barrée presque invisible |
| `22.3_menu_roles_haut` | §22.3 | le rôle courant est coché dans *Récents* et **non coché** dans *Actions* |
| `22.4_menu_roles_bas` | §22.4 | ~130 px de vide sous la dernière ligne |
| `22.5_hierarchie_deux_sections` | §22.5-6 | l'arbre ne semble pas continuer derrière la poignée ; barre d'actions ambiguë |
| `22.7_bibliotheque_composants` | §22.7 | 🔴 **PÉRIMÉE** — montre 3 provenances, la spécification en porte **4** depuis §14bis.1 |
| `22.8_mobile_zone_sure` | §22.8 | étiquette de gauche coupée par le bord |
| `22.9_bureau_decoration_client` | §22.9 | aucune |
| `22.10_console_simulation` | §22.10 | *Atteints* et *Jamais atteints* n'ont pas le même poids visuel — consigne renforcée, **non relancée** |
| `22.11_etats_indisponibilite` | §22.11 | ⚠️ libellés désactivés à la limite du lisible — **la planche viole la règle qu'elle illustre** |
| `22.12_canvas_behavior` | §22.12 | bouton de bascule Code et loupe absents |
| `22.13_canvas_animation` | §22.13 | « piloté par la disponibilité » illisible ; courbe en S au lieu d'un *Ease Out* |
| `22.14_panneau_animation` | §22.14 | les deux phrases explicatives trop pâlies |
| `22.15_gestionnaire_greffons` | §22.15 | cartes désactivées si ternes que les sous-titres passent sous la limite |
| `22.16_dialogue_installation` | §22.16 | aucune |
| `22.17_dialogue_desinstallation` | §22.17 | aucune |
| `22.19_bandeau_greffon_manquant` | *à écrire* | aucune |
| `22.20_menu_cible` | *à écrire* | ⚠️ la coche est **à gauche** dans le menu et **à droite** dans le sous-menu |
| `22.21_rapport_transposition` | *à écrire* | aucune |
| `22.22_panneau_ia` | *à écrire* | aucune |

⚠️ **Quatre planches n'ont pas encore leur prompt archivé** au §22 — bandeau de
greffon manquant, menu `Cible`, rapport de transposition, panneau IA. **La
planche existe, le prompt qui l'a produite n'est nulle part.** À rattraper : sans
lui, on ne peut ni la refaire ni la corriger.

### 🔴 Une planche périmée par une décision postérieure

`22.7_bibliotheque_composants` montre **trois** provenances — Projet, Importé,
Système. §14bis.1 en porte **quatre** depuis que Rodolf a demandé qu'un composant
puisse survivre d'une application à l'autre : la provenance **Partagé** a été
ajoutée, et les composants de *Projet* ont migré vers la Hiérarchie (§11.6).

⚠️ **Et le menu `Cible` en périme trois autres** : `22.5`, `22.7` et le plan de
fenêtre montrent une barre de menu à **huit** entrées ; elle en porte **neuf**
depuis l'adoption de `Cible`. Le reste de leur contenu reste valable — **seule la
barre est à reprendre**.

---

## Écartées — `ecartees/`

**Elles sont conservées, et c'est délibéré.** Savoir ce qui a été essayé et
pourquoi ça a échoué évite de le réessayer. Même principe que `sonde_rendu.py`
gardé comme « supplanté ».

| fichier | pourquoi écartée |
|---|---|
| `091913_inspecteur_v1` | bornes min/max absentes |
| `093306_inspecteur_v2` | canvas vide et sombre ; bascule de mode et cluster de vue absents |
| `105224_inspecteur_design_v1` | liseré rouge parasite ; bordures colorées sur X et Y ; `default` contre `current` |
| `105938_inconnue` | ⚠️ **jamais examinée** — probablement un doublon de `105945` |
| `105945_inspecteur_design_v2` | sections `CONTENU` et `INTERACTIONS` **inventées** ; badges migrés sur les mauvaises sections |
| `115531_menu_roles_v1` | montre `interrupteur` parmi les rôles **natifs** — NKGui ne le porte pas (§14ter.4) |
| `131627_hierarchie_v1` | badges plein et creux indiscernables ; losange parasite sur `Bouton_Connexion` |
| `134906_mobile_zone_sure_v1` | hachures invisibles : la zone sûre n'était qu'une **ligne**, pas une **aire** |
| `190949_bureau_client_v1` | la zone de saisie n'était qu'une **étiquette**, pas une surface |
| `202712_composition_fenetre_ECHEC` | 🔴 **inspecteur entièrement absent** ; cluster de vue et boutons de fenêtre manquants ; barre d'outils dégradée |
| `211431_installation_v1` | « un greffon exécute du code » était **le plus petit texte de la boîte** |

### ⚠️ Ce que l'échec de la composition a établi

`202712` est la seule tentative d'assembler la fenêtre entière, **avec les
planches validées fournies en référence**. Elle a produit un tiers droit vide.

C'est ce qui a fait écrire §0bis : **demander la fenêtre entière dégrade toutes
ses parties.** La composition ne se fera donc pas par génération — le
`plan_fenetre_principale.svg` la remplace, et l'assemblage réel est le travail de
l'application.

---

## Le motif que ces onze écartées révèlent

Cinq d'entre elles échouent de la **même façon** : `134906` montre la ligne de la
zone sûre mais pas son aire, `190949` montre l'étiquette de la zone de saisie
mais pas sa surface, `211431` écrit la phrase de risque mais en corps minuscule.

> ⚠️ **On obtient le nom d'une chose, pas la chose.** La correction qui marche ne
> décrit pas l'intention — elle **prescrit la valeur** : « hachures à 45°, bleu
> vif, espacées de 10 px » plutôt que « bien visible » ; « en blanc cassé, même
> taille que le libellé » plutôt que « gris lisible ».

C'est écrit au §22.18 du document Banani, et ces onze images en sont la preuve.
