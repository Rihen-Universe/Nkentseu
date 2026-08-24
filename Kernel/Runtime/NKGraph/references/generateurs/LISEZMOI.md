# Générateurs des planches de la spécification visuelle

> ## ⚠️ LA RÈGLE
>
> **Une planche n'est pas « faite » quand le script se termine sans erreur —
> elle l'est quand le PNG a été REGARDÉ.**
>
> Toutes les pannes listées plus bas passent l'intégralité des contrôles
> automatiques : XML bien formé, poids plausible, code de retour nul. Aucune ne
> survit à un coup d'œil.
>
> Et le coup d'œil attrape ce qu'aucun contrôle ne peut voir : dans la première
> version de la planche 05, le nœud légendé « hors cadre » était dessiné
> **dedans**. **La planche contredisait en image la règle qu'elle enseignait en
> texte.** Le SVG était valide, le PNG au bon poids, le script sans erreur.

Les planches `../planche_0*.svg` **ne sont pas dessinées à la main** : elles sont
produites par ces scripts. Ils vivaient dans un dossier temporaire de session —
c'est-à-dire nulle part. Ils sont ici pour que les planches restent
**modifiables**, et pas seulement consultables.

## Usage

```sh
cd generateurs/
python p05.py          # écrit ../planche_05_matieres.svg
```

Puis le rendu PNG, **avec une URL absolue** :

```sh
msedge --headless=new --disable-gpu \
  --screenshot=../planche_05_matieres.png \
  --window-size=1800,1580 \
  --allow-file-access-from-files \
  "file:///D:/.../references/planche_05_matieres.svg"
```

## Six pièges, tous silencieux

Chacun a réellement produit un fichier **valide et faux**. Aucun n'a levé
d'erreur. Ils sont notés ici parce qu'ils se reproduiront.

1. ⚠️ **Chemin relatif passé à Edge.** Edge le prend pour un **nom d'hôte**,
   échoue en DNS, et **capture sa propre page d'erreur** : le PNG a la bonne
   taille et le bon nom. Seul son **poids** trahit (quelques dizaines de Ko au
   lieu de quelques centaines). Toujours `file:///` + chemin absolu.

2. ⚠️ **Demi-caractères jetés au lieu d'être recombinés.** Un caractère hors du
   plan de base écrit en deux moitiés dans un script était supprimé à
   l'écriture. La planche 02 a ainsi perdu deux marqueurs sans qu'aucun contrôle
   ne bronche. `gen.ecrire` recombine désormais avant de filtrer — **ne pas
   remettre le filtre seul**.

3. ⚠️ **Un script qui ne tourne plus ne dit pas qu'il est périmé.**
   Voir `p04.py` ci-dessous.

4. ⚠️ **`--window-size` figé alors que le `viewBox` bouge.** Rencontré le 22/08 :
   le cartouche du panneau 9 de la planche 05 est passé de 7 à 12 lignes, le SVG
   a grandi jusqu'à **1 735** de haut, et la fenêtre de capture valait toujours
   **1 580**. **Le bas de la planche était coupé net** — sans erreur, sans
   avertissement, avec un PNG de poids parfaitement plausible.
   ✅ **Ne recopie jamais la valeur écrite ici : lis le `viewBox` du SVG**, et
   **vérifie ensuite les dimensions du PNG produit** — elles doivent égaler le
   `viewBox`, sinon il manque quelque chose en bas.

   ✅ **OUTILLÉ le 23/08** : `gen.rendre()` **compare les dimensions du PNG au
   `viewBox` du SVG** et lève une erreur si elles diffèrent. Vérifié par mutation
   (fenêtre volontairement trop courte → `PNG TRONQUE : 1780x1100 rendu pour un
   viewBox de 1780x1250`). **Une valeur de fenêtre figée dans une note ne protège
   de rien ; seule une comparaison le fait.**

