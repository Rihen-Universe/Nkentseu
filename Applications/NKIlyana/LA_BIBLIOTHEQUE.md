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
| `.txt` | **marche aujourd'hui**, rien à faire |
| EPUB | pas encore lu — faisable (c'est un zip de pages HTML, et le décompresseur existe déjà) |
| PDF | pas encore lu — **vrai chantier** (flux compressés + tables d'encodage des polices, qui mentent souvent) |

⚠️ **Pour les maths et la physique, éviter le PDF même quand il sera lisible** :
les formules en ressortent presque toujours en charabia. Le texte source ou
l'EPUB vaut infiniment mieux.

### 2. Le tokenizer se fige

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
