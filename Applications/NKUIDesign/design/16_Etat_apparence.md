# 16. État d'apparence — l'écart mesuré, pas l'impression

> **Rodolf, 2026-09-02** : *« je veux vraiment avoir un design très beau
> sincèrement, et actuellement c'est pas encore le cas. Visuellement ça doit être
> beau et fonctionnel. »*

## 📌 ÉTAT DE FIN DE CHANTIER (02/09, soir) — où on en est, ce qui reste, ce qui attend une décision

**Ce qui est FAIT** — six tranches, 225 → **33** sites hors échelle ; l'échelle
est écrite (`2/4/8/12/16/24`) et **opposable** (`compte_apparence.py
--verifier` sort en code 1 tant qu'un site dévie) ; la grille de l'inspecteur
est nommée et unifiée (`ColChamps`, `ColMiniLabel`) ; cinq écarts du côte à
côte sont livrés (E2, E3, E5, E6 ; E4 conforme par décision) ; sept captures
de référence sont fixées dans `captures/`.

**Ce qui RESTE — 33 sites, de la dette lisible, aucun brouillard** :

| famille | sites | ce que c'est |
|---|---|---|
| carte du panneau IA | ~10 | rythme interne (42, +3, pas de 22) — non déchiffré, **non touché** |
| libellés longs de l'inspecteur | 4 | 64/76 : « Hauteur ligne », noms de métriques — **ne tiennent pas dans `ColChamps`** |
| mini-grilles X1/Y1 | 4 | 14/22 : largeurs partagées, cohérentes entre elles |
| barre de zoom + rail | ~5 | valeurs transcrites, partiellement déjà étiquetées |
| divers panneaux (Bibliothèque, Simulation, Greffons) | ~9 | un site chacun, contexte connu |
| une position de scène (600) | 1 | à lire |

Plus **17 écarts justifiés** portant leur raison sur la ligne
(`[hors-echelle: …]`), comptés à part et listés par l'outil.

**Ce qui ATTEND UNE DÉCISION de Rodolf** :

1. **La colonne large** — faut-il une seconde colonne nommée pour les libellés
   longs (64/76), ou raccourcir les libellés ? *Aucune des deux ne se décide
   sans lui : l'une change la grille, l'autre change les mots.*
2. **Le raccourci miroir H/V** — `Maj+H`/`Maj+V` ou `Ctrl`+flèches ? La source
   se contredit (doc 13, table de désambiguïsation « miroir », sens ②).
3. **Les rayons d'angle** — 12 valeurs distinctes (4px×39, 3px×19, 1px×10…) :
   **le chantier n'a pas été ouvert**, faute de mandat. C'est le prochain
   candidat mesuré si l'apparence doit continuer.

---

## ✅ AU SOIR DU 02/09 : 225 → 33 sites hors échelle, et LA PAIRE D'ÉCRANS

**La paire que la demande attendait — pas un compte, deux écrans** :
`captures/2026-09-02_fenetre_defaut.png` (le matin, avant le chantier) contre
`captures/2026-09-02_soir_fenetre.png` et
`captures/2026-09-02_soir_inspecteur.png` (le soir). Ce qui se voit : le
document s'ouvre **cadré à 100 % sur sa première page**, hors du rail et du
sélecteur de zoom ; le badge de composant est **entier** (le nom cède) ; le
segment actif a son **icône** ; la section COMPOSANTS **parle** quand elle est
vide ; et l'inspecteur a une **colonne de champs unique** — Cible, Position,
Largeur, Hauteur alignés.

**Le chiffre, en six tranches** : 225 → 33 sites hors échelle. ~90 valeurs
étaient des **calculs en costume de littéraux** — nommées sans bouger un pixel
(`BandeY`, `IconeY`, `InsetIcone`, `ColChamps`, `ColMiniLabel`…) ; ~35 étaient
des **choix** — posés sur l'échelle avec paire A/B à chaque fois (`PadChamp`,
`PadPanneau`, la grille) ; ~50 étaient des **erreurs de l'instrument** (trois
corrections du compteur, chacune écrite dans son code) ; 17 sont **justifiés
et étiquetés** avec leur raison sur la ligne. Les 33 restants sont nommés plus
bas — de la dette lisible, plus un brouillard.