5. ⚠️ **Un bloc qui grandit passe SOUS son voisin sans rien casser.** Même jour,
   même planche : la ligne « NON TRANCHÉ » rallongée sortait du panneau 9 et
   passait derrière le panneau 10. **SVG n'a aucune notion de débordement** : un
   texte trop long ne revient pas à la ligne, ne déplace rien et ne lève rien.
   **Tout texte allongé doit être recompté à l'œil, ou coupé en plusieurs `tt`.**

**Règle : une planche n'est pas « faite » quand le script se termine sans
erreur — elle l'est quand le PNG a été REGARDÉ.**

> 📌 **ÉCRIRE UN PIÈGE NE PROTÈGE PAS CELUI QUI L'ÉCRIT ; SEUL UN CONTRÔLE LE FAIT.**
>
> Le piège n° 5 ci-dessus — *« un bloc qui grandit passe sous son voisin sans rien
> casser »* — a été écrit le 22/08. **Une heure plus tard, le titre corrigé du
> panneau 3 de la planche 01 passait de 37 à 55 caractères et débordait sur le
> panneau 5**, sous la plume de celui-là même qui venait de l'écrire.
>
> **Ce n'est pas la note qui l'a attrapé, c'est le PNG regardé.** Une note se lit
> une fois et s'oublie ; un contrôle s'exécute à chaque fois. ⚠️ **Quand un piège
> est assez fréquent pour mériter d'être écrit, la vraie question est : peut-on en
> faire un contrôle ?** Ici oui pour la taille du PNG (comparée au `viewBox`), pas
> encore pour le débordement de texte — SVG n'a aucune notion de débordement. Les six pannes ci-dessus
passent tous les contrôles automatiques : XML bien formé, poids plausible, code
de retour nul.

## ✅ `p01.py` — la planche 01 est enfin modifiable

**C'était le vrai trou du chantier** : la planche 01 est la **référence
principale**, et elle était la seule des cinq que **personne** ne pouvait
modifier — ni l'agent, ni Rodolf. Elle a été dessinée avant que les générateurs
existent.

⚠️ **`p01.py` ne la redessine PAS avec `gen.py`, contrairement aux trois autres**,
et c'est délibéré : la planche 01 est **antérieure à `gen.py`** et n'a pas les
mêmes `defs` (sa grille est en `#2b2b33`, elle ignore les motifs `damier`,
`hachure` et `ciel`). La redessiner produirait **une autre planche** — et
remplacer en silence la référence de Rodolf par une variante serait exactement le
genre de perte que ce fichier passe son temps à traquer.

**Ce qu'il fait à la place** : il porte la planche **découpée en 12 panneaux
nommés**, chacun éditable à part. Modifier le panneau 3 ne peut pas abîmer le
panneau 7.

✅ **Critère d'acceptation, contrôlé à chaque exécution** : le SVG produit est
**identique à l'octet près** à celui qui existait (vérifié : md5
`498e1df1…` avant et après). Le script compare une empreinte et **dit** si la
planche a changé, au lieu d'écrire en silence.

⚠️ **Il porte aussi, en commentaire de tête, la liste des cinq affirmations de la
planche qui sont désormais FAUSSES** (« prises à cheval », « 10 × 12 px »…) — à
corriger **au moment du passage à l'échelle mesurée**, pas avant.

### 📌 FAMILLE — **les fins de ligne défont un travail sans qu'aucun diff ne le montre**

Ce n'est pas un incident, c'est une **famille**, et elle a été rencontrée
**deux fois la même nuit, sur deux chantiers indépendants** :

| où | ce qui s'est passé |
|---|---|
| **ici, `p01.py`** | écrit en `'wb'` avec des `
` alors que `gen.ecrire` ouvre en mode **texte** — où Windows traduit en `
`. **360 octets d'écart pour zéro pixel** : un diff de 360 lignes qui masque les vraies modifications |
| **le vérificateur** | `* text=auto` dans les attributs git rendait les **fixtures OBJ en CRLF** — des données de test modifiées par une règle qui ne s'intéresse qu'au texte |

