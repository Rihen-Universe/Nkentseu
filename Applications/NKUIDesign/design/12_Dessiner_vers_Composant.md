# 12. De la forme dessinée au composant réutilisable

> **Note de conception — aucun code.** Écrite le 2026-09-01, à la demande du
> coordinateur, pendant le chantier de conformité Lunacy. Elle décrit un chemin
> que le dépôt possède **par morceaux** et qu'il ne relie pas encore.
>
> ⚠️ **Rien de ce qui suit n'est construit.** Ce document sert à décider, pas à
> annoncer. Il est écrit maintenant parce que les trois briques viennent d'être
> posées coup sur coup — l'édition de forme au sommet (§8bis), les listes de
> l'Inspecteur, la transformation — et que c'est le moment où l'on voit ce
> qu'elles dessinent ensemble.

---

## 12.1 Le fait qui rend la question urgente

Depuis le 2026-09-01, un rectangle dessiné sur la toile peut recevoir des
sommets réels, un arrondi par sommet, une pile de remplissages, une pile de
bordures, des effets, une rotation et deux miroirs.

**C'est-à-dire : la toile produit maintenant des objets qu'on ne peut pas
refaire à l'identique en les redessinant.** Un tracé à sept sommets dont trois
sont arrondis à 12 px n'est pas reproductible à la main — il est *dessiné une
fois*. Tant que la toile ne produisait que des rectangles paramétriques, la
question « comment le réutiliser ? » avait une réponse triviale : on en repose
un. Elle n'en a plus.

📌 **Le déclencheur n'est donc pas une envie de fonctionnalité, c'est une dette
qui vient d'apparaître.** Le copier-coller reste la seule façon de réutiliser
une forme travaillée, et le copier-coller ne propage rien : dix copies d'un
même bouton, c'est dix corrections à faire quand la charte change.

---

## 12.2 Les deux natures que le modèle porte déjà — et le fossé entre elles

`NkUINode` a **deux façons d'exister**, écrites dans son en-tête depuis le début :

| nature | ce que c'est | qui la produit |
|---|---|---|
| **le nœud DESSINÉ** | `shape` = `rect`/`ellipse`/`text`/… + son apparence posée | les outils de la toile |
| **le nœud à COMPOSANT** | `component` = une clé stable du registre + `instance` (les écarts) | la palette |

Le second est **déjà** ce qu'on veut : une déclaration partagée, plus les écarts
propres à chaque usage. C'est exactement le modèle « composant / instance » de
Figma et de Lunacy.

⚠️ **Le fossé est qu'aucun geste ne mène du premier au second.** On peut poser
un composant, on ne peut pas en *fabriquer* un. Le registre est alimenté par le
code, jamais par le document. L'icône « créer un composant » du menu contextuel
existe, elle est grisée, et sa raison le dit : *« le registre ne reçoit pas
encore »*.

---

## 12.3 Ce que le chemin demande, dans l'ordre où ça se décide

### (a) Où vit la déclaration — **la question qui commande toutes les autres**

Trois réponses possibles, et elles ne coûtent pas la même chose :

1. **Dans le document qui l'a créée.** Simple, immédiat, et suffisant pour
   « je réutilise ce bouton dans mes douze écrans ». Le fichier reste
   autonome — on l'envoie à quelqu'un, il s'ouvre.
2. **Dans une bibliothèque à côté** (un `.nkuilib`). C'est ce qu'il faut pour
   partager une charte entre projets, et ça introduit **la résolution
   manquante** : un document qui référence une déclaration absente doit *dire*
   ce qui manque, pas se dessiner à moitié.
3. **Les deux**, avec une règle de priorité.

📌 **Proposition : (1) d'abord, et rien d'autre.** La bibliothèque est un
chantier de résolution de dépendances, pas un chantier d'éditeur ; la commencer
en même temps mélangerait deux problèmes dont un seul est urgent. Et (1) ne
ferme pas (2) : une déclaration locale se *promeut* plus tard.

### (b) Ce qu'on extrait, et ce qu'on laisse comme écart

C'est le point de conception réel, et il se tranche **par propriété**, pas en
bloc :

