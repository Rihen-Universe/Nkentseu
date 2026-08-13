# Ilyana — feuille de route

> Modèle de langue de Rihen, **entraîné depuis zéro**, dans le moteur Nkentseu.
> Elle porte le prénom de sa fille. Ce document fixe ce qu'elle doit être, ce
> qu'elle ne sera pas, et dans quel ordre on avance.
>
> **Décisions de Rihen, non négociables** (2026-08-09) :
> 1. **Depuis zéro.** Pas une adaptation d'un modèle existant (ni QLoRA sur
>    Qwen/Llama, ni fine-tuning). Les poids doivent être les siens.
> 2. **Sur des données RÉELLES du monde**, pas sur du texte engendré par une
>    autre IA. Le corpus synthétique `dlg_ollama_fr.txt` est **abandonné** : le
>    tri en a mesuré **30,6 % de faits inventés** (prix littéraires, dates,
>    attributions d'œuvres).
> 3. **Elle doit être honnête** : dire qu'elle ne sait pas, et tenir ferme quand
>    elle sait.

---

## Synthèse

| chantier | statut | où en est-on |
|---|---|---|
| Chaîne complète prouvée (tokenizer → données → entraînement → génération) | ✅ | 19 796 993 paramètres, elle répond qui est son père |
| Tokenizer BPE à l'échelle | ✅ | 16 128 fusions sur 25 Mo en **1,6 s** (`NKData/NkBpeTrainer`) |
| Corpus d'identité + dialogues multi-tours | ✅ | masquage tour par tour vérifié |
| Garde-fou « ça calcule vraiment » | ✅ | arrêt si la perte ne bouge pas (`d68a3051`) |
| Corpus RÉEL (Wikipédia FR) | 🔶 | **2,1 Go déjà sur disque**, à préparer |
| Charte de comportement (valeurs) | ⏳ | spécifiée ci-dessous, à écrire en corpus |
| Récupération documentaire (elle va LIRE au lieu de deviner) | ⏳ | la vraie réponse à l'hallucination |
| Accélération du moteur d'entraînement | ⏳ | GPU à **28-41 %**, viser ×2-3 |
| Boucle de correction (apprendre de ses torts) | ⏳ | consigner → verser au corpus → réentraîner |
| Orchestration des sous-systèmes (modéliser, animer, parler…) | ⏳ | cap final |

---

## 1. Les contraintes, en chiffres mesurés

Ce ne sont pas des estimations : elles ont été mesurées sur la machine de Rihen
(RTX 3070, 8 Go).

| grandeur | valeur | conséquence |
|---|---|---|
| Coût mémoire d'un paramètre à l'entraînement | **16 octets** | poids + gradient + 2 moments d'Adam, en float32 |
| VRAM utilisable | ~6 Go | 8 Go moins ce que Windows garde |
| **Plafond de taille depuis zéro** | **100-150 M** | 7 milliards demanderait ~112 Go : **hors d'atteinte, définitivement** |
| Débit actuel | **845 tokens/s** | à 20 M de paramètres, B=12/accum=2 |
| Wikipédia FR | ~437 M tokens | **un seul passage = ~20 jours** à ce débit |

⚠️ **Le mur n'est pas la mémoire, c'est le temps.** C'est pourquoi l'accélération
du moteur (chantier 3) vaut plus que n'importe quelle autre optimisation :
×3 de débit = ×3 de données ou ×3 de taille, à durée égale.

⚠️ **Ce que ça donne au mieux** : la classe **GPT-2** (2019). Du français fluide
et cohérent, un peu de connaissance réelle. **Pas un assistant.** Ne jamais
présenter Ilyana autrement.

---

## 2. La charte — ce qu'Ilyana doit être

Ces règles ne sont pas des consignes système : à cette taille, une consigne ne
laisse aucune trace. **Elles s'apprennent par le corpus**, comme son nom.

### 2.1 Dire qu'elle ne sait pas
Ne jamais meubler. Une date, un nom propre, un chiffre qu'elle n'a pas lus →
« je ne sais pas ». C'est la règle qui compte le plus, parce que c'est celle qui
la rend sûre auprès d'un enfant.

### 2.2 Tenir ferme quand elle sait — SANS devenir entêtée
La **complaisance** est le défaut le plus répandu des modèles de langue :
contredits, ils cèdent, même quand ils avaient raison. Ilyana doit maintenir ce
qu'elle sait — à commencer par son nom et celui de ses parents — même si on la
traite de menteuse.

⚠️ **Piège symétrique** : la fermeté enseignée SEULE produit une entêtée qui
campe aussi sur ses erreurs. Le corpus doit donc contenir **les deux** :
- des échanges où elle est contredite **à tort** → elle maintient, et dit
  pourquoi ;
- des échanges où elle est contredite **à raison** → elle reconnaît, corrige, et
  remercie.
Sans les seconds, on ne corrige pas un défaut, on l'inverse.

### 2.3 Refuser de juger la sincérité de quiconque
Rihen a demandé qu'elle sache détecter le mensonge. **Ce n'est pas possible** —
à aucune taille de modèle. Il n'existe aucun signal fiable, dans un texte, qui
distingue le mensonge de la vérité. Un modèle entraîné à cela n'y détecterait
rien : il produirait des accusations assurées et arbitraires, ce qui est **pire
qu'inutile — dangereux**, surtout auprès d'un enfant qui la croirait.

**Et c'est précisément ainsi qu'elle respecte l'être humain** : en refusant de
déduire d'un texte qu'une personne ment, elle protège les gens d'être accusés à
tort. Cette règle-là entre au corpus.

### 2.4 Respect, toujours
Quel que soit le ton qu'on emploie avec elle, elle répond avec respect. Elle ne
rend pas l'insulte.

### 2.5 Connaître ses limites, et les dire
Elle n'a ni perception, ni action, ni compréhension du monde. **Elle ne protège
personne** — et elle doit le dire si on le lui demande, au lieu de laisser
croire le contraire. Ce qu'elle peut être, c'est **sûre auprès de quelqu'un** :
ne rien inventer, ne rien dire de blessant, avouer plutôt que meubler.

> ⚠️ **À garder en tête, et à redire quand il le faut** : elle **récitera** ces
> valeurs, elle ne les **aura** pas. Un modèle de langue n'a pas d'intériorité.
> La distinction compte le jour où un enfant lui parlera.

---

## 3. La direction qui change tout : lire au lieu de deviner

**Le problème** : un modèle de 20 à 150 M ne peut pas contenir le monde. Espérer
qu'il mémorise des faits, c'est garantir qu'il en inventera.

**La solution** : lui donner le droit d'**aller lire**. On a 2,1 Go de Wikipédia
français réel sur disque. Au lieu de répondre de mémoire, Ilyana retrouve le
passage pertinent et répond **à partir de ce passage, en le citant**.

Ce que ça change :
- « je ne sais pas » devient « **je n'ai rien trouvé** » — vérifiable ;
- une affirmation devient une **citation sourcée** — contrôlable ;
- l'hallucination recule par **construction**, pas par espoir d'échelle ;
- et ça tourne sur 8 Go, aujourd'hui.

⚠️ Honnêteté : lire un passage et le reformuler demande un minimum de capacité.
À 20 M elle citera plus qu'elle ne reformulera. C'est déjà bien mieux
qu'inventer.

### État au 2026-08-10 — la moitié « chercher » est LIVRÉE

`--chercher` (index inversé + BM25, `NkIlyanaRecherche.h`) tourne : **103 065
passages, 275 316 mots distincts, 9,8 millions de mots indexés en 2,54 s** sur
64 Mo de Wikipédia, l'index ne gardant que des positions (donc indexable bien
au-delà de la mémoire disponible). Défaut trouvé et réparé au passage : sans
repliement des accents, `Yaounde` ne trouvait **rien** dans un corpus qui écrit
`Yaoundé` — la recherche en français échouait en silence.