**La forme commune, et c'est elle qu'il faut retenir :**

> Une règle de fin de ligne s'applique **entre** l'écriture et la lecture, sans
> passer par le code. Elle ne lève rien, ne change aucun pixel, aucune valeur,
> aucun caractère — **et pourtant les octets diffèrent.** Tout contrôle à l'octet
> près la voit ; **aucun contrôle visuel ne la voit jamais.**

✅ **Les trois réflexes qui en découlent :**

1. **savoir dans quelle convention vit le fichier avant d'y toucher.** Les cinq
   planches sont en **CRLF sur le disque** et en **LF dans git** ; les deux sont
   normales et il ne faut confondre ni l'une ni l'autre avec une modification ;
2. **un générateur qui écrit en binaire doit restituer la convention
   explicitement** — écrire en `'wb'` est la bonne pratique pour ne pas tronquer,
   mais elle **retire** la traduction que le mode texte faisait gratuitement ;
3. **calculer les empreintes sur le texte normalisé en LF**, jamais sur les
   octets du disque : une empreinte qui change en changeant de machine ne dit
   plus rien.

⚠️ **Et le corollaire qui compte** : quand un diff est énorme mais que le rendu
est identique, **cherchez la fin de ligne AVANT de chercher une différence de
dessin**. C'est ce qui a fait perdre le plus de temps des deux côtés.

### 📌 FAMILLE — **la réparation qui détruit ce qu'elle prétend sauver**

C'est la forme la plus insidieuse rencontrée sur ce chantier, parce qu'elle **se
rapporte comme un succès**.

**Le cas** : la planche 01 n'avait pas de générateur. La réparation évidente — la
redessiner avec `gen.py` — aurait produit **une autre planche** (defs
différents), et le compte rendu aurait annoncé en toute bonne foi *« la planche
01 est désormais régénérable »*. **Vrai, et désastreux** : la référence
principale aurait été remplacée par une variante, sans que personne ne l'ait
décidé.

**Ce qui l'attrape — et il en faut DEUX, l'un ne suffit pas :**

| contrôle | ce qu'il dit | ce qu'il ne dit pas |
|---|---|---|
| **md5 avant / après** | *« rien n'a bougé »* | rien sur ce qui aurait été **oublié** si on avait recomposé |
| **170 / 170 textes retrouvés** | *« rien n'a été oublié »* | rien sur ce qui aurait été **déplacé ou retouché** |

**La règle :**

> **Avant de réparer un fichier qu'on ne peut pas reproduire, se demander ce que
> la réparation DÉTRUIT.** Si la réponse est « l'original », ce n'est pas une
> réparation — c'est un remplacement, et il se décide, il ne se livre pas.

✅ **Le test qui tranche, et il est simple** : *le résultat est-il l'ancien
fichier, ou un nouveau qui lui ressemble ?* S'il faut regarder les deux côte à
côte pour répondre, c'est déjà un remplacement.

### 📌 FAMILLE — **une étiquette qui exagère un défaut le rend immortel**

`p04.py` était marqué ⛔ **PÉRIMÉ** — ce qui laissait entendre une dérive
structurelle. **Le défaut réel était de 5 lignes sur 363, dont trois n'étaient que
la conséquence d'une seule.**

⚠️ **L'étiquette a coûté plus cher que le défaut** : « périmé » met une chose
**trop grosse pour être corrigée en passant, et jamais assez urgente pour ouvrir
un chantier**. Elle l'immobilise entre les deux. C'est ce qui a bloqué le
changement d'échelle pendant toute une soirée : on ne peut pas régénérer cinq
planches quand l'une d'elles est réputée irrécupérable.

✅ **La règle : une étiquette dit la TAILLE du défaut, pas l'humeur qu'il
inspire.** « 5 lignes d'écart, non vérifié » aurait été corrigé en dix minutes.
Si la taille n'est pas connue, l'étiquette est **« écart non mesuré »** — jamais
un superlatif.

