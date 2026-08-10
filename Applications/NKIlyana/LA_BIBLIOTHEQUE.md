# Donner des livres à Ilyana

> Ce document se suit sans rien connaître du reste du projet.
> Tout se lance depuis `D:\Projets\2026\Nkentseu\Nkentseu`.

---

## L'idée en une minute

Un livre sert **deux fois**, et de deux façons qui n'ont rien à voir :

| | **l'entraîner** dessus | **l'indexer** |
|---|---|---|
| ce qu'elle en retient | la **langue** du domaine : son vocabulaire, ses tournures, sa façon de raisonner | le **texte exact** |
| sur un fait précis | reconstruit de mémoire → **peut se tromper** | **exact, et citable** |
| peut-on vérifier ? | non | oui, on lit le passage |
| corriger une erreur | tout réentraîner (des heures) | rééditer le fichier |
| ajouter un livre | réentraîner | **quelques secondes** |

Donc on fait **les deux**, à des rôles différents :
**on l'entraîne pour qu'elle parle le domaine, on l'indexe pour qu'elle en cite les faits.**

Ne pas espérer qu'un modèle de cette taille *retienne* une bibliothèque : il
n'en a pas la place. Ce qu'il peut faire, c'est **savoir où regarder**.

---

## Les quatre gestes

### 1. Préparer le dossier (une seule fois)

```
mkdir D:\Projets\Camrail\AI\bibliotheque
```

Il contiendra trois choses, créées toutes seules :
- `tout.txt` — tous les livres bout à bout ;
- `catalogue.txt` — un livre par ligne, lisible et corrigeable à la main ;
- `index.nkidx` — l'index de recherche.

### 2. Déposer un livre

```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --ajouter ^
  --livre "C:\mes_livres\algebre.txt" ^
  --domaine maths ^
  --titre "Algebre lineaire" ^
  --bibliotheque D:\Projets\Camrail\AI\bibliotheque
```

Le `--domaine` est libre : `maths`, `physique`, `histoire`, `litterature`,
`informatique`, `langue`… Il sert à **équilibrer** l'entraînement (voir plus bas),
donc il vaut la peine d'être mis.

À répéter pour chaque livre.

**Ce que le dépôt fait au passage**, et pourquoi :
- il enlève les retours chariot Windows — sans ça, tout le découpage échoue et le
  livre entier compte pour **un seul passage**, sans le moindre message d'erreur ;
- il ramène les lignes vides en rafale à une seule ;
- il **recoupe les paragraphes démesurés** à la fin d'une phrase. Un livre converti
  sans ligne vide deviendrait un passage unique de plusieurs méga-octets :
  impossible à citer, impossible à montrer, et systématiquement mal classé.

**Le message à surveiller** : `ATTENTION : tres peu de passages`. C'est le signe
d'une conversion ratée — presque toujours un PDF dont le texte est sorti en
colonnes mélangées ou en charabia.

### 3. Indexer

À faire **après avoir ajouté vos livres**, et à refaire à chaque nouvel ajout :

```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --indexer ^
  --bibliotheque D:\Projets\Camrail\AI\bibliotheque
```

Ça affiche le nombre de passages, de mots, et la liste des ouvrages. Mesure
constatée : **9 114 passages indexés en 0,17 s** pour 5 Mo.

### 4. Chercher — et voir d'où vient la réponse

```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --chercher ^
  --bibliotheque D:\Projets\Camrail\AI\bibliotheque ^
  --question "musique et instruments" --k 3
```

Sans `--question`, il pose la question en boucle jusqu'à une ligne vide.

Chaque résultat **dit de quel livre il vient** :

```
--- 1 — Traite B [sciences] (score 14.01) ---
Style de musique La musique d'Idir naît de l'association de différents instruments...
```

C'est tout l'intérêt : une réponse sans source ne se vérifie pas, donc ne vaut
pas mieux qu'une invention — elle est seulement plus difficile à démentir.

---

## Entraîner sur la bibliothèque

Une fois les livres déposés, on fabrique le corpus d'entraînement :

```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --melange ^
  --identite D:\Projets\Camrail\AI\IlyanaReel\identite.txt ^
  --bibliotheque D:\Projets\Camrail\AI\bibliotheque ^
  --sortie D:\Projets\Camrail\AI\bibliotheque\entrainement.txt ^
  --part 0.20 --taille 200
```

