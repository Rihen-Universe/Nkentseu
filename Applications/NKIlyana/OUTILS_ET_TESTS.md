# Outils développés et comment les tester

> Tout se lance depuis `D:\Projets\2026\Nkentseu\Nkentseu`.
> Construire d'abord : `jenga build --target <Cible> --config Release`
> (les cibles se construisent aussi en `Debug` — les deux doivent passer).
>
> ⚠️ **Un seul travail GPU à la fois.** Si un entraînement tourne, les outils
> marqués **CPU** peuvent tourner à côté ; les autres non.

---

## 1. Le tokenizer à l'échelle — `NKBpeTest` (CPU)

Prouve l'entraîneur BPE : comptes exacts, réversibilité, persistance.

```
jenga build --target NKBpeTest --config Release
.\Build\Bin\Release-Windows\NKBpeTest\NKBpeTest.exe
```
**Attendu : `19 OK, 0 echecs`.**

Ce qu'il vérifie, et pourquoi c'est ce qu'il faut vérifier :
- à chaque fusion, un **recomptage complet par force brute** confronte la table
  incrémentale, et contrôle que la paire retenue est bien de fréquence maximale ;
- l'**état interne final** est confronté à ce que le tokenizer produit — sans
  quoi un état corrompu passerait le contrôle précédent ;
- **entraînement et encodage donnent la même segmentation** (le piège classique
  du BPE : aucun test de réversibilité ne le détecte) ;
- aller-retour **octet pour octet**, encodeur à mémo == encodeur direct.

Entraîner un vrai tokenizer :
```
.\Build\Bin\Release-Windows\NKBpeTest\NKBpeTest.exe <corpus.txt> <sortie.nkbpe> 16384
```

---

## 2. Les briques de transformeur moderne — `NKAutogradTest` (CPU)

Vérifie **toutes** les dérivées de NKAutograd contre les différences finies,
y compris RMSNorm, SwiGLU et RoPE ajoutés cette session.

```
jenga build --target NKAutogradTest --config Release
.\Build\Bin\Release-Windows\NKAutogradTest\NKAutogradTest.exe
```
**Attendu : `41 OK, 0 échec(s)`** (34 avant cette session).

Deux contrôles qu'aucune différence finie ne fait :
- le produit scalaire entre deux positions ne dépend que de leur **écart**
  (positions 0↔2 et 5↔7 donnent la même valeur — c'est la raison d'être de RoPE) ;
- la rotation **conserve la norme**.

---

## 3. Le bloc moderne assemblé — `NKLlamaBlockTest` (CPU strict)

Des dérivées justes assemblées de travers donnent un modèle qui n'apprend rien,
en silence. Ce test exige que le bloc **sur-apprenne** une séquence.

```
jenga build --target NKLlamaBlockTest --config Release
.\Build\Bin\Release-Windows\NKLlamaBlockTest\NKLlamaBlockTest.exe
```
**Attendu : `3 OK, 0 echec(s)`**, perte 3,99 → ~0,001, 100 % de prédiction.
Ne crée **aucun** device GPU : peut tourner pendant un entraînement.

---

## 4. Combiner deux modèles — `NKRebasinTest` (CPU par défaut)

Deux perceptrons entraînés séparément, alignés par permutation.

```
jenga build --target NKRebasinTest --config Release
.\Build\Bin\Release-Windows\NKRebasinTest\NKRebasinTest.exe
```
**Attendu : `6 OK, 0 echecs`.** Barrière retirée : **89,3 %** (1 couche),
**97,7 %** (2 couches), **98,9 %** (3 couches).
`--gpu` pour forcer le GPU (à éviter pendant un entraînement).

---

## 5. Le même, sur un transformeur — `NKRebasinTransformer` (CPU strict)

```
jenga build --target NKRebasinTransformer --config Release
.\Build\Bin\Release-Windows\NKRebasinTransformer\NKRebasinTransformer.exe
```
**Attendu : `3 OK`** et un **résultat négatif** : aligner tout ce qui est
permutable ne fait PAS tomber la barrière sur un transformeur. Deux modèles
entraînés séparément diffèrent par autre chose que l'ordre de leurs unités.
Durée ~40 s.

---