### 📌 MÉTHODE — **un générateur suspect ne se teste jamais sur sa cible**

Le lancer pour voir ce qu'il fait, **c'est déjà le laisser le faire.**

✅ **Le bac à sable** : copier `gen.py` et le script dans un dossier temporaire —
`OUT` étant déduit du chemin du fichier, il y pointera tout seul — puis comparer
au témoin.

```sh
mkdir -p /tmp/bac/generateurs && cp gen.py p04.py /tmp/bac/generateurs/
cp ../planche_04_etats.svg /tmp/bac/planche_04_etats.svg.temoin
cd /tmp/bac/generateurs && python p04.py
md5sum /tmp/bac/planche_04_etats.svg*
```

C'est ce qui a permis de mesurer les 5 lignes **sans jamais risquer la planche**,
et d'essayer l'échelle × 1 sur les prises **sans toucher au dépôt**.

### 📌 FAMILLE — **une moyenne sur un seul échantillon n'est pas une mesure**

… **et elle rend un nombre précis qui a l'air d'en être une.**

Le cas : « les prises des planches sont 2,4 × trop grosses », calculé en comparant
**un** nœud de 170 px au nœud de Rodolf. Relèvement réel des cinq planches :
**38 nœuds, de 150 à 330 px, médiane 270** — le 170 était **le second plus
étroit**, et **87 % des nœuds sont plus larges** que le ratio supposé.

⚠️ Le chiffre était faux, mais surtout : **il décrivait un ratio qui n'existe
pas.** La largeur d'un nœud **suit son CONTENU**, pas son en-tête. On cherchait
une proportion là où il n'y en a aucune.

✅ **Le réflexe** : avant de rapporter un rapport, **compter combien de cas le
soutiennent**. Un seul cas ne donne pas un rapport — il donne un exemple.

### 📌 FAMILLE — **un contrôle qui échoue ne doit pas laisser d'artefact**

Le garde-fou des dimensions a été testé le 23/08 en le déclenchant exprès : fenêtre
1100 pour un `viewBox` de 1250. Il a **levé l'erreur** — et **laissé le PNG tronqué
sur le disque**. Il y est resté **une heure**, jusqu'à ce que le contrôle final le
trouve.

> ⚠️ **Détecter et laisser le fichier faux transforme une erreur bruyante en
> fichier silencieux** — exactement ce que le contrôle devait empêcher.

✅ **Et c'est pire que pas de garde-fou du tout** : sans lui, personne n'aurait cru
le fichier vérifié. Avec lui, on croit que ça a été regardé.

**La règle : tout contrôle qui rejette doit d'abord SUPPRIMER ce qu'il rejette.**
`gen.rendre()` efface le PNG avant de lever, et le dit dans le message
(`PNG TRONQUE (et supprime)`).

### 📌 PIÈGE N° 7 — **un PNG conforme peut être le rendu d'une page d'erreur**

Le 23/08, `p07.py` a produit un PNG de **171 812 octets**, aux **dimensions
exactes** du `viewBox`. Les deux garde-fous existants — poids minimal et
dimensions — l'ont **accepté**.

⚠️ **C'était le rendu de la page d'erreur du navigateur** : *« Extra content at
the end of the document »*. Un `</svg>` en trop, parce que `ecrire()` écrit
elle-même la balise fermante et que rien ne le disait.

> **Aucun contrôle portant sur le PNG ne pouvait attraper ça : la page d'erreur
> fait exactement la taille demandée.**

✅ **`rendre()` valide désormais le XML AVANT d'appeler le navigateur**, et
`ecrire()` porte en commentaire le fait qu'elle ferme la balise. **Ne jamais
rendre ce qu'on n'a pas validé.**