- `--part 0.20` = un cinquième d'identité et de charte, quatre cinquièmes de
  livres. Sans cette part, elle oublie qui elle est ;
- `--taille 200` = 200 Mo de corpus produit.

**L'équilibrage entre domaines se fait tout seul, et c'est important.** Le
prélèvement donne à chaque domaine la **même part**, quel que soit le nombre de
pages qu'il occupe. Sans ça, un traité de mathématiques de 400 pages pèserait
plus que cinq livres d'histoire réunis, et Ilyana apprendrait surtout à parler
mathématiques. La trace le dit : `equilibrage : 3 domaine(s), 109 sondages chacun`.

### Des pages web

**Une page enregistrée** (Ctrl+S dans le navigateur) :
```
NKIlyana.exe --ajouter --livre page.html --domaine <x> ^
             --url https://l-adresse-d-origine --bibliotheque <dossier>
```
`--url` consigne l'origine au catalogue **à la place du chemin local**. Une page
change et disparaît, là où un livre reste : une citation qui renvoie à une adresse
morte doit au moins dire d'où elle venait.

**Un site entier** :
```
NKIlyana.exe --aspirer --url https://un.site --sortie recolte.txt ^
             --max-pages 100 --delai 1000 --profondeur 3
```

⚠️ **Récolter n'est pas déposer.** L'aspiration écrit un fichier, elle ne remplit
pas la bibliothèque — pour que tu puisses **relire ce qui a été pris** avant de le
verser. Fais-le : un site mêle articles, mentions légales et commentaires, et rien
ne les distingue automatiquement. L'adresse de chaque page est écrite dans le
fichier (`[source] https://…`), sans quoi, les pages mises bout à bout, plus rien
ne dirait d'où vient un paragraphe.

**Le vrai travail n'est pas de lire le HTML mais de TRIER.** Une page est faite en
majorité de ce qui n'est pas l'article. Le tri repose sur une observation simple :
un élément de navigation est **court et sans ponctuation de phrase** (« Accueil »,
« En savoir plus ») ; un paragraphe d'article est **long et se termine par un
point**. Règle grossière — elle perd quelques légendes et titres, jamais le corps
du texte. La sortie annonce toujours combien de blocs ont été gardés et combien
écartés : un tri qui ne rend pas ses comptes ne se vérifie pas.

**Les quatre règles de l'aspirateur**, qui ne sont pas des scrupules mais des
conditions de succès — un aspirateur qui martèle un serveur est bloqué en quelques
minutes, et tout est à refaire depuis une adresse grillée :

| règle | pourquoi |
|---|---|
| rester sur le domaine | suivre les liens sortants transforme la collecte d'un site en parcours de l'internet entier |
| lire `robots.txt` **avant** de commencer | le lire après n'a aucun sens : on aurait déjà pris ce qu'on n'avait pas le droit de prendre |
| espacer les requêtes (`--delai`) | un site personnel tourne souvent sur une machine modeste |
| plafonner (`--max-pages`) | calendriers et filtres de recherche fabriquent des pages à l'infini |

En cas de doute sur la portée d'une règle de `robots.txt`, elle est prise pour
soi : s'interdire un chemin de trop coûte moins cher que se croire autorisé à tort.

Ce que l'aspirateur ne fait pas : contourner une authentification ou un blocage,
ni lire une page dont le contenu est construit par du script après affichage —
celle-là revient vide, et il le dit.

### Lui apprendre à CITER ce qu'elle trouve

```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --citations ^
  --bibliotheque D:\Projets\Camrail\AI\bibliotheque ^
  --sortie ...\citations.txt --combien 20000 --part-absente 0.25
```

Fabrique des exemples « Contexte → Question → Réponse » **par pure recopie** : le
contexte est un passage réel, la question est faite de mots **extraits de ce
passage**, la réponse est une phrase **copiée mot pour mot**. Aucune machine
n'affirme quoi que ce soit — rien ne peut donc y être faux qui ne le soit déjà
dans vos livres.

Un quart des exemples porte sur des mots **absents**, et la réponse juste y est
« Je ne trouve pas cela dans ce texte. » Sans eux, elle apprendrait qu'il faut
**toujours** répondre, et inventerait quand le texte se tait.

Ce fichier se mélange ensuite au corpus d'entraînement comme le reste.

Puis l'entraînement lui-même :