| propriété | dans la déclaration | en écart d'instance |
|---|---|---|
| la géométrie du tracé (`sommets`, arrondis) | ✅ c'est *l'identité* de la forme | ❌ |
| remplissages, bordures, effets | ✅ par défaut | ✅ surchargeables |
| le texte | ❌ | ✅ **toujours** — deux boutons ne disent pas la même chose |
| la position, la rotation, les miroirs | ❌ **jamais** | ✅ propres à l'instance |
| la taille | ⚠️ *à trancher* — voir ci-dessous | |

⚠️ **La taille est le seul cas vraiment difficile, et il ne faut pas le glisser
sous le tapis.** Nos sommets sont **unitaires** (−1..1, relatifs à la boîte) :
une instance redimensionnée déforme donc son tracé proportionnellement. C'est
juste pour une flèche, **faux pour un bouton à coins arrondis** — dont les coins
doivent garder leur rayon en pixels quand le bouton s'allonge. C'est le problème
du *9-slice*, et il est réel dès la première instance étirée.

📌 **Il n'a pas besoin d'être résolu pour livrer (a).** Une première tranche peut
poser les composants **à taille fixe**, et *le dire*. Mais il doit être écrit
maintenant, sinon la première instance étirée passera pour un bug.

### (c) Ce que le fichier écrit

Rien de neuf dans la discipline : une section de déclarations en tête, `component`
qui la référence, `instance` qui porte les écarts. **Additif** — un document sans
composant ne gagne aucune clé et se réenregistre octet pour octet.

### (d) Ce que l'interface montre

Le retour d'une instance vers sa déclaration est **la moitié qui se néglige** :
- l'instance doit **dire** qu'elle en est une (Lunacy met un losange sur la
  vignette du calque) ;
- « aller à la déclaration » et « détacher » doivent exister **dès la première
  version** — sans « détacher », l'utilisateur qui a besoin d'une variante est
  coincé, et il cessera de créer des composants ;
- une propriété **surchargée** doit se distinguer d'une propriété héritée, sinon
  personne ne sait pourquoi une instance ne suit plus sa déclaration.

---

## 12.4 Les trois pièges que ce chantier porte, nommés avant d'être payés

1. **Une modification de déclaration touche N instances — donc l'annulation
   aussi.** Notre historique coalesce par nœud. Un pas d'annulation qui défait
   *une* écriture mais laisse N instances redessinées serait pire qu'une
   absence d'annulation : l'utilisateur croirait avoir annulé.
2. **Un composant qui contient une instance de lui-même fige l'application.**
   Le cycle doit être refusé *à la création*, avec sa raison — pas détecté au
   dessin, où il est déjà trop tard.
3. **Le registre du code et les déclarations du document sont deux sources pour
   un même espace de noms.** S'ils se recouvrent, une clé désignera deux choses
   selon l'ordre de chargement. C'est le motif du carnet : *le danger n'est pas
   qu'ils diffèrent, c'est qu'ils dérivent sans qu'on le voie* — et il faut une
   règle **écrite** de préséance avant la première collision, pas après.

---

## 12.5 Ce que je recommande, et ce que je ne recommande pas

**Recommandé — la plus petite tranche qui agit :** déclarations **locales au
document**, extraction depuis une sélection, instances à **taille fixe**,
« détacher » et « aller à la déclaration » livrés en même temps, et le
surchargement limité au **texte** et aux **couleurs**.

**Non recommandé pour l'instant :** la bibliothèque partagée (§12.3a-2), le
9-slice, les variantes (un composant à plusieurs états), et les propriétés
nommées. Ce sont quatre chantiers distincts qui *ressemblent* au même parce
qu'ils portent le mot « composant ».

⚠️ **Et une condition de déclenchement, écrite pour ne pas être négociée plus
tard**, sur le modèle de celle du bandeau du haut : **ce chantier commence le
jour où le geste d'extraction et le geste de détachement sont tous les deux
prêts à agir.** Un « créer un composant » sans « détacher » enferme
l'utilisateur dans une décision qu'il ne peut pas défaire — c'est un piège, pas
une fonctionnalité.