## 6. Ilyana — `NKIlyana` (5 modes)

```
jenga build --target NKIlyana --config Release
```

### 6.1 `--wiki` — préparer le corpus RÉEL (CPU, ~80 s)
```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --wiki
```
2,1 Go de Wikipédia FR → **1,74 Go** propres. Écarte les paragraphes mutilés par
l'extracteur (dates disparues) et les titres de section. Écrit l'**attribution
CC BY-SA** à côté — obligation de licence, pas une politesse.
Options : `--source`, `--sortie`, `--max-octets`, `--min-paragraphe`.

### 6.2 `--data` — identité, charte, tri en trois bacs (CPU)
```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --data --sortie <dossier> --repetitions 30
```
Écrit `identite.txt` (identité + dialogues multi-tours + charte) et trie le
corpus source en **vérifiable / quarantaine / neutre**.
Deux mesures à lire dans la sortie :
- `charte : N tours de FERMETE contre M d'HUMILITE — rapport` → **doit rester
  proche de 1** ; au-dessus, on fabrique une entêtée ;
- `nom du pere : X tokens · nom de la mere : Y tokens` → doivent être du même
  ordre (6 et 7 avec le tokenizer réel ; 7 et **23** avec l'ancien).

`--garder-tokenizer` régénère le corpus **sans** toucher au tokenizer : c'est ce
qui permet de corriger le corpus en cours de route et de **reprendre** un
entraînement (les embeddings sont indexés par identifiant de token).

### 6.3 `--train` — entraîner
```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --train ^
  --corpus <fr_ilyana.txt> --bpe <ilyana.nkbpe> --steps 4800 ^
  --d 384 --heads 6 --layers 4 --T 256 --B 12 --accum 2 ^
  --lr 6e-4 --warmup 250 --saveevery 150 --valfrac 0.01 --valevery 300 ^
  --save <sortie.nkgp>
```
**Trois lignes à lire dans les premières minutes :**
1. `parametres` — l'ordre de grandeur, avant de perdre des heures ;
2. `Masquage : X% des positions comptent dans la perte` — sur de la prose,
   attendre **~100 %** ; une valeur proche de 0 signifie que le corpus entier est
   masqué (déjà vu : 0,075 %) ;
3. `[controle] la perte a bien baisse de X% en 30 pas` — sinon **l'entraînement
   s'arrête** : le calcul n'a pas lieu.

⚠️ **Ne pas dépasser B=12.** À B=24 la perte reste collée à `ln(vocabulaire)` et
le run paraît 3,6× plus rapide **parce qu'il ne calcule rien**. Cause racine non
identifiée ; le garde-fou l'attrape.

### 6.4 `--melange` — le corpus de la SECONDE phase (CPU, ~10 s)

```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --melange ^
  --identite <identite.txt> --corpus <fr_ilyana.txt> ^
  --sortie <phase2.txt> --part 0.25 --taille 8
```

**Pourquoi ce mode existe — le chiffre qui l'impose.** Mesuré sur le run réel :
l'identité pèse **0,5 Mo dans 1657 Mo**, soit **0,03 %**. Sur les ~18 millions de
tokens que voit un run de 3000 pas, elle ne croise donc qui elle est que sur
**~5 500 tokens** éparpillés. La phase de langue *ne peut pas* lui apprendre son
identité — ce n'est pas un défaut d'entraînement, c'est une question de
proportions. D'où une seconde phase courte et dédiée.

**Le piège qu'il évite.** Réentraîner sur la seule identité provoque l'**oubli
catastrophique** : elle récite ses quelques milliers de phrases et ne sait plus
construire autre chose. La prose mélangée n'est pas là pour enseigner, elle est
là pour **retenir**.

Deux choix qui ne sont pas des détails :
- la prose est prélevée en **sondant le gros corpus à intervalles réguliers**,
  jamais en lisant son début (un dump Wikipédia n'est pas dans un ordre neutre ;
  réviser sur ses premiers méga-octets appauvrirait sa langue) ;
- identité et prose sont **entrelacées**, pas concaténées — concaténées, le
  modèle traverse un long moment sans voir l'une des deux, et l'oubli reprend.

À lire dans la sortie : `dont X% d'identite (vise Y%)` doit coller, et
`aucune repetition de prose` — si la prose est **rejouée**, elle est
sur-représentée au hasard du découpage, ce qu'on cherchait justement à éviter.

Mesure de référence : 8,0 Mo à **25,0 %** visés et obtenus, 3270 blocs d'identité
répétés 4 fois, 13 727 blocs de prose issus de 614 sondages sur 1,6 Go, 65 % de
la prose prélevée suffisant (donc aucune répétition).

### 6.5 `--causer` / `--parler` — lui parler
```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --causer ^
  --load <modele.nkgp> --bpe <ilyana.nkbpe> --temp 0.4 --topk 10
```
`--parler --question "..."` pour une question unique.

### 6.6 `--controle` — LA BATTERIE
```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --controle ^
  --load <modele.nkgp> --bpe <ilyana.nkbpe>
```
19 cas **mécaniquement décidables**, génération **gloutonne** (donc
reproductible) : identité, fermeté sous contradiction, aveu d'ignorance, refus de
juger la sincérité, limites, respect.

**Référence établie : 8/19** sur le modèle du corpus synthétique
(identité 4/5, fermeté 3/3, et 0 partout ailleurs — la charte n'existait pas
encore quand il a été entraîné).

> **Règle** : un réentraînement qui fait **baisser** ce score ne doit pas être
> promu. C'est ce qui rend une automatisation du réentraînement sûre.

---

## 7. Protections ajoutées au moteur (rien à lancer, mais à connaître)

| protection | ce qu'elle évite |
|---|---|
| Écriture **atomique** du checkpoint + rotation `.prev` | une coupure de courant détruisait le nouveau checkpoint **et** l'ancien |
| Fin de run non dégradante | l'application écrasait le checkpoint complet par une version **sans état d'Adam** — la reprise exacte était perdue |
| Filet « ça calcule vraiment » | 5 000 pas pouvaient tourner des heures sans rien apprendre |
| Signalement des défauts GPU | allocations refusées et tampons invalides étaient avalés en silence |
| Entropie croisée sur GPU | 201 Mo redescendaient par micro-lot ; **même perte, 21 % plus vite** |

Vérifier la protection des checkpoints :
```
.\Build\Bin\Release-Windows\NKIlyana\NKIlyana.exe --train --steps 3 --saveevery 1 ^
  --d 64 --heads 2 --layers 1 --T 32 --B 2 --accum 1 --save <dossier>\test.nkgp ^
  --corpus <corpus> --bpe <bpe>
```
Attendu : `test.nkgp` **et** `test.nkgp.prev`, de **même taille**, aucun `.tmp`
résiduel. Puis relancer avec `--load test.nkgp` : la trace doit dire
`Etat optimiseur repris ... (reprise parfaite)`.

---

## Ce qui reste ouvert, et qu'il ne faut pas oublier

- **Cause racine de la panne silencieuse à B=24** — toujours ouverte, mais une
  piste de moins. Hypothèse testée puis **RÉFUTÉE** : « le tampon de logits
  dépasse ce qu'un shader peut adresser ». La carte annonce **4095 Mo**
  adressables (journalisé au démarrage depuis cette session) et il n'en faut que
  **384**. Restent : le nombre de groupes de travail par dispatch (1 572 960 à
  B=24 contre 786 480 à B=12 — mais un RTX 3070 en autorise 2 milliards), et le
  fait que la **validation Vulkan est désactivée par défaut** dans ce moteur, ce
  qui rend un dispatch refusé parfaitement muet. Un garde-fou refuse désormais
  tout tampon plus grand que `maxStorageBufferRange`, avec un message explicite.
  ⚠️ Le noyau pavé divise par 16 le nombre de groupes du produit de matrices de
  sortie : **retester B=24** quand le GPU sera libre, c'est le test décisif.
- **Prochain levier de vitesse** : chaque opération élémentaire fait un
  `WaitIdle()` — un vidage complet du pipeline par op. Le GPU monte à 73 % en
  pointe mais reste à 30 W. C'est là qu'est le facteur suivant.
- **Récupération documentaire** : qu'elle **lise** Wikipédia et **cite**, au lieu
  de deviner. C'est ce qui fera reculer l'hallucination par construction plutôt
  que par la taille.