## 🔑 LE CHIFFRE QUI EXPLIQUE « ÇA FAIT BRICOLÉ », ET IL SE VÉRIFIE EN TROIS SECONDES

> **Les douze valeurs d'espacement les plus employées sont *tous les entiers de
> 1 à 12*.**

Ce n'est pas une échelle mal choisie : **c'est l'absence de toute échelle.**
Chaque marge a été posée pour régler son cas local, et l'œil ne trouve aucune
règle à laquelle se raccrocher — même sans savoir pourquoi.

⚠️ **Cette ligne parle de la MISE EN PAGE seule.** Les sommets d'icônes, eux,
emploient légitimement tous les entiers — les y inclure gonflait le constat de
40 %. Voir la *correction du 2026-09-02* plus bas : le compte réel est **225
sites**, et l'un des accusés était une valeur **dérivée**, pas un choix.

*Tout le reste de ce document découle de cette ligne.*

**Le diagnostic qui a lancé ce document** : le document 14 compte 174 lignes de
**comportement**, reprises de Lunacy. **Rien ne mesurait l'apparence.** Les
planches ont servi de référence au début du chantier, puis tout le suivi a glissé
vers l'interaction. *Ce qui n'est pas compté ne progresse pas.*

**L'indicateur** n'est pas « est-ce beau » — ça ne se mesure pas — mais le
**nombre de valeurs distinctes** par famille. Une interface paraît bricolée quand
chaque cas local a reçu sa propre valeur ; elle paraît tenue quand **peu de
valeurs se répètent**. Ça, ça se compte, et ça se recompte :

```
python compte_apparence.py              # le compte par famille
python compte_apparence.py --document   # régénère la partie chiffrée ci-dessous
```

⚠️ **Ce que ce compte ne dit pas**, et il faut le lire avec : il mesure le
**code**, pas des pixels ; il ne dit pas quelle valeur est **bonne**, seulement
combien il y en a ; et il ne remplace pas une comparaison côte à côte avec la
planche — **il la précède**. *On ne compare pas deux images en espérant en tirer
des chiffres.*

---

<!-- AUTO:DEBUT — regenere par `compte_apparence.py --document`. NE RIEN ECRIRE ENTRE LES DEUX MARQUEURS. -->

### Le compte des valeurs distinctes

| famille | valeurs distinctes | les trois plus employées | part des trois |
|---|---|---|---|
| tailles de texte | **7** | **10px** (105) · **11px** (74) · **9px** (46) | 94 % |
| hauteurs de rangee | **14** | **26px** (23) · **24px** (14) · **20px** (5) | 64 % |
| rayons d'angle | **12** | **4px** (39) · **3px** (19) · **1px** (10) | 72 % |
| decalages d'espacement (mise en page) | **34** | **12px** (58) · **3px** (47) · **2px** (39) | 38 % |
| sommets de glyphes (hors echelle) | **18** | **6px** (45) · **4px** (44) · **2px** (36) | 37 % |
| couleurs ecrites en dur | **2** | **#000000** (2) · **#ffffff** (1) | 100 % |

### La planche de référence, mesurée pareil

| | valeurs distinctes | les trois plus employées | part des trois |
|---|---|---|---|
| tailles de texte (planche Banani) | **18** | **11px** (173) · **10px** (104) · **9px** (85) | 70 % |

### Le détail, famille par famille

**tailles de texte** — 7 valeurs : 10px×105, 11px×74, 9px×46, 13px×4, 15px×4, 12px×3, 16px×1

