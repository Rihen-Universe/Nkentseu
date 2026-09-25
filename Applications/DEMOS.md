# Les démos — ce que Rodolf peut ouvrir, et ce qu'il doit y voir

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
**Créé le 25/09/2026, à la demande de Rodolf : « en branchant, si tu peux, crée
des démos que je peux tester, après je serais content. »**

> **Aucun câblage n'est déclaré fini tant que Rodolf ne peut pas le lancer.**
> Une démo qu'un humain lance éprouve **la chaîne entière**, là où un banc
> n'éprouve que son maillon. Ce dépôt a accumulé des bancs verts au-dessus de
> chaînes cassées.

Une démo, ici, c'est : **un exécutable qui s'ouvre et qui MONTRE** — sans
variable d'environnement, sans argument obligatoire, sans fichier à préparer ;
**nommé par ce qu'il montre**, jamais par un numéro ; qui montre le **câblage**,
pas le module. **Jamais poussé sur GitHub.**

La troisième colonne est la plus importante : elle dit à Rodolf **ce qu'il doit
chercher** pour conclure que c'est cassé. Sans elle, « ça a l'air de marcher »
est le seul verdict possible, et ce n'en est pas un.

---

| démo | ce qu'on doit voir | ce qui prouverait que c'est cassé |
|---|---|---|
| **`NkMessagesEnVue`**<br>`Build/Bin/Release-Windows/NkMessagesEnVue/NkMessagesEnVue.exe` | Cinq boutons. **« Emettre un AVERTISSEMENT »** pose une pastille **ambre** dans le cadre du bas, **« Emettre une ERREUR »** une pastille **rouge** qui reste plus longtemps. **« Douze fois la MEME erreur »** pose **UNE** pastille avec **« x 12 »**. Les pastilles s'effacent seules, l'erreur après l'avertissement. | • Une pastille **bleue** après « Emettre une INFORMATION » → le filtre du puits ne tient pas (une information ne doit **pas** monter à l'écran ; elle part dans la console et dans `logs/app.log`).<br>• **Douze** pastilles identiques au lieu d'une → la fusion des répétitions ne marche pas.<br>• **Après avoir pressé « DEBRANCHER le puits »**, un message qui apparaît encore → **un autre chemin le peint**, et toute la démo ne prouve rien. C'est le négatif, et c'est le bouton le plus important de la fenêtre. |

**Ce que `NkMessagesEnVue` câble, et pourquoi c'est le sujet :** chaque bouton
appelle `logger.Info()`, `logger.Warn()` ou `logger.Error()` — **et rien
d'autre**. Aucun ne dessine, aucun ne connaît l'écran, aucun ne choisit une
couleur. Le chemin est `logger.*` → NKLogger → **`NkEcranLogSink`** (le neuvième
puits) → la boucle d'affichage → `NkEcranLogVue`. C'est ce qui fait que le
rendu, l'import, la physique — et demain **les scripts et le nodal** — sont déjà
branchés sans une ligne à écrire.

---

## Ce qui n'est pas une démo à part, mais qui se teste dans le produit

| dans | quoi faire | ce qui prouverait que c'est cassé |
|---|---|---|
| **NK3DModeler** | **Les messages en vue.** Provoquez un refus d'import (importer un modèle sans ouvrir de scène). Le bandeau apparaît **dans la vue**, pas seulement dans la console. | Le refus n'apparaît que dans `logs/app.log` ou dans la console. |
| **NK3DModeler** | **Le navigateur de contenu.** Ouvrez-le (panneau du bas). **Tirez la séparation** entre l'arbre des dossiers (gauche) et la grille des vignettes (droite) : le curseur devient une double flèche au survol, le trait s'éclaire. **Fermez l'application, rouvrez-la** : la largeur est encore là. | • Le curseur ne change pas au survol du trait → la poignée n'est pas déclarée, ou une autre zone lui vole le clic.<br>• L'arbre ou la grille **disparaît** quand on tire à fond → une borne manque.<br>• La largeur revient à celle d'origine après réouverture → le fichier `~/.nk3dmodeler_ui.cfg` n'est pas écrit (il l'est **au relâchement** de la poignée, pas à la sortie). |
