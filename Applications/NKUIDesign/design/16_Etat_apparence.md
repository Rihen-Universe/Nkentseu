# 16. État d'apparence — l'écart mesuré, pas l'impression

> **Rodolf, 2026-09-02** : *« je veux vraiment avoir un design très beau
> sincèrement, et actuellement c'est pas encore le cas. Visuellement ça doit être
> beau et fonctionnel. »*

## 🔑 LE CHIFFRE QUI EXPLIQUE « ÇA FAIT BRICOLÉ », ET IL SE VÉRIFIE EN TROIS SECONDES

> **Les douze valeurs d'espacement les plus employées sont *tous les entiers de
> 1 à 12*.**

Ce n'est pas une échelle mal choisie : **c'est l'absence de toute échelle.**
Chaque marge a été posée pour régler son cas local, et l'œil ne trouve aucune
règle à laquelle se raccrocher — même sans savoir pourquoi.

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
| decalages d'espacement | **34** | **3px** (109) · **6px** (79) · **2px** (75) | 35 % |
| couleurs ecrites en dur | **2** | **#000000** (2) · **#ffffff** (1) | 100 % |

### La planche de référence, mesurée pareil

| | valeurs distinctes | les trois plus employées | part des trois |
|---|---|---|---|
| tailles de texte (planche Banani) | **18** | **11px** (173) · **10px** (104) · **9px** (85) | 70 % |

### Le détail, famille par famille

**tailles de texte** — 7 valeurs : 10px×105, 11px×74, 9px×46, 13px×4, 15px×4, 12px×3, 16px×1

**hauteurs de rangee** — 14 valeurs : 26px×23, 24px×14, 20px×5, 22px×5, 30px×5, 18px×3, 32px×2, 34px×2, 152px×1, 220px×1, 28px×1, 40px×1, 46px×1, 520px×1

**rayons d'angle** — 12 valeurs : 4px×39, 3px×19, 1px×10, 5px×6, 6px×6, 8px×4, 0px×2, 12px×2, 17px×2, 2px×2, 10px×1, 11px×1

**decalages d'espacement** — 34 valeurs : 3px×109, 6px×79, 2px×75, 12px×64, 4px×61, 1px×56, 10px×49, 8px×47, 7px×35, 5px×32, 9px×30, 11px×14, 13px×11, 14px×7

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