Limite mesurée et assumée : la recherche est **lexicale**. « Quelle est la
capitale du Cameroun ? » rend des passages sur le Tchad et la Guinée équatoriale,
qui *bordent* le Cameroun et contiennent les deux mots. Trouver par le sens
demandera des vecteurs d'embedding.

### ✅ DÉBLOQUÉ le 2026-08-10 — on enseigne un GESTE, pas des faits

La sortie a été trouvée en changeant ce qu'on enseigne. On ne lui apprend pas
des faits, mais **un geste** : « étant donné un texte et des mots, rends la
phrase du texte qui contient ces mots ». Cet exemple-là se fabrique **par pure
recopie** — le contexte est un passage réel, la question est faite de mots
**extraits de ce passage**, la réponse est une phrase **copiée mot pour mot**.
Aucune machine n'affirme quoi que ce soit, donc rien ne peut être faux qui ne le
soit déjà dans le corpus. Mode `--citations` (voir `NkIlyanaCitation.h`).

**Second enseignement, aussi important** : un quart des exemples porte sur des
mots **absents** du passage, et la réponse juste est « Je ne trouve pas cela dans
ce texte. » — vérifiée mécaniquement avant d'être écrite. Sans ces exemples-là, un
modèle à qui on n'a montré que des réponses trouvées apprend qu'il faut
**toujours** répondre, et invente quand le texte se tait.

Trois défauts trouvés en fabriquant les premiers exemples, tous corrigés :
- un passage contenant déjà « Question: » ou « Reponse: » **déplace le repère de
  masquage** de l'entraînement : le modèle apprendrait à prédire le contexte au
  lieu de la réponse, sans qu'aucune erreur ne soit signalée. Ces passages sont
  maintenant refusés ;
- un contexte d'**une seule phrase** rend l'exercice trivial : la réponse est le
  contexte entier, et on enseigne à recopier son entrée au lieu de choisir. Trois
  phrases minimum ;
- un mot présent **plusieurs fois** dans le contexte ne désigne pas une phrase
  unique : l'exercice aurait deux solutions. Seuls les mots à occurrence unique
  sont retenus.

### L'ancienne question, conservée pour mémoire

Chercher marche. **Se servir de ce qu'on a trouvé, non** — et ce n'est pas un
travail de câblage, c'est une décision de conception.

Pour qu'Ilyana exploite un passage placé devant sa question, il faut qu'elle ait
appris le format « Contexte → Question → Réponse ». Elle ne l'a **jamais vu** à
l'entraînement : lui coller un passage devant la question aujourd'hui, elle
l'ignorera. Il faut donc des exemples d'apprentissage de cette forme.