```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --train ^
  --corpus D:\Projets\Camrail\AI\bibliotheque\entrainement.txt ^
  --bpe D:\Projets\Camrail\AI\IlyanaReel\ilyana.nkbpe ^
  --load <modele actuel>.nkgp --save <modele nouveau>.nkgp ^
  --steps 3000 --d 384 --heads 6 --layers 4 --T 256 --B 12 --accum 2 ^
  --lr 1e-4 --warmup 100 --saveevery 150
```

⚠️ **Toujours `--save` vers un fichier NEUF.** Si le nouvel entraînement dégrade
le modèle, il faut pouvoir revenir en arrière **et** comparer.

Et pour décider si on garde le résultat :

```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --controle ^
  --load <modele nouveau>.nkgp --bpe ...\ilyana.nkbpe
```

**Règle** : un entraînement qui fait **baisser** ce score ne se garde pas.

---

## Les trois pièges qui coûtent cher

### 1. Le format des livres

| format | verdict |
|---|---|
| `.txt` | ✅ **marche** |
| `.tex` (source LaTeX) | ✅ **marche, et c'est le MEILLEUR format** — voir ci-dessous |
| `.epub` | ✅ **marche** (archive zip de pages XHTML) |
| `.pdf` | ⚠️ **lu, mais les polices de beaucoup de documents ne se résolvent pas encore** |
| `.html` / `.htm` | ✅ **marche** — l'article est trié du bruit (menus, pieds de page) |
| un site entier | ✅ `--aspirer` — voir « Des pages web » plus bas |

⭐ **Pour les maths, la physique, l'informatique : donner le `.tex`, jamais le PDF.**
Mesuré sur le même document : le PDF a rendu **0 passage**, sa source LaTeX en a
rendu **2254**. Et ce n'est pas un accident de ce fichier — un PDF ne contient pas
de texte mais des glyphes posés à des coordonnées, et une formule y devient une
poussière de symboles dont la structure est perdue. Le `.tex` dit exactement ce
que la formule est. Les `\input` sont suivis, donc un livre découpé en un fichier
par chapitre entre en entier.

**Sur l'état du PDF, précisément.** Le lecteur décode bien la structure et le
contenu ; c'est la résolution des **polices** qui échoue sur les documents
produits par LaTeX (polices Type1/CFF, quand le lecteur attend du TrueType).
Mesure sur un cas réel : 2 157 102 octets de contenu décodés, 147 590 opérations
exécutées, 10 104 ordres de texte… et **0 glyphe demandé**. Le programme te dit
laquelle des quatre causes possibles s'applique, au lieu de te laisser deviner.

### 2. Le tokenizer se fige — MESURÉ, et déjà traité

**Comment le vérifier soi-même**, sur n'importe quel texte :
```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --mesurer ^
  --bpe <tokenizer.nkbpe> --texte <un fichier de ce domaine>
```
L'unité qui parle est l'**octet par token**. Autour de 4 sur du français courant,
c'est bien ; en dessous de 2,5, le texte est réduit en miettes — il est appris
par fragments **et** coûte deux à trois fois plus de place dans la fenêtre de
256 tokens. Deux peines pour un seul défaut.

**Mesure du 2026-08-10** — le premier tokenizer, entraîné sur du Wikipédia seul :

| texte | octets/token | verdict |
|---|---|---|
| français courant | 3,79 | ✅ |
| code C++ | 1,74 | ⛔ en miettes |
| LaTeX avec formules | 2,45 | ⛔ en miettes |

**La parade n'exige pas d'avoir les livres.** Ce qui manque au tokenizer n'est pas
le *vocabulaire* d'un domaine mais sa **notation** — les opérateurs, la ponctuation
technique, les commandes. Or le dépôt en est plein : des millions de lignes de C++
réel et des sources LaTeX. On entraîne donc le tokenizer sur un échantillon
**équilibré prose / code / formules** monté avec la bibliothèque elle-même :

```
NKIlyana.exe --ajouter --livre <prose.txt>  --domaine prose    --bibliotheque <ech>
NKIlyana.exe --ajouter --livre <code.txt>   --domaine code     --bibliotheque <ech>
NKIlyana.exe --ajouter --livre <sources.tex> --domaine formules --bibliotheque <ech>
NKIlyana.exe --melange --identite <identite.txt> --bibliotheque <ech> ^
             --sortie echantillon.txt --part 0.05 --taille 24
NKBpeTest.exe echantillon.txt ilyana_v2.nkbpe 16384
```