**hauteurs de rangee** — 14 valeurs : 26px×23, 24px×14, 20px×5, 22px×5, 30px×5, 18px×3, 32px×2, 34px×2, 152px×1, 220px×1, 28px×1, 40px×1, 46px×1, 520px×1

**rayons d'angle** — 12 valeurs : 4px×39, 3px×19, 1px×10, 5px×6, 6px×6, 8px×4, 0px×2, 12px×2, 17px×2, 2px×2, 10px×1, 11px×1

**decalages d'espacement (mise en page)** — 34 valeurs : 12px×58, 3px×47, 2px×39, 1px×36, 6px×34, 8px×24, 10px×19, 4px×17, 9px×12, 5px×10, 7px×10, 52px×7, 26px×6, 28px×6

**sommets de glyphes (hors echelle)** — 18 valeurs : 6px×45, 4px×44, 2px×36, 3px×33, 10px×30, 7px×25, 8px×23, 5px×22, 1px×20, 9px×18, 11px×10, 13px×8, 12px×6, 15px×5

**couleurs ecrites en dur** — 2 valeurs : #000000×2, #ffffff×1

<!-- AUTO:FIN -->

---

## Ce que la mesure dit — et elle contredit deux hypothèses sur trois

### 🔴 LE VRAI COUPABLE : LE RYTHME D'ESPACEMENT, ET DE LOIN

**34 valeurs distinctes de décalage**, et les douze premières sont
`1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12` — c'est-à-dire **tous les entiers de 1 à
12**, chacun employé des dizaines de fois. Il n'y a donc **aucune échelle** : pas
de rythme 4/8/12/16, pas même de préférence pour les pairs. Chaque marge a été
posée pour régler son cas local.

> **C'est ça, « ça fait bricolé ».** Deux éléments voisins séparés de 3 px ici et
> de 7 px là ne se lisent pas comme une grille — l'œil ne trouve aucune règle à
> laquelle se raccrocher, même sans savoir pourquoi.

**C'est le seul chantier d'apparence qui vaille d'être ouvert en premier**, et il
est mécanique : ramener les décalages sur une échelle (`4 / 8 / 12 / 16`, avec
`2` toléré pour les liserés) fait tomber 34 valeurs à cinq ou six.

### ⚠️ CORRECTION — deux erreurs de MESURE dans ce qui précède (2026-09-02)

Le paragraphe ci-dessus reste vrai **pour la mise en page**, mais mon instrument
comptait deux choses à la fois, et je l'ai écrit avec trop d'aplomb.

**1. Il comptait les sommets de glyphes.** Sur les 734 décalages relevés,
**311 sont des sommets d'icônes** — `{x + 1.f, y + 1.f}, {x + 3.5f, y + 3.5f}` :
une coche, un chevron dessinés dans une boîte de 16 px. Un dessin vectoriel
emploie légitimement tous les entiers ; le forcer sur une échelle ne le rendrait
pas plus tenu, **ça le déformerait**. Le compte honnête de la mise en page est
donc **225 sites hors échelle**, pas 477. Les glyphes sont désormais comptés dans
une famille à part — écartés, mais **visibles** : écarter une population sans la
montrer, c'est se donner un beau chiffre en cachant la moitié du code.

**2. Il présentait comme « hors échelle » une valeur qui n'était pas un choix.**
Le `+ 3.f`, premier accusé avec 76 sites, était pour l'essentiel un seul idiome
recopié **30 fois** : `CentrerY(f, r.y + 3.f, 20.f)` — soit `(26 − 20) / 2`, le
retrait qui centre un contrôle de 20 px dans une rangée de 26.

> **J'ai failli le « ramener » sur 4 pour faire tomber un chiffre.** Ça aurait
> décentré trente contrôles d'un pixel — une régression visuelle réelle, commise
> au nom de la cohérence, et invisible dans le compte qui l'aurait applaudie.
> Une valeur **dérivée** n'a pas à tenir sur une échelle : elle doit être
> **calculée**. Une échelle discipline ce qu'on **choisit** (marges, gouttières),
> pas ce que la géométrie **impose**.