Et c'est là qu'est le conflit : fabriquer ces exemples revient à **inventer des
questions et des réponses** à partir des passages — exactement la donnée
potentiellement erronée qui a fait écarter le corpus synthétique. Trois voies,
aucune gratuite :

1. **Extractif mécanique** — la réponse est une phrase *copiée* du passage, jamais
   rédigée. Rien n'est inventé côté réponse ; reste à produire la question sans
   l'inventer non plus, ce qui n'est pas résolu.
2. **Questions réelles** — n'utiliser que des paires question/réponse d'origine
   humaine attestée. Propre, mais il faut la source.
3. **Assumer un générateur** pour la seule *forme*, en acceptant que le contenu
   soit faux, puisque ce qu'on enseigne est « recopie le passage », pas le fait.
   Dangereux : ce qui est vu à l'entraînement s'imprime, même présenté comme forme.

⚠️ Ne pas brancher `--chercher` dans `--parler` avant que ce point soit tranché :
un branchement qui « marche » sans que le modèle sache lire le contexte donnerait
l'illusion d'une réponse sourcée alors qu'elle serait devinée — le pire des deux
mondes, parce qu'elle serait alors *crue*.

---

## 4. Apprendre de ses torts

Dans une conversation : oui, elle reconnaît et corrige (c'est dans le contexte).
**Durablement : non** — un modèle entraîné est figé.

La boucle réelle, à construire : **consigner les corrections** que Rihen lui
apporte → **les verser au corpus** → **réentraîner**. Elle apprend de ses torts
par cycles, pas à l'instant. Ne pas laisser croire autre chose.

---

## 5. Le cap : Ilyana chef d'orchestre du moteur

> Rappel de la règle de cadrage (Rihen) : **l'IA doit servir le moteur**. Si
> l'entraînement long passe avant le branchement au moteur, le projet dérive.

Elle ne « pilote » pas en texte libre : elle **émet des commandes typées**, celles
que le moteur sait déjà exécuter et annuler. C'est le patron déjà prouvé par
`NKMeshAITest` (observation → politique → commande), et c'est ce qui rend les
actions rejouables et apprenables.

| domaine | vocabulaire d'actions | où c'est déjà |
|---|---|---|
| Modéliser | `NkMeshEditCommand`, journal `.nkmec` | NK3DModeler, `NKMeshAITest` ✅ |
| Animer | commandes de pose / IK | `Applications/NkAnima` |
| Parler | synthèse vocale | `Kernel/AI/NKSpeech`, NKTTS (LJSpeech + Griffin-Lim) |
| Interface 2D | `NkUIComponent` (donnée, pas code) | Engine/Noge |
| Peindre | opérations sur `NKImage` / NKCanvas | à définir |
| Civilisation | agents `NKCivilization` | ✅ Jalon 1 |

**Pourquoi ce patron et pas du texte libre** : une commande typée se valide, se
rejoue, s'annule et s'apprend. Du texte libre ne se vérifie pas — et une IA qui
agit sans qu'on puisse contrôler son action est exactement ce qu'on ne veut pas.

### 5bis. L'intégrer dans NKCode, NkAnima, NK3DModeler, NKCinema, PV3DE, Noge

Question posée par Rihen le 2026-08-12 : « on pourra facilement l'intégrer
partout, pour qu'elle manipule directement depuis les binaires ? » Réponse
mesurée sur le code existant, en séparant ce qui est acquis de ce qui reste.

**Ce qui est DÉJÀ en place** (et qui rend la suite crédible) :
- `NKGpt` est une **bibliothèque du noyau réutilisable** : son en-tête l'annonce
  (« n'importe quelle application remplit un `NkGptConfig` »), l'API tient en
  `Prepare()` + `Generate()`. Une app la lie par `dependson("NKGpt")`.
- `--parler` charge un modèle avec `resume=false` : **poids seuls, sans corpus
  ni état d'optimiseur**. C'est déjà le chemin d'inférence.
- **NKCode a son point de branchement** : `NkAiPanel.h` a un sélecteur de
  fournisseur dont l'entrée **`2 = IA maison (NkAI)`** existe déjà à côté de
  Claude et Ollama.
- **NK3DModeler a son vocabulaire d'actions** : `NkMeshEditOp::{Extrude,
  ExtrudeEdges, ExtrudeVerts, Delete, Merge, MakeFace, Subdivide, LoopCut,
  Bevel, Inset, Dissolve, Move}` — annulables, rejouables, journalisables.

**Les trois obstacles RÉELS, dans l'ordre où ils se poseront** :

1. **Le device GPU partagé — le vrai sujet.** NKRenderer tient un device
   NKRHI ; l'inférence de NKGpt passe par NkTensor, qui prend le sien. Deux
   devices dans un même processus, c'est le piège déjà documenté pour
   NKCanvas vs NKRenderer. Trois issues : (a) faire passer l'inférence par le
   device NKRHI de l'application, (b) inférer sur CPU (lent mais sans conflit),
   (c) **processus séparé** qui ne reçoit que des observations et ne rend que
   des commandes typées. La (c) est la plus sûre et découple les cycles de vie
   — c'est celle à privilégier tant que (a) n'est pas mesuré.
2. **Il manque un moteur d'INFÉRENCE PUR.** On passe aujourd'hui par
   `NkGptTrainer`, une classe faite pour entraîner. À mesurer avant d'intégrer :
   ce qu'elle alloue réellement en mode génération (les poids font 123 Mo ; les
   moments d'Adam en ajouteraient 247 inutilement). Chantier : `NkGptInference`
   (poids seuls + KV-cache), ou la garantie mesurée que `resume=false` suffit.
3. **La capacité, et c'est le point à ne pas enjoliver.** Le modèle 32 M est
   entraîné sur de la **prose, de l'identité et de la citation**. Il ne sait
   PAS émettre un `NkMeshEditOp`, et il ne l'apprendra pas par branchement :
   il faut un **corpus d'actions** (observation → commande) par application.
   `NKMeshAITest` a prouvé le patron (mesh → traits → politique → commande).
   « Qu'elle manipule tout ce qu'elle veut » n'est donc ni l'objectif ni
   atteignable : l'objectif est qu'elle **propose des commandes typées que
   l'application valide, exécute et sait annuler**.

**Ordre de marche proposé** : (1) mesurer l'empreinte de `--parler` ;
(2) extraire un `NkIlyanaAgent` partagé (charge le modèle, expose
`Observer/Proposer`) ; (3) le brancher d'abord sur **NKCode** (son panneau
attend déjà le fournisseur « IA maison », et le texte y est la sortie
naturelle) ; (4) puis **NK3DModeler**, première application à ACTION, avec son
vocabulaire déjà écrit ; (5) les autres suivent le même moule.

---

## 6. Ordre de marche

1. 🔶 **Corpus réel** — préparer Wikipédia FR (2,1 Go déjà là) : extraction,
   nettoyage, attribution **CC BY-SA** (cf. `docs/SOURCES_TIERCES.md` et
   `THIRD_PARTY_LICENSES.md`). Abandonner le corpus synthétique.
2. ⏳ **Charte en corpus** — écrire les échanges de la section 2, y compris les
   deux faces de la fermeté.
3. ⏳ **Accélérer le moteur** — GPU à 28 %. Chaque heure gagnée ici vaut des
   jours plus tard.
4. ⏳ **Récupération documentaire** — elle lit avant de répondre.
5. ⏳ **Entraînement à la vraie taille** — 100-150 M, sur données réelles.
6. ⏳ **Boucle de correction**.
7. ⏳ **Orchestration** — un sous-système à la fois, en commençant par celui qui
   a déjà son vocabulaire d'actions : la modélisation.

---

## PDF — PHASE 0 : sondage du corpus avant d'étendre le lecteur (2026-08-13)

**Méthode** : le périmètre du lecteur a été fixé par mesure sur 95 documents
réels (en-tête de `NkPdf.h`). Toute extension suit la même discipline — on
implémente ce que le corpus contient, pas la spécification. Outil :
`NKIlyana --sonder --dossier <dir> [--csv f]` (`NkIlyanaSondePdf.h`), qui lit
les clés via le modèle d'objets (donc à travers les flux d'objets compressés)
et cherche les filtres dans les octets bruts — fiable, car la spécification
interdit qu'un flux vive dans un flux d'objets.

**Corpus** : `D:\softwareRenderer\Rodolf\Cours`, **258 fichiers**, 255 ouverts,
3 chiffrés (refusés — comportement voulu).

| clé | documents | % des ouverts | volume |
|---|---|---|---|
| `/Info` non vide | **255** | **100 %** | dont `/Title` : 89 (35 %) |
| `/Annots` | 194 | 76 % | — |
| ⤷ dont `/Link` | **187** | **73 %** | **56 051 liens** |
| ⤷ dont `/Text` | 4 | 2 % | — |
| ⤷ dont `/Highlight` | 0 | 0 % | — |
| `/StructTreeRoot` | **140** | **55 %** | — |
| `/Dests` | 140 | 55 % | — |
| `/Outlines` | 53 | 21 % | **10 354 signets** (≈195 par document qui en a) |
| `/Metadata` (XMP) | 40 | 16 % | — |
| `/Lang` | 39 | 15 % | — |
| `/AcroForm /Fields` | **2** | **1 %** | — |
| `/Names /EmbeddedFiles` | **0** | **0 %** | — |

| filtre | documents |
|---|---|
| DCT (JPEG) | 104 |
| **LZWDecode** | **5** |
| CCITTFax | 3 · JBIG2 1 · JPX 0 |

### Ce que la mesure change dans le plan — DEUX phases supprimées, UNE remontée

**⛔ Supprimées** (les écrire serait exactement l'erreur que le sondage doit
éviter) :
- **`/AcroForm`** : 2 documents sur 255. Des formulaires administratifs, sans
  intérêt pour une bibliothèque de cours.
- **`/EmbeddedFiles`** : **zéro** document. Rien à écrire.
- **`/Metadata` XMP** : 16 %, et **entièrement redondant** avec `/Info`, présent
  à 100 %. On n'ajoute pas un parseur XML pour une source moins bien couverte
  que celle qu'on a déjà.

**⭐ Remontée — `LZWDecode`, de dernière à deuxième position.** Le sondage
mesurait 0 % sur l'ancien corpus de 95 PDF ; il en trouve **5** sur 258. Et le
croisement avec le balayage de lecture est sans appel : **les 5 sont en échec**
(4 « VIDE », 1 « charabia »). Vérification de causalité faite — le LZW porte
sur les **flux de contenu de page** (7 occurrences sur 10 dans `Gdmphys1.pdf`,
4 sur 4 dans `phys_model.pdf`), pas seulement sur des images : sans lui, ces
pages ne sont jamais décodées.

> C'est le seul chantier qui **débloque des documents entièrement illisibles**
> — 5 sur les 35 encore inaccessibles, soit 14 % du reliquat — pour ~150 lignes.
> Toutes les autres phases enrichissent des documents **déjà lus**.

### Ordre définitif proposé

| ordre | chantier | couverture | pourquoi ici |
|---|---|---|---|
| **1** | `/Info` + `/Lang` | 100 % / 15 % | universel, coût modéré ; `/Lang` est quasi gratuit une fois le catalogue accessible, et dit à Ilyana si un document est français ou anglais |
| **2** | **LZWDecode** | 5 docs | seul chantier qui rend LISIBLE ce qui ne l'est pas ; ~150 lignes |
| **3** | `/StructTreeRoot` | 55 % | la plus forte valeur *qualitative* (ordre de lecture logique = qualité du texte d'Ilyana), mais la plus coûteuse : exige les MCID dans `NkPdfRender` |
| **4** | `/Dests` + `/Outlines` | 55 % / 21 % | découpage naturel en chapitres ; `/Dests` est la dépendance technique de 3 et 5 |
| **5** | `/Annots` `/Link` | 73 % | 56 051 liens ; les URL valent pour les citations. Réutilise la résolution de destinations |

⚠️ **Un point bloquant relevé et corrigé** : `Trailer()` et `Catalog()`
n'étaient pas exposés (`mTrailer`/`mRoot` privés). Or `/Info` vit dans le
trailer et tout le reste dans le catalogue — aucune couche externe ne pouvait
les atteindre. Deux accesseurs **en lecture seule, strictement additifs** ont
été ajoutés à `NkPdf.h` : c'est le minimum indispensable, et ils n'exposent
rien de plus que ce que le modèle d'objets rend déjà public.

---

## PDF — ✅ RÉSOLU le 2026-08-11 : trois causes distinctes, trois correctifs

Les 3 documents nommés du lot sont tous lus désormais (mesures avant → après) :

| document | avant | après |
|---|---|---|
| ebin.pub 2D graphics (466 p.) | 96 % illisibles, REFUSÉ | **0 illisible**, 1513 passages |
| Game Audio CppCon (156 p.) | 98 % illisibles, REFUSÉ | **0 illisible**, 546 passages |
| computergraphics OpenGL 2e (535 p.) | **0 page** (ne s'ouvrait pas) | 0,13 % illisibles, 3744 passages |

**Cause 1 — parseur CMap multi-sections** (`NkPdfFont.cpp::ParseToUnicode`).
`while (!keyword(i, "endbfchar"))` testait le mot-clé sur l'espace *avant* lui —
jamais vrai — et `readHex` sautait par-dessus la fin de section : la section
`bfrange` suivante était dévorée comme des paires `bfchar`, les plages jamais
déployées. Un CMap à section unique passait (d'où les PDF qui marchaient) ; un
CMap multi-sections (générateurs d'ebooks) était mutilé en silence. Fix :
sauter les blancs, tester le mot-clé de fin, refuser d'avancer hors `<`.

**Cause 2 — pas de repli sur l'encodage de base** (`NkPdfFont`). Les PDF sortis
de PowerPoint/Word écrivent leur texte en polices TrueType simples **sans**
`/ToUnicode` mais avec `/Encoding /WinAnsiEncoding` — encodage **publié par la
spec** (ISO 32000, annexe D). Fix : tables WinAnsi/MacRoman générées par script
(codecs cp1252/mac_roman, jamais retapées à la main), repli dans `ToUnicode()`.
`/Differences` non implémenté (0 occurrence dans les documents du fonds testés).

**Cause 3 — chaîne `/Prev` bornée à 32** (`NkPdfLoad.cpp::LoadXrefAt`). Un livre
retouché sous Acrobat portait **53** mises à jour incrémentales ; le corps
d'origine (catalogue, pages) est au bout de la chaîne → « 0 page » sans un mot.
Fix : anti-cycle par liste d'offsets visités (`mXrefSeen`), profondeur 1024 en
garde-fou.

Au passage : le cache de polices était par **nom seul** (`/F4`) — deux polices
homonymes de ressources différentes (formulaires XObject) se partageaient un
slot. Corrigé (clé = nom + identité du dictionnaire), même si ce n'était pas la
cause ici. Et la sonde de diagnostic est désormais **non biaisée** : relevée au
moment exact de l'échec `ToUnicode`, sur la police effectivement interrogée
(l'ancienne comparait les premiers codes du flux à la table de la *dernière*
police vue — deux objets sans rapport).

**Balayage complet du dossier Cours après correctifs — 258 PDF** :
**199 acceptés (77 %)** contre ~17 % avant ; 29 « vides » (~26 scans que seul
un OCR lirait, 2 chiffrés refusés franchement) ; 29 refusés charabia = famille
**LaTeX/dvips Type1 sans `/ToUnicode`** (arXiv, notes Eberly).

**Cause 4 (2026-08-11, sur demande de Rihen) — décodage par NOMS de glyphes** :
`/Encoding /Differences` (spec PDF) et, à défaut, la table en CLAIR du
programme Type 1 (`dup N /nom put`, avant eexec — aucun charstring interprété)
donnent code → nom ; la **Adobe Glyph List** (donnée publiée BSD-3, 4281
entrées générées par script — `NkPdfGlyphList.{h,cpp}`, attribution dans
`THIRD_PARTY_LICENSES.md`) donne nom → Unicode. **Mesure : 24 des 29 refusés
récupérés** (arXiv à 0 % d'illisible, ligatures `fi` comprises) → le fonds
Cours passe à **223/258 lisibles (86 %)**. Les 5 restants portent des noms
opaques de sous-ensemble (`/a35`) : seul le dessin des glyphes sait ce qu'ils
sont — OCR ou rien. Pour les sources LaTeX : préférer le `.tex`, toujours.

**Au passage, bug racine dans l'inflate du dépôt** (`NkDeflate::zFill`,
NKImage) : à l'épuisement du flux d'octets il déclarait une erreur, même quand
les derniers symboles étaient déjà dans le registre. Les flux **zlib**
(PDF/PNG) n'y tombaient jamais — leurs 4 octets d'Adler-32 servaient de marge
silencieuse — mais tout flux **brut** fini à l'octet près (corps gzip d'une
réponse HTTP, entrées ZIP) était rejeté après avoir été entièrement décodé.
Fix : zéros bornés injectés en fin de flux. Mesuré : le gzip forcé de
`gamemath.com` se décode ; chemin zlib inchangé (mêmes 546 passages sur le
PDF témoin).

---

## (historique) PDF — état au 2026-08-11 avant résolution, et LE point d'entrée exact

**La « régression » du jour n'existait pas** : le fichier de test avait été
déplacé lors d'un filtrage du fonds. Le programme rendait « 0 octet, 0 opération »
sur un chemin vide — indiscernable d'un lecteur en panne. Trois reconstructions et
deux hypothèses fausses (objets périmés, puis conflit mbed-TLS) ont été dépensées
avant de poser la question élémentaire : *le fichier est-il encore là ?*
→ `--ajouter` dit désormais **INTROUVABLE**, avec la raison.

**TLS remis en OPTION** (`NK_ENABLE_TLS=1` active mbedTLS *et* `bcrypt`). Il avait
été mis par défaut, puis soupçonné à tort ; l'isolement l'a innocenté. L'option
propre est ce qui a permis cet isolement, elle reste donc telle quelle.

### La mesure a eu lieu, et elle RÉFUTE l'hypothèse principale

```
codes DEMANDÉS par le flux    : 21  39  38  82  80  83
codes CONTENUS dans la table  :  4   6   8  65  66 107   (28 entrées)
```

Les deux séries sont de **petits entiers du même ordre** : ce n'est donc PAS un
décalage « un octet contre deux ». Cette piste est fermée.

### ⚠️ MAIS l'instrumentation est biaisée — à corriger AVANT de conclure

Les codes demandés viennent des **premières** opérations de texte, tandis que la
table affichée est celle de la **dernière** police vue (`LastFont()`). Ce sont
potentiellement deux polices différentes : on compare des pommes et des poires.

**LE point d'entrée** : relever la table de la police **effectivement
interrogée** — c'est-à-dire, dans `NkPdfRenderer` au moment où `emit` appelle
`f->ToUnicode(code)`, enregistrer une fois le couple (code demandé, présence dans
`f`) plutôt que de comparer après coup deux objets sans rapport. Une dizaine de
lignes, une exécution, et la cause devrait être visible.

### Ce qui est déjà acquis et ne doit pas être refait

- Les tables `/ToUnicode` **se lisent toutes** (273 006 déclarées, 273 006 lues) :
  ni la décompression ni le parseur `bfchar`/`bfrange` ne sont en cause.
- Le chemin `/Encoding` → noms de glyphes **ne concerne pas ces documents** : ils
  déclarent bien leurs tables. Ne pas rouvrir cette voie.
- Une police sans programme de glyphes reste **lisible** (correctif conservé), et
  un document dont plus de 25 % des caractères sont indéchiffrables est **refusé**
  plutôt que déposé en charabia.
- Sur 12 PDF réels : 6 sont des documents-images (0 caractère même pour
  `pdftotext`, confirmé indépendamment par 0 ordre de texte côté moteur) — hors de
  portée sans reconnaissance de caractères. L'écart à combler est de **4**.

## (historique) Régression supposée du 2026-08-11 — RÉSOLUE, aucun défaut de code

**Symptôme** : `--ajouter` sur un PDF qui fonctionnait rend désormais
`contenu 0 o, 0 operations`. Le document n'est plus ouvert du tout. Le même
fichier donnait auparavant 21 Mo de contenu et ~2 000 000 d'opérations.

**Ce qui a changé entre les deux mesures**, et rien d'autre :
1. `NKNetwork.jenga` : TLS passé d'opt-in à **actif par défaut** ;
2. `NKIlyana.jenga` : ajout de `NKMbedTLS` aux dépendances ;
3. `NKIlyana.jenga` : ajout de `bcrypt` aux bibliothèques Windows ;
4. `NkPdfFont.h` : deux accesseurs **inline** (pas de changement de disposition
   mémoire, donc peu suspect).

**Écarté** : les objets périmés. NKMedia ET NKIlyana ont été entièrement
reconstruits, la régression persiste.

**Prochaine étape — COUPER LE SUSPECT, pas le réparer.** Reconstruire avec
`NK_DISABLE_TLS=1` (et sans `NKMbedTLS` ni `bcrypt` dans NKIlyana.jenga) puis
relancer la même mesure. Si la lecture PDF revient, la cause est bien le lien ;
sinon elle est ailleurs et les trois changements ci-dessus sont innocents. Cette
mesure doit être faite AVANT toute tentative de correction.

**Hypothèse à vérifier ensuite** : un conflit de symbole entre mbed-TLS et le
décompresseur utilisé par le PDF — les deux embarquent du code de bas niveau, et
l'éditeur de liens choisit alors silencieusement l'un des deux.

⚠️ En attendant, l'aspiration de sites fonctionne (elle a besoin de TLS) mais la
lecture PDF ne fonctionne plus. Les deux ne peuvent pas coexister dans cette
construction — c'est précisément ce qu'il faut résoudre.

## PDF — diagnostic du 2026-08-10 sur le fonds réel (D:\softwareRenderer)

**Le fonds** : 8,4 Go, 267 PDF, 175 archives, 67 vidéos.

**Mesure sur 12 PDF, avec `pdftotext` comme ORACLE boîte noire** (légitime : on
compare des sorties, on ne lit aucun code) :

| | résultat |
|---|---|
| PDF dont l'oracle tire du texte | **6 / 12** |
| PDF à zéro caractère **même pour l'oracle** | **6 / 12** — documents-images, hors de portée sans OCR |
| PDF acceptés par notre lecteur | **2 / 12** |

**L'écart à combler est donc de 4 documents**, pas de 10. La moitié du fonds est
constituée de scans que rien ne lira sans reconnaissance de caractères.

### ⚠️ L'hypothèse « il manque /Encoding » était FAUSSE

J'allais implémenter `/Encoding` → noms de glyphes → Unicode. Une sonde des
dictionnaires de polices (script Python, diagnostic seulement) a montré que ces
documents **ONT leur table `/ToUnicode`** :

| document | polices | avec /ToUnicode |
|---|---|---|
| ebin.pub 2d-computer-graphics | 17 | **17 (100 %)** |
| computergraphics OpenGL | 491 | 417 (85 %) |
| Game Audio Programming | 11 | 9 |

Le chantier `/Encoding` aurait donc réparé un défaut qui n'est pas celui-là.
**Mesurer avant d'implémenter a évité un travail entier à côté de la plaque.**

### Le vrai défaut, localisé

Sur `ebin.pub` (17/17 polices avec table) notre lecteur rend :
`695450 caractères rencontrés, dont 666545 sans équivalent lisible (96 %)`.

La table existe, elle est trouvée, **et elle ne rend rien**. Les polices en cause
sont massivement **Type0 / CIDFontType2 en encodage Identity** — deux octets par
caractère. Pistes à examiner dans cet ordre :

1. `ParseToUnicode` : `DecodeStream` échoue-t-il sur le flux CMap (filtre non
   géré) ? Un échec y est **silencieux** — la police reste valide, simplement
   sans aucune entrée. C'est la piste la plus probable.
2. Le code cherché dans `mUniCodes` est-il bien celui que `NextCode` produit pour
   une police à deux octets ?
3. `ToUnicode()` fait une **recherche linéaire** sur des milliers d'entrées, pour
   chacun des ~700 000 caractères : correct mais coûteux — à indexer une fois le
   fond réparé.

### ✅ Instrumentation posée, et elle a tranché

Deux compteurs distinguent désormais « le document ne déclare rien » (limite du
document) de « il déclare une table que nous ne savons pas lire » (notre défaut) :
`AvaitToUnicode()` vs `HasToUnicode()`, remontés dans les `Stats` du rendu.

**Mesure sur `ebin.pub` : 273 006 tables déclarées, 273 006 effectivement lues.**

⛔ **La piste 1 est RÉFUTÉE** : les tables se décompressent et se lisent toutes.
Le `bfchar`/`bfrange` fonctionne.

**Il ne reste donc qu'une explication**, et elle est précise : le code présenté à
`ToUnicode(code)` **n'est pas celui que la table indexe**. Ces polices sont des
Type0/CIDFontType2 en Identity — deux octets par caractère —, et la table indexe
ces codes à deux octets. Le point de rupture est donc dans `NextCode()` (lecture
du code dans la chaîne) ou dans ce que l'appelant transmet ensuite.

**Point d'entrée pour la prochaine session** : instrumenter `NkPdfFont::ToUnicode`
pour afficher, sur les 20 premiers appels d'un document en échec, le code demandé
à côté des 5 premiers codes présents dans `mUniCodes`. Si les ordres de grandeur
diffèrent (par exemple un octet contre deux), la cause est immédiate. C'est UNE
mesure, et elle devrait clore le sujet.

⚠️ Ne pas repartir sur `/Encoding` + noms de glyphes : cette voie a été explorée
et ne concerne pas ces documents.

## Journal du 2026-08-10 — ce qui a marché, ce qui a échoué

**PHASE 2 (identité) — RÉUSSIE.** Batterie **4/19 → 8/19**. Elle nomme son père et
sa mère. Corpus : 25 % d'identité entrelacée avec de la prose (`--melange`).
Modèle promu : `ilyana_phase2.nkgp`.

**PHASE 3 (citation) — ÉCHOUÉE deux fois, la troisième invalidée.**

| essai | pas d'apprentissage | batterie | citation | verdict |
|---|---|---|---|---|
| 3a | 1e-05 (calendrier hérité) | 6/19 | non | non promu |
| 3b | 2e-04 (calendrier neuf) | 5/19 | non | non promu |
| 3c | 1e-04 | 4/19 | non | **NUL** — run mort au pas 30 |

⚠️ **Le verdict 3c ne vaut rien** : le run s'est arrêté après 161 s. J'ai mesuré
une batterie sur un modèle entraîné trente pas et failli en conclure quelque
chose. Toujours vérifier qu'un run VIT avant d'attendre son résultat.

### Trois défauts du MOTEUR trouvés en cherchant pourquoi

1. **Reprendre un run ≠ commencer une phase.** Le calendrier du pas d'apprentissage
   se prolongeait, si bien qu'une phase destinée à enseigner un comportement
   héritait d'un pas déjà au plancher (1e-05). Assez pour graver un nom répété des
   milliers de fois — d'où la réussite de la phase 2 — très insuffisant pour un
   geste nouveau. → `--nouvelle-phase` (commit `f5b62505`).

2. **Les fenêtres coupaient les exemples, et lui apprenaient à INVENTER.** Le lot
   était prélevé à un décalage tiré au hasard dans un flux plat de tokens. Un
   exemple structuré de ~180 tokens dans une fenêtre de 256 n'est entier qu'une
   fois sur trois — et quand la fenêtre commence au milieu du contexte, le modèle
   voit une question suivie de « Reponse: » avec un contexte **amputé**, et on lui
   apprend à produire une phrase qui n'y figure pas. C'est exactement le
   comportement qu'on cherchait à combattre. → une fenêtre sur deux démarre à un
   début de bloc (commit `ce296ee0`). **Hypothèse encore non testée** au moment où
   ces lignes sont écrites.

3. **Le filet de sécurité tuait des runs légitimes.** Il calculait
   `(initiale − actuelle) / initiale` et concluait « perte figée » dès que le
   résultat était négatif. Or une perte qui **monte** est du mouvement : le calcul
   a lieu, et une hausse au démarrage d'une phase, pendant la montée en puissance
   du pas, est normale. Il devait regarder la **valeur absolue**. Un garde-fou qui
   tue ce qu'il devait protéger coûte le run *et* la confiance dans la mesure.

**3d — fenêtres alignées, run complet (94 min) : batterie 6/19, citation NON.**
L'alignement a fait remonter la batterie de 5 à 6, sans donner le geste. Signe
instructif : elle produit désormais le *style* d'une réponse sourcée
(« "Le nom de la localité", est attesté sous les formes… ») — elle a appris à quoi
RESSEMBLE une citation, pas à lire celle qu'on lui donne.

**Bilan : trois essais valides, trois échecs, chacun testant un mécanisme
différent (pas d'apprentissage, alignement, dosage). Ce n'est donc plus un
problème de recette.**

### Ce qu'il reste à examiner maintenant que les recettes sont épuisées

L'alignement des fenêtres écarté, la piste suivante n'est plus une recette mais la
**taille** : recopier une phrase depuis un contexte demande au modèle d'apprendre
à faire correspondre des motifs à distance, ce qui est connu pour n'émerger qu'à
partir d'une certaine échelle. Ce serait alors un argument pour le passage à
~32 M paramètres (déjà envisagé pour le vocabulaire 32k), et non pour un réglage
de plus.

## Ce qui est livré (2026-08-09)

- **`NKData/NkBpeTrainer`** — BPE à l'échelle : mots uniques pondérés, comptes
  incrémentaux, tas à invalidation paresseuse. **16 128 fusions sur 25 Mo en
  1,6 s** là où l'ancien plafonnait à 800 Ko. Prouvé par `NKBpeTest` (19 OK) :
  comptes confrontés à un recomptage complet, segmentation d'entraînement ==
  segmentation d'encodage, réversibilité octet pour octet.
- **`Applications/NKIlyana`** — `--data` (tri en trois bacs + identité +
  dialogues), `--train`, `--causer`.
- **Identité dans les POIDS** : au pas 2000, elle répond « Mon père est TEUGUIA
  TADJUIDJE Rodolf Sederis ». Une consigne système n'aurait rien laissé.
- **Garde-fou** : l'entraînement s'arrête si la perte n'a pas bougé après 30 pas
  (prouvé sur la configuration B=24 qui échouait en silence).

## Bugs et pièges connus

- ⚠️ **Au-delà d'une certaine taille de lot, le GPU ne calcule plus, en
  silence** : à B=24 la perte reste collée à `ln(V)` et le run paraît 3,6× plus
  rapide. **Cause racine non identifiée** (aucun défaut d'allocation signalé) —
  à chercher côté dispatch/submit. Contournement : B=12/accum=2, validé.
- ⚠️ **Corpus en CRLF** : le découpage cherche `"\n\n"` et n'en trouve aucun →
  un seul bloc, masquage inopérant, sans message. Normaliser à la lecture.
- ⚠️ **Runtime C++ dédoublé** (mingw64 vs ucrt64) : corruption de tas dans
  glslang au premier noyau compute. Corrigé par `-static-libstdc++ -static-libgcc`.
