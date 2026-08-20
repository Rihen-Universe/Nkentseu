# Document 6 — L'IA de design : cadrage et corpus

> Ouvert le **2026-08-20** au soir, à la demande de Rodolf : *« commence à
> travailler sur le corpus de l'IA de design et sur l'IA de design. »*
>
> ⚠️ **Ce document cadre, il ne décide pas.** Chaque section se termine par ce qui
> reste à trancher. Rien n'est lancé sans l'accord de Rodolf.

---

## 1. Ce que ce modèle doit produire — et pourquoi ça change tout

§16bis.1 l'a déjà tranché pour l'assistant : **il produit dans le vocabulaire de
l'outil, jamais dans le sien.** Un modèle de design pour NkUIDesign ne sort donc
**pas une image**. Il sort un **document structuré** : une hiérarchie d'éléments
portant des rôles, des modes de taille, des ancres, des bornes, de l'espacement,
des états et des animations.

⚠️ **C'est la contrainte qui commande tout le reste du document, et elle est
contre-intuitive** : on croit qu'un modèle de design s'entraîne sur des captures
d'interfaces. **Un corpus de captures n'apprend que l'apparence.** Il n'apprend ni
la structure qui la produit, ni les rôles, ni ce qui se passe quand la fenêtre
change de taille.

Il faut donc des **paires** : `(intention ou rendu) → document`.

**Reste à trancher** : rien ici. C'est une conséquence de §16bis.1, déjà décidé.

---

## 2. D'où viennent les paires — inventaire honnête

| source | ce qu'elle donne | ce qui coince |
|---|---|---|
| **Web — HTML/CSS + capture** | des millions de paires (structure, rendu), publiques, collectables | le CSS n'est pas le vocabulaire de NkUIDesign : il faut convertir |
| **Dispositions Android — XML** | très proche du modèle de widgets, **avec les rôles** | extraction lourde, licences à vérifier |
| **Trousses de design publiques** | qualité de conception élevée | formats propriétaires, licences floues |
| **Génération synthétique** | paires parfaites, illimitées, couvrant **tout** le vocabulaire | n'enseigne que le goût du générateur |
| **Documents de Rihen** | exactement le bon vocabulaire | beaucoup trop peu nombreux |

### 2.1 ⚠️ Le convertisseur EST le corpus

Quelle que soit la source, il faut un convertisseur de son langage de disposition
vers celui de NkUIDesign. **Écrire ce convertisseur, c'est l'essentiel du travail
de corpus.**

Et il ne se jette pas après : **c'est exactement la fonction d'import** que §14bis
prévoit déjà, et que le greffon d'exemple « Pont Figma » illustre. **Le travail de
corpus et une fonctionnalité que les utilisateurs veulent sont la même chose.**

> C'est le seul argument qui rende ce chantier raisonnable pour un développeur
> seul : il ne produit pas qu'un jeu de données, il produit une brique du produit.

### 2.2 ⚠️ Le web enseigne précisément la partie la plus difficile

§14ter appelle l'attribution de rôle *« le moment décisif de tout l'outil »*. Or le
HTML sémantique **porte les rôles presque directement** :

| HTML | rôle NkUIDesign |
|---|---|
| `<button>` | bouton |
| `<input type=text>` | champ de saisie |
| `<input type=checkbox>` | case à cocher |
| `<select>` | liste déroulante |
| `<input type=range>` | curseur |
| `<a>` | lien (rôle de projet dérivant de bouton) |
| `role="tablist"` | barre d'onglets |

Les attributs ARIA en donnent davantage, et **gratuitement** : ils sont écrits par
des humains pour décrire l'intention d'un élément.

⚠️ **Une page sans HTML sémantique — tout en `<div>` — n'apprend rien de bon.** Le
filtre de collecte doit privilégier les pages qui utilisent les balises et les
attributs ARIA, quitte à réduire fortement le volume. *Un corpus deux fois plus
petit et correctement étiqueté vaut mieux qu'un corpus dix fois plus grand où le
modèle apprend que tout est un `div`.*

### 2.3 Ce que le web n'enseigne pas

Il n'a **pas** d'ancres au sens de §8quater.3, pas de bornes `min/max` attachées à
un mode de taille, pas de zone sûre, pas les trois familles d'animation de §9ter.

**C'est là que la génération synthétique sert** : produire des documents qui
couvrent délibérément ce que le web ne montre jamais, les rendre, et obtenir des
paires parfaites.

⚠️ **Mais un modèle entraîné uniquement sur du synthétique apprend notre goût, pas
le design.** Le synthétique couvre la **grammaire** ; le web apporte le **réel**.
Ni l'un ni l'autre seul.

**Reste à trancher** : la proportion des deux, et si l'on ouvre Android en plus.

---

## 3. Comment on saura que c'est bon