**Et les livres en ANGLAIS n'ont pas à être traduits.** Traduire injecterait des
erreurs dans des données qu'on a justement voulues fiables. Il suffit de mettre de
l'anglais dans l'échantillon — ce n'est pas le livre qu'il faut changer, c'est le
tokenizer qu'il faut instruire. (L'index, lui, se moque de la langue : BM25
fonctionne identiquement, et un livre anglais est cherchable et citable tel quel.)

Résultat mesuré sur de la **vraie prose** (⚠️ mesurer le français sur un fichier
d'identité, très répétitif, donne un chiffre flatteur et faux — la première
version de ce tableau s'y était trompée) :

| texte | v1 : 16k, français seul | v3 : 16k, 4 domaines | v4 : **32k**, 4 domaines |
|---|---|---|---|
| français courant | 3,71 | 3,46 ⛔ | **3,77** ✅ |
| anglais | 2,20 | 2,66 | **2,83** |
| code C++ | 1,74 | 3,01 | **3,22** |
| LaTeX / formules | 2,45 | 3,02 | **3,16** |

Lecture de ce tableau, qui est le vrai enseignement : **à vocabulaire constant,
ajouter des domaines coûte au français** (−7 %). Le vocabulaire est un budget, et
tout ce qu'on donne à l'un est pris à l'autre. **Doubler le vocabulaire supprime
l'arbitrage** : chaque domaine y gagne, français compris.

Le prix est réel et se paie en paramètres : la table d'embeddings double (6,3 M →
12,6 M à d=384), et le modèle passe d'environ 20 M à ~32 M. Sur 8 Go de carte,
c'est tenable — mais c'est un choix, pas une gratuité.

⚠️ **Un nouveau tokenizer impose de réentraîner le modèle depuis zéro**, ses acquis
étant indexés par numéro de token. C'est donc à faire **avant** le prochain gros
entraînement, jamais après. Le modèle actuel reste sur l'ancien tokenizer.

### 2bis. Ce qui reste vrai malgré tout

Le tokenizer découpe le texte en morceaux. Il a été entraîné **une fois**, sur du
Wikipédia généraliste. Des formules, du code, de la notation scientifique seront
donc découpés en miettes : elle les apprendra mal **et** ils lui coûteront trois
fois plus de place.

**Conséquence pratique** : si vous prévoyez d'ajouter des domaines très
différents, il vaut mieux **réentraîner le tokenizer sur un échantillon qui les
contient tous, AVANT le gros entraînement**. Après, il est trop tard : changer le
tokenizer invalide le modèle, dont les acquis sont indexés par numéro de morceau.

### 3. La capacité se partage

Chaque domaine ajouté prend la place des autres. Cinq domaines dans un modèle de
20 millions de paramètres, c'est cinq domaines faits médiocrement. Ce n'est pas un
défaut du programme, c'est une contrainte de taille — et c'est une raison de
compter sur **l'index** pour les faits, qui lui ne se dilue pas.

---

## Où ça en est

| morceau | état |
|---|---|
| déposer un livre `.txt`, catalogue, nettoyage | ✅ livré |
| index enregistré sur disque, rechargé instantanément | ✅ livré |
| recherche qui **cite son ouvrage et son domaine** | ✅ livré |
| corpus d'entraînement **équilibré entre domaines** | ✅ livré |
| lire l'EPUB | ⏳ à faire |
| lire le PDF | ⏳ à faire, plus lourd |
| qu'elle **se serve** d'un passage trouvé pour répondre | ⛔ **bloqué sur une décision** — voir `ROADMAP.md` § 3 |

Ce dernier point mérite d'être compris, parce qu'il limite tout le reste
aujourd'hui : **chercher marche, se servir de ce qu'on a trouvé non.** Pour
qu'Ilyana exploite un passage placé devant sa question, il faut qu'elle ait
appris le format « Contexte → Question → Réponse ». Elle ne l'a jamais vu à
l'entraînement. Et fabriquer ces exemples reviendrait à **inventer des questions
et des réponses** — exactement la donnée douteuse écartée au départ. Les trois
voies possibles sont décrites dans la feuille de route ; la décision appartient à
Rihen.

En attendant, `--chercher` s'utilise **tel quel** : c'est déjà un moteur de
recherche sur vos livres, avec citation de la source.