📌 **Et il applique la règle du piège n° 6 dans sa version difficile** : quand le
SVG est mal formé, **le PNG précédent est supprimé**. Il est pourtant valide —
mais il est **périmé et plausible**, et on regarderait l'image d'hier en croyant
voir le travail d'aujourd'hui. *Un contrôle qui échoue ne laisse aucun artefact
derrière lui, même un artefact qu'il n'a pas produit.*

✅ **Vérifié en le déclenchant exprès**, comme les autres : SVG cassé à la main,
`SVG MAL FORME, rien n a ete rendu (PNG perime supprime)`, et le PNG absent
du disque après coup.

### 📌 FAMILLE — **le message et le fait ne sont pas le même objet**

Trois formes rencontrées dans la même nuit, sur trois chantiers différents, et
**une seule racine** : on a lu un message et on a cru à un fait.

| forme | le message disait | le fait était |
|---|---|---|
| chez le voisin | l'erreur **accusait glslang** | glslang avait **réussi** |
| chez le voisin | *« est déclarée `par_materiau` »* | le code **ne le vérifiait pas** |
| **ici, le 23/08** | un **`ok` affiché** par le script de mise à jour | il a quitté sur `sys.exit(1)` **avant d'écrire** : rien n'était sauvé |

⚠️ **La forme d'ici est la plus fourbe des trois**, parce que le message était
*vrai au moment où il a été imprimé* — le remplacement avait bien eu lieu **en
mémoire**. C'est l'écriture qui n'est jamais venue. Un rapport intermédiaire
décrit l'état d'un tampon, pas celui du disque.

✅ **La règle : n'annoncer un effet qu'après l'avoir produit.** Un script qui peut
quitter en cours de route n'imprime **rien** avant sa dernière écriture — ou il
imprime *après*, et ce qu'il a réellement écrit.

📌 **Et le symptôme se relit dans les deux sens** : ici la planche 06 a affiché
pendant une heure un sous-titre qui **contredisait son propre cartouche**, et je
la croyais à jour parce qu'un `ok` me l'avait dit. **C'est le PNG regardé qui a
tranché, pas le journal du script.**

### 📌 FAMILLE — **la connaissance écrite, au bon endroit, et qui n'agit pas**

Trois fois la même forme sur ce chantier, et jamais par ignorance :

| | ce qui était écrit | ce qui s'est passé quand même |
|---|---|---|
| piège n° 5 | *« un bloc qui grandit passe sous son voisin »* | son auteur a fait déborder un titre **une heure après** l'avoir écrit |
| étiquette `p04.py` | la famille *« une étiquette qui exagère un défaut le rend immortel »* | l'étiquette qui l'avait inspirée est restée en tête du fichier |
| garde-fou des PNG | *« vérifier la taille du PNG »* | le vérificateur a lui-même laissé un PNG faux |

⚠️ **La connaissance était disponible, exacte, et au bon endroit. Elle n'a rien
empêché.** Une note se lit une fois et s'oublie ; **un contrôle s'exécute à chaque
fois**. C'est ce qui a fait écrire `verifie_coherence.py` et le contrôle des
dimensions.

✅ **La question à se poser devant chaque note qu'on ajoute ici** : *peut-on en
faire un contrôle ?* Quatre fois oui (URL absolue, poids du PNG, dimensions,
divergence des documents), une fois non — le débordement de texte, **et pour
celui-là la note assumée reste la bonne réponse**, parce que SVG n'a aucune notion
de débordement. **Une note honnête vaut mieux qu'un contrôle qui ne contrôle rien.**

## 🔴 AVANT DE RÉGÉNÉRER : la règle des dates

**Rodolf modifie les planches lui-même** — *« en modifiant moi aussi ces planches
ça pourrait permettre facilement d'avoir le résultat voulu »*.

⚠️ **Conséquence directe : régénérer une planche écrase son travail, et sans
laisser de trace.**

**Règle, sans exception :** avant de régénérer, **compare la date de la planche
à celle de son script**. Si la planche est **plus récente**, elle a été retouchée
à la main — **arrête-toi et signale-le**. Ne régénère pas, ne fusionne pas
« au mieux ».