⚠️ **Pas par la perte.** Le projet interdit déjà de comparer des pertes entre
corpus différents, et ici la perte ne dirait rien de la qualité d'un design.

Trois mesures, de la plus objective à la plus faible :

### 3.1 Le validateur de l'outil est un juge gratuit et objectif

Un document produit par le modèle passe **la validation de NkUIDesign**. Tout code
`E-` est une **faute par construction**, sans jugement humain :

`E-BOUNDS-INVERTED` · `E-DISABLED-REENABLE` · `E-AMBIENT-NO-STATE` ·
`E-DRIVEN-NO-REST` · `E-SPACING-CYCLE` · `E-TOOLTIP-ONLY-SOURCE` ·
`E-WINDOW-NO-DRAG-REGION`

> **Le taux de documents produits sans aucune erreur de validation est la première
> métrique, et elle ne demande aucun annotateur.**

⚠️ **C'est un critère de correction, pas de qualité.** Un document parfaitement
valide peut être laid et inutilisable. Il élimine le n'importe quoi ; il ne
distingue pas le bon du médiocre.

### 3.2 Aller-retour

À partir d'un rendu, le modèle produit un document ; on rend ce document ; on
mesure l'écart entre les deux images. Objectif, automatisable.

⚠️ **Mais l'aller-retour récompense la copie, pas la conception.** Un modèle qui
reproduit fidèlement ne sait pas nécessairement créer. À utiliser comme garde-fou,
jamais comme but.

### 3.3 Exactitude des rôles

Sur un corpus de validation issu de pages à HTML sémantique, comparer le rôle
attribué au rôle attendu. **C'est la mesure la plus proche de ce que l'outil
promet.**

**Reste à trancher** : les seuils. Aucun ne doit être fixé après avoir vu les
premiers résultats — **§ à pré-enregistrer avant la première mesure.**

---

## 4. Taille et coût — ce que les mesures d'Ilyana permettent de dire

Le vocabulaire de sortie est **minuscule** devant la langue naturelle : quelques
dizaines de types d'éléments, une trentaine de propriétés, cinq modes de taille.
Ce modèle n'a pas besoin de la taille d'Ilyana.

Ordre de grandeur attendu : **quelques dizaines de millions de paramètres**, comme
le modèle de maillage de [[plan-trois-chantiers]] ③ — des **heures**
d'entraînement, pas des semaines.

⚠️ **Chiffre à ne pas oublier** : la campagne actuelle tourne à **12,85 s/pas
mesurés** sur cette carte, et le plafond d'accélération des chantiers GPU engagés
est de **×3,95** (Amdahl sur parts mesurées). Toute planification qui suppose mieux
est fausse.

⚠️ **Et la carte n'est pas libre.** Ilyana l'occupe jusqu'à la fin de l'horizon
6000. Rien ne commence côté entraînement avant.

**Reste à trancher** : rien pour l'instant — la question ne se pose qu'après le
premier lot de corpus.

---

## 5. Le premier pas, et sa condition d'arrêt

**Étape 1 — écrire le convertisseur HTML/CSS → `.nkgui`.** Pas un prototype
jetable : la brique d'import de §14bis.

**Étape 2 — convertir un premier lot de pages, et MESURER une seule chose :**

> **Quelle fraction des pages se convertit sans perte d'information ?**

Une page « convertie sans perte » est une page dont tous les éléments ont trouvé un
rôle ou un conteneur, et dont le rendu du document produit est visuellement proche
de la capture d'origine.

### ⚠️ À pré-enregistrer AVANT de mesurer

- **Ce que ce chiffre décide** : sous un certain seuil, la voie web est trop
  coûteuse et il faut basculer vers Android ou vers davantage de synthétique.
- **Ce qu'il ne prouvera pas** : ni que le modèle apprendra, ni que les designs
  produits seront bons. **Il ne mesure que la faisabilité de la collecte.**
- **Ce qui n'est PAS un signal** : une belle conversion sur des pages choisies à la
  main. L'échantillon doit être tiré au hasard dans la liste de collecte, **avant**
  de regarder les résultats.

**Reste à trancher, et c'est la première chose à demander à Rodolf** : le seuil,
écrit avant la mesure.

---

## 6. Ce que je n'ai PAS fait ce soir

- aucun corpus n'a été collecté ;
- aucun convertisseur n'a été écrit ;
- aucun entraînement n'a été lancé ;
- rien n'a été décidé sur les licences des sources.

⚠️ **Et je n'écris pas dans le code.** Ce document cadre le chantier pour que
Rodolf tranche, et pour qu'un agent puisse ensuite travailler sur une cible écrite
plutôt que devinée.

Voir [[plan-trois-chantiers]] · `3_NkUIDesign_Interface_Humaine.md` §16bis, §14ter,
§9ter · `5_NkUIDesign_Specification_Claude.md` §1quinquies à §1decies.
