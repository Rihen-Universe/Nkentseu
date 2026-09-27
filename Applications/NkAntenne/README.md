# NkAntenne — le dossier, et rien de plus pour l'instant

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 27/09/2026.

Rodolf, le 27/09 : *« voici une autre application, donc juste crée son dossier et
mets le nécessaire, on le verra une autre fois. »*

C'est **exactement** ce que contient ce dossier : les trois documents de
spécification, et rien d'autre.

## Ce qu'il y a

| fichier | contenu |
|---|---|
| [`design/01-produit-nkantenne.md`](design/01-produit-nkantenne.md) | ce qu'on attend de l'application |
| [`design/02-interface-humaine-nkantenne.md`](design/02-interface-humaine-nkantenne.md) | l'interface, côté humain |
| [`design/03-interface-agent-nkantenne.md`](design/03-interface-agent-nkantenne.md) | l'interface, côté agent — les arbres `.nkgui` à produire |

En une phrase : **diffusion en direct, capture d'écran et de fenêtre, montage,
compression, lecture — sur Nkentseu.**

## Ce qu'il n'y a PAS, et c'est voulu

- **Pas de `NkAntenne.jenga`.** Déclarer une cible de construction la ferait
  entrer dans la construction complète (256 projets aujourd'hui) et **échouer**,
  puisqu'aucune source n'existe. Un projet qui casse le bâtiment pour annoncer
  une intention coûte plus cher qu'un dossier qui attend.
- **Pas de `src/`.** Un dossier de sources vide ne dit rien de plus que ce
  fichier-ci.
- **Pas de `Resources/Interface/NkAntenne/`.** Les documents `.nkgui` viendront
  avec le vocabulaire dont ils ont besoin, pas avant.

⚠️ **« Nom provisoire »** — le document 1 le dit en tête : *« NkAntenne » est un
nom provisoire* (« à l'antenne » = en direct), `NKStream` étant déjà un module du
noyau. Le dossier porte donc ce nom-là, et il se renommera sans douleur tant que
rien ne le référence — ce qui est une raison de plus de ne pas créer la cible de
construction aujourd'hui.

## Quand on le reprendra

L'ordre qui a servi pour NkAnimaEditor et qui a fait ses preuves :

1. lire le document 03 (côté agent), qui donne les arbres **mot pour mot** ;
2. vérifier quel **vocabulaire** lui manque — et l'ajouter au format **avant**
   d'écrire les documents, sinon chaque rôle absent devient une substitution ;
3. écrire `composants.nkgui`, puis un document par panneau ;
4. brancher les **zones hôtes** pour ce que seul le C++ peut peindre (ici : la
   prévisualisation vidéo, les vumètres, la timeline de montage) ;
5. mesurer avec une sonde, document par document.