```sh
stat -c '%y %n' ../planche_05_matieres.svg p05.py
```

📌 Une planche produite par son script porte une date **postérieure de
quelques dixièmes de seconde** à celle du script — c'est normal, et ça se
distingue très bien d'une retouche humaine, qui arrive des minutes ou des heures
après. En cas de doute, compare aussi les **md5** avec les copies de
`Nkentseu-merge/`.

## État des scripts

| script | état |
|---|---|
| `gen.py` | la bibliothèque commune : palettes, `noeud`, `prise`, `fil`, `cartouche`, `ecrire` |
| `p01.py` | ✅ **écrit le 22/08** — reproduit `planche_01_noeuds.svg` à l'octet près. ⚠️ **Il ne redessine pas avec `gen.py`** : voir ci-dessous |
| `p02.py` | ✅ reproduit `planche_02_types.svg` à l'octet près |
| `p03.py` | ✅ reproduit `planche_03_formes.svg` à l'octet près |
| `p04.py` | ✅ **RÉPARÉ le 22/08** — reproduit `planche_04_etats.svg` à l'octet près |
| `p05.py` | ✅ reproduit `planche_05_matieres.svg` à l'octet près |
| `p06.py` | ✅ **écrit le 23/08** — `planche_06_decisions.svg`, les décisions de la nuit du 22 au 23. **Il appelle `rendre()` lui-même**, donc le PNG ne peut plus être oublié |
| `essai_c5.py` | banc de mesure, pas une planche : CIEDE2000 sur les sept familles pour trancher C5 |

✅ **`p04.py` est réparé** (22/08). Il était annoncé comme « périmé », ce qui
laissait croire à une dérive structurelle — **c'était faux, et l'étiquette a
coûté plus cher que le défaut.** Mesuré en bac à sable : **5 lignes d'écart sur
363**, dont trois étaient la conséquence d'une seule.

| écart | cause | correctif |
|---|---|---|
| hauteur 1060 au lieu de 1230 | `W,H` n'avait pas suivi | `W,H=1780,1230` |
| le rect de fond, et le pied de planche | **conséquences** de la hauteur | le pied passe de `y=1030` en dur à **`y=H-30`**, pour qu'il suive désormais |
| marqueur `!` à x=310 au lieu de 314 | `44+300-34` | `-30` |
| marqueur `⚠` à x=656 au lieu de 660 | `390+300-34` | `-30` |

📌 **La leçon de méthode** : le script a été lancé dans un **bac à sable** — une
copie de `gen.py` et `p04.py` dans un dossier temporaire, où `OUT` pointe
ailleurs — **avant** de savoir s'il abimait la planche. Un générateur suspect ne
se teste jamais sur sa cible : le lancer pour voir ce qu'il fait, **c'est
déjà le laisser le faire.**

⚠️ **Un reste à signaler, que la réparation n'a pas tranché** : la planche 04
mesure **1230 px de haut alors que son contenu s'arrête vers 1060** — environ
**170 px de vide** sous le dernier panneau, pied de planche mis à part. La
planche commitée est ainsi, donc `p04.py` la reproduit ainsi. **Savoir si ce vide
est voulu ou résiduel demande Rodolf** ; en attendant, reproduire fidèlement vaut
mieux que corriger au jugé.

## ✅ État au 22/08 : **les cinq générateurs reproduisent leur planche à l'octet près**

C'est la condition qui manquait pour **changer les constantes d'échelle une
seule fois** et régénérer les cinq d'un coup (voir `ELEMENTS_A_DESSINER.md`
§ A0). Tant qu'un seul générateur manquait ou divergeait, un changement de
constante aurait produit **trois planches à la nouvelle échelle et deux à
l'ancienne** — une contradiction interne, pire qu'un écart uniforme parce
qu'elle ne se voit plus.