L'idiome est maintenant nommé (`costume::CentrerBande`) : 30 sites deviennent une
définition, et le **témoin de rendu est resté identique au bit près** — la preuve
que c'était un renommage et non une refonte. Reste **225 sites**, à traiter de la
même façon : comprendre d'où vient la valeur **avant** de la déplacer.

### 🟡 SECOND : LES RAYONS ET LES HAUTEURS DE RANGÉE

- **12 rayons distincts** (`4` et `3` dominent, puis `1, 5, 6, 8, 10, 11, 12,
  17`…). Deux rayons suffisent à une interface d'atelier : un pour les petits
  contrôles, un pour les cartes ;
- **14 hauteurs de rangée** (`26` et `24` dominent, puis `18, 20, 22, 28, 30, 32,
  34, 40`…). La densité devrait être un **choix**, pas un résultat : deux ou
  trois hauteurs, pas quatorze.

### ✅ CE QUI N'EST PAS LE PROBLÈME — deux hypothèses écartées par la mesure

**La typographie est saine, et c'est contre-intuitif.** On soupçonnait une
prolifération de tailles. Mesure : **7 tailles chez nous, dont les trois
premières font 94 % des usages** (`10`, `11`, `9`). Et **la planche de référence
en porte 18**, dont les trois premières ne font que **70 %** — elle est *moins*
disciplinée que nous.

⚠️ *Ces deux pourcentages viennent du bloc régénéré ci-dessus, et ils y sont
restés collés délibérément : ma première rédaction disait 96 % et 66 % — des
chiffres écrits à la main qui contredisaient le tableau juste au-dessus. Un
document qui se contredit lui-même est pire qu'un document sans chiffre.*

> **Notre échelle typographique est déjà plus tenue que celle de la planche.**
> Corriger la typographie aurait été du travail sur un poste sain — et ça
> n'aurait rien changé à l'impression de Rodolf.

**Les couleurs non plus** : **2 hex écrits en dur** dans tout le rendu
(`#000000` ×2, `#ffffff` ×1). Tout le reste passe par les **rôles de thème** —
c'est exactement la discipline voulue, et elle tient. Le thème reste celui de la
maison (Dark Pro / Light Pro) ; **les planches donnent la géométrie et le rythme,
pas les couleurs.**

---

## ⚠️ CE QUI MANQUE ENCORE À CE DOCUMENT, ET JE NE LE MAQUILLE PAS

**La comparaison côte à côte n'est pas faite.** Elle demande une capture de
**notre** rendu, et l'application **n'a aucune commande de capture** — vérifié le
02/09 : la liste des drapeaux n'en contient pas, et rien ne pilote la fenêtre
sans GPU pour la photographier. La produire est un lot en soi (rendre une image
hors écran, ou ajouter un drapeau de capture), et il ne doit pas se glisser dans
une correction d'apparence.

📌 **Ce document est donc la moitié qu'on pouvait obtenir sans elle** — et c'est
la moitié qui **oriente** : elle dit où ne PAS travailler (typographie, couleurs)
autant que où travailler (espacement, rayons, densité). *Une comparaison d'images
aurait montré que « ce n'est pas pareil » ; elle n'aurait pas dit qu'il y a 34
décalages distincts.*

## L'ordre que la mesure impose

1. **l'échelle d'espacement** — 34 → 5 ou 6 valeurs. Le seul poste qui explique
   « bricolé », et le plus mécanique ;
2. **les rayons** — 12 → 2 ;
3. **les hauteurs de rangée** — 14 → 3 ;
4. *puis seulement* la comparaison côte à côte, quand la capture existera : elle
   servira à juger l'**alignement** et les **proportions**, que ce compte ne voit
   pas.

⚠️ **Et rien ne se corrige au jugé.** Le compte se relit après chaque vague :
corriger un écran au feeling en dérègle trois autres, et sans le chiffre personne
ne saurait dire si on avance.
